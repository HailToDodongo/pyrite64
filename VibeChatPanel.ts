/**
 * VibeChatPanel.ts
 * Chat-based UI panel for Pyrite64's Vibe Coding workflow.
 *
 * Provides a conversational interface for AI-assisted game scripting:
 *  - Message history with user/assistant distinction
 *  - Inline NodeGraphConfig patch previews with "Apply" buttons
 *  - Quick-action buttons for common vibe coding tasks
 *  - Context-aware suggestions based on current scene state
 *  - Integration with VibeNode for multi-turn chat
 *
 * Designed to dock in the Pyrite64 editor layout alongside the
 * node graph canvas, viewport, and timeline panels.
 */

import {
  VibeNode,
  type VibeContext,
  type VibeChatOptions,
  type NodeGraphConfig,
  type ChatMessage,
} from './VibeNode';

// ─── Panel callbacks ──────────────────────────────────────────────────────────

export interface VibeChatPanelCallbacks {
  /** Fired when the user applies a generated NodeGraphConfig to the node graph. */
  onApplyPatch:  (patch: NodeGraphConfig) => void;
  /** Fired when the panel wants to update the status bar or notification system. */
  onStatusUpdate: (message: string) => void;
}

// ─── Panel options ────────────────────────────────────────────────────────────

export interface VibeChatPanelOptions {
  /** Container DOM element. */
  container:  HTMLElement;
  /** Scene context for the AI assistant. */
  context:    VibeContext;
  /** Callbacks for panel events. */
  callbacks:  VibeChatPanelCallbacks;
}

// ─── Quick actions ────────────────────────────────────────────────────────────

interface QuickAction {
  label:  string;
  icon:   string;
  prompt: string;
}

const QUICK_ACTIONS: QuickAction[] = [
  {
    label:  'Patrol',
    icon:   '🚶',
    prompt: 'Create a patrol behavior: move between waypoints A and B, wait 2 seconds at each.',
  },
  {
    label:  'Chase Player',
    icon:   '🏃',
    prompt: 'Create enemy AI: chase the player when within detection range, return to start when player escapes.',
  },
  {
    label:  'Animate',
    icon:   '🎬',
    prompt: 'Play the idle animation on start, switch to the run animation when moving.',
  },
  {
    label:  'Collectible',
    icon:   '⭐',
    prompt: 'Make this a collectible: bob up and down, rotate slowly, play pickup sound on collision with player.',
  },
  {
    label:  'Door/Switch',
    icon:   '🚪',
    prompt: 'Create a door that opens when the player activates a nearby switch.',
  },
  {
    label:  'Damage',
    icon:   '💥',
    prompt: 'Deal damage to the player on contact, with a 1-second cooldown between hits.',
  },
];

// ─── VibeChatPanel ────────────────────────────────────────────────────────────

export class VibeChatPanel {
  private container:  HTMLElement;
  private context:    VibeContext;
  private callbacks:  VibeChatPanelCallbacks;
  private vibeNode:   VibeNode;
  private isLoading   = false;

  // DOM refs
  private messagesDiv:  HTMLDivElement | null = null;
  private inputArea:    HTMLTextAreaElement | null = null;
  private sendBtn:      HTMLButtonElement | null = null;
  private statusDiv:    HTMLDivElement | null = null;

