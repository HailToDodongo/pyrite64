/**
 * AnimationClip.ts
 * Animation data model for Pyrite64's keyframe animation system.
 *
 * Represents skeletal and transform animations that can be:
 *  - Edited in the AnimationTimeline panel
 *  - Previewed in the Three.js viewport
 *  - Exported to N64 tiny3d format via N64AnimExporter
 *
 * All keyframe values use engine coordinates (Y-up, Z-forward).
 * Time values are in seconds; export bakes to 30fps fixed-point.
 */

// ─── Easing ───────────────────────────────────────────────────────────────────

/**
 * Easing modes supported by the N64 runtime.
 * 'linear' and 'step' are free on hardware; 'bezier' requires a small
 * RSP interpolation routine (included in Pyrite64's engine overlay).
 */
export type EasingMode = 'linear' | 'step' | 'bezier';

// ─── Keyframe ─────────────────────────────────────────────────────────────────

export interface Keyframe {
  /** Time offset within the clip, in seconds. */
  time:   number;
  /** Value at this keyframe — 3-component (pos/rot/scale). */
  value:  [number, number, number];
  /** Interpolation to the *next* keyframe. */
  easing: EasingMode;
}

// ─── Track ────────────────────────────────────────────────────────────────────

export type TrackProperty = 'position' | 'rotation' | 'scale';

export interface AnimTrack {
  /** Scene-graph node name this track drives. */
  targetNode: string;
  /** Which transform channel. */
  property:   TrackProperty;
  /** Ordered list of keyframes (sorted by time). */
  keyframes:  Keyframe[];
}

// ─── Clip ─────────────────────────────────────────────────────────────────────

export interface AnimClip {
  /** Unique clip name (e.g. "idle", "run", "attack"). */
  name:     string;
  /** Total duration in seconds. */
  duration: number;
  /** Whether the clip loops on the N64 runtime. */
  loop:     boolean;
  /** Ordered list of tracks. */
  tracks:   AnimTrack[];
}

// ─── Clip helpers ─────────────────────────────────────────────────────────────

/** Create a blank clip with sensible defaults. */
export function createClip(name: string, duration = 1.0): AnimClip {
  return { name, duration, loop: false, tracks: [] };
}

/** Add a track to a clip. Returns the new track. */
export function addTrack(
  clip: AnimClip,
  targetNode: string,
  property: TrackProperty,
): AnimTrack {
  const track: AnimTrack = { targetNode, property, keyframes: [] };
  clip.tracks.push(track);
  return track;
}

/** Insert a keyframe into a track, keeping keyframes sorted by time. */
export function insertKeyframe(track: AnimTrack, kf: Keyframe): void {
  const idx = track.keyframes.findIndex(k => k.time > kf.time);
  if (idx === -1) {
    track.keyframes.push(kf);
  } else {
    track.keyframes.splice(idx, 0, kf);
  }
}

/** Remove the keyframe at a given index. */
export function removeKeyframe(track: AnimTrack, index: number): void {
  if (index >= 0 && index < track.keyframes.length) {
    track.keyframes.splice(index, 1);
  }
}

/**
 * Evaluate a track at a given time using the appropriate easing.
 * Returns the interpolated [x, y, z] value.
 */
export function evaluateTrack(
  track: AnimTrack,
  time: number,
): [number, number, number] {
  const kfs = track.keyframes;
  if (kfs.length === 0) return [0, 0, 0];
  if (kfs.length === 1 || time <= kfs[0].time) return [...kfs[0].value];
  if (time >= kfs[kfs.length - 1].time) return [...kfs[kfs.length - 1].value];

  // Find surrounding keyframes
  let lo = 0;
  for (let i = 0; i < kfs.length - 1; i++) {
    if (time >= kfs[i].time && time < kfs[i + 1].time) {
      lo = i;
      break;
    }
  }
  const a = kfs[lo];
  const b = kfs[lo + 1];
  const t = (time - a.time) / (b.time - a.time);

  return interpolate(a.value, b.value, t, a.easing);
}

function interpolate(
  a: [number, number, number],
  b: [number, number, number],
  t: number,
  easing: EasingMode,
): [number, number, number] {
  switch (easing) {
    case 'step':
      return [...a];
    case 'bezier': {
      // Smooth-step approximation (cubic Hermite)
      const s = t * t * (3 - 2 * t);
      return [
        a[0] + (b[0] - a[0]) * s,
        a[1] + (b[1] - a[1]) * s,
        a[2] + (b[2] - a[2]) * s,
      ];
    }
    default: // linear
      return [
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
      ];
  }
}

// ─── Serialization ────────────────────────────────────────────────────────────

/** Serialize a clip to a JSON string (.p64anim format). */
export function serializeClip(clip: AnimClip): string {
  return JSON.stringify(clip, null, 2);
}

/** Deserialize a clip from a JSON string. Throws on invalid data. */
export function deserializeClip(json: string): AnimClip {
  const obj = JSON.parse(json);
  validateClip(obj);
  return obj as AnimClip;
}

/** Validate that a parsed object conforms to the AnimClip schema. */
export function validateClip(obj: unknown): void {
  if (!obj || typeof obj !== 'object') {
    throw new Error('AnimClip must be a non-null object');
  }
  const clip = obj as Record<string, unknown>;

  if (typeof clip.name !== 'string' || clip.name.length === 0) {
    throw new Error('AnimClip.name must be a non-empty string');
  }
  if (typeof clip.duration !== 'number' || clip.duration <= 0) {
    throw new Error('AnimClip.duration must be a positive number');
  }
  if (typeof clip.loop !== 'boolean') {
    throw new Error('AnimClip.loop must be a boolean');
  }
  if (!Array.isArray(clip.tracks)) {
    throw new Error('AnimClip.tracks must be an array');
  }

  const validProps: TrackProperty[] = ['position', 'rotation', 'scale'];
  const validEasings: EasingMode[] = ['linear', 'step', 'bezier'];

  for (const track of clip.tracks as unknown[]) {
    if (!track || typeof track !== 'object') {
      throw new Error('Each track must be an object');
    }
    const t = track as Record<string, unknown>;
    if (typeof t.targetNode !== 'string') {
      throw new Error('track.targetNode must be a string');
    }
    if (!validProps.includes(t.property as TrackProperty)) {
      throw new Error(`track.property must be one of: ${validProps.join(', ')}`);
    }
    if (!Array.isArray(t.keyframes)) {
      throw new Error('track.keyframes must be an array');
    }
    for (const kf of t.keyframes as unknown[]) {
      if (!kf || typeof kf !== 'object') {
        throw new Error('Each keyframe must be an object');
      }
      const k = kf as Record<string, unknown>;
      if (typeof k.time !== 'number') {
        throw new Error('keyframe.time must be a number');
      }
      if (!Array.isArray(k.value) || k.value.length !== 3) {
        throw new Error('keyframe.value must be a 3-element array');
      }
      const valueComponents = k.value as unknown[];
      if (!valueComponents.every((component) => typeof component === 'number' && Number.isFinite(component))) {
        throw new Error('keyframe.value must contain three finite numbers');
      }
      if (!validEasings.includes(k.easing as EasingMode)) {
        throw new Error(`keyframe.easing must be one of: ${validEasings.join(', ')}`);
      }
    }
  }
}
