/**
 * VibeChat.ts
 * Right-panel chat interface for the Vibe Coding Dashboard.
 *
 * Responsibilities:
 *  - Chat history display (user / assistant / system messages)
 *  - Text input + quick-prompt chips
 *  - Calls VibeNode.generate() and renders the returned NodeGraphConfig patch
 *  - "Apply Patch" button injects the patch into the live node graph
 *  - Context chips show the active entity / scene / available node types
 *  - Typing indicator during API calls
 *  - Emits events: patchApply, contextChange
 */

import { VibeNode, VibeContext, NodeGraphConfig } from './VibeNode.js';
import type { VibeAgentPool, PoolResult } from './VibeAgentPool.js';
import type { AgentRole } from './VibeAgent.js';

// ─── Message types ────────────────────────────────────────────────────────────

export type MessageRole = 'user' | 'assistant' | 'system';

export interface ChatMessage {
  id:        string;
  role:      MessageRole;
  text:      string;
  timestamp: Date;
  patch?:    NodeGraphConfig;   // assistant messages may carry a generated patch
  applied?:  boolean;           // true once patch is fed to graph
}

// ─── Event map ────────────────────────────────────────────────────────────────

export interface ChatEvents {
  /** Fired when user clicks "Apply Patch" on an assistant message. */
  patchApply:    (patch: NodeGraphConfig, msgId: string) => void;
  /** Fired when user removes/changes a context chip. */
  contextChange: (ctx: Partial<VibeContext>) => void;
}

// ─── Quick prompt suggestions (prefixed per category) ─────────────────────────

const QUICK_PROMPTS: string[] = [
  'Chase player on enter',
  'Patrol between waypoints',
  'Play idle anim on tick',
  'Jump when grounded',
  'Play hit sound on collide',
  'Destroy self after 3s',
  'Rotate 90° every second',
  'Fade out and despawn',
];

// ─── VibeChat ─────────────────────────────────────────────────────────────────

export class VibeChat {
  readonly el: HTMLElement;

  private vibeNode:   VibeNode;
  private agentPool:  VibeAgentPool | null     = null;
  private context:    VibeContext;
  private messages:   ChatMessage[]           = [];
  private handlers:   Partial<ChatEvents>     = {};
  private isThinking: boolean                 = false;
  private activeAgentRoles: AgentRole[]       = [];

  // Cached DOM refs
  private messagesEl!: HTMLElement;
  private inputEl!:    HTMLTextAreaElement;
  private sendBtn!:    HTMLButtonElement;
  private statusDot!:  HTMLElement;
  private statusSub!:  HTMLElement;
  private ctxBarEl!:   HTMLElement;
  private routeChip!:  HTMLElement;

  constructor(initialContext: VibeContext, pool?: VibeAgentPool) {
    this.context = { ...initialContext };
    if (pool) {
      this.agentPool = pool;
      pool.on('agentStatus', (e) => this.onAgentStatus(e.role, e.status));
    }

    this.vibeNode = new VibeNode({
      onResult: (patch) => this.handleResult(patch),
      onError:  (msg)   => this.handleError(msg),
    });

    this.el = this.buildDOM();
    this.renderContextBar();
    this.pushSystem('Vibe engine ready. Describe behavior in plain English and I\'ll generate node graph logic for you.');
  }

  // ── Public API ─────────────────────────────────────────────────────────────

  on<K extends keyof ChatEvents>(event: K, handler: ChatEvents[K]): this {
    this.handlers[event] = handler as any;
    return this;
  }

  /** Update which entity / scene / node types are in context. */
  updateContext(ctx: Partial<VibeContext>): void {
    this.context = { ...this.context, ...ctx };
    this.renderContextBar();
  }

  /** Mark a previously pending patch as applied (updates button state). */
  markPatchApplied(msgId: string): void {
    const msg = this.messages.find(m => m.id === msgId);
    if (!msg) return;
    msg.applied = true;

    const btn = this.messagesEl.querySelector<HTMLButtonElement>(`[data-msg-id="${msgId}"] .patch-apply-btn`);
    if (btn) {
      btn.textContent = '✓ Applied to Graph';
      btn.classList.add('applied');
      btn.disabled = true;
    }
  }

  /** Programmatically inject a system message (e.g. from Viewport3D budget warnings). */
  pushSystem(text: string): void {
    this.addMessage({ role: 'system', text });
  }

