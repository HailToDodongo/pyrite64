/**
 * VibeAgentPool.js
 * Parallel multi-agent orchestrator for the Pyrite64 Vibe Coding Engine.
 *
 * The pool:
 *  1. Receives a natural-language prompt + scene context
 *  2. Routes the request to one or more specialized agents via keyword analysis
 *  3. Dispatches all selected agents IN PARALLEL (Promise.allSettled)
 *  4. Merges the returned NodeGraphConfig patches into a single coherent graph
 *  5. Emits per-agent status events for the UI
 *
 * Routing table (keyword → agent roles, can match multiple):
 *  anim / clip / walk / run / idle / blend     → animation
 *  move / jump / velocity / stick / patrol     → movement
 *  enemy / chase / flee / AI / behavior / NPC  → ai-behavior
 *  sound / music / SFX / audio / footstep      → audio
 *  scene / spawn / wave / load / transition    → scene
 *  optim / budget / refactor / clean           → build
 *
 * When no keywords match, the pool falls back to the general VibeNode
 * generator (all-nodes system prompt).
 *
 * Patch merging strategy:
 *  - Nodes from multiple agents are offset on the canvas to avoid overlap
 *  - Node ids are namespaced by agent role to prevent collisions
 *  - Edges referencing nodes from the same batch are preserved
 *  - Cross-agent edges are NOT generated (agents don't coordinate node ids)
 */
