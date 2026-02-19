# Pyrite64 → Vibe Coding Game + Cartoon Animation Engine
## Transformation Scaffold

> Fork: HailToDodongo/pyrite64  
> Stack: Electron + TypeScript (editor) · C / libdragon + tiny3d (N64 runtime)  
> Milestone 1: Three.js 3D Viewport  

---

## What We're Adding on Top of Pyrite64

| Layer | What changes |
|---|---|
| **Cartoon Renderer** | New N64 render mode: cel-shading, outline pass, flat-palette materials |
| **Animation Engine** | Keyframe timeline panel + skeletal animation export to N64 |
| **Vibe Coding** | Claude API node in the Node-Graph: describe behavior → generate control-flow |
| **3D Viewport** | Three.js live preview replacing current viewport, with N64-accurate constraints |

---

## Directory Structure (Delta on fork)

```
pyrite64/                          ← existing repo root
│
├── src/                           ← existing Electron editor source
│   ├── main/                      (Electron main process)
│   │   └── ipc/
│   │       ├── build.ipc.ts       ← invoke CLI build pipeline
│   │       └── emulator.ipc.ts    ← launch Ares / gopher64
│   │
│   └── renderer/                  (Electron renderer process)
│       ├── index.html
│       ├── index.ts
│       │
│       ├── viewport/              ← NEW: Three.js 3D Viewport
│       │   ├── Viewport3D.ts      ← Main viewport class (see stub below)
│       │   ├── N64MaterialBridge.ts  ← Maps Fast64 materials → Three.js shaders
│       │   ├── CartoonPass.ts     ← Cel-shade + outline post-process
│       │   ├── GridHelper.ts      ← N64 unit grid overlay
│       │   └── CameraController.ts   ← Orbit + fly cam
│       │
│       ├── timeline/              ← NEW: Animation Timeline
│       │   ├── Timeline.ts        ← Keyframe track editor
│       │   ├── Clip.ts            ← Animation clip data model
│       │   └── N64AnimExporter.ts ← Bake keyframes → tiny3d anim format
│       │
│       ├── vibe/                  ← NEW: AI-assisted scripting
│       │   ├── VibeNode.ts        ← Claude API node for Node-Graph
│       │   └── prompts/
│       │       ├── movement.txt
│       │       ├── enemy_ai.txt
│       │       └── animation_trigger.txt
│       │
│       ├── panels/                ← existing panels (keep, extend)
│       │   ├── SceneTree.ts
│       │   ├── Properties.ts
│       │   ├── AssetManager.ts
│       │   └── NodeGraph.ts       ← add VibeNode to palette
│       │
│       └── styles/
│           ├── theme-n64.css      ← retro CRT aesthetic
│           └── cartoon.css
│
├── n64/                           ← existing N64 C runtime
│   ├── engine/                    (existing)
│   └── cartoon/                   ← NEW: Cartoon render module
│       ├── cel_shader.c
│       ├── cel_shader.h
│       ├── outline_pass.c         ← edge detect via RDP trick
│       └── palette_reduce.c       ← quantize to N64 palette bands
│
└── tools/                         ← NEW: build helpers
    ├── gltf_to_n64.ts             ← enhanced GLTF importer with cartoon mat support
    └── bake_lightmap.ts           ← cartoon flat-shading bake
```

---

## Milestone 1: Three.js Viewport

**Goal:** Replace / extend existing viewport with a live Three.js scene preview that:
1. Loads the same scene graph used by the N64 runtime
2. Applies N64-accurate constraints (64 tris/mesh budget warning, 32×32 / 64×64 / 256×256 texture slots)
3. Has a **Cartoon Preview toggle** that shows cel-shading approximation in-editor
4. Matches the camera model used by tiny3d

### Files to create first:
- `src/renderer/viewport/Viewport3D.ts` ← see stub file
- `src/renderer/viewport/N64MaterialBridge.ts`
- `src/renderer/viewport/CartoonPass.ts`
- `src/renderer/viewport/CameraController.ts`

### npm deps to add:
```json
{
  "three": "^0.172.0",
  "@types/three": "^0.172.0",
  "postprocessing": "^6.36.0"
}
```

---

## Milestone 2: Cartoon Render Mode

**N64 side (C):**
- `cel_shader.c`: single diffuse band threshold using tiny3d's combiner modes
- `outline_pass.c`: 2-pass trick — first pass draws solid slightly scaled mesh black, second draws normal  
- `palette_reduce.c`: quantize vertex colors to 4–8 discrete steps

**Editor side:**
- CartoonPass.ts: Three.js EffectComposer with OutlineEffect + custom CelShader
- Material panel gains "Cartoon Mode" toggle that syncs to N64 combiner settings

---

## Milestone 3: Animation Timeline

**Data model:**
```typescript
interface AnimClip {
  name: string;
  duration: number;          // in seconds
  tracks: AnimTrack[];
}
interface AnimTrack {
  targetNode: string;        // scene node name
  property: 'position' | 'rotation' | 'scale';
  keyframes: Keyframe[];
}
interface Keyframe {
  time: number;
  value: [number, number, number];
  easing: 'linear' | 'step' | 'bezier';
}
```

**N64 export:**  
Bake at 30fps → fixed-point arrays → emit as C header included by entity .c file.  
Tiny3d supports skeletal via `T3DModel` — wire into existing model loading.

---

## Milestone 4: Vibe Coding Node

**Concept:** In the Node-Graph, add a "🎙 Vibe" node. User types natural language:
> *"patrol between point A and B, play attack animation when player is within 3 units"*

Claude API generates the Node-Graph JSON config (state machine + transitions) which gets deserialized back into the graph canvas.

**IPC flow:**
```
VibeNode.ts (renderer)
  → IPC: 'vibe:generate' + prompt
    → main process
      → Anthropic API (claude-sonnet)
        → returns NodeGraphConfig JSON
    → IPC reply
  → deserialize into NodeGraph canvas
```

**Key constraint:** Output must be a valid subset of Pyrite64's Node-Graph format — no heap allocations, no dynamic strings. The system prompt enforces this.

---

## Immediate Next Steps

1. `npm install three @types/three postprocessing` in the editor package
2. Create `src/renderer/viewport/Viewport3D.ts` (stub provided separately)
3. Wire the viewport canvas into the main editor layout HTML
4. Create `src/renderer/viewport/N64MaterialBridge.ts` to map Fast64 material JSON → `THREE.MeshToonMaterial`
5. Add N64 polygon budget overlay (red highlight > 64 tris per mesh)

---

## N64 Constraints Cheat Sheet (for viewport accuracy)

| Constraint | Value | Notes |
|---|---|---|
| Texture sizes | 32, 64, 128, 256 px | 256 only with big-tex mode |
| Vertex colors | 8-bit per channel | Quantize preview |
| Max tris/mesh | ~64 recommended | RDP display list limits |
| Coordinate system | Y-up, Z-forward | Same as Three.js default |
| Fixed-point coords | 16.16 | Sub-unit precision limit |
| Max verts/frame | ~800 (safe) | Budget warning in viewport |
| Audio channels | 32 (RSP mixer) | |
| RDRAM total | 8MB (4MB base) | Asset budget bar |