  /** Set the input textarea text (used by prompt library). */
  setInput(text: string): void {
    if (this.inputEl) {
      this.inputEl.value = text;
      this.inputEl.focus();
      this.updateRouteChip(text);
    }
  }

  // ── DOM Construction ───────────────────────────────────────────────────────

  private buildDOM(): HTMLElement {
    const el = document.createElement('div');
    el.id = 'vibe-chat';

    // ── Header ───────────────────────────────────────────────────────────────
    const header = document.createElement('div');
    header.className = 'chat-header';
    header.innerHTML = `
      <div class="chat-header-icon">✦</div>
      <div class="chat-header-info">
        <div class="chat-header-title">Vibe Coder</div>
        <div class="chat-header-sub" id="chat-status-sub">claude-sonnet-4-6 · Ready</div>
      </div>
      <div class="chat-status-dot" id="chat-status-dot"></div>
    `;
    el.appendChild(header);

    this.statusDot = header.querySelector('#chat-status-dot')!;
    this.statusSub = header.querySelector('#chat-status-sub')!;

    // ── Context bar ───────────────────────────────────────────────────────────
    const ctxBar = document.createElement('div');
    ctxBar.className = 'chat-context-bar';
    ctxBar.id = 'chat-ctx-bar';
    el.appendChild(ctxBar);
    this.ctxBarEl = ctxBar;

    // ── Messages ──────────────────────────────────────────────────────────────
    const msgs = document.createElement('div');
    msgs.className = 'chat-messages';
    msgs.id = 'chat-messages';
    el.appendChild(msgs);
    this.messagesEl = msgs;

    // ── Input area ────────────────────────────────────────────────────────────
    const inputArea = document.createElement('div');
    inputArea.className = 'chat-input-area';

    // Quick prompts
    const quickBar = document.createElement('div');
    quickBar.className = 'quick-prompts';
    QUICK_PROMPTS.forEach(prompt => {
      const chip = document.createElement('button');
      chip.className = 'quick-chip';
      chip.textContent = prompt;
      chip.addEventListener('click', () => {
        this.inputEl.value = prompt;
        this.inputEl.focus();
        this.autoGrow();
      });
      quickBar.appendChild(chip);
    });
    inputArea.appendChild(quickBar);

    // Input row
    const inputRow = document.createElement('div');
    inputRow.className = 'chat-input-row';

    const textarea = document.createElement('textarea');
    textarea.className = 'chat-input-box';
    textarea.placeholder = 'Describe the behavior you want… (e.g. "chase the player when within 5 units")';
    textarea.rows = 1;
    textarea.addEventListener('input', () => { this.autoGrow(); this.updateRouteChip(textarea.value); });
    textarea.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        this.submit();
      }
    });
    this.inputEl = textarea;

    const sendBtn = document.createElement('button');
    sendBtn.className = 'chat-send-btn';
    sendBtn.title = 'Send (Enter)';
    sendBtn.innerHTML = '▶';
    sendBtn.addEventListener('click', () => this.submit());
    this.sendBtn = sendBtn;

    inputRow.appendChild(textarea);
    inputRow.appendChild(sendBtn);
    inputArea.appendChild(inputRow);

    // Route-preview chip (shows which agents will handle the input)
    const routeChip = document.createElement('div');
    routeChip.className = 'route-preview-chip hidden';
    routeChip.title = 'Agents that will process this prompt';
    inputArea.appendChild(routeChip);
    this.routeChip = routeChip;

    el.appendChild(inputArea);

    return el;
  }

  // ── Context bar renderer ───────────────────────────────────────────────────

  private renderContextBar(): void {
    this.ctxBarEl.innerHTML = '';

    const addChip = (icon: string, label: string, cls: string, onClick?: () => void) => {
      const chip = document.createElement('div');
      chip.className = `context-chip ${cls}`;
      chip.innerHTML = `<span>${icon}</span><span>${label}</span>`;
      if (onClick) chip.addEventListener('click', onClick);
      this.ctxBarEl.appendChild(chip);
    };

    addChip('◈', this.context.entityName || 'No entity', 'entity');

    if (this.context.sceneEntities.length > 0) {
      addChip('⬡', `Scene (${this.context.sceneEntities.length})`, 'scene');
    }

    if (this.context.availableNodeTypes.length > 0) {
      addChip('⋮', `${this.context.availableNodeTypes.length} node types`, 'node');
    }

    if (this.context.animations.length > 0) {
      addChip('▶', `${this.context.animations.length} anims`, 'node');
    }
  }

  // ── Message management ─────────────────────────────────────────────────────

  private addMessage(opts: Pick<ChatMessage, 'role' | 'text' | 'patch'>): ChatMessage {
    const msg: ChatMessage = {
      id:        `msg_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`,
      role:      opts.role,
      text:      opts.text,
      timestamp: new Date(),
      patch:     opts.patch,
      applied:   false,
    };
    this.messages.push(msg);
    this.renderMessage(msg);
    this.scrollToBottom();
    return msg;
  }

  private renderMessage(msg: ChatMessage): void {
    const wrap = document.createElement('div');
    wrap.className = `chat-msg ${msg.role}`;
    wrap.dataset.msgId = msg.id;

    const bubble = document.createElement('div');
    bubble.className = 'msg-bubble';

    // Escape HTML to prevent injection; support **bold** and `code` via simple regex
    bubble.innerHTML = this.renderText(msg.text);

    // Patch preview for assistant messages that carry a NodeGraphConfig
    if (msg.patch && msg.role === 'assistant') {
      bubble.appendChild(this.buildPatchPreview(msg));
    }

    wrap.appendChild(bubble);

    // Timestamp metadata
    if (msg.role !== 'system') {
      const meta = document.createElement('div');
      meta.className = 'msg-meta';
      meta.textContent = this.formatTime(msg.timestamp);
      wrap.appendChild(meta);
    }

    this.messagesEl.appendChild(wrap);
  }

  private buildPatchPreview(msg: ChatMessage): HTMLElement {
    const preview = document.createElement('div');
    preview.className = 'msg-patch-preview';

    const nodeCount = msg.patch!.nodes.length;
    const edgeCount = msg.patch!.edges.length;

    preview.innerHTML = `
      <div class="patch-preview-header">
        <span>↯ Node Graph Patch</span>
        <span>${nodeCount} nodes · ${edgeCount} edges</span>
      </div>
      <div class="patch-preview-body">${this.escapeHTML(JSON.stringify(msg.patch, null, 2))}</div>
      <button class="patch-apply-btn${msg.applied ? ' applied' : ''}"
              ${msg.applied ? 'disabled' : ''}
              data-msg-id="${msg.id}">
        ${msg.applied ? '✓ Applied to Graph' : '↯ Apply Patch to Graph'}
      </button>
    `;

    preview.querySelector('.patch-apply-btn')?.addEventListener('click', () => {
      this.handlers.patchApply?.(msg.patch!, msg.id);
    });

    return preview;
  }

  // ── Submit flow ────────────────────────────────────────────────────────────

  private async submit(): Promise<void> {
    const text = this.inputEl.value.trim();
    if (!text || this.isThinking) return;

    // Clear input + route chip
    this.inputEl.value = '';
    this.autoGrow();
    this.routeChip.classList.add('hidden');

    // Show user message
    this.addMessage({ role: 'user', text });

    // Set thinking state
    this.setThinking(true);

    // Show typing indicator
    const typingId = this.showTypingIndicator();
    this.typingMsgId = typingId;

    if (this.agentPool) {
      try {
        const result = await this.agentPool.dispatch(text, this.context);
        this.removeTypingIndicator(typingId);
        this.handlePoolResult(result);
      } catch (err: unknown) {
        this.removeTypingIndicator(typingId);
        this.handleError(err instanceof Error ? err.message : String(err));
      }
    } else {
      try {
        await this.vibeNode.generate(text, this.context);
        // Result handled by onResult callback
      } catch (err: unknown) {
        this.removeTypingIndicator(typingId);
        this.handleError(err instanceof Error ? err.message : String(err));
      }
    }
  }

  private typingMsgId: string | null = null;

  private handlePoolResult(result: PoolResult): void {
    this.setThinking(false);
    this.activeAgentRoles = [];

    const { merged, succeeded, failed, totalMs } = result;
    const agentTags = succeeded.join(', ');
    const failNote  = failed.length ? ` (${failed.join(', ')} failed)` : '';
    const nodeNames = merged.nodes.map(n => `**${n.type}**`).slice(0, 3).join(', ');
    const more      = merged.nodes.length > 3 ? ` +${merged.nodes.length - 3} more` : '';
    const text = `[${totalMs}ms] ${agentTags}${failNote} Generated ${merged.nodes.length} nodes (${nodeNames}${more}) with ${merged.edges.length} connections.`;

    this.addMessage({ role: 'assistant', text, patch: merged });
  }

  private onAgentStatus(role: AgentRole, status: string): void {
    if (status === 'thinking') {
      if (!this.activeAgentRoles.includes(role)) this.activeAgentRoles.push(role);
    } else {
      this.activeAgentRoles = this.activeAgentRoles.filter(r => r !== role);
    }
    if (this.isThinking && this.activeAgentRoles.length > 0) {
      this.statusSub.textContent = `${this.activeAgentRoles.join(' + ')} · thinking…`;
    } else if (this.isThinking) {
      this.statusSub.textContent = 'claude-sonnet-4-6 · Thinking…';
    }
  }

  private updateRouteChip(text: string): void {
    if (!this.agentPool || !text.trim()) {
      this.routeChip.classList.add('hidden');
      return;
    }
    const roles = this.agentPool.route(text);
    if (roles.length === 0) {
      this.routeChip.classList.add('hidden');
      return;
    }
    this.routeChip.classList.remove('hidden');
    this.routeChip.innerHTML = '⚡ ' + roles.map(r => `<span class="agent-tag agent-tag--${r}">${r}</span>`).join(' ');
  }

  private handleResult(patch: NodeGraphConfig): void {
    if (this.typingMsgId) this.removeTypingIndicator(this.typingMsgId);
    this.setThinking(false);

    const nodeNames = patch.nodes.map(n => `**${n.type}**`).slice(0, 3).join(', ');
    const more = patch.nodes.length > 3 ? ` +${patch.nodes.length - 3} more` : '';
    const text = `Generated ${patch.nodes.length} nodes (${nodeNames}${more}) with ${patch.edges.length} connections. Review the patch and apply it to your node graph.`;

    this.addMessage({ role: 'assistant', text, patch });
  }

  private handleError(msg: string): void {
    if (this.typingMsgId) this.removeTypingIndicator(this.typingMsgId);
    this.setThinking(false);
    this.addMessage({ role: 'assistant', text: `⚠ Error: ${msg}` });
  }

  // ── Typing indicator ───────────────────────────────────────────────────────

  private showTypingIndicator(): string {
    const id = `typing_${Date.now()}`;
    const wrap = document.createElement('div');
    wrap.className = 'chat-msg assistant typing';
    wrap.id = id;
    wrap.innerHTML = `
      <div class="msg-bubble">
        <div class="typing-dots">
          <div class="typing-dot"></div>
          <div class="typing-dot"></div>
          <div class="typing-dot"></div>
        </div>
      </div>
    `;
    this.messagesEl.appendChild(wrap);
    this.scrollToBottom();
    return id;
  }

  private removeTypingIndicator(id: string): void {
    this.messagesEl.querySelector(`#${id}`)?.remove();
  }

  // ── State helpers ──────────────────────────────────────────────────────────

  private setThinking(val: boolean): void {
    this.isThinking = val;
    this.sendBtn.disabled = val;
    this.statusDot.classList.toggle('thinking', val);
    if (!val) {
      this.activeAgentRoles = [];
      this.statusSub.textContent = 'claude-sonnet-4-6 · Ready';
    } else if (this.activeAgentRoles.length === 0) {
      this.statusSub.textContent = 'claude-sonnet-4-6 · Thinking…';
    }
  }

  private autoGrow(): void {
    this.inputEl.style.height = 'auto';
    this.inputEl.style.height = `${Math.min(this.inputEl.scrollHeight, 120)}px`;
  }

  private scrollToBottom(): void {
    requestAnimationFrame(() => {
      this.messagesEl.scrollTop = this.messagesEl.scrollHeight;
    });
  }

  // ── Text utils ─────────────────────────────────────────────────────────────

  private renderText(raw: string): string {
    let s = this.escapeHTML(raw);
    // **bold**
    s = s.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
    // `code`
    s = s.replace(/`(.+?)`/g, '<code style="font-family:var(--font-mono); font-size:10px; background:rgba(0,212,255,0.10); padding:1px 4px; border-radius:3px; color:var(--neon-cyan);">$1</code>');
    return s;
  }

  private escapeHTML(s: string): string {
    return s
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  private formatTime(d: Date): string {
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  }
}
