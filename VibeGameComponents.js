// ─── Helpers ─────────────────────────────────────────────────────────────────
let _idCounter = 1000;
const nextId = () => `node_${_idCounter++}`;
/** Creates a standard node config. */
const node = (type, x, y, data = {}) => ({
    id: nextId(),
    type,
    position: [x, y],
    data
});
/** Creates an edge between two nodes. */
const connect = (from, fromPort, to, toPort) => ({
    from: from.id,
    fromPort: fromPort,
    to: to.id,
    toPort: toPort
});
// ─── Component Definitions ───────────────────────────────────────────────────
export const GAME_COMPONENTS = [
    // ─── MOVEMENT ──────────────────────────────────────────────────────────────
    {
        id: 'platformer-controller',
        label: 'Platformer Controller',
        category: 'Movement',
        icon: 'directions_run',
        description: 'Basic 2D/3D platformer movement with jump and gravity.',
        requirements: ['Collision Body (Dynamic)', 'Freeze Rotation'],
        buildGraph: (ent) => {
            const nodes = [];
            const edges = [];
            const start = node('OnTick', 50, 50);
            const stick = node('ReadStick', 250, 50, { stick: 'Left' });
            const move = node('MoveDirection', 500, 50, { speed: 10, relative: 'Camera' });
            const btn = node('OnButtonPress', 50, 200, { button: 'A' });
            const jump = node('SetVelocity', 300, 200, { y: 15, mode: 'Add' }); // naive jump
            nodes.push(start, stick, move, btn, jump);
            edges.push(connect(start, 'out', stick, 'in'));
            edges.push(connect(stick, 'vec', move, 'direction'));
            edges.push(connect(stick, 'out', move, 'in'));
            edges.push(connect(btn, 'out', jump, 'in'));
            return { nodes, edges };
        }
    },
    {
        id: 'fps-controller',
        label: 'FPS Controller',
        category: 'Movement',
        icon: 'api',
        description: 'First-person movement with camera look.',
        requirements: ['Camera (Child)', 'Collision Body'],
        buildGraph: (ent) => {
            const nodes = [];
            const edges = [];
            // Movement
            const tick = node('OnTick', 50, 50);
            const move = node('MoveDirection', 300, 50, { speed: 8, relative: 'Self' });
            const look = node('ReadStick', 300, 200, { stick: 'Right' });
            const rot = node('SetRotation', 550, 200, { axis: 'Y', speed: 2 });
            nodes.push(tick, move, look, rot);
            edges.push(connect(tick, 'out', move, 'in'));
            edges.push(connect(tick, 'out', look, 'in'));
            edges.push(connect(look, 'out', rot, 'in'));
            return { nodes, edges };
        }
    },
    // ─── INTERACTION ───────────────────────────────────────────────────────────
    {
        id: 'door-logic',
        label: 'Simple Door',
        category: 'Interaction',
        icon: 'sensor_door',
        description: 'Opens when player is near and presses interact.',
        requirements: ['IsTrigger (Proximity)'],
        buildGraph: (ent) => {
            const nodes = [];
            const edges = [];
            const onEnter = node('OnCollide', 50, 50, { type: 'Enter', tag: 'Player' });
            const check = node('OnButtonPress', 300, 50, { button: 'B' }); // Interact
            const anim = node('PlayAnim', 550, 50, { anim: 'Open', loop: false });
            const sound = node('PlaySound', 550, 150, { sound: 'DoorOpen' });
            nodes.push(onEnter, check, anim, sound);
            edges.push(connect(onEnter, 'out', check, 'in'));
            edges.push(connect(check, 'out', anim, 'in'));
            edges.push(connect(check, 'out', sound, 'in'));
            return { nodes, edges };
        }
    },
    {
        id: 'collectible-health',
        label: 'Health Pickup',
        category: 'Interaction',
        icon: 'favorite',
        description: 'Restores health and destroys self on touch.',
        requirements: ['IsTrigger'],
        buildGraph: (ent) => {
            const nodes = [];
            const edges = [];
            const hit = node('OnCollide', 50, 50, { tag: 'Player' });
            const heal = node('SetHealth', 300, 50, { amount: 25, mode: 'Add', target: 'Other' });
            const sfx = node('PlaySound', 300, 150, { sound: 'Pickup' });
            const kill = node('Destroy', 550, 50, { target: 'Self', delay: 0.1 });
            nodes.push(hit, heal, sfx, kill);
            edges.push(connect(hit, 'out', heal, 'in'));
            edges.push(connect(hit, 'out', sfx, 'in'));
            edges.push(connect(heal, 'out', kill, 'in'));
            return { nodes, edges };
        }
    },
    // ─── LOGIC ─────────────────────────────────────────────────────────────────
    {
        id: 'patrol-ai',
        label: 'Patrol AI',
        category: 'Logic',
        icon: 'visibility',
        description: 'Moves between waypoints.',
        requirements: ['Waypoints (Children)'],
        buildGraph: (ent) => {
            const nodes = [];
            const edges = [];
            const tick = node('OnTick', 50, 50);
            const move = node('MoveDirection', 300, 50, { speed: 4, direction: 'Forward' });
            const hit = node('OnCollide', 50, 200, { tag: 'Wall' });
            const turn = node('SetRotation', 300, 200, { y: 180, mode: 'Add' });
            nodes.push(tick, move, hit, turn);
            edges.push(connect(tick, 'out', move, 'in'));
            edges.push(connect(hit, 'out', turn, 'in'));
            return { nodes, edges };
        }
    }
];
export function getGroupedComponents() {
    const groups = {};
    for (const c of GAME_COMPONENTS) {
        if (!groups[c.category])
            groups[c.category] = [];
        groups[c.category].push(c);
    }
    return groups;
}
