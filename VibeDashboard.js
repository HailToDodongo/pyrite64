/**
 * VibeDashboard.ts
 * Top-level orchestrator for the Pyrite64 Vibe Coding chat dashboard.
 *
 * Layout (grid):
 *  ┌────────────────────────────────────────────────────────────┐
 *  │  [TopBar: logo · project name · build/run actions]        │
 *  ├──────────────┬─────────────────────────────┬──────────────┤
 *  │  VibeSidebar │   Viewport3D (Three.js)     │  VibeChat    │
 *  │  - Nav tabs  │   - Live 3D scene preview   │  - History   │
 *  │  - Entities  │   - CRT scanline overlay    │  - Context   │
 *  │  - Overlays  │   - Floating toolbar        │  - Input     │
 *  │  - N64 budget│   - HUD stats               │  - Patches   │
 *  └──────────────┴─────────────────────────────┴──────────────┘
 *
 * Connection map:
 *  VibeSidebar.entitySelect     → VibeChat.updateContext  (entity name → AI context)
 *  VibeSidebar.renderModeChange → Viewport3D.setRenderMode
 *  VibeSidebar.gridToggle       → Viewport3D.grid.setVisible
 *  VibeSidebar.cartoonToggle    → Viewport3D.setRenderMode('cartoon' | 'standard')
 *  Viewport3D.onBudgetWarning   → VibeSidebar.updateBudget + VibeChat.pushSystem
 *  Viewport3D.onSelectionChange → VibeSidebar.setActiveEntity + VibeChat.updateContext
 *  VibeChat.patchApply          → applyPatch() → emits onPatchApply
 *  VibeChat.contextChange       → kept in sync with sidebar selection
 */
