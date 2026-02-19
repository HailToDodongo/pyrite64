/**
 * VibeAgentStatusPanel.ts
 * Real-time visual display of agent pool activity.
 *
 * Renders a compact panel showing:
 *  ┌─────────────────────────────────────────────────────┐
 *  │ ◈ Agents                         2 active   [⊟]    │
 *  ├──────────┬──────────┬──────────┬──────────┬────────┤
 *  │ ▶ Anim  │ ↕ Move  │ ⚙ AI    │ ♪ Audio │ ✦ Scene│
 *  │ thinking │  idle   │ done ✓  │  idle   │  idle  │
 *  │ ░░░░░░▒▒ │         │         │         │        │
 *  └──────────┴──────────┴──────────┴──────────┴────────┘
 *  │ Dispatch history (last 5)                           │
 *  │ · Player idle↔walk blend — 2 agents — 1840ms ✓    │
 *  └─────────────────────────────────────────────────────┘
 *
 * The panel wires directly into a VibeAgentPool instance and
 * updates in real-time as agents emit status events.
 */

import type { VibeAgentPool, PoolResult, PoolDispatch } from './VibeAgentPool.js';
import type { AgentStatusEvent, AgentRole, AgentStatus, VibeAgent } from './VibeAgent.js';

// ─── Display metadata per role ────────────────────────────────────────────────

const AGENT_META: Record<AgentRole, { label: string; icon: string; color: string }> = {
  'animation':   { label: 'Anim',   icon: '▶', color: '#e040fb' },
  'movement':    { label: 'Move',   icon: '↕', color: '#00d4ff' },
  'ai-behavior': { label: 'AI',     icon: '⚙', color: '#ff6b35' },
  'audio':       { label: 'Audio',  icon: '♪', color: '#39ff14' },
  'scene':       { label: 'Scene',  icon: '✦', color: '#ffd700' },
  'build':       { label: 'Build',  icon: '⬡', color: '#90caf9' },
};

const ROLE_ORDER: AgentRole[] = [
  'animation', 'movement', 'ai-behavior', 'audio', 'scene', 'build'
];

// ─── VibeAgentStatusPanel ─────────────────────────────────────────────────────

export class VibeAgentStatusPanel {
  readonly el: HTMLElement;

  private pool:       VibeAgentPool;
  private collapsed:  boolean = false;
  private statusEls:  Map<AgentRole, HTMLElement> = new Map();
  private barEls:     Map<AgentRole, HTMLElement> = new Map();
  private msgEls:     Map<AgentRole, HTMLElement> = new Map();
  private historyEl!: HTMLElement;
  private activeCountEl!: HTMLElement;
  private headerChipEl!:  HTMLElement;
  private dispatchHistory: PoolResult[] = [];

  constructor(pool: VibeAgentPool) {
    this.pool = pool;
    this.el   = this.buildDOM();

    // Listen to pool events
    pool
      .on('agentStatus',   (e)  => this.onAgentStatus(e))
      .on('dispatchStart', (d)  => this.onDispatchStart(d))
      .on('dispatchDone',  (r)  => this.onDispatchDone(r));
  }

  // ── DOM Build ──────────────────────────────────────────────────────────────

  private buildDOM(): HTMLElement {
    const el = document.createElement('div');
    el.className = 'vibe-agent-panel';
    el.innerHTML = `
      <div class="agent-panel-header">
        <div class="agent-panel-title">
          <span class="agent-panel-icon">◈</span>
          <span>Agents</span>
        </div>
        <div class="agent-panel-meta">
          <span class="agent-active-count" id="agent-active-count">all idle</span>
          <button class="agent-collapse-btn" id="agent-collapse-btn" title="Collapse">⊟</button>
        </div>
      </div>
      <div class="agent-grid" id="agent-grid"></div>
      <div class="agent-dispatch-history" id="agent-dispatch-history">
        <div class="agent-history-label">Recent dispatches</div>
        <div class="agent-history-list" id="agent-history-list"></div>
      </div>
    `;

    // Build agent cards
    const grid = el.querySelector('#agent-grid')!;
    for (const role of ROLE_ORDER) {
      grid.appendChild(this.buildAgentCard(role));
    }

    // Collapse toggle
    const collapseBtn = el.querySelector<HTMLButtonElement>('#agent-collapse-btn')!;
    collapseBtn.addEventListener('click', () => {
      this.collapsed = !this.collapsed;
      el.classList.toggle('collapsed', this.collapsed);
      collapseBtn.textContent = this.collapsed ? '⊞' : '⊟';
    });

    this.activeCountEl = el.querySelector('#agent-active-count')!;
    this.historyEl     = el.querySelector('#agent-history-list')!;
    this.headerChipEl  = el.querySelector('#agent-active-count')!;

    return el;
  }

