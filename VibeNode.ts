/**
 * VibeNode.ts
 * AI-assisted scripting node for Pyrite64's Node-Graph editor.
 *
 * The user types natural language into this node. On confirm,
 * it calls the Anthropic API (via Electron IPC) and receives
 * a NodeGraphConfig JSON patch which gets inserted into the graph.
 *
 * Constraints enforced by the system prompt:
 *  - No heap allocations at runtime
 *  - No dynamic strings
 *  - Only valid Pyrite64 node types and connection types
 *  - Output must be serializable to the existing .p64graph format
 */

// ─── Types (subset of Pyrite64 Node-Graph format) ────────────────────────────

export interface NodeGraphNode {
  id:       string;
  type:     string;           // e.g. "OnTick", "MoveToward", "PlayAnim", "Branch"
  position: [number, number]; // canvas position
  data:     Record<string, unknown>;
}

export interface NodeGraphEdge {
  from:     string;  // node id
  fromPort: string;
  to:       string;
  toPort:   string;
}

export interface NodeGraphConfig {
  nodes: NodeGraphNode[];
  edges: NodeGraphEdge[];
}

// ─── IPC channel names (must match main process handlers) ────────────────────

export const VIBE_IPC = {
  GENERATE: 'vibe:generate',
  RESULT:   'vibe:result',
  ERROR:    'vibe:error',
} as const;

// ─── VibeNode ─────────────────────────────────────────────────────────────────

export interface VibeNodeOptions {
  /** Callback fired when the API returns a valid NodeGraphConfig patch. */
  onResult: (patch: NodeGraphConfig) => void;
  /** Callback fired on error. */
  onError:  (msg: string) => void;
}

export class VibeNode {
  private opts: VibeNodeOptions;

  constructor(opts: VibeNodeOptions) {
    this.opts = opts;
  }

  /**
   * Submit a natural-language prompt to the Anthropic API.
   * Returns immediately; result is delivered via the onResult callback.
   *
   * @param prompt  Plain English description of desired behavior
   * @param context Current scene context (node types available, entity names, etc.)
   */
  async generate(prompt: string, context: VibeContext): Promise<void> {
    // In Electron renderer, send via IPC to main process which holds the API key
    if (typeof window !== 'undefined' && (window as any).electronAPI) {
      (window as any).electronAPI.invoke(VIBE_IPC.GENERATE, { prompt, context })
        .then((result: NodeGraphConfig) => this.opts.onResult(result))
        .catch((err: Error) => this.opts.onError(err.message));
    } else {
      // Dev fallback: direct API call (requires ANTHROPIC_API_KEY in env)
      await this.directGenerate(prompt, context);
    }
  }

  // ── Private ────────────────────────────────────────────────────────────────

  private async directGenerate(prompt: string, context: VibeContext): Promise<void> {
    const systemPrompt = buildSystemPrompt(context);
    // Browser-safe API key access (mock or localStorage)
    const apiKey = (typeof window !== 'undefined' ? (window as any).ANTHROPIC_API_KEY : undefined)
  || (typeof localStorage !== 'undefined' ? localStorage.getItem('anthropic_key') : undefined)
  || '';
    
    if (!apiKey) {
      console.warn('ANTHROPIC_API_KEY not set — API call will fail with 401.');
    }

    try {
      const res = await fetch('https://api.anthropic.com/v1/messages', {
        method:  'POST',
        headers: { 
          'Content-Type': 'application/json',
          'x-api-key': apiKey,
          'anthropic-version': '2023-06-01'
        },
        body:    JSON.stringify({
          model:      'claude-sonnet-4-6',
          max_tokens: 2048,
          system:     systemPrompt,
          messages:   [{ role: 'user', content: prompt }],
        }),
      });

      if (!res.ok) {
        // Attempt to extract a useful error message from the response body.
        const rawBody = await res.text();
        let apiMessage = '';
        try {
          const parsed = JSON.parse(rawBody);
          // Many APIs use an { error: { message } } or { message } shape for errors.
          if (parsed && typeof parsed === 'object') {
            if (parsed.error && typeof parsed.error.message === 'string') {
              apiMessage = parsed.error.message;
            } else if (typeof parsed.message === 'string') {
              apiMessage = parsed.message;
            }
          }
        } catch {
          // Body was not JSON; fall back to raw text.
        }

        const base = `Anthropic API request failed with status ${res.status}`;
        const detail = apiMessage || rawBody || res.statusText;
        throw new Error(detail ? `${base}: ${detail}` : base);
      }

      const data = await res.json();
      const text = data.content
        .filter((b: any) => b.type === 'text')
        .map((b: any) => b.text)
        .join('');

      const json = extractJSON(text);
      if (!json) throw new Error('No valid JSON in response');

      const patch = JSON.parse(json) as NodeGraphConfig;
      validatePatch(patch);  // throws on invalid
      this.opts.onResult(patch);
    } catch (e: unknown) {
      const message = e instanceof Error ? e.message : String(e);
      this.opts.onError(message);
    }
  }
}