import { VibeSidebar } from './VibeSidebar.js';
import { VibeChat } from './VibeChat.js';
import { Viewport3D, N64_LIMITS } from './Viewport3D.js';
import { VibeAnimTimeline } from './VibeAnimTimeline.js';
import { PROMPT_LIBRARY, getGroupedTemplates } from './VibePromptLibrary.js';
import { VibeAgentPool } from './VibeAgentPool.js';
import { VibeAgentStatusPanel } from './VibeAgentStatusPanel.js';
import { VibeCreatorPage } from './VibeCreatorPage.js';
// ─── VibeDashboard ────────────────────────────────────────────────────────────
export class VibeDashboard {
    constructor(opts) {
        this.viewport = null;
        this.isCreatorView = false;
        this.opts = opts;
        this.projectName = opts.projectName ?? 'untitled';
        this.activeEntity = opts.initialEntity ?? 'Player';
        this.sceneEntities = opts.initialScene ?? [];
        this.nodeTypes = opts.nodeTypes ?? DEFAULT_NODE_TYPES;
        // ── Build initial VibeContext ───────────────────────────────────────────
        const ctx = {
            entityName: this.activeEntity,
            availableNodeTypes: this.nodeTypes,
            sceneEntities: this.sceneEntities.map(e => e.name),
            animations: [],
            sounds: [],
        };
        // ── Construct sub-components ───────────────────────────────────────────
        this.agentPool = new VibeAgentPool();
        this.agentPanel = new VibeAgentStatusPanel(this.agentPool);
        this.sidebar = new VibeSidebar();
        this.chat = new VibeChat(ctx, this.agentPool);
        this.timeline = new VibeAnimTimeline();
        this.creatorPage = new VibeCreatorPage();
        this.toastHost = document.createElement('div');
        this.toastHost.id = 'vibe-toast-host';
        // ── Wire agent pool events ──────────────────────────────────────────────
        this.agentPool.on('dispatchStart', (d) => {
            this.toast(`⚡ ${d.agents.join(' + ')} dispatched`, 'info', 1800);
        });
        this.agentPool.on('dispatchDone', (r) => {
            const names = r.succeeded.join(' + ');
            const ms = r.totalMs;
            const nodes = r.merged.nodes.length;
            this.toast(`✓ ${names} — ${nodes} node${nodes !== 1 ? 's' : ''} in ${ms}ms`, 'success');
        });
        this.agentPool.on('dispatchError', (msg) => {
            this.toast(`✗ Agent error: ${msg}`, 'error');
        });
        // ── Wire events ────────────────────────────────────────────────────────
        this.wireSidebarEvents();
        this.wireChatEvents();
        this.wireTimelineEvents();
        this.wireCreatorEvents();
        // ── Build shell DOM ────────────────────────────────────────────────────
        this.mount();
        // ── Mount Viewport3D ───────────────────────────────────────────────────
        this.initViewport();
        // ── Seed initial data ──────────────────────────────────────────────────
        if (this.sceneEntities.length > 0) {
            this.sidebar.setEntities(this.sceneEntities);
            this.sidebar.setActiveEntity(this.sceneEntities[0]?.id ?? null);
        }
    }
    // ── Public API ─────────────────────────────────────────────────────────────
    /** Replace the scene entity list (e.g. after loading a .p64scene file). */
    setScene(entities) {
        this.sceneEntities = entities;
        this.sidebar.setEntities(entities);
        this.chat.updateContext({
            sceneEntities: entities.map(e => e.name),
        });
        this.updateHUDEntityCount(entities.length);
    }
    /** Add or update a 3D mesh node in the viewport. */
    setSceneNode(data) {
        this.viewport?.upsertNode(data);
    }
    /** Remove a 3D node from the viewport. */
    removeSceneNode(id) {
        this.viewport?.removeNode(id);
    }
    /** Update available node type palette for AI context. */
    setNodeTypes(types) {
        this.nodeTypes = types;
        this.chat.updateContext({ availableNodeTypes: types });
    }
    /** Set animations on the active entity. */
    setAnimations(animations) {
        this.chat.updateContext({ animations });
    }
    /** Set sound IDs available in the project. */
    setSounds(sounds) {
        this.chat.updateContext({ sounds });
    }
    /** Update top-bar project name. */
    setProjectName(name) {
        this.projectName = name;
        const el = document.querySelector('#topbar-project-name');
        if (el)
            el.textContent = name;
    }
    /** Show a toast notification. */
    toast(msg, type = 'info', durationMs = 3500) {
        const t = document.createElement('div');
        t.className = `vibe-toast ${type}`;
        t.textContent = msg;
        this.toastHost.appendChild(t);
        setTimeout(() => t.remove(), durationMs);
    }
    dispose() {
        this.viewport?.dispose();
        this.timeline.dispose();
        this.agentPanel.dispose();
        this.creatorPage.dispose();
        this.opts.container.innerHTML = '';
    }
    // ── Event wiring ───────────────────────────────────────────────────────────
    wireSidebarEvents() {
        this.sidebar
            .on('entitySelect', (name) => {
            this.activeEntity = name;
            this.chat.updateContext({ entityName: name });
            this.toast(`Focusing entity: ${name}`, 'info', 2000);
        })
            .on('renderModeChange', (mode) => {
            this.viewport?.setRenderMode(mode);
            this.updateHUDMode(mode);
        })
            .on('gridToggle', (visible) => {
            this.setViewportGridVisible(visible);
        })
            .on('cartoonToggle', (enabled) => {
            const mode = enabled ? 'cartoon' : 'standard';
            this.viewport?.setRenderMode(mode);
            this.sidebar.syncRenderMode(mode);
            this.updateHUDMode(mode);
        })
            .on('applyComponent', (comp) => {
            const patch = comp.buildGraph(this.activeEntity);
            if (this.opts.onPatchApply) {
                this.opts.onPatchApply(patch, this.activeEntity);
                this.toast(`Applied component: ${comp.label}`, 'success');
                this.chat.pushSystem(`Installed ${comp.label} component on ${this.activeEntity}.`);
            }
            else {
                this.toast(`Component ${comp.label} selected (no patch handler)`, 'info');
            }
        });
    }
    wireChatEvents() {
        this.chat
            .on('patchApply', (patch, msgId) => {
            this.chat.markPatchApplied(msgId);
            this.opts.onPatchApply?.(patch, this.activeEntity);
            this.toast(`Patch applied to ${this.activeEntity} (${patch.nodes.length} nodes)`, 'success');
        })
            .on('contextChange', (ctx) => {
            // Chat can request context updates (e.g. entity rename)
            if (ctx.entityName) {
                this.activeEntity = ctx.entityName;
                this.sidebar.setActiveEntity(this.sceneEntities.find(e => e.name === ctx.entityName)?.id ?? null);
            }
        });
    }
    wireTimelineEvents() {
        this.timeline
            .on('scrub', (time) => {
            // Sync playhead with viewport animation preview
        })
            .on('export', (tracks) => {
            // Convert keyframe data into a node graph patch
            this.toast(`Exported ${tracks.length} tracks to node graph`, 'info');
        })
            .on('play', () => {
            this.toast('Timeline playing', 'info', 1500);
        });
    }
    wireCreatorEvents() {
        this.creatorPage
            .on('presetApply', (preset, config) => {
            this.toast(`Preset applied: ${preset.title}`, 'success');
        })
            .on('openChat', (prompt) => {
            this.showDashboardView();
            this.chat.setInput(prompt);
        })
            .on('back', () => {
            this.showDashboardView();
        });
    }
    /** Switch to the Creator page view. */
    showCreatorView() {
        if (this.isCreatorView)
            return;
        this.isCreatorView = true;
        this.shellEl.classList.add('hidden');
        this.creatorPage.el.classList.remove('hidden');
    }
    /** Switch back to the main dashboard (editor) view. */
    showDashboardView() {
        if (!this.isCreatorView)
            return;
        this.isCreatorView = false;
        this.shellEl.classList.remove('hidden');
        this.creatorPage.el.classList.add('hidden');
    }
    /** Get prompt templates available for the current context. */
    getPromptTemplates() {
        const ctx = {
            entityName: this.activeEntity,
            availableNodeTypes: this.nodeTypes,
            sceneEntities: this.sceneEntities.map(e => e.name),
            animations: [],
            sounds: [],
        };
        return getGroupedTemplates(ctx);
    }
    /** Apply a prompt template from the library. */
    applyTemplate(templateId) {
        const tmpl = PROMPT_LIBRARY.find(t => t.id === templateId);
        if (!tmpl)
            return;
        const ctx = {
            entityName: this.activeEntity,
            availableNodeTypes: this.nodeTypes,
            sceneEntities: this.sceneEntities.map(e => e.name),
            animations: [],
            sounds: [],
        };
        const prompt = tmpl.build(ctx);
        this.chat.setInput(prompt);
        this.toast(`Template: ${tmpl.title}`, 'info', 2000);
    }
    /** Get the timeline component for external integration. */
    getTimeline() {
        return this.timeline;
    }
    // ── Viewport init ──────────────────────────────────────────────────────────
    initViewport() {
        const canvasHost = document.getElementById('viewport-canvas-host');
        if (!canvasHost)
            return;
        const vpOpts = {
            container: canvasHost,
            onBudgetWarning: (w) => this.onBudgetWarning(w),
            onSelectionChange: (ids) => this.onViewportSelection(ids),
        };
        try {
            this.viewport = new Viewport3D(vpOpts);
            this.viewport.startRendering();
        }
        catch (e) {
            console.warn('[VibeDashboard] Three.js not available — viewport disabled.', e);
            canvasHost.innerHTML = `
        <div style="
          display:flex; align-items:center; justify-content:center;
          height:100%; flex-direction:column; gap:12px;
          color:var(--text-dim); font-size:12px; text-align:center;
        ">
          <div style="font-size:32px; opacity:0.3;">◈</div>
          <div>Three.js viewport not available</div>
          <div style="font-size:10px; opacity:0.6;">Install three + postprocessing</div>
        </div>
      `;
        }
    }
    onBudgetWarning(w) {
        // Update sidebar budget strip using the warning type to indicate which budget is exceeded
        const snapshot = {
            tris:    { used: w.type === 'tris'  ? N64_LIMITS.MAX_TRIS_PER_MESH + 1   : 0, max: N64_LIMITS.MAX_TRIS_PER_MESH },
            verts:   { used: w.type === 'verts' ? N64_LIMITS.MAX_VERTS_PER_FRAME + 1 : 0, max: N64_LIMITS.MAX_VERTS_PER_FRAME },
            rdramKB: { used: w.type === 'rdram' ? 4097                               : 0, max: 4096 },
        };
        this.sidebar.updateBudget(snapshot);
        // Notify in chat if critical
        if (w.type === 'tris' || w.type === 'verts') {
            this.chat.pushSystem(`⚠ N64 budget warning: ${w.message}`);
        }
        // Update HUD dot
        this.updateHUDBudgetDots(w);
    }
    onViewportSelection(ids) {
        if (ids.length === 0)
            return;
        const firstId = ids[0];
        const entity = this.sceneEntities.find(e => e.id === firstId);
        if (entity) {
            this.activeEntity = entity.name;
            this.sidebar.setActiveEntity(firstId);
            this.chat.updateContext({ entityName: entity.name });
        }
    }
    setViewportGridVisible(visible) {
        this.viewport?.setGridVisible(visible);
    }
    // ── DOM construction ───────────────────────────────────────────────────────
    mount() {
        const c = this.opts.container;
        c.innerHTML = '';
        const shell = document.createElement('div');
        shell.id = 'vibe-dashboard';
        this.shellEl = shell;
        // Top bar
        shell.appendChild(this.buildTopBar());
        // Sidebar column (sidebar + agent status panel)
        const sidebarCol = document.createElement('div');
        sidebarCol.id = 'vibe-sidebar-col';
        sidebarCol.appendChild(this.sidebar.el);
        sidebarCol.appendChild(this.agentPanel.el);
        shell.appendChild(sidebarCol);
        // Viewport
        shell.appendChild(this.buildViewportPanel());
        // Timeline (below viewport, spans middle column)
        shell.appendChild(this.timeline.el);
        // Chat
        shell.appendChild(this.chat.el);
        c.appendChild(shell);
        // Creator page (hidden by default, toggled via Create button)
        this.creatorPage.el.classList.add('hidden');
        c.appendChild(this.creatorPage.el);
        c.appendChild(this.toastHost);
    }
    buildTopBar() {
        const bar = document.createElement('div');
        bar.id = 'vibe-topbar';
        bar.innerHTML = `
      <!-- Logo -->
      <div class="topbar-logo">
        <div class="logo-mark">P</div>
        <span>Pyrite<span style="color:var(--neon-cyan);">64</span></span>
      </div>
      <div class="topbar-divider"></div>

      <!-- Project name -->
      <div class="topbar-project">
        <span id="topbar-project-name">${this.escapeHTML(this.projectName)}</span>
        <span style="color:var(--text-dim);"> · vibe engine</span>
      </div>

      <!-- Actions -->
      <div class="topbar-actions" id="topbar-actions"></div>
    `;
        const actions = bar.querySelector('#topbar-actions');
        const createBtn = document.createElement('button');
        createBtn.className = 'btn accent';
        createBtn.innerHTML = '◈ Create';
        createBtn.addEventListener('click', () => {
            this.showCreatorView();
        });
        const buildBtn = document.createElement('button');
        buildBtn.className = 'btn';
        buildBtn.innerHTML = '⬡ Build';
        buildBtn.addEventListener('click', () => {
            this.toast('Build started…', 'info');
            this.opts.onBuild?.();
        });
        const runBtn = document.createElement('button');
        runBtn.className = 'btn primary';
        runBtn.innerHTML = '▶ Run';
        runBtn.addEventListener('click', () => {
            this.toast('Launching emulator…', 'info');
            this.opts.onRun?.();
        });
        actions.appendChild(createBtn);
        actions.appendChild(buildBtn);
        actions.appendChild(runBtn);
        return bar;
    }
    buildViewportPanel() {
        const panel = document.createElement('div');
        panel.id = 'vibe-viewport';
        panel.innerHTML = `
      <!-- Canvas host (Three.js mounts here) -->
      <div id="viewport-canvas-host"></div>

      <!-- CRT scanline overlay (CSS ::after) -->

      <!-- Floating toolbar -->
      <div id="viewport-toolbar">
        <button class="vp-btn active" data-mode="standard">Standard</button>
        <button class="vp-btn" data-mode="cartoon">Cartoon</button>
        <div class="vp-btn-sep"></div>
        <button class="vp-btn" data-mode="wireframe">Wire</button>
        <button class="vp-btn" data-mode="n64-accurate">N64</button>
        <div class="vp-btn-sep"></div>
        <button class="vp-btn" id="vp-btn-grid" title="Toggle grid">Grid</button>
      </div>

      <!-- HUD overlays -->
      <div id="viewport-hud">
        <!-- Top left: render stats -->
        <div class="hud-corner top-left" id="hud-tl">
          <div class="hud-stat">
            <div class="hud-stat-dot" id="hud-tris-dot"></div>
            <span id="hud-tris-stat">Tris: 0 / 64</span>
          </div>
          <div class="hud-stat">
            <div class="hud-stat-dot" id="hud-vert-dot"></div>
            <span id="hud-vert-stat">Verts: 0 / 800</span>
          </div>
          <div class="hud-stat">
            <div class="hud-stat-dot" id="hud-ram-dot"></div>
            <span id="hud-ram-stat">RDRAM: 0K / 4096K</span>
          </div>
        </div>

        <!-- Top right: mode + entity -->
        <div class="hud-corner top-right" id="hud-tr">
          <div id="hud-mode-stat" style="color:var(--neon-cyan); font-weight:700;">STANDARD</div>
          <div id="hud-ent-stat" style="margin-top:2px;">Entity: —</div>
        </div>

        <!-- Bottom right: camera hint -->
        <div class="hud-corner bottom-right" style="font-size:9px; opacity:0.4;">
          RMB drag — orbit  ·  Scroll — zoom  ·  MMB drag — pan
        </div>
      </div>
    `;
        // Wire toolbar render mode buttons
        panel.querySelectorAll('[data-mode]').forEach(btn => {
            btn.addEventListener('click', () => {
                const mode = btn.dataset.mode;
                panel.querySelectorAll('[data-mode]').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                this.viewport?.setRenderMode(mode);
                this.sidebar.syncRenderMode(mode);
                this.updateHUDMode(mode);
            });
        });
        // Wire grid toggle
        panel.querySelector('#vp-btn-grid')?.addEventListener('click', (e) => {
            const btn = e.currentTarget;
            const visible = btn.classList.toggle('active');
            this.setViewportGridVisible(visible);
        });
        // Cache HUD refs
        this.hudTrisStat = panel.querySelector('#hud-tris-stat');
        this.hudVertStat = panel.querySelector('#hud-vert-stat');
        this.hudRamStat = panel.querySelector('#hud-ram-stat');
        this.hudModeStat = panel.querySelector('#hud-mode-stat');
        this.hudEntStat = panel.querySelector('#hud-ent-stat');
        return panel;
    }
    // ── HUD updaters ───────────────────────────────────────────────────────────
    updateHUDMode(mode) {
        if (this.hudModeStat) {
            this.hudModeStat.textContent = mode.toUpperCase().replace('-', ' ');
        }
    }
    updateHUDBudgetDots(w) {
        const dotId = w.type === 'tris' ? 'hud-tris-dot' :
            w.type === 'verts' ? 'hud-vert-dot' :
                w.type === 'rdram' ? 'hud-ram-dot' : null;
        if (dotId) {
            const dot = document.getElementById(dotId);
            dot?.classList.add('warn');
        }
    }
    updateHUDEntityCount(_count) {
        // Will be shown in top-right HUD area in future
        if (this.hudEntStat) {
            this.hudEntStat.textContent = `Entities: ${_count}`;
        }
    }
    // ── Utils ──────────────────────────────────────────────────────────────────
    escapeHTML(s) {
        return s
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
    }
}
// ─── Default node types (matches Pyrite64 built-in palette) ──────────────────
const DEFAULT_NODE_TYPES = [
    // Entry points
    'OnStart', 'OnTick', 'OnCollide', 'OnAnimEnd', 'OnTimer',
    'OnButtonPress', 'OnButtonHeld', 'OnButtonRelease', 'OnStateChange',
    // Flow control
    'Branch', 'Sequence', 'Repeat', 'Wait', 'SwitchCase', 'StateMachine',
    // Movement
    'MoveToward', 'MoveDirection', 'SetVelocity', 'SetPosition',
    // Animation
    'PlayAnim', 'StopAnim', 'SetAnimSpeed', 'SetAnimBlend', 'WaitAnimEnd',
    // Audio
    'PlaySound', 'StopSound',
    // Spawning & destruction
    'Spawn', 'Destroy',
    // Visibility & scene
    'SetVisible', 'SceneLoad',
    // Input
    'ReadStick',
    // State management
    'SetState', 'GetState',
    // Values & math
    'GetPosition', 'GetDistance', 'GetHealth', 'SetHealth',
    'GetScore', 'AddScore', 'Value', 'Compare', 'CompBool', 'MathOp',
    // Misc
    'Func', 'Arg', 'Note', 'DebugLog',
];