  private buildAgentCard(role: AgentRole): HTMLElement {
    const meta = AGENT_META[role];

    const card = document.createElement('div');
    card.className = 'agent-card idle';
    card.dataset.role = role;

    card.innerHTML = `
      <div class="agent-card-header">
        <span class="agent-card-icon" style="color:${meta.color}">${meta.icon}</span>
        <span class="agent-card-label">${meta.label}</span>
      </div>
      <div class="agent-card-status">idle</div>
      <div class="agent-card-bar">
        <div class="agent-card-bar-fill" style="background:${meta.color}"></div>
      </div>
      <div class="agent-card-msg"></div>
    `;

    this.statusEls.set(role, card.querySelector('.agent-card-status')!);
    this.barEls.set(role, card.querySelector('.agent-card-bar-fill')!);
    this.msgEls.set(role, card.querySelector('.agent-card-msg')!);

    return card;
  }

  // ── Event handlers ─────────────────────────────────────────────────────────

  private onAgentStatus(e: AgentStatusEvent): void {
    const card = this.el.querySelector<HTMLElement>(`[data-role="${e.role}"]`);
    if (!card) return;

    // Update card class
    card.className = `agent-card ${e.status}`;

    // Update status label
    const statusEl = this.statusEls.get(e.role);
    if (statusEl) statusEl.textContent = e.status;

    // Update progress bar
    const barEl = this.barEls.get(e.role);
    if (barEl) {
      if (e.status === 'thinking') {
        barEl.style.width = '100%';
        barEl.style.animation = 'agent-thinking 1.2s ease-in-out infinite alternate';
      } else {
        barEl.style.animation = 'none';
        barEl.style.width = e.status === 'done' ? '100%' : '0%';
        barEl.style.opacity = e.status === 'done' ? '0.4' : '0.8';
      }
    }

    // Update message
    const msgEl = this.msgEls.get(e.role);
    if (msgEl) {
      const truncated = e.message.length > 40 ? e.message.slice(0, 37) + '…' : e.message;
      msgEl.textContent = truncated;
    }

    // Update active count
    this.updateActiveCount();
  }

  private onDispatchStart(d: PoolDispatch): void {
    // Flash the header
    this.headerChipEl.textContent = `${d.agents.length} running`;
    this.headerChipEl.classList.add('running');
  }

  private onDispatchDone(r: PoolResult): void {
    this.dispatchHistory.unshift(r);
    if (this.dispatchHistory.length > 8) this.dispatchHistory.pop();

    this.renderHistory();
    this.updateActiveCount();
    this.headerChipEl.classList.remove('running');
  }

  private updateActiveCount(): void {
    const agents = this.pool.getAgents();
    const thinking = Object.values(agents).filter(a => a.status === 'thinking').length;
    const done     = Object.values(agents).filter(a => a.status === 'done').length;

    if (thinking > 0) {
      this.activeCountEl.textContent = `${thinking} thinking…`;
      this.activeCountEl.className = 'agent-active-count running';
    } else if (done > 0) {
      this.activeCountEl.textContent = `${done} done`;
      this.activeCountEl.className = 'agent-active-count done';
    } else {
      this.activeCountEl.textContent = 'all idle';
      this.activeCountEl.className = 'agent-active-count';
    }
  }

  private renderHistory(): void {
    this.historyEl.innerHTML = '';

    for (const result of this.dispatchHistory.slice(0, 5)) {
      const row = document.createElement('div');
      row.className = 'agent-history-row';

      const ok    = result.failed.length === 0;
      const icon  = ok ? '✓' : '⚠';
      const color = ok ? '#39ff14' : '#ff6b35';
      const agentTag = result.results
        .filter(r => r.patch)
        .map(r => AGENT_META[r.agentRole]?.label ?? r.agentName)
        .join(' + ');

      const nodeCount = result.merged.nodes.length;

      const iconEl = document.createElement('span');
      iconEl.className = 'history-icon';
      iconEl.style.color = color;
      iconEl.textContent = icon;

      const promptEl = document.createElement('span');
      promptEl.className = 'history-prompt';
      promptEl.textContent = this.truncate(result.dispatch.prompt, 32);

      const agentsEl = document.createElement('span');
      agentsEl.className = 'history-agents';
      agentsEl.textContent = agentTag || '—';

      const detailEl = document.createElement('span');
      detailEl.className = 'history-detail';
      detailEl.textContent = `${nodeCount}n · ${result.totalMs}ms`;

      row.appendChild(iconEl);
      row.appendChild(promptEl);
      row.appendChild(agentsEl);
      row.appendChild(detailEl);

      row.title = `${result.dispatch.prompt}\nAgents: ${agentTag || 'none'}\nNodes: ${nodeCount}\nTime: ${result.totalMs}ms`;
      this.historyEl.appendChild(row);
    }
  }

  private truncate(s: string, max: number): string {
    return s.length > max ? s.slice(0, max - 1) + '…' : s;
  }
  dispose(): void {
    this.el.remove();
  }}