// ─── Context type ─────────────────────────────────────────────────────────────

export interface VibeContext {
  /** Entity this node graph belongs to */
  entityName:       string;
  /** Available node types in the current Pyrite64 build */
  availableNodeTypes: string[];
  /** Names of other entities in the scene */
  sceneEntities:    string[];
  /** Names of animation clips on this entity */
  animations:       string[];
  /** Audio clip ids */
  sounds:           string[];
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

/**
 * Sanitize a string to prevent prompt injection attacks.
 * Removes newlines and certain control characters that could be used
 * to inject additional instructions into the system prompt.
 */
function sanitize(str: string): string {
  return str
    .replace(/[\r\n]/g, ' ')  // Replace newlines with spaces
    .replace(/[^\x20-\x7E]/g, '')  // Remove non-printable characters
    .trim();
}

/**
 * Sanitize an array of strings for safe inclusion in prompts.
 */
function sanitizeArray(arr: string[]): string[] {
  return arr.map(sanitize).filter(s => s.length > 0);
}

function buildSystemPrompt(ctx: VibeContext): string {
  // Sanitize all user-provided context data to prevent prompt injection
  const entityName = sanitize(ctx.entityName);
  const nodeTypes = sanitizeArray(ctx.availableNodeTypes);
  const animations = sanitizeArray(ctx.animations);
  const sounds = sanitizeArray(ctx.sounds);
  const entities = sanitizeArray(ctx.sceneEntities);
  
  return `You are a Pyrite64 Node-Graph assistant. Generate a NodeGraphConfig JSON patch for the entity "${entityName}".
This is a vibe-coding engine for Nintendo 64 homebrew. Users describe gameplay in plain English and you produce node graphs.

AVAILABLE NODE TYPES (only use these):
${nodeTypes.join(', ')}

NODE REFERENCE:
Entry points (no flow-in, only flow-out):
  OnStart         – fires once when the entity spawns
  OnTick          – fires every frame (use sparingly on N64)
  OnCollide       – fires when this entity collides with another; data: { "tag": string }
  OnTimer         – fires after delay; data: { "interval": float, "repeat": bool }
  OnAnimEnd       – fires when current animation completes
  OnButtonPress   – fires on joypad button press (edge); data: { "button": string, "port": int }
  OnButtonHeld    – fires every tick while button held; data: { "button": string, "port": int }
  OnButtonRelease – fires on joypad button release; data: { "button": string, "port": int }

Flow control:
  Branch        – boolean condition split; ports: "true" / "false"
  Sequence      – execute children in order
  Repeat        – loop N times or forever; data: { "count": int, "forever": bool }
  Wait          – pause coroutine; data: { "seconds": float }
  SwitchCase    – multi-way branch; data: { "cases": string[] }
  StateMachine  – multi-output flow based on state var; data: { "stateCount": int }; out-ports: "S0", "S1", ...

Movement:
  MoveToward    – move toward a target position; data: { "speed": float, "target": [x,y,z] }; out-ports: "arrived", "moving"
  SetPosition   – teleport; data: { "position": [x,y,z] }
  SetVelocity   – set velocity vector; data: { "velocity": [x,y,z] }

Animation:
  PlayAnim      – play named clip; data: { "name": string, "speed": float, "loop": bool }
  StopAnim      – stop current animation
  SetAnimSpeed  – change playback rate; data: { "speed": float }
  SetAnimBlend  – crossfade blend; data: { "factor": float }
  WaitAnimEnd   – coroutine wait until current anim finishes

Spawning & destruction:
  Spawn         – instantiate a prefab; data: { "prefab": string, "offset": [x,y,z] }
  Destroy       – destroy this entity

Audio:
  PlaySound     – play sound by id; data: { "sound": string }

Input:
  ReadStick     – read analog stick; data: { "port": int, "deadzone": float }; value-out: X (float), Y (float)

State management:
  SetState      – set a named state var; data: { "name": string, "value": int }
  GetState      – read a named state var; data: { "name": string }; value-out: int

Values & math:
  GetDistance    – distance between two entities; data: { "target": string }; value-out: float
  GetPosition   – read current position; value-out: [x,y,z]
  GetHealth     – read health; value-out: int
  SetHealth     – set health; data: { "value": int }
  GetScore      – read score; value-out: int
  AddScore      – add to score; data: { "value": int }
  MathOp        – arithmetic; data: { "op": "Add"|"Sub"|"Mul"|"Div" }; value-in: A, B; value-out: result
  Value         – constant; data: { "value": number|string }
  Compare       – compare two values; data: { "op": "="|">"|"<"|">="|"<="|"!=" }
  CompBool      – boolean AND/OR; data: { "op": "AND"|"OR" }

Visibility & scene:
  SetVisible    – show/hide entity; data: { "visible": bool }
  SceneLoad     – load a scene; data: { "scene": string }

Misc:
  Func          – call a named function; data: { "name": string }
  Arg           – function argument
  Note          – comment (no runtime effect)
  DebugLog      – print to console; data: { "message": string }

N64 JOYPAD BUTTONS:
  A, B, Z, Start, D-Up, D-Down, D-Left, D-Right, L, R, C-Up, C-Down, C-Left, C-Right

RULES (non-negotiable — N64 hardware constraints):
- No heap allocations. No dynamic strings. No recursion.
- Keep graphs small: prefer 3–15 nodes. The N64 runs at 30fps with ~3% CPU for scripts.
- Animation names must be one of: ${animations.join(', ') || 'none'}
- Sound ids must be one of: ${sounds.join(', ') || 'none'}
- Scene entities: ${entities.join(', ') || 'none'}
- Connect flow ports (logic) top-to-bottom. Connect value ports (data) horizontally.
- Every graph MUST start from an entry-point node (OnStart, OnTick, OnCollide, OnTimer, OnAnimEnd).
- Position values are canvas coordinates (arbitrary integers, for layout only).

EDGE PORT NAMES:
- Flow out-ports: "out" (default), or named like "true"/"false" for Branch
- Flow in-ports: "in"
- Value out-ports: "value" or named (e.g. "arrived" for MoveToward)
- Value in-ports: "A", "B", or named per node

OUTPUT: Respond ONLY with a single JSON object. No markdown fences. No explanation. Schema:
{
  "nodes": [{ "id": string, "type": string, "position": [number, number], "data": {} }],
  "edges": [{ "from": string, "fromPort": string, "to": string, "toPort": string }]
}`.trim();
}

function extractJSON(text: string): string | null {
  const start = text.indexOf('{');
  const end   = text.lastIndexOf('}');
  if (start === -1 || end === -1) return null;
  return text.slice(start, end + 1);
}

function validatePatch(patch: NodeGraphConfig): void {
  if (!Array.isArray(patch.nodes)) throw new Error('patch.nodes must be an array');
  if (!Array.isArray(patch.edges)) throw new Error('patch.edges must be an array');
  for (const node of patch.nodes) {
    if (!node.id || !node.type) throw new Error(`Invalid node: ${JSON.stringify(node)}`);
  }
}