import { createAllAgents, } from './VibeAgent.js';
const ROUTING_RULES = [
    {
        keywords: /\b(anim|clip|walk|run|idle|blend|keyframe|pose|loop|pose|skin|skeleton|strafe|crouch|land)\b/i,
        roles: ['animation'],
    },
    {
        keywords: /\b(move|jump|velocity|stick|patrol|patrol|locomot|sprint|dash|knockback|push|force|physics|gravity|slide|roll)\b/i,
        roles: ['movement'],
    },
    {
        keywords: /\b(enemy|chase|flee|ai\b|behavior|npc|guard|aggro|detect|perceive|react|aggress|stealth|neutral|roam)\b/i,
        roles: ['ai-behavior'],
    },
    {
        keywords: /\b(sound|music|sfx|audio|footstep|bgm|ost|jingle|voice|effect|volume|mute)\b/i,
        roles: ['audio'],
    },
    {
        keywords: /\b(scene|spawn|wave|load|transition|pickup|coin|item|respawn|despawn|zone|trigger|door|warp|level)\b/i,
        roles: ['scene'],
    },
    {
        keywords: /\b(optim|budget|refactor|clean|simplif|reduce|fix|improve|minimiz|effici)\b/i,
        roles: ['build'],
    },
];
// Requests that clearly span multiple domains should route to multiple agents
const MULTI_AGENT_KEYWORDS = /\b(and|while|also|then|with|plus|both)\b/i;
// ── Canvas layout: each agent's nodes are placed in a separate horizontal band
const AGENT_CANVAS_OFFSETS = {
    'animation': [0, 0],
    'movement': [400, 0],
    'ai-behavior': [800, 0],
    'audio': [0, 300],
    'scene': [400, 300],
    'build': [800, 300],
};
// ─── VibeAgentPool ────────────────────────────────────────────────────────────
export class VibeAgentPool {
    constructor() {
        this.listeners = {};
        this.dispatchHistory = [];
        this.agents = createAllAgents();
        // Bubble per-agent status events up
        for (const agent of Object.values(this.agents)) {
            agent.onStatus((e) => this.emit('agentStatus', e));
        }
    }
    // ── Public API ─────────────────────────────────────────────────────────────
    on(event, cb) {
        var _a;
        ((_a = this.listeners)[event] ?? (_a[event] = [])).push(cb);
        return this;
    }
    /** Get all agent instances for direct status querying. */
    getAgents() {
        return this.agents;
    }
    /** Get dispatch history (most recent first). */
    getHistory() {
        return [...this.dispatchHistory].reverse();
    }
    /**
     * Analyze a prompt and dispatch to matching agents in parallel.
     * Returns the merged NodeGraphConfig patch.
     */
    async dispatch(prompt, context) {
        const id = `d_${Date.now()}_${Math.random().toString(36).slice(2, 6)}`;
        const roles = this.route(prompt);
        const dispatch = { id, prompt, context, agents: roles };
        this.emit('dispatchStart', dispatch);
        const t0 = performance.now();
        // Build tasks for each selected agent
        const tasks = roles.map((role, i) => ({
            agent: this.agents[role],
            task: {
                id: `${id}_${role}`,
                prompt: this.buildAgentPrompt(prompt, role),
                context,
                priority: roles.length - i,
            },
        }));
        // Launch all agents in parallel
        const settled = await Promise.allSettled(tasks.map(({ agent, task }) => agent.run(task)));
        const results = settled.map((s, i) => s.status === 'fulfilled'
            ? s.value
            : {
                agentRole: roles[i],
                agentName: this.agents[roles[i]].name,
                task: tasks[i].task,
                patch: null,
                error: s.reason instanceof Error ? s.reason.message : String(s.reason),
                durationMs: 0,
            });
        const totalMs = Math.round(performance.now() - t0);
        const succeeded = results.filter(r => r.patch !== null).map(r => r.agentName);
        const failed = results.filter(r => r.patch === null).map(r => r.agentName);
        const merged = this.mergePatches(results);
        const poolResult = {
            dispatch, merged, results, totalMs, succeeded, failed,
        };
        this.dispatchHistory.push(poolResult);
        this.emit('dispatchDone', poolResult);
        return poolResult;
    }
    /** Route a prompt to one or more agent roles. */
    route(prompt) {
        const matches = new Set();
        for (const rule of ROUTING_RULES) {
            if (rule.keywords.test(prompt)) {
                for (const role of rule.roles)
                    matches.add(role);
            }
        }
        // If multiple domains match and the prompt uses "and/also/while" connectors,
        // keep all matches — otherwise keep only the strongest match (first found)
        if (matches.size > 1 && !MULTI_AGENT_KEYWORDS.test(prompt)) {
            // Return only the first matched role (highest priority in order of rules)
            const first = ROUTING_RULES.find(r => r.keywords.test(prompt))?.roles[0];
            return first ? [first] : ['animation'];
        }
        return matches.size > 0 ? [...matches] : this.fallbackRoute(prompt);
    }
    // ── Private ────────────────────────────────────────────────────────────────
    /**
     * When no keywords match, score each agent's domain and pick the best one.
     * Falls back to 'animation' if nothing scores.
     */
    fallbackRoute(prompt) {
        const lower = prompt.toLowerCase();
        const scores = [
            ['animation', lower.split(' ').filter(w => /anim|clip|pose/.test(w)).length],
            ['movement', lower.split(' ').filter(w => /move|go|speed|fast|slow/.test(w)).length],
            ['ai-behavior', lower.split(' ').filter(w => /npc|villain|bot|auto/.test(w)).length],
            ['audio', lower.split(' ').filter(w => /hear|play|music|loud/.test(w)).length],
            ['scene', lower.split(' ').filter(w => /scene|world|map|area/.test(w)).length],
            ['build', 0],
        ];
        const best = scores.sort((a, b) => b[1] - a[1])[0];
        return [best[1] > 0 ? best[0] : 'animation'];
    }
    /**
     * Optionally enrich the user prompt with role-specific framing.
     * This helps the agent focus on its domain when the prompt is broad.
     */
    buildAgentPrompt(prompt, role) {
        const framing = {
            'animation': 'Focus only on the ANIMATION aspects: ',
            'movement': 'Focus only on the MOVEMENT/LOCOMOTION aspects: ',
            'ai-behavior': 'Focus only on the AI BEHAVIOR/DECISION aspects: ',
            'audio': 'Focus only on the AUDIO/SOUND aspects: ',
            'scene': 'Focus only on the SCENE/LIFECYCLE aspects: ',
            'build': 'Optimize for N64 performance: ',
        };
        return framing[role] + prompt;
    }
    /**
     * Merge patches from multiple agents into one coherent NodeGraphConfig.
     *
     * Strategy:
     *  - Prefix all node ids with the agent role to avoid collision
     *  - Offset canvas positions per agent so nodes don't overlap
     *  - Rewire edges to use the prefixed ids
     *  - Deduplicate identical node types at the same canvas position
     */
    mergePatches(results) {
        const mergedNodes = [];
        const mergedEdges = [];
        for (const result of results) {
            if (!result.patch)
                continue;
            const role = result.agentRole;
            const prefix = role.replace('-', '_') + '__';
            const offset = AGENT_CANVAS_OFFSETS[role] ?? [0, 0];
            // Remap node ids and offset positions
            const idMap = new Map();
            for (const node of result.patch.nodes) {
                const newId = prefix + node.id;
                idMap.set(node.id, newId);
                mergedNodes.push({
                    ...node,
                    id: newId,
                    position: [node.position[0] + offset[0], node.position[1] + offset[1]],
                });
            }
            // Remap edge references
            for (const edge of result.patch.edges) {
                const from = idMap.get(edge.from);
                const to = idMap.get(edge.to);
                if (from && to) {
                    mergedEdges.push({ ...edge, from, to });
                }
            }
        }
        return { nodes: mergedNodes, edges: mergedEdges };
    }
    emit(event, ...args) {
        for (const cb of this.listeners[event] ?? []) {
            cb(...args);
        }
    }
}
