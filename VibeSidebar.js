/**
 * VibeSidebar.js
 * Left sidebar panel for the Vibe Coding Dashboard.
 *
 * Responsibilities:
 *  - Scene entity tree (select active entity → updates VibeContext)
 *  - Quick-nav: Scenes | Entities | Node Graphs | Settings
 *  - N64 budget strip (tris / verts / RDRAM) — fed from Viewport3D
 *  - Render-mode selector (standard | cartoon | wireframe | n64-accurate)
 *  - Emits events consumed by VibeDashboard to keep everything in sync
 */
import { getGroupedComponents } from './VibeGameComponents.js';
// ─── Type icon map ────────────────────────────────────────────────────────────
const TYPE_ICON = {
    mesh: '◈',
    light: '◉',
    camera: '⊡',
    empty: '◎',
    collision: '⬡',
};
// ─── VibeSidebar ─────────────────────────────────────────────────────────────
export class VibeSidebar {
    constructor() {
        this.handlers = {};
        this.entities = [];
        this.activeEntity = null;
        this.renderMode = 'standard';
        this.gridVisible = true;
        this.cartoonEnabled = false;
        this.budget = {
            tris: { used: 0, max: 64 },
            verts: { used: 0, max: 800 },
            rdramKB: { used: 0, max: 4096 },
        };
        this.renderBtns = new Map();
        this.el = this.buildDOM();
    }
    // ── Public API ─────────────────────────────────────────────────────────────
    on(event, handler) {
        this.handlers[event] = handler;
        return this;
    }
    /** Replace the entity list and re-render the tree. */
    setEntities(entities) {
        this.entities = entities;
        this.renderEntityTree();
    }
    /** Highlight an entity as selected (can be driven externally). */
    setActiveEntity(id) {
        this.activeEntity = id;
        this.renderEntityTree();
    }
    /** Push a fresh budget snapshot — called from Viewport3D.onBudgetWarning feed. */
    updateBudget(snapshot) {
        this.budget = snapshot;
        this.renderBudget();
    }
    /** Sync render mode pill without firing change event (avoids loop). */
    syncRenderMode(mode) {
        this.renderMode = mode;
        this.renderBtns.forEach((btn, m) => {
            btn.classList.toggle('active', m === mode);
        });
    }
    // ── DOM Construction ───────────────────────────────────────────────────────
    buildDOM() {
        const el = document.createElement('div');
        el.id = 'vibe-sidebar';
        el.innerHTML = `
      <!-- Nav tabs -->
      <div class="sidebar-section">
        <div class="sidebar-tabs" id="sidebar-tabs"></div>
      </div>

      <!-- Render mode selector -->
      <div class="sidebar-section">
        <div class="sidebar-section-header">
          <span>Render Mode</span>
          <span class="section-icon">◈</span>
        </div>
        <div class="sidebar-section-body" id="sidebar-render-modes" style="padding: 0 10px 8px;"></div>
      </div>

      <div class="sidebar-separator"></div>

      <!-- Scene tree / Component Browser -->
      <div id="sidebar-view-entities" class="sidebar-section" style="flex:1; display:flex; flex-direction:column; min-height:0;">
        <div class="sidebar-section-header">
          <span>Scene</span>
          <span class="section-icon" id="scene-entity-count" style="min-width:24px; text-align:right;">0</span>
        </div>
        <div class="sidebar-scroll">
          <div id="sidebar-entity-list"></div>
        </div>
      </div>

      <div id="sidebar-view-components" class="sidebar-section hidden" style="flex:1; display:flex; flex-direction:column; min-height:0;">
        <div class="sidebar-section-header">
          <span>Components</span>
          <span class="section-icon">◈</span>
        </div>
        <div class="sidebar-scroll">
          <div id="sidebar-component-list"></div>
        </div>
      </div>

      <div class="sidebar-separator"></div>

      <!-- View toggles -->
      <div class="sidebar-section">
        <div class="sidebar-section-header">
          <span>Overlays</span>
        </div>
        <div class="sidebar-section-body" style="padding: 0 0 4px;">
          <div class="sidebar-item" id="toggle-grid">
            <span class="item-icon">⋮⋮</span>
            <span>Grid</span>
            <span class="item-badge" id="grid-badge" style="margin-left:auto; color:var(--neon-green); border-color:rgba(57,255,20,0.35); background:rgba(57,255,20,0.10);">ON</span>
          </div>
          <div class="sidebar-item" id="toggle-cartoon">
            <span class="item-icon">◑</span>
            <span>Cartoon Pass</span>
            <span class="item-badge" id="cartoon-badge">OFF</span>
          </div>
        </div>
      </div>

      <div class="sidebar-separator"></div>

      <!-- N64 Budget strip -->
      <div class="budget-strip">
        <div class="budget-strip-label">N64 Budget</div>
        <div class="budget-row">
          <div class="budget-row-label">Tris</div>
          <div class="budget-bar-track"><div class="budget-bar-fill" id="budget-tris-fill"></div></div>
          <div class="budget-row-value" id="budget-tris-val">0</div>
        </div>
        <div class="budget-row">
          <div class="budget-row-label">Verts</div>
          <div class="budget-bar-track"><div class="budget-bar-fill" id="budget-vert-fill"></div></div>
          <div class="budget-row-value" id="budget-vert-val">0</div>
        </div>
        <div class="budget-row">
          <div class="budget-row-label">RDRAM</div>
          <div class="budget-bar-track"><div class="budget-bar-fill" id="budget-ram-fill"></div></div>
          <div class="budget-row-value" id="budget-ram-val">0K</div>
        </div>
      </div>

      <div style="height:8px;"></div>
    `;
        this.buildTabs(el);
        this.buildRenderModes(el);
        this.cacheRefs(el);
        this.wireEvents(el);
        return el;
    }
    buildTabs(root) {
        const tabs = root.querySelector('#sidebar-tabs');
        const defs = [
            { id: 'scenes', icon: '⬡', label: 'Scenes' },
            { id: 'components', icon: '◈', label: 'Components' },
            { id: 'graphs', icon: '⋮', label: 'Graphs' },
            { id: 'settings', icon: '⚙', label: 'Settings' },
        ];
        const entView = root.querySelector('#sidebar-view-entities');
        const cmpView = root.querySelector('#sidebar-view-components');
        defs.forEach(({ id, icon, label }, i) => {
            const item = document.createElement('div');
            item.className = `sidebar-item${i === 0 ? ' active' : ''}`;
            item.dataset.tab = id;
            item.innerHTML = `<span class="item-icon">${icon}</span><span>${label}</span>`;
            item.addEventListener('click', () => {
                root.querySelectorAll('[data-tab]').forEach(el => el.classList.remove('active'));
                item.classList.add('active');
                entView.classList.add('hidden');
                cmpView.classList.add('hidden');
                if (id === 'scenes') {
                    entView.classList.remove('hidden');
                }
                else if (id === 'components') {
                    cmpView.classList.remove('hidden');
                    this.renderComponentList(root);
                }
            });
            tabs.appendChild(item);
        });
    }
    renderComponentList(root) {
        const list = root.querySelector('#sidebar-component-list');
        if (list.innerHTML !== '')
            return;
        const groups = getGroupedComponents();
        Object.entries(groups).forEach(([cat, comps]) => {
            const header = document.createElement('div');
            header.className = 'sidebar-group-header';
            header.style.cssText = 'padding:8px 14px 4px; color:var(--text-dim); font-size:10px; text-transform:uppercase; letter-spacing:1px; margin-top:4px; font-weight:600;';
            header.textContent = cat;
            list.appendChild(header);
            comps.forEach(c => {
                const item = document.createElement('div');
                item.className = 'sidebar-item component-item';
                item.style.cssText = 'padding:6px 14px; display:flex; align-items:center; gap:8px; cursor:pointer; font-size:12px;';
                // Simplified icons (unicode fallback since Material Icons might not be loaded)
                const icon = c.icon === 'directions_run' ? '🏃' :
                    c.icon === 'api' ? '🔧' :
                        c.icon === 'sensor_door' ? '🚪' :
                            c.icon === 'favorite' ? '♥' : '📦';
                item.innerHTML = `
          <span style="opacity:0.7; font-size:14px; width:16px;">${icon}</span>
          <div style="flex:1; display:flex; flex-direction:column; min-width:0;">
            <span style="font-weight:500;">${c.label}</span>
            <span style="font-size:10px; color:var(--text-dim); overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">${c.description}</span>
          </div>
          <button class="add-btn" style="border:none; background:transparent; color:var(--neon-green); font-size:14px; cursor:pointer;">+</button>
        `;
                item.querySelector('.add-btn')?.addEventListener('click', (e) => {
                    e.stopPropagation();
                    this.handlers.applyComponent?.(c);
                });
                list.appendChild(item);
            });
        });
    }
    buildRenderModes(root) {
        const container = root.querySelector('#sidebar-render-modes');
        const modes = [
            { mode: 'standard', label: 'Standard', icon: '◐' },
            { mode: 'cartoon', label: 'Cartoon', icon: '◑' },
            { mode: 'wireframe', label: 'Wireframe', icon: '◻' },
            { mode: 'n64-accurate', label: 'N64 Accurate', icon: '◈' },
        ];
        const row = document.createElement('div');
        row.style.cssText = 'display:flex;flex-wrap:wrap;gap:4px;';
        modes.forEach(({ mode, label, icon }) => {
            const btn = document.createElement('button');
            btn.className = `vp-btn${mode === this.renderMode ? ' active' : ''}`;
            btn.style.cssText = 'flex:1 0 calc(50% - 2px); padding:5px 6px; font-size:10px;';
            btn.innerHTML = `${icon} ${label}`;
            btn.addEventListener('click', () => {
                this.renderMode = mode;
                this.renderBtns.forEach((b, m) => b.classList.toggle('active', m === mode));
                this.handlers.renderModeChange?.(mode);
            });
            this.renderBtns.set(mode, btn);
            row.appendChild(btn);
        });
        container.appendChild(row);
    }
    cacheRefs(root) {
        this.entityListEl = root.querySelector('#sidebar-entity-list');
        this.budgetTrisFill = root.querySelector('#budget-tris-fill');
        this.budgetVertFill = root.querySelector('#budget-vert-fill');
        this.budgetRamFill = root.querySelector('#budget-ram-fill');
        this.budgetTrisVal = root.querySelector('#budget-tris-val');
        this.budgetVertVal = root.querySelector('#budget-vert-val');
        this.budgetRamVal = root.querySelector('#budget-ram-val');
    }
    wireEvents(root) {
        // Grid toggle
        root.querySelector('#toggle-grid')?.addEventListener('click', () => {
            this.gridVisible = !this.gridVisible;
            const badge = root.querySelector('#grid-badge');
            if (this.gridVisible) {
                badge.textContent = 'ON';
                badge.style.cssText = 'margin-left:auto; color:var(--neon-green); border-color:rgba(57,255,20,0.35); background:rgba(57,255,20,0.10);';
            }
            else {
                badge.textContent = 'OFF';
                badge.style.cssText = 'margin-left:auto;';
            }
            this.handlers.gridToggle?.(this.gridVisible);
        });
        // Cartoon toggle
        root.querySelector('#toggle-cartoon')?.addEventListener('click', () => {
            this.cartoonEnabled = !this.cartoonEnabled;
            const badge = root.querySelector('#cartoon-badge');
            if (this.cartoonEnabled) {
                badge.textContent = 'ON';
                badge.style.cssText = 'margin-left:auto; color:var(--neon-cyan); border-color:rgba(0,212,255,0.35); background:rgba(0,212,255,0.10);';
            }
            else {
                badge.textContent = 'OFF';
                badge.style.cssText = 'margin-left:auto;';
            }
            this.handlers.cartoonToggle?.(this.cartoonEnabled);
        });
    }
    // ── Renderers ──────────────────────────────────────────────────────────────
    renderEntityTree() {
        const countEl = this.el.querySelector('#scene-entity-count');
        if (countEl)
            countEl.textContent = String(this.entities.length);
        this.entityListEl.innerHTML = '';
        if (this.entities.length === 0) {
            const empty = document.createElement('div');
            empty.style.cssText = 'padding:12px 14px; font-size:11px; color:var(--text-dim); font-style:italic;';
            empty.textContent = 'No entities in scene';
            this.entityListEl.appendChild(empty);
            return;
        }
        const renderNode = (entity, depth) => {
            const item = document.createElement('div');
            item.className = `sidebar-item${entity.id === this.activeEntity ? ' active' : ''}`;
            item.style.paddingLeft = `${14 + depth * 12}px`;
            item.dataset.entityId = entity.id;
            const iconSpan = document.createElement('span');
            iconSpan.className = 'item-icon';
            iconSpan.textContent = TYPE_ICON[entity.type] ?? '○';
            const nameSpan = document.createElement('span');
            nameSpan.style.cssText = 'flex:1; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;';
            nameSpan.textContent = entity.name;
            item.appendChild(iconSpan);
            item.appendChild(nameSpan);
            if (entity.children.length > 0) {
                const countSpan = document.createElement('span');
                countSpan.style.cssText = 'font-size:9px; color:var(--text-dim); margin-left:4px;';
                countSpan.textContent = String(entity.children.length);
                item.appendChild(countSpan);
            }
            item.addEventListener('click', () => {
                this.activeEntity = entity.id;
                this.renderEntityTree();
                this.handlers.entitySelect?.(entity.name);
            });
            this.entityListEl.appendChild(item);
            entity.children.forEach(child => renderNode(child, depth + 1));
        };
        this.entities.forEach(e => renderNode(e, 0));
    }
    renderBudget() {
        const tPct = Math.min(this.budget.tris.used / this.budget.tris.max * 100, 100);
        const vPct = Math.min(this.budget.verts.used / this.budget.verts.max * 100, 100);
        const rPct = Math.min(this.budget.rdramKB.used / this.budget.rdramKB.max * 100, 100);
        const setBar = (fill, val, pct, used, suffix) => {
            fill.style.width = `${pct}%`;
            fill.classList.toggle('warn', pct > 65 && pct <= 85);
            fill.classList.toggle('critical', pct > 85);
            val.textContent = used + suffix;
        };
        setBar(this.budgetTrisFill, this.budgetTrisVal, tPct, this.budget.tris.used, '');
        setBar(this.budgetVertFill, this.budgetVertVal, vPct, this.budget.verts.used, '');
        setBar(this.budgetRamFill, this.budgetRamVal, rPct, this.budget.rdramKB.used, 'K');
    }
}