  constructor(opts: VibeChatPanelOptions) {
    this.container = opts.container;
    this.context   = opts.context;
    this.callbacks = opts.callbacks;

    this.vibeNode = new VibeNode({
      onResult: (patch) => this.callbacks.onApplyPatch(patch),
      onError:  (msg) => this.showError(msg),
    });

    this.buildUI();
    this.renderWelcome();
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  /** Update the scene context (e.g. when user selects a different entity). */
  updateContext(context: VibeContext): void {
    this.context = context;
    this.updateContextBadge();
  }

  /** Clear the conversation and start fresh. */
  clearConversation(): void {
    this.vibeNode.clearChatHistory();
    if (this.messagesDiv) {
      this.messagesDiv.innerHTML = '';
    }
    this.renderWelcome();
  }

  /** Programmatically send a message (e.g. from a quick action). */
  async sendMessage(text: string): Promise<void> {
    if (this.isLoading || !text.trim()) return;

    this.isLoading = true;
    this.updateSendButton();
    this.showStatus('Thinking…');

    const chatOpts: VibeChatOptions = {
      onResult:  (patch) => this.callbacks.onApplyPatch(patch),
      onError:   (msg) => {
        this.showError(msg);
        this.isLoading = false;
        this.updateSendButton();
        this.showStatus('');
      },
      onMessage: (msg) => {
        this.renderMessage(msg);
        if (msg.role === 'assistant') {
          this.isLoading = false;
          this.updateSendButton();
          this.showStatus('');
        }
      },
    };

    try {
      await this.vibeNode.chat(text, this.context, chatOpts);
    } catch {
      this.isLoading = false;
      this.updateSendButton();
      this.showStatus('');
    }
  }

  /** Clean up DOM and event listeners. */
  dispose(): void {
    this.container.innerHTML = '';
  }

  // ── Private: UI Construction ────────────────────────────────────────────────

  private buildUI(): void {
    this.container.style.cssText =
      'display:flex; flex-direction:column; background:#12122a; ' +
      'color:#e0e0e0; font-family:"Inter","Segoe UI",system-ui,sans-serif; ' +
      'font-size:13px; height:100%; overflow:hidden;';

    // ── Header ──
    const header = document.createElement('div');
    header.style.cssText =
      'display:flex; align-items:center; gap:8px; padding:8px 12px; ' +
      'background:#16162e; border-bottom:1px solid #2a2a4a; flex-shrink:0;';

    const title = document.createElement('div');
    title.style.cssText = 'font-weight:700; font-size:14px; color:#ffcc00;';
    title.textContent = '🎙 Vibe Coding';

    const contextBadge = document.createElement('div');
    contextBadge.id = 'p64-vibe-context';
    contextBadge.style.cssText =
      'margin-left:auto; font-size:11px; color:#888; ' +
      'background:#1a1a3a; padding:2px 8px; border-radius:10px;';
    contextBadge.textContent = `Entity: ${this.context.entityName}`;

    const clearBtn = document.createElement('button');
    clearBtn.textContent = '🗑';
    clearBtn.title = 'Clear conversation';
    clearBtn.style.cssText =
      'background:none; border:1px solid #3a3a5a; color:#888; ' +
      'border-radius:4px; padding:2px 6px; cursor:pointer; font-size:12px;';
    clearBtn.addEventListener('click', () => this.clearConversation());

    header.append(title, contextBadge, clearBtn);

    // ── Messages area ──
    this.messagesDiv = document.createElement('div');
    this.messagesDiv.style.cssText =
      'flex:1; overflow-y:auto; padding:12px; display:flex; ' +
      'flex-direction:column; gap:8px;';

    // ── Quick actions bar ──
    const quickBar = document.createElement('div');
    quickBar.style.cssText =
      'display:flex; flex-wrap:wrap; gap:4px; padding:6px 12px; ' +
      'border-top:1px solid #2a2a4a; flex-shrink:0;';

    for (const action of QUICK_ACTIONS) {
      const btn = document.createElement('button');
      btn.textContent = `${action.icon} ${action.label}`;
      btn.title = action.prompt;
      btn.style.cssText =
        'background:#1e1e3a; color:#ccc; border:1px solid #2a2a5a; ' +
        'border-radius:12px; padding:3px 10px; cursor:pointer; font-size:11px; ' +
        'transition:background 0.15s;';
      btn.addEventListener('mouseenter', () => { btn.style.background = '#2a2a5a'; });
      btn.addEventListener('mouseleave', () => { btn.style.background = '#1e1e3a'; });
      btn.addEventListener('click', () => this.sendMessage(action.prompt));
      quickBar.append(btn);
    }

    // ── Input area ──
    const inputRow = document.createElement('div');
    inputRow.style.cssText =
      'display:flex; gap:6px; padding:8px 12px; ' +
      'background:#16162e; border-top:1px solid #2a2a4a; flex-shrink:0;';

    this.inputArea = document.createElement('textarea');
    this.inputArea.placeholder = 'Describe game behavior… (Enter to send, Shift+Enter for newline)';
    this.inputArea.rows = 2;
    this.inputArea.style.cssText =
      'flex:1; background:#1a1a3a; color:#e0e0e0; border:1px solid #3a3a5a; ' +
      'border-radius:8px; padding:8px 10px; resize:none; font-size:13px; ' +
      'font-family:inherit; outline:none;';
    this.inputArea.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        this.handleSend();
      }
    });

    this.sendBtn = document.createElement('button');
    this.sendBtn.textContent = '➤';
    this.sendBtn.style.cssText =
      'background:#ffcc00; color:#12122a; border:none; border-radius:8px; ' +
      'padding:8px 14px; cursor:pointer; font-size:16px; font-weight:700; ' +
      'align-self:flex-end; min-height:40px; transition:opacity 0.15s;';
    this.sendBtn.addEventListener('click', () => this.handleSend());

    inputRow.append(this.inputArea, this.sendBtn);

    // ── Status bar ──
    this.statusDiv = document.createElement('div');
    this.statusDiv.style.cssText =
      'padding:2px 12px; font-size:11px; color:#888; height:16px; flex-shrink:0;';

    this.container.append(header, this.messagesDiv, quickBar, inputRow, this.statusDiv);
  }

  // ── Private: Message rendering ──────────────────────────────────────────────

  private renderWelcome(): void {
    if (!this.messagesDiv) return;

    const welcome = document.createElement('div');
    welcome.style.cssText =
      'text-align:center; padding:24px 16px; color:#888;';

    welcome.innerHTML = `
      <div style="font-size:32px; margin-bottom:8px;">🎙</div>
      <div style="font-size:15px; font-weight:600; color:#ccc; margin-bottom:4px;">
        Vibe Coding Assistant
      </div>
      <div style="font-size:12px; line-height:1.5;">
        Describe game behavior in plain English and I'll generate<br>
        the node graph for your N64 game.<br>
        <span style="color:#ffcc00;">Try a quick action below or type your own prompt.</span>
      </div>
    `;

    this.messagesDiv.append(welcome);
  }

  private renderMessage(msg: ChatMessage): void {
    if (!this.messagesDiv) return;

    // Remove welcome message on first real message
    if (this.vibeNode.getChatHistory().length <= 2) {
      const welcome = this.messagesDiv.querySelector('div[style*="text-align:center"]');
      if (welcome) welcome.remove();
    }

    const bubble = document.createElement('div');
    const isUser = msg.role === 'user';

    bubble.style.cssText =
      `max-width:85%; padding:10px 14px; border-radius:12px; line-height:1.5; ` +
      `word-wrap:break-word; white-space:pre-wrap; ` +
      (isUser
        ? 'align-self:flex-end; background:#2a2a5a; color:#e0e0e0; border-bottom-right-radius:4px;'
        : 'align-self:flex-start; background:#1e1e38; color:#ddd; border-bottom-left-radius:4px;');

    // Render content
    if (isUser) {
      bubble.textContent = msg.content;
    } else {
      this.renderAssistantContent(bubble, msg);
    }

    this.messagesDiv.append(bubble);
    this.messagesDiv.scrollTop = this.messagesDiv.scrollHeight;
  }

  private renderAssistantContent(container: HTMLElement, msg: ChatMessage): void {
    if (msg.patch) {
      // Split content around the JSON block
      const jsonStart = msg.content.indexOf('{');
      const jsonEnd = msg.content.lastIndexOf('}');

      if (jsonStart > 0) {
        const beforeText = document.createElement('div');
        beforeText.textContent = msg.content.slice(0, jsonStart).trim();
        beforeText.style.marginBottom = '8px';
        container.append(beforeText);
      }

      // Render patch preview
      const patchPreview = this.renderPatchPreview(msg.patch);
      container.append(patchPreview);

      if (jsonEnd < msg.content.length - 1) {
        const afterText = document.createElement('div');
        afterText.textContent = msg.content.slice(jsonEnd + 1).trim();
        afterText.style.marginTop = '8px';
        container.append(afterText);
      }
    } else {
      container.textContent = msg.content;
    }
  }

  private renderPatchPreview(patch: NodeGraphConfig): HTMLElement {
    const wrapper = document.createElement('div');
    wrapper.style.cssText =
      'background:#16162a; border:1px solid #3a3a5a; border-radius:8px; ' +
      'padding:10px; margin:4px 0;';

    // Summary
    const summary = document.createElement('div');
    summary.style.cssText = 'font-size:12px; color:#aaa; margin-bottom:6px;';
    summary.textContent = `📊 ${patch.nodes.length} node${patch.nodes.length !== 1 ? 's' : ''}, ` +
      `${patch.edges.length} connection${patch.edges.length !== 1 ? 's' : ''}`;
    wrapper.append(summary);

    // Node list
    const nodeList = document.createElement('div');
    nodeList.style.cssText = 'font-size:11px; color:#88cc44; font-family:monospace; margin-bottom:8px;';
    const nodeNames = patch.nodes.map(n => `  ${n.type}`).join('\n');
    nodeList.textContent = nodeNames;
    wrapper.append(nodeList);

    // Apply button
    const applyBtn = document.createElement('button');
    applyBtn.textContent = '✅ Apply to Node Graph';
    applyBtn.style.cssText =
      'background:#2a6a2a; color:#e0e0e0; border:1px solid #3a8a3a; ' +
      'border-radius:6px; padding:5px 14px; cursor:pointer; font-size:12px; ' +
      'width:100%; transition:background 0.15s;';
    applyBtn.addEventListener('mouseenter', () => { applyBtn.style.background = '#3a8a3a'; });
    applyBtn.addEventListener('mouseleave', () => { applyBtn.style.background = '#2a6a2a'; });
    applyBtn.addEventListener('click', () => {
      this.callbacks.onApplyPatch(patch);
      applyBtn.textContent = '✅ Applied!';
      applyBtn.style.background = '#1a4a1a';
      applyBtn.disabled = true;
      this.callbacks.onStatusUpdate('Patch applied to node graph');
    });
    wrapper.append(applyBtn);

    return wrapper;
  }

  // ── Private: Interaction ────────────────────────────────────────────────────

  private handleSend(): void {
    if (!this.inputArea || this.isLoading) return;
    const text = this.inputArea.value.trim();
    if (!text) return;
    this.inputArea.value = '';
    this.sendMessage(text);
  }

  private updateSendButton(): void {
    if (!this.sendBtn) return;
    this.sendBtn.disabled = this.isLoading;
    this.sendBtn.style.opacity = this.isLoading ? '0.5' : '1';
    this.sendBtn.textContent = this.isLoading ? '⏳' : '➤';
  }

  private updateContextBadge(): void {
    const badge = document.getElementById('p64-vibe-context');
    if (badge) {
      badge.textContent = `Entity: ${this.context.entityName}`;
    }
  }

  private showStatus(text: string): void {
    if (this.statusDiv) {
      this.statusDiv.textContent = text;
    }
  }

  private showError(msg: string): void {
    if (!this.messagesDiv) return;

    const errorDiv = document.createElement('div');
    errorDiv.style.cssText =
      'align-self:center; background:#4a1a1a; color:#ff6666; ' +
      'border:1px solid #6a2a2a; border-radius:8px; padding:8px 14px; ' +
      'font-size:12px; max-width:90%;';
    errorDiv.textContent = `⚠ ${msg}`;
    this.messagesDiv.append(errorDiv);
    this.messagesDiv.scrollTop = this.messagesDiv.scrollHeight;
  }
}
