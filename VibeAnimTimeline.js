/**
 * VibeAnimTimeline.js
 * Keyframe timeline / sequencer component for the Pyrite64 Vibe Dashboard.
 *
 * Renders a horizontal timeline with:
 *  - Track lanes for each animation property (position, rotation, scale, clip)
 *  - Keyframe diamonds that can be selected/moved
 *  - A playhead scrubber
 *  - Playback controls (play / pause / stop / loop)
 *  - Zoom & scroll
 *
 * The timeline drives animation preview in Viewport3D and can export
 * keyframe data to the node graph as PlayAnim / SetAnimBlend sequences.
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │ ▶ ■ ⟳  │ 0:00       0:30       1:00       1:30       2:00 │
 * ├─────────┼────────────────────────────────────────────────────┤
 * │ pos.x   │   ◆──────────◆                    ◆              │
 * │ pos.y   │         ◆────────◆                               │
 * │ rot.y   │              ◆────────────────◆                  │
 * │ clip    │ ▓▓▓▓ walk ▓▓▓▓│▓▓▓ run ▓▓▓▓▓▓│▓▓ idle ▓▓       │
 * └─────────┴────────────────────────────────────────────────────┘
 */
// ─── Constants ────────────────────────────────────────────────────────────────
const TRACK_HEIGHT = 28;
const HEADER_HEIGHT = 32;
const LABEL_WIDTH = 80;
const KEYFRAME_SIZE = 10;
const MIN_ZOOM = 40; // px per second
const MAX_ZOOM = 400;
const DEFAULT_ZOOM = 120;
const TICK_MINOR_INTERVAL = 0.25; // seconds
const TICK_MAJOR_INTERVAL = 1.0;
const PLAYHEAD_COLOR = '#e040fb';
const GRID_COLOR = 'rgba(255,255,255,0.06)';
const GRID_MAJOR_COLOR = 'rgba(255,255,255,0.12)';
// ─── VibeAnimTimeline ─────────────────────────────────────────────────────────
export class VibeAnimTimeline {
    constructor(initialTracks, duration = 2.0) {
        this.listeners = {};
        this.animFrameId = null;
        this.lastFrameTs = 0;
        // Drag state
        this.draggingKeyframe = null;
        this.draggingPlayhead = false;
        this.state = {
            tracks: initialTracks ?? [],
            duration,
            playheadTime: 0,
            playing: false,
            looping: true,
            zoom: DEFAULT_ZOOM,
            scrollX: 0,
            selectedKeyframes: new Set(),
        };
        this.el = document.createElement('div');
        this.el.className = 'vibe-timeline';
        // Build controls bar
        this.controls = this.buildControls();
        this.el.appendChild(this.controls);
        // Build canvas
        this.canvas = document.createElement('canvas');
        this.canvas.className = 'timeline-canvas';
        this.el.appendChild(this.canvas);
        this.ctx2d = this.canvas.getContext('2d');
        // Events
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', () => this.onMouseUp());
        this.canvas.addEventListener('wheel', (e) => this.onWheel(e), { passive: false });
        this.canvas.addEventListener('dblclick', (e) => this.onDoubleClick(e));
        // Initial render
        requestAnimationFrame(() => this.resizeAndDraw());
        if (typeof ResizeObserver !== 'undefined') {
            new ResizeObserver(() => this.resizeAndDraw()).observe(this.el);
        }
    }
    // ── Public API ─────────────────────────────────────────────────────────────
    on(event, cb) {
        var _a;
        ((_a = this.listeners)[event] ?? (_a[event] = [])).push(cb);
        return this;
    }
    setTracks(tracks) {
        this.state.tracks = tracks;
        this.draw();
    }
    addTrack(track) {
        this.state.tracks.push(track);
        this.draw();
    }
    setDuration(d) {
        this.state.duration = Math.max(0.1, d);
        this.draw();
    }
    getState() {
        return this.state;
    }
    dispose() {
        if (this.animFrameId !== null)
            cancelAnimationFrame(this.animFrameId);
    }
    // ── Controls bar ───────────────────────────────────────────────────────────
    buildControls() {
        const bar = document.createElement('div');
        bar.className = 'timeline-controls';
        const mkBtn = (label, title, onClick) => {
            const b = document.createElement('button');
            b.className = 'tl-btn';
            b.textContent = label;
            b.title = title;
            b.addEventListener('click', onClick);
            return b;
        };
        const playBtn = mkBtn('▶', 'Play', () => this.togglePlay());
        const stopBtn = mkBtn('■', 'Stop', () => this.stop());
        const loopBtn = mkBtn('⟳', 'Loop', () => this.toggleLoop(loopBtn));
        loopBtn.classList.add('active');
        const exportBtn = mkBtn('⬡ Export', 'Export keyframes to node graph', () => {
            this.emit('export', this.state.tracks);
        });
        exportBtn.style.marginLeft = 'auto';
        const timeLabel = document.createElement('span');
        timeLabel.className = 'tl-time';
        timeLabel.id = 'tl-time-label';
        timeLabel.textContent = '0:00.00';
        bar.append(playBtn, stopBtn, loopBtn, timeLabel, exportBtn);
        // Store refs
        this._playBtn = playBtn;
        this._timeLabel = timeLabel;
        return bar;
    }
    togglePlay() {
        if (this.state.playing) {
            this.pause();
        }
        else {
            this.play();
        }
    }
    play() {
        this.state.playing = true;
        this._playBtn.textContent = '⏸';
        this.lastFrameTs = performance.now();
        this.tick();
        this.emit('play');
    }
    pause() {
        this.state.playing = false;
        this._playBtn.textContent = '▶';
        if (this.animFrameId !== null) {
            cancelAnimationFrame(this.animFrameId);
            this.animFrameId = null;
        }
        this.emit('pause');
    }
    stop() {
        this.pause();
        this.state.playheadTime = 0;
        this.updateTimeLabel();
        this.draw();
        this.emit('stop');
    }
    toggleLoop(btn) {
        this.state.looping = !this.state.looping;
        btn.classList.toggle('active', this.state.looping);
    }
    tick() {
        const now = performance.now();
        const dt = (now - this.lastFrameTs) / 1000;
        this.lastFrameTs = now;
        this.state.playheadTime += dt;
        if (this.state.playheadTime >= this.state.duration) {
            if (this.state.looping) {
                this.state.playheadTime %= this.state.duration;
            }
            else {
                this.state.playheadTime = this.state.duration;
                this.pause();
                return;
            }
        }
        this.updateTimeLabel();
        this.draw();
        this.emit('scrub', this.state.playheadTime);
        if (this.state.playing) {
            this.animFrameId = requestAnimationFrame(() => this.tick());
        }
    }
    updateTimeLabel() {
        const t = this.state.playheadTime;
        const sec = Math.floor(t);
        const frac = Math.floor((t - sec) * 100);
        const label = this._timeLabel;
        if (label)
            label.textContent = `${sec}:${String(frac).padStart(2, '0')}`;
    }
    // ── Canvas drawing ─────────────────────────────────────────────────────────
    resizeAndDraw() {
        const rect = this.el.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        const w = rect.width;
        const h = HEADER_HEIGHT + this.state.tracks.length * TRACK_HEIGHT + 4;
        this.canvas.width = w * dpr;
        this.canvas.height = h * dpr;
        this.canvas.style.width = `${w}px`;
        this.canvas.style.height = `${h}px`;
        this.ctx2d.setTransform(dpr, 0, 0, dpr, 0, 0);
        this.draw();
    }
    draw() {
        const c = this.ctx2d;
        const W = this.canvas.width / (window.devicePixelRatio || 1);
        const H = this.canvas.height / (window.devicePixelRatio || 1);
        const { tracks, zoom, scrollX, duration, playheadTime, selectedKeyframes } = this.state;
        c.clearRect(0, 0, W, H);
        // ── Background ──
        c.fillStyle = 'rgba(10, 8, 18, 0.85)';
        c.fillRect(0, 0, W, H);
        // ── Time ruler (header) ──
        c.fillStyle = 'rgba(20, 16, 32, 0.9)';
        c.fillRect(0, 0, W, HEADER_HEIGHT);
        // Minor ticks
        const startTime = Math.max(0, scrollX / zoom);
        const endTime = Math.min(duration, (scrollX + W) / zoom);
        for (let t = Math.floor(startTime / TICK_MINOR_INTERVAL) * TICK_MINOR_INTERVAL; t <= endTime; t += TICK_MINOR_INTERVAL) {
            const x = LABEL_WIDTH + (t * zoom) - scrollX;
            if (x < LABEL_WIDTH)
                continue;
            const isMajor = Math.abs(t % TICK_MAJOR_INTERVAL) < 0.001;
            c.strokeStyle = isMajor ? GRID_MAJOR_COLOR : GRID_COLOR;
            c.lineWidth = isMajor ? 1 : 0.5;
            c.beginPath();
            c.moveTo(x, 0);
            c.lineTo(x, H);
            c.stroke();
            if (isMajor) {
                c.fillStyle = 'rgba(255,255,255,0.5)';
                c.font = '9px monospace';
                c.fillText(`${t.toFixed(1)}s`, x + 2, 12);
            }
        }
        // ── Track labels + lanes ──
        for (let i = 0; i < tracks.length; i++) {
            const y = HEADER_HEIGHT + i * TRACK_HEIGHT;
            // Lane background
            c.fillStyle = i % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'rgba(255,255,255,0.04)';
            c.fillRect(0, y, W, TRACK_HEIGHT);
            // Label
            c.fillStyle = tracks[i].color;
            c.font = '10px monospace';
            c.fillText(tracks[i].label, 6, y + TRACK_HEIGHT / 2 + 3);
            // Keyframes
            for (const kf of tracks[i].keyframes) {
                const kx = LABEL_WIDTH + (kf.time * zoom) - scrollX;
                if (kx < LABEL_WIDTH - KEYFRAME_SIZE || kx > W + KEYFRAME_SIZE)
                    continue;
                const ky = y + TRACK_HEIGHT / 2;
                const selected = selectedKeyframes.has(kf.id);
                c.save();
                c.translate(kx, ky);
                c.rotate(Math.PI / 4);
                if (tracks[i].type === 'clip') {
                    // Clip keyframes: filled rectangles
                    c.fillStyle = selected ? '#ffffff' : tracks[i].color;
                    c.fillRect(-KEYFRAME_SIZE / 2, -KEYFRAME_SIZE / 2, KEYFRAME_SIZE, KEYFRAME_SIZE);
                }
                else {
                    // Number keyframes: diamonds
                    c.fillStyle = selected ? '#ffffff' : tracks[i].color;
                    c.fillRect(-KEYFRAME_SIZE / 2, -KEYFRAME_SIZE / 2, KEYFRAME_SIZE, KEYFRAME_SIZE);
                    if (selected) {
                        c.strokeStyle = tracks[i].color;
                        c.lineWidth = 2;
                        c.strokeRect(-KEYFRAME_SIZE / 2, -KEYFRAME_SIZE / 2, KEYFRAME_SIZE, KEYFRAME_SIZE);
                    }
                }
                c.restore();
            }
        }
        // ── Label column separator ──
        c.strokeStyle = 'rgba(255,255,255,0.1)';
        c.lineWidth = 1;
        c.beginPath();
        c.moveTo(LABEL_WIDTH, 0);
        c.lineTo(LABEL_WIDTH, H);
        c.stroke();
        // ── Playhead ──
        const phx = LABEL_WIDTH + (playheadTime * zoom) - scrollX;
        if (phx >= LABEL_WIDTH && phx <= W) {
            c.strokeStyle = PLAYHEAD_COLOR;
            c.lineWidth = 2;
            c.beginPath();
            c.moveTo(phx, 0);
            c.lineTo(phx, H);
            c.stroke();
            // Playhead triangle at top
            c.fillStyle = PLAYHEAD_COLOR;
            c.beginPath();
            c.moveTo(phx - 5, 0);
            c.lineTo(phx + 5, 0);
            c.lineTo(phx, 8);
            c.closePath();
            c.fill();
        }
    }
    // ── Mouse interaction ──────────────────────────────────────────────────────
    canvasToTime(clientX) {
        const rect = this.canvas.getBoundingClientRect();
        const px = clientX - rect.left - LABEL_WIDTH + this.state.scrollX;
        return Math.max(0, Math.min(this.state.duration, px / this.state.zoom));
    }
    hitTestKeyframe(clientX, clientY) {
        const rect = this.canvas.getBoundingClientRect();
        const mx = clientX - rect.left;
        const my = clientY - rect.top;
        for (let i = 0; i < this.state.tracks.length; i++) {
            const ty = HEADER_HEIGHT + i * TRACK_HEIGHT + TRACK_HEIGHT / 2;
            if (Math.abs(my - ty) > KEYFRAME_SIZE)
                continue;
            for (const kf of this.state.tracks[i].keyframes) {
                const kx = LABEL_WIDTH + (kf.time * this.state.zoom) - this.state.scrollX;
                if (Math.abs(mx - kx) <= KEYFRAME_SIZE) {
                    return { trackIdx: i, kfId: kf.id };
                }
            }
        }
        return null;
    }
    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const my = e.clientY - rect.top;
        // Playhead header drag?
        if (my < HEADER_HEIGHT) {
            this.draggingPlayhead = true;
            this.state.playheadTime = this.canvasToTime(e.clientX);
            this.updateTimeLabel();
            this.draw();
            this.emit('scrub', this.state.playheadTime);
            return;
        }
        // Keyframe hit?
        const hit = this.hitTestKeyframe(e.clientX, e.clientY);
        if (hit) {
            this.draggingKeyframe = hit.kfId;
            if (!e.shiftKey)
                this.state.selectedKeyframes.clear();
            this.state.selectedKeyframes.add(hit.kfId);
            this.draw();
            this.emit('keyframeSelect', [...this.state.selectedKeyframes]);
        }
        else {
            this.state.selectedKeyframes.clear();
            this.draw();
        }
    }
    onMouseMove(e) {
        if (this.draggingPlayhead) {
            this.state.playheadTime = this.canvasToTime(e.clientX);
            this.updateTimeLabel();
            this.draw();
            this.emit('scrub', this.state.playheadTime);
            return;
        }
        if (this.draggingKeyframe) {
            const newTime = this.canvasToTime(e.clientX);
            // Find and move the keyframe
            for (const track of this.state.tracks) {
                const kf = track.keyframes.find(k => k.id === this.draggingKeyframe);
                if (kf) {
                    kf.time = Math.round(newTime * 40) / 40; // snap to 1/40s (N64 frame rate ~30fps)
                    this.emit('keyframeMove', kf.id, kf.time);
                    break;
                }
            }
            this.draw();
        }
    }
    onMouseUp() {
        this.draggingPlayhead = false;
        this.draggingKeyframe = null;
    }
    onWheel(e) {
        e.preventDefault();
        if (e.ctrlKey || e.metaKey) {
            // Zoom
            const delta = e.deltaY > 0 ? 0.9 : 1.1;
            this.state.zoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.state.zoom * delta));
        }
        else {
            // Scroll
            this.state.scrollX = Math.max(0, this.state.scrollX + e.deltaX + e.deltaY);
        }
        this.draw();
    }
    onDoubleClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const my = e.clientY - rect.top;
        // Only on track lanes
        if (my < HEADER_HEIGHT)
            return;
        const trackIdx = Math.floor((my - HEADER_HEIGHT) / TRACK_HEIGHT);
        if (trackIdx < 0 || trackIdx >= this.state.tracks.length)
            return;
        const time = this.canvasToTime(e.clientX);
        const track = this.state.tracks[trackIdx];
        const defaultValue = track.type === 'clip' ? 'idle' : 0;
        const newKf = {
            id: `kf_${Date.now()}_${Math.random().toString(36).slice(2, 6)}`,
            time: Math.round(time * 40) / 40,
            value: defaultValue,
            easing: 'linear',
        };
        track.keyframes.push(newKf);
        track.keyframes.sort((a, b) => a.time - b.time);
        this.draw();
        this.emit('keyframeAdd', track.id, newKf.time, newKf.value);
    }
    // ── Event emit ─────────────────────────────────────────────────────────────
    emit(event, ...args) {
        for (const cb of this.listeners[event] ?? []) {
            cb(...args);
        }
    }
}
