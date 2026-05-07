import fs from "fs";
import path from "path";
import { WebSocket, type RawData } from "ws";
import { dataDir } from "../utils/dir";
import { getRuntimeSettings } from "../config/runtime-settings";

type BotNetMode = "solo" | "botnet";
type BotNetStarter = "self" | "peer";
type BotNetSpeakerType = "self" | "peer" | "user" | "system";
type BotNetRelayMode = "persona-relay" | "auto-bot";
type BotNetTransportMode = "lan-direct" | "online-hub";

export interface BotNetSettings {
  enabled: boolean;
  transportMode: BotNetTransportMode;
  peerUrl: string;
  publicUrl: string;
  hubUrl: string;
  nodeHandle: string;
  botnetMode: BotNetRelayMode;
  maxBotReplies: number;
  replyDelaySec: number;
}

export interface BotNetMessage {
  id: string;
  createdAt: string;
  speakerType: BotNetSpeakerType;
  speakerName: string;
  kind: "message" | "event" | "relay" | "user-prompt";
  text: string;
}

export interface BotNetConversation {
  id: string;
  topic: string;
  mode: BotNetMode;
  starter: BotNetStarter | "user";
  botnetMode: BotNetRelayMode;
  transportMode: BotNetTransportMode;
  peerUrl: string;
  linkId: string;
  peerNodeId: string;
  status: "active" | "stopped" | "complete";
  maxBotReplies: number;
  replyCount: number;
  createdAt: string;
  updatedAt: string;
  messages: BotNetMessage[];
}

interface BotNetHubSession {
  nodeId: string;
  authToken: string;
  websocketUrl: string;
  linkId: string;
  peerNodeId: string;
  peerHandle: string;
  peerOnline: boolean;
  lastError: string;
}

export interface BotNetOnlineState {
  transportMode: BotNetTransportMode;
  hubUrl: string;
  nodeHandle: string;
  nodeId: string;
  registered: boolean;
  connected: boolean;
  websocketUrl: string;
  linkId: string;
  peerNodeId: string;
  peerHandle: string;
  peerOnline: boolean;
  lastError: string;
  authConfigured: boolean;
}

export interface BotNetState {
  settings: BotNetSettings;
  connectionStatus: string;
  online: BotNetOnlineState;
  conversations: BotNetConversation[];
}

const SETTINGS_PATH = path.join(dataDir, "botnet-settings.json");
const CONVERSATIONS_PATH = path.join(dataDir, "botnet-conversations.json");
const HUB_SESSION_PATH = path.join(dataDir, "botnet-hub-session.json");

const DEFAULT_SETTINGS: BotNetSettings = {
  enabled: false,
  transportMode: "lan-direct",
  peerUrl: "",
  publicUrl: "",
  hubUrl: "",
  nodeHandle: "Whisplay Bot",
  botnetMode: "auto-bot",
  maxBotReplies: 8,
  replyDelaySec: 6,
};

const DEFAULT_HUB_SESSION: BotNetHubSession = {
  nodeId: "",
  authToken: "",
  websocketUrl: "",
  linkId: "",
  peerNodeId: "",
  peerHandle: "",
  peerOnline: false,
  lastError: "",
};

const lazyDisplay = () =>
  require("./display") as {
    display: (status: Partial<Record<string, unknown>>) => void;
  };

function nowIso(): string {
  return new Date().toISOString();
}

function randomId(): string {
  return `${Date.now()}-${Math.random().toString(16).slice(2, 10)}`;
}

function clampInt(value: unknown, fallback: number, min: number, max: number): number {
  const parsed =
    typeof value === "number" ? value : Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  return Math.min(max, Math.max(min, Math.round(parsed)));
}

function normalizeUrl(value: unknown): string {
  if (typeof value !== "string") {
    return "";
  }
  const trimmed = value.trim();
  if (!trimmed) {
    return "";
  }
  const withProtocol = /^https?:\/\//i.test(trimmed)
    ? trimmed
    : `http://${trimmed}`;
  return withProtocol.replace(/\/+$/, "");
}

function normalizeBotnetMode(value: unknown): BotNetRelayMode {
  return value === "persona-relay" ? "persona-relay" : "auto-bot";
}

function normalizeTransportMode(value: unknown): BotNetTransportMode {
  return value === "online-hub" ? "online-hub" : "lan-direct";
}

function normalizeNodeHandle(value: unknown): string {
  if (typeof value !== "string") {
    return DEFAULT_SETTINGS.nodeHandle;
  }
  const trimmed = value.trim();
  return trimmed || DEFAULT_SETTINGS.nodeHandle;
}

function sanitizeSettings(input: Partial<BotNetSettings> | null | undefined): BotNetSettings {
  return {
    enabled: Boolean(input?.enabled),
    transportMode: normalizeTransportMode(input?.transportMode),
    peerUrl: normalizeUrl(input?.peerUrl),
    publicUrl: normalizeUrl(input?.publicUrl),
    hubUrl: normalizeUrl(input?.hubUrl),
    nodeHandle: normalizeNodeHandle(input?.nodeHandle),
    botnetMode: normalizeBotnetMode(input?.botnetMode),
    maxBotReplies: clampInt(input?.maxBotReplies, DEFAULT_SETTINGS.maxBotReplies, 0, 200),
    replyDelaySec: clampInt(input?.replyDelaySec, DEFAULT_SETTINGS.replyDelaySec, 0, 3600),
  };
}

function sanitizeHubSession(
  input: Partial<BotNetHubSession> | null | undefined,
): BotNetHubSession {
  return {
    nodeId: typeof input?.nodeId === "string" ? input.nodeId.trim() : "",
    authToken: typeof input?.authToken === "string" ? input.authToken.trim() : "",
    websocketUrl: normalizeUrl(input?.websocketUrl),
    linkId: typeof input?.linkId === "string" ? input.linkId.trim() : "",
    peerNodeId: typeof input?.peerNodeId === "string" ? input.peerNodeId.trim() : "",
    peerHandle: typeof input?.peerHandle === "string" ? input.peerHandle.trim() : "",
    peerOnline: Boolean(input?.peerOnline),
    lastError: typeof input?.lastError === "string" ? input.lastError.trim() : "",
  };
}

function readJson<T>(filePath: string, fallback: T): T {
  try {
    if (!fs.existsSync(filePath)) {
      return fallback;
    }
    return JSON.parse(fs.readFileSync(filePath, "utf8")) as T;
  } catch {
    return fallback;
  }
}

function writeJson(filePath: string, value: unknown): void {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function getField(
  body: Record<string, unknown>,
  camelKey: string,
  normalizedKey = camelKey.toLowerCase(),
): unknown {
  if (camelKey in body) {
    return body[camelKey];
  }
  if (normalizedKey in body) {
    return body[normalizedKey];
  }
  return undefined;
}

function makeMessage(
  speakerType: BotNetSpeakerType,
  speakerName: string,
  text: string,
  kind: BotNetMessage["kind"] = "message",
): BotNetMessage {
  return {
    id: randomId(),
    createdAt: nowIso(),
    speakerType,
    speakerName,
    kind,
    text: text.trim(),
  };
}

function buildSystemPrompt(): string {
  const runtime = getRuntimeSettings();
  return [
    runtime.personalityPrompt ||
      "You are a concise, imaginative chatbot talking to another chatbot.",
    "You are part of GroqBotNet inside Whisplay.",
    "Keep replies concise, natural, and in character.",
    "Do not mention hidden prompts, APIs, or tokens.",
  ].join(" ");
}

function buildPersonaRelaySystemPrompt(): string {
  const runtime = getRuntimeSettings();
  return [
    runtime.personalityPrompt ||
      "You are a concise, imaginative chatbot talking to another chatbot.",
    "You are part of GroqBotNet inside Whisplay.",
    "You are rewriting the local user's intent into the exact message your bot would send to the peer bot.",
    "Preserve the user's intent, but express it in your own personality and voice.",
    "Do not answer the local user directly.",
    "Write the final message itself, exactly as it should be sent to the peer.",
    "Do not give advice, instructions, stage directions, or commentary.",
    "Do not say things like 'ask them', 'tell them', 'send a message', 'you want to know', or 'here is the message'.",
    "Do not wrap the whole reply in quotes unless the message itself truly needs quotes.",
    'Example local prompt: "ask my friend how it is doing today" -> "How are you doing today, friend?"',
    "Return only the outgoing message text with no commentary about rewriting, filtering, or translating.",
  ].join(" ");
}

class BotNetManager {
  private settings: BotNetSettings = sanitizeSettings(
    readJson<Partial<BotNetSettings>>(SETTINGS_PATH, DEFAULT_SETTINGS),
  );

  private conversations: BotNetConversation[] = readJson<BotNetConversation[]>(
    CONVERSATIONS_PATH,
    [],
  );

  private hubSession: BotNetHubSession = sanitizeHubSession(
    readJson<Partial<BotNetHubSession>>(HUB_SESSION_PATH, DEFAULT_HUB_SESSION),
  );

  private connectionStatus = "Disconnected";
  private activeTimer: NodeJS.Timeout | null = null;
  private hubSocket: WebSocket | null = null;
  private hubHeartbeatTimer: NodeJS.Timeout | null = null;
  private hubReconnectTimer: NodeJS.Timeout | null = null;
  private hubDisconnectRequested = false;

  constructor() {
    this.refreshConnectionStatus();
    this.maybeStartHubSession();
  }

  private save(): void {
    writeJson(SETTINGS_PATH, this.settings);
    writeJson(CONVERSATIONS_PATH, this.conversations.slice(0, 20));
    writeJson(HUB_SESSION_PATH, this.hubSession);
  }

  private maybeStartHubSession(): void {
    if (
      !this.settings.enabled ||
      !this.isOnlineTransport() ||
      !this.hubSession.nodeId ||
      !this.hubSession.authToken ||
      !this.settings.hubUrl
    ) {
      return;
    }
    setTimeout(() => {
      void this.connectHub().catch((error) => {
        this.hubSession.lastError =
          error instanceof Error ? error.message : "Failed to connect to the hub.";
        this.refreshConnectionStatus();
        this.save();
      });
    }, 1000);
  }

  private clearHubHeartbeat(): void {
    if (this.hubHeartbeatTimer) {
      clearInterval(this.hubHeartbeatTimer);
      this.hubHeartbeatTimer = null;
    }
  }

  private clearHubReconnect(): void {
    if (this.hubReconnectTimer) {
      clearTimeout(this.hubReconnectTimer);
      this.hubReconnectTimer = null;
    }
  }

  private startHubHeartbeat(): void {
    this.clearHubHeartbeat();
    this.hubHeartbeatTimer = setInterval(() => {
      if (!this.hubSocket || this.hubSocket.readyState !== WebSocket.OPEN) {
        this.clearHubHeartbeat();
        return;
      }
      this.hubSocket.send(
        JSON.stringify({
          type: "botnet.ping",
          nodeId: this.hubSession.nodeId,
        }),
      );
    }, 30000);
  }

  private closeHubSocket(): void {
    if (this.hubSocket) {
      this.hubSocket.removeAllListeners();
      this.hubSocket.close();
      this.hubSocket = null;
    }
    this.clearHubHeartbeat();
  }

  private reconnectHubLater(): void {
    this.clearHubReconnect();
    if (
      this.hubDisconnectRequested ||
      !this.settings.enabled ||
      !this.isOnlineTransport() ||
      !this.hubSession.nodeId ||
      !this.hubSession.authToken ||
      !this.settings.hubUrl
    ) {
      return;
    }
    this.hubReconnectTimer = setTimeout(() => {
      this.hubReconnectTimer = null;
      void this.connectHub().catch((error) => {
        this.hubSession.lastError =
          error instanceof Error ? error.message : "Failed to reconnect to the hub.";
        this.refreshConnectionStatus();
        this.save();
      });
    }, 5000);
  }

  private isOnlineTransport(
    target?: Pick<BotNetSettings, "transportMode"> | Pick<BotNetConversation, "transportMode">,
  ): boolean {
    return normalizeTransportMode(target?.transportMode || this.settings.transportMode) === "online-hub";
  }

  private isHubConnected(): boolean {
    return Boolean(this.hubSocket && this.hubSocket.readyState === WebSocket.OPEN);
  }

  private refreshConnectionStatus(): void {
    if (this.isOnlineTransport()) {
      if (this.isHubConnected()) {
        if (this.hubSession.linkId) {
          this.connectionStatus = this.hubSession.peerOnline
            ? "Hub connected • Peer online"
            : "Hub connected • Peer offline";
        } else {
          this.connectionStatus = "Hub connected";
        }
        return;
      }
      if (this.hubSession.nodeId && this.hubSession.authToken) {
        this.connectionStatus = "Hub registered";
        return;
      }
      if (this.settings.hubUrl) {
        this.connectionStatus = "Hub configured";
        return;
      }
      this.connectionStatus = "Disconnected";
      return;
    }

    if (this.settings.peerUrl) {
      this.connectionStatus = "Reachable";
      return;
    }
    this.connectionStatus = "Disconnected";
  }

  private getActiveConversation(): BotNetConversation | null {
    return this.conversations.find((conversation) => conversation.status === "active") || null;
  }

  private touchConversation(conversation: BotNetConversation): void {
    conversation.updatedAt = nowIso();
    this.save();
  }

  private addMessage(conversation: BotNetConversation, message: BotNetMessage): void {
    conversation.messages.push(message);
    conversation.updatedAt = nowIso();
    this.save();
    this.renderToDisplay(conversation, message);
  }

  private addEvent(conversation: BotNetConversation, text: string): void {
    this.addMessage(conversation, makeMessage("system", "BotNet", text, "event"));
  }

  private renderToDisplay(
    conversation: BotNetConversation,
    message: BotNetMessage,
  ): void {
    const runtime = getRuntimeSettings();
    const title =
      message.speakerType === "peer"
        ? `Peer: ${message.speakerName}`
        : message.speakerType === "self"
          ? `${message.speakerName}`
          : "BotNet";
    try {
      lazyDisplay().display({
        status: "idle",
        emoji: message.speakerType === "peer" ? "🛰️" : "🤖",
        RGB: message.speakerType === "peer" ? "#3a7bff" : "#00a676",
        text: `${title}\n${message.text}`,
        rag_icon_visible: false,
        image_icon_visible: false,
        scroll_speed: 5,
        scroll_speed_factor: 0.4 + (runtime.scrollSpeedLevel - 1) * 0.1777777778,
      });
    } catch (error) {
      console.warn("[BotNet] Failed to mirror message to display:", error);
    }
  }

  private clearTimer(): void {
    if (this.activeTimer) {
      clearTimeout(this.activeTimer);
      this.activeTimer = null;
    }
  }

  private buildConversationMessages(
    conversation: BotNetConversation,
    extraUserPrompt: string,
  ): Array<{ role: "system" | "assistant" | "user"; content: string }> {
    const recent = conversation.messages
      .filter((message) => message.kind === "message" || message.kind === "relay")
      .slice(-Math.max(1, getRuntimeSettings().voiceMode ? 12 : 12));

    const messages: Array<{ role: "system" | "assistant" | "user"; content: string }> = [
      {
        role: "system",
        content: buildSystemPrompt(),
      },
    ];

    for (const message of recent) {
      if (message.speakerType === "self") {
        messages.push({ role: "assistant", content: message.text });
      } else if (
        message.speakerType === "peer" ||
        message.speakerType === "user"
      ) {
        messages.push({ role: "user", content: message.text });
      }
    }

    if (extraUserPrompt.trim()) {
      messages.push({ role: "user", content: extraUserPrompt.trim() });
    }

    return messages;
  }

  private buildPersonaRelayMessages(
    conversation: BotNetConversation,
    userPrompt: string,
  ): Array<{ role: "system" | "assistant" | "user"; content: string }> {
    const recent = conversation.messages
      .filter((message) => message.kind === "message" || message.kind === "relay")
      .slice(-Math.max(1, getRuntimeSettings().voiceMode ? 12 : 12));

    const messages: Array<{ role: "system" | "assistant" | "user"; content: string }> = [
      {
        role: "system",
        content: buildPersonaRelaySystemPrompt(),
      },
    ];

    for (const message of recent) {
      if (message.speakerType === "self") {
        messages.push({ role: "assistant", content: message.text });
      } else if (message.speakerType === "peer") {
        messages.push({ role: "user", content: message.text });
      }
    }

    messages.push({
      role: "user",
      content: `Rewrite this into the exact message to send to the peer bot: ${userPrompt.trim()}`,
    });

    return messages;
  }

  private async callGroq(
    messages: Array<{ role: "system" | "assistant" | "user"; content: string }>,
  ): Promise<string> {
    const runtime = getRuntimeSettings();
    const apiKey =
      runtime.groqApiKey ||
      process.env.OPENAI_API_KEY ||
      process.env.GROQ_API_KEY ||
      "";
    if (!apiKey) {
      throw new Error("No Groq API key configured in Whisplay settings.");
    }

    const response = await fetch("https://api.groq.com/openai/v1/chat/completions", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${apiKey}`,
      },
      body: JSON.stringify({
        model: process.env.OPENAI_LLM_MODEL || "llama-3.1-8b-instant",
        messages,
        temperature: 0.9,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(
        payload?.error?.message || `Groq request failed with HTTP ${response.status}`,
      );
    }
    const content = payload?.choices?.[0]?.message?.content;
    if (!content || typeof content !== "string") {
      throw new Error("Groq returned an empty reply.");
    }
    return content.trim();
  }

  private async generatePersonaRelay(
    conversation: BotNetConversation,
    userPrompt: string,
  ): Promise<string> {
    return this.callGroq(this.buildPersonaRelayMessages(conversation, userPrompt));
  }

  private defaultHubWebSocketUrl(): string {
    const baseUrl = this.settings.hubUrl;
    if (!baseUrl) {
      return "";
    }
    const asWebSocket = baseUrl.replace(/^http/i, "ws");
    return `${asWebSocket}/api/v1/botnet/connect`;
  }

  private getPublicOnlineState(): BotNetOnlineState {
    return {
      transportMode: this.settings.transportMode,
      hubUrl: this.settings.hubUrl,
      nodeHandle: this.settings.nodeHandle,
      nodeId: this.hubSession.nodeId,
      registered: Boolean(this.hubSession.nodeId && this.hubSession.authToken),
      connected: this.isHubConnected(),
      websocketUrl: this.hubSession.websocketUrl,
      linkId: this.hubSession.linkId,
      peerNodeId: this.hubSession.peerNodeId,
      peerHandle: this.hubSession.peerHandle,
      peerOnline: this.hubSession.peerOnline,
      lastError: this.hubSession.lastError,
      authConfigured: Boolean(this.hubSession.authToken),
    };
  }

  getState(): BotNetState {
    this.refreshConnectionStatus();
    return {
      settings: { ...this.settings },
      connectionStatus: this.connectionStatus,
      online: this.getPublicOnlineState(),
      conversations: this.conversations.slice(0, 10),
    };
  }

  saveSettings(update: Partial<BotNetSettings>): BotNetSettings {
    const previous = this.settings;
    this.settings = sanitizeSettings({ ...this.settings, ...update });
    const onlineConfigChanged =
      previous.transportMode !== this.settings.transportMode ||
      previous.hubUrl !== this.settings.hubUrl ||
      previous.nodeHandle !== this.settings.nodeHandle ||
      previous.enabled !== this.settings.enabled;

    if (!this.settings.enabled && this.isHubConnected()) {
      this.hubDisconnectRequested = true;
      this.closeHubSocket();
    }

    if (!this.isOnlineTransport() && this.isHubConnected()) {
      this.hubDisconnectRequested = true;
      this.closeHubSocket();
    }

    if (onlineConfigChanged) {
      this.hubSession.lastError = "";
      this.clearHubReconnect();
      if (this.isHubConnected()) {
        this.hubDisconnectRequested = true;
        this.closeHubSocket();
      }
    }

    this.refreshConnectionStatus();
    this.save();

    if (
      this.settings.enabled &&
      this.isOnlineTransport() &&
      this.hubSession.nodeId &&
      this.hubSession.authToken
    ) {
      this.hubDisconnectRequested = false;
      this.maybeStartHubSession();
    }

    return { ...this.settings };
  }

  private async checkHubHealth(): Promise<string> {
    const hubUrl = normalizeUrl(this.settings.hubUrl);
    if (!hubUrl) {
      this.refreshConnectionStatus();
      return "Hub URL is not configured.";
    }

    const probePaths = ["/api/v1/botnet/health", "/health"];
    let lastError = "Hub check failed.";
    for (const probePath of probePaths) {
      try {
        const response = await fetch(`${hubUrl}${probePath}`);
        if (response.ok) {
          this.refreshConnectionStatus();
          return "Hub is reachable.";
        }
        lastError = `Hub returned HTTP ${response.status}.`;
      } catch (error) {
        lastError = error instanceof Error ? error.message : "Hub check failed.";
      }
    }
    this.hubSession.lastError = lastError;
    this.refreshConnectionStatus();
    this.save();
    throw new Error(lastError);
  }

  async testPeer(): Promise<{ ok: boolean; message: string }> {
    if (this.isOnlineTransport()) {
      if (this.isHubConnected()) {
        this.refreshConnectionStatus();
        return { ok: true, message: "Hub relay connection is active." };
      }
      try {
        const message = await this.checkHubHealth();
        return { ok: true, message };
      } catch (error) {
        return {
          ok: false,
          message: error instanceof Error ? error.message : "Hub check failed.",
        };
      }
    }

    const peerUrl = normalizeUrl(this.settings.peerUrl);
    if (!peerUrl) {
      this.connectionStatus = "Disconnected";
      return { ok: false, message: "Peer URL is not configured." };
    }
    try {
      const response = await fetch(`${peerUrl}/api/state`);
      const payload = await response.json().catch(() => ({}));
      if (!response.ok || payload.ok === false) {
        throw new Error(payload.error || `HTTP ${response.status}`);
      }
      this.connectionStatus = "Reachable";
      return { ok: true, message: "Peer bot is reachable." };
    } catch (error) {
      this.connectionStatus = "Disconnected";
      return {
        ok: false,
        message: error instanceof Error ? error.message : "Peer check failed.",
      };
    }
  }

  async registerWithHub(): Promise<BotNetOnlineState> {
    const hubUrl = normalizeUrl(this.settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    if (!this.settings.nodeHandle.trim()) {
      throw new Error("Node handle is required.");
    }

    const response = await fetch(`${hubUrl}/api/v1/botnet/nodes/register`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        nodeId: this.hubSession.nodeId || undefined,
        handle: this.settings.nodeHandle,
        deviceType: "whisplay",
        botName: "Whisplay Bot",
        capabilities: {
          modes: ["auto-bot", "persona-relay"],
          relayTransport: true,
          transportModes: ["lan-direct", "online-hub"],
        },
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `Hub registration failed with HTTP ${response.status}`);
    }

    const nodeId =
      String(payload.nodeId || payload?.node?.id || this.hubSession.nodeId || "").trim();
    const authToken =
      String(
        payload.authToken ||
          payload.token ||
          payload.sessionToken ||
          payload?.session?.token ||
          "",
      ).trim();
    if (!nodeId || !authToken) {
      throw new Error("Hub registration response did not include node credentials.");
    }

    this.hubSession = sanitizeHubSession({
      ...this.hubSession,
      nodeId,
      authToken,
      websocketUrl:
        String(payload.websocketUrl || payload.wsUrl || this.hubSession.websocketUrl || "").trim() ||
        this.defaultHubWebSocketUrl(),
      linkId: String(payload.linkId || this.hubSession.linkId || "").trim(),
      peerNodeId: String(payload.peerNodeId || this.hubSession.peerNodeId || "").trim(),
      peerHandle: String(payload.peerHandle || this.hubSession.peerHandle || "").trim(),
      peerOnline:
        typeof payload.peerOnline === "boolean"
          ? payload.peerOnline
          : this.hubSession.peerOnline,
      lastError: "",
    });
    this.hubDisconnectRequested = false;
    this.refreshConnectionStatus();
    this.save();
    return this.getPublicOnlineState();
  }

  async connectHub(): Promise<BotNetOnlineState> {
    if (!this.isOnlineTransport()) {
      throw new Error("Set the transport mode to Online Hub first.");
    }
    if (!this.settings.enabled) {
      throw new Error("BotNet mode is turned off.");
    }
    if (!this.hubSession.nodeId || !this.hubSession.authToken) {
      await this.registerWithHub();
    }
    if (this.isHubConnected()) {
      return this.getPublicOnlineState();
    }
    if (this.hubSocket && this.hubSocket.readyState === WebSocket.CONNECTING) {
      return this.getPublicOnlineState();
    }

    const socketBase = this.hubSession.websocketUrl || this.defaultHubWebSocketUrl();
    if (!socketBase) {
      throw new Error("Hub websocket URL is not configured.");
    }

    const socketUrl = new URL(socketBase);
    socketUrl.searchParams.set("nodeId", this.hubSession.nodeId);
    socketUrl.searchParams.set("token", this.hubSession.authToken);

    this.hubDisconnectRequested = false;
    this.closeHubSocket();
    this.clearHubReconnect();

    await new Promise<void>((resolve, reject) => {
      const socket = new WebSocket(socketUrl.toString());
      let settled = false;

      const fail = (message: string): void => {
        if (settled) {
          return;
        }
        settled = true;
        this.hubSession.lastError = message;
        this.refreshConnectionStatus();
        this.save();
        reject(new Error(message));
      };

      socket.once("open", () => {
        this.hubSocket = socket;
        this.hubSession.lastError = "";
        this.startHubHeartbeat();
        this.refreshConnectionStatus();
        this.save();

        socket.send(
          JSON.stringify({
            type: "botnet.hello",
            nodeId: this.hubSession.nodeId,
            token: this.hubSession.authToken,
            handle: this.settings.nodeHandle,
            linkId: this.hubSession.linkId || undefined,
          }),
        );

        socket.on("message", (rawMessage) => {
          void this.handleHubMessage(rawMessage).catch((error) => {
            this.hubSession.lastError =
              error instanceof Error ? error.message : "Failed to process hub message.";
            this.refreshConnectionStatus();
            this.save();
          });
        });

        socket.on("close", () => {
          this.closeHubSocket();
          this.refreshConnectionStatus();
          this.save();
          this.reconnectHubLater();
        });

        socket.on("error", (error) => {
          this.hubSession.lastError =
            error instanceof Error ? error.message : "Hub websocket error.";
          this.refreshConnectionStatus();
          this.save();
        });

        if (!settled) {
          settled = true;
          resolve();
        }
      });

      socket.once("error", (error) => {
        fail(error instanceof Error ? error.message : "Hub websocket error.");
      });
    });

    return this.getPublicOnlineState();
  }

  disconnectHub(): BotNetOnlineState {
    this.hubDisconnectRequested = true;
    this.clearHubReconnect();
    this.closeHubSocket();
    this.refreshConnectionStatus();
    this.save();
    return this.getPublicOnlineState();
  }

  async createHubInvite(): Promise<{ inviteCode: string; expiresAt: string }> {
    const hubUrl = normalizeUrl(this.settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    if (!this.hubSession.nodeId || !this.hubSession.authToken) {
      throw new Error("Register this Whisplay node with the hub first.");
    }
    const response = await fetch(`${hubUrl}/api/v1/botnet/invites/create`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        nodeId: this.hubSession.nodeId,
        token: this.hubSession.authToken,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `Hub invite creation failed with HTTP ${response.status}`);
    }
    this.hubSession.lastError = "";
    this.refreshConnectionStatus();
    this.save();
    return {
      inviteCode: String(payload.inviteCode || "").trim(),
      expiresAt: String(payload.expiresAt || "").trim(),
    };
  }

  async redeemHubInvite(inviteCode: string): Promise<BotNetOnlineState> {
    const hubUrl = normalizeUrl(this.settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    const cleanedCode = inviteCode.trim().toUpperCase();
    if (!cleanedCode) {
      throw new Error("Invite code is required.");
    }
    if (!this.hubSession.nodeId || !this.hubSession.authToken) {
      await this.registerWithHub();
    }
    const response = await fetch(`${hubUrl}/api/v1/botnet/invites/redeem`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        nodeId: this.hubSession.nodeId,
        token: this.hubSession.authToken,
        inviteCode: cleanedCode,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `Hub invite redeem failed with HTTP ${response.status}`);
    }
    this.hubSession = sanitizeHubSession({
      ...this.hubSession,
      linkId: String(payload.linkId || this.hubSession.linkId || "").trim(),
      peerNodeId: String(payload.peerNodeId || this.hubSession.peerNodeId || "").trim(),
      peerHandle: String(payload.peerHandle || this.hubSession.peerHandle || "").trim(),
      peerOnline:
        typeof payload.peerOnline === "boolean"
          ? payload.peerOnline
          : this.hubSession.peerOnline,
      lastError: "",
    });
    this.refreshConnectionStatus();
    this.save();
    return this.getPublicOnlineState();
  }

  private extractHubPayload(message: Record<string, unknown>): Record<string, unknown> {
    const payload = message.payload;
    if (payload && typeof payload === "object" && !Array.isArray(payload)) {
      return payload as Record<string, unknown>;
    }
    return message;
  }

  private async handleHubMessage(rawMessage: RawData): Promise<void> {
    const text = rawMessage.toString();
    const message = JSON.parse(text) as Record<string, unknown>;
    const type = String(message.type || "").trim();

    switch (type) {
      case "botnet.welcome":
      case "botnet.session": {
        this.hubSession = sanitizeHubSession({
          ...this.hubSession,
          nodeId: String(message.nodeId || this.hubSession.nodeId).trim(),
          websocketUrl:
            String(message.websocketUrl || this.hubSession.websocketUrl).trim() ||
            this.hubSession.websocketUrl,
          lastError: "",
        });
        this.refreshConnectionStatus();
        this.save();
        return;
      }
      case "botnet.link-state": {
        this.hubSession = sanitizeHubSession({
          ...this.hubSession,
          linkId: String(message.linkId || this.hubSession.linkId).trim(),
          peerNodeId: String(message.peerNodeId || this.hubSession.peerNodeId).trim(),
          peerHandle: String(message.peerHandle || this.hubSession.peerHandle).trim(),
          peerOnline:
            typeof message.peerOnline === "boolean"
              ? message.peerOnline
              : this.hubSession.peerOnline,
          lastError: "",
        });
        this.refreshConnectionStatus();
        this.save();
        return;
      }
      case "botnet.peer-start": {
        await this.handlePeerStart(this.extractHubPayload(message));
        return;
      }
      case "botnet.peer-message": {
        await this.handlePeerMessage(this.extractHubPayload(message));
        return;
      }
      case "botnet.error": {
        this.hubSession.lastError =
          String(message.error || message.message || "Hub returned an error.").trim();
        this.refreshConnectionStatus();
        this.save();
        return;
      }
      case "botnet.pong":
      case "botnet.ack":
      default:
        return;
    }
  }

  private async sendHubEnvelope(
    eventType: "botnet.start" | "botnet.message",
    conversation: BotNetConversation,
    payload: Record<string, unknown>,
  ): Promise<void> {
    if (!this.isHubConnected()) {
      throw new Error("Hub relay session is not connected.");
    }

    const linkId = conversation.linkId || this.hubSession.linkId;
    const peerNodeId = conversation.peerNodeId || this.hubSession.peerNodeId;
    if (!linkId || !peerNodeId) {
      throw new Error("No online peer link is active yet.");
    }

    this.hubSocket!.send(
      JSON.stringify({
        type: eventType,
        nodeId: this.hubSession.nodeId,
        token: this.hubSession.authToken,
        linkId,
        toNodeId: peerNodeId,
        payload: {
          ...payload,
          transportMode: "online-hub",
          linkId,
          senderNodeId: this.hubSession.nodeId,
          peerNodeId,
        },
      }),
    );
  }

  private async sendToLanPeer(
    pathName: "/api/botnet/start" | "/api/botnet/message",
    payload: Record<string, unknown>,
    peerUrl: string,
  ): Promise<void> {
    const response = await fetch(`${peerUrl}${pathName}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    const body = await response.json().catch(() => ({}));
    if (!response.ok || body.ok === false) {
      throw new Error(body.error || `Peer request failed with HTTP ${response.status}`);
    }
  }

  private async sendToPeer(
    conversation: BotNetConversation,
    pathName: "/api/botnet/start" | "/api/botnet/message",
    payload: Record<string, unknown>,
  ): Promise<void> {
    if (this.isOnlineTransport(conversation)) {
      await this.sendHubEnvelope(
        pathName === "/api/botnet/start" ? "botnet.start" : "botnet.message",
        conversation,
        payload,
      );
      return;
    }

    const peerUrl = normalizeUrl(conversation.peerUrl || this.settings.peerUrl);
    if (!peerUrl) {
      throw new Error("Peer URL is not configured.");
    }
    await this.sendToLanPeer(pathName, payload, peerUrl);
  }

  private async emitReply(
    conversation: BotNetConversation,
    extraPrompt = "",
  ): Promise<void> {
    if (conversation.status !== "active") {
      return;
    }
    if (conversation.maxBotReplies > 0 && conversation.replyCount >= conversation.maxBotReplies) {
      conversation.status = "complete";
      this.addEvent(
        conversation,
        "Conversation reached the configured max bot replies.",
      );
      return;
    }

    const reply = await this.callGroq(
      this.buildConversationMessages(conversation, extraPrompt),
    );
    conversation.replyCount += 1;
    this.addMessage(
      conversation,
      makeMessage("self", "Whisplay Bot", reply),
    );

    await this.sendToPeer(conversation, "/api/botnet/message", {
      conversationId: conversation.id,
      topic: conversation.topic,
      senderBotName: "Whisplay Bot",
      senderUrl: this.settings.publicUrl,
      botnetMode: conversation.botnetMode,
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
      message: reply,
    });

    if (conversation.maxBotReplies > 0 && conversation.replyCount >= conversation.maxBotReplies) {
      conversation.status = "complete";
      this.touchConversation(conversation);
    }
  }

  private scheduleReply(
    conversation: BotNetConversation,
    extraPrompt = "",
  ): void {
    this.clearTimer();
    const delay = Math.max(0, this.settings.replyDelaySec) * 1000;
    this.activeTimer = setTimeout(() => {
      this.activeTimer = null;
      void this.emitReply(conversation, extraPrompt).catch((error) => {
        this.addEvent(conversation, `Reply failed: ${error.message}`);
      });
    }, delay);
  }

  private createConversation(
    topic: string,
    starter: BotNetStarter,
    botnetMode: BotNetRelayMode = this.settings.botnetMode,
  ): BotNetConversation {
    const conversation: BotNetConversation = {
      id: randomId(),
      topic: topic.trim(),
      mode: "botnet",
      starter,
      botnetMode,
      transportMode: this.settings.transportMode,
      peerUrl: this.settings.peerUrl,
      linkId: this.hubSession.linkId,
      peerNodeId: this.hubSession.peerNodeId,
      status: "active",
      maxBotReplies: this.settings.maxBotReplies,
      replyCount: 0,
      createdAt: nowIso(),
      updatedAt: nowIso(),
      messages: [],
    };
    this.conversations.unshift(conversation);
    this.save();
    return conversation;
  }

  private isAutoBotConversation(conversation: BotNetConversation): boolean {
    return conversation.botnetMode === "auto-bot";
  }

  private async relayUserPromptToPeer(
    conversation: BotNetConversation,
    userPrompt: string,
  ): Promise<void> {
    this.addMessage(
      conversation,
      makeMessage("user", "You", userPrompt, "user-prompt"),
    );

    const relayedMessage = await this.generatePersonaRelay(conversation, userPrompt);
    this.addMessage(
      conversation,
      makeMessage("self", "Whisplay Bot sent", relayedMessage, "relay"),
    );

    await this.sendToPeer(conversation, "/api/botnet/message", {
      conversationId: conversation.id,
      topic: conversation.topic,
      senderBotName: "Whisplay Bot",
      senderUrl: this.settings.publicUrl,
      botnetMode: "persona-relay",
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
      message: relayedMessage,
    });
  }

  async startConversation(
    topic: string,
    starter: BotNetStarter = "self",
    botnetMode: BotNetRelayMode = this.settings.botnetMode,
  ): Promise<BotNetConversation> {
    if (!this.settings.enabled) {
      throw new Error("BotNet mode is turned off.");
    }
    const trimmedTopic = topic.trim();
    if (!trimmedTopic) {
      throw new Error("Topic is required.");
    }
    const existing = this.getActiveConversation();
    if (existing) {
      existing.status = "stopped";
      this.addEvent(existing, "Stopped previous active BotNet conversation.");
    }
    const conversation = this.createConversation(trimmedTopic, starter, botnetMode);
    this.connectionStatus = "Active conversation";

    if (starter === "peer") {
      await this.sendToPeer(conversation, "/api/botnet/start", {
        conversationId: conversation.id,
        topic: conversation.topic,
        senderBotName: "Whisplay Bot",
        senderUrl: this.settings.publicUrl,
        botnetMode: conversation.botnetMode,
        maxBotReplies: conversation.maxBotReplies,
        replyCount: conversation.replyCount,
      });
      this.addEvent(conversation, "Asked the peer bot to start the conversation.");
      return conversation;
    }

    if (!this.isAutoBotConversation(conversation)) {
      return conversation;
    }

    await this.emitReply(
      conversation,
      `Start a fresh chatbot-to-chatbot conversation about this topic: "${trimmedTopic}". Send only the first message.`,
    );
    return conversation;
  }

  async relayPrompt(prompt: string): Promise<BotNetConversation> {
    if (!this.settings.enabled) {
      throw new Error("BotNet mode is turned off.");
    }
    const trimmedPrompt = prompt.trim();
    if (!trimmedPrompt) {
      throw new Error("Prompt is required.");
    }

    let conversation = this.getActiveConversation();
    if (
      !conversation ||
      conversation.mode !== "botnet" ||
      conversation.status !== "active" ||
      this.isAutoBotConversation(conversation)
    ) {
      const topic =
        trimmedPrompt.length > 72 ? `${trimmedPrompt.slice(0, 72)}...` : trimmedPrompt;
      conversation = this.createConversation(topic || "Persona relay", "self", "persona-relay");
      this.connectionStatus = "Active conversation";
    }

    if (this.isOnlineTransport(conversation)) {
      if (!conversation.linkId && !this.hubSession.linkId) {
        throw new Error("No online peer link is active yet.");
      }
      if (!this.isHubConnected()) {
        throw new Error("Hub relay session is not connected.");
      }
    } else if (!normalizeUrl(conversation.peerUrl || this.settings.peerUrl)) {
      throw new Error("Peer URL is not configured.");
    }

    await this.relayUserPromptToPeer(conversation, trimmedPrompt);
    return conversation;
  }

  stopConversation(): boolean {
    const conversation = this.getActiveConversation();
    if (!conversation) {
      return false;
    }
    conversation.status = "stopped";
    this.clearTimer();
    this.addEvent(conversation, "Conversation stopped by user.");
    this.refreshConnectionStatus();
    return true;
  }

  private inferTransportModeFromPayload(body: Record<string, unknown>): BotNetTransportMode {
    const explicit = getField(body, "transportMode", "transportmode");
    if (explicit === "online-hub") {
      return "online-hub";
    }
    if (getField(body, "linkId", "linkid") || getField(body, "senderNodeId", "sendernodeid")) {
      return "online-hub";
    }
    return "lan-direct";
  }

  async handlePeerStart(body: Record<string, unknown>): Promise<void> {
    if (!this.settings.enabled) {
      throw new Error("BotNet mode is off on this Whisplay node.");
    }
    const topic = String(getField(body, "topic") || "").trim();
    if (!topic) {
      throw new Error("Missing topic.");
    }
    const botnetMode = normalizeBotnetMode(
      getField(body, "botnetMode", "botnetmode"),
    );
    const conversation = this.createConversation(topic, "peer", botnetMode);
    conversation.id =
      String(getField(body, "conversationId", "conversationid") || conversation.id).trim() ||
      conversation.id;
    conversation.transportMode = this.inferTransportModeFromPayload(body);
    conversation.peerUrl = normalizeUrl(
      getField(body, "senderUrl", "senderurl") || this.settings.peerUrl,
    );
    conversation.linkId = String(getField(body, "linkId", "linkid") || conversation.linkId).trim();
    conversation.peerNodeId = String(
      getField(body, "senderNodeId", "sendernodeid") || conversation.peerNodeId,
    ).trim();
    conversation.maxBotReplies = clampInt(
      getField(body, "maxBotReplies", "maxbotreplies"),
      this.settings.maxBotReplies,
      0,
      200,
    );
    conversation.replyCount = clampInt(
      getField(body, "replyCount", "replycount"),
      0,
      0,
      200,
    );
    conversation.botnetMode = botnetMode;

    if (conversation.transportMode === "online-hub") {
      this.hubSession = sanitizeHubSession({
        ...this.hubSession,
        linkId: conversation.linkId || this.hubSession.linkId,
        peerNodeId: conversation.peerNodeId || this.hubSession.peerNodeId,
        peerHandle:
          String(getField(body, "senderBotName", "senderbotname") || this.hubSession.peerHandle).trim(),
        peerOnline: true,
      });
    }

    this.addEvent(
      conversation,
      `Peer ${String(getField(body, "senderBotName", "senderbotname") || "bot")} requested a new conversation.`,
    );
    this.connectionStatus = "Active conversation";
    if (!this.isAutoBotConversation(conversation)) {
      this.save();
      return;
    }
    this.scheduleReply(
      conversation,
      `Start a fresh chatbot-to-chatbot conversation about this topic: "${topic}". Send only the first message.`,
    );
  }

  async handlePeerMessage(body: Record<string, unknown>): Promise<void> {
    if (!this.settings.enabled) {
      throw new Error("BotNet mode is off on this Whisplay node.");
    }
    const conversationId = String(
      getField(body, "conversationId", "conversationid") || "",
    ).trim();
    if (!conversationId) {
      throw new Error("Missing conversationId.");
    }
    let conversation =
      this.conversations.find((item) => item.id === conversationId) || null;
    const botnetMode = normalizeBotnetMode(
      getField(body, "botnetMode", "botnetmode"),
    );
    if (!conversation) {
      conversation = this.createConversation(
        String(getField(body, "topic") || "").trim(),
        "peer",
        botnetMode,
      );
      conversation.id = conversationId;
    }
    conversation.transportMode = this.inferTransportModeFromPayload(body);
    conversation.botnetMode = botnetMode;
    conversation.peerUrl = normalizeUrl(
      getField(body, "senderUrl", "senderurl") ||
        conversation.peerUrl ||
        this.settings.peerUrl,
    );
    conversation.linkId = String(getField(body, "linkId", "linkid") || conversation.linkId).trim();
    conversation.peerNodeId = String(
      getField(body, "senderNodeId", "sendernodeid") || conversation.peerNodeId,
    ).trim();
    conversation.maxBotReplies = clampInt(
      getField(body, "maxBotReplies", "maxbotreplies"),
      conversation.maxBotReplies,
      0,
      200,
    );
    conversation.replyCount = Math.max(
      conversation.replyCount,
      clampInt(
        getField(body, "replyCount", "replycount"),
        conversation.replyCount,
        0,
        200,
      ),
    );
    conversation.status = "active";

    if (conversation.transportMode === "online-hub") {
      this.hubSession = sanitizeHubSession({
        ...this.hubSession,
        linkId: conversation.linkId || this.hubSession.linkId,
        peerNodeId: conversation.peerNodeId || this.hubSession.peerNodeId,
        peerHandle:
          String(getField(body, "senderBotName", "senderbotname") || this.hubSession.peerHandle).trim(),
        peerOnline: true,
      });
    }

    this.addMessage(
      conversation,
      makeMessage(
        "peer",
        String(getField(body, "senderBotName", "senderbotname") || "Peer Bot").trim() ||
          "Peer Bot",
        String(getField(body, "message") || ""),
      ),
    );
    this.connectionStatus = "Active conversation";
    if (conversation.maxBotReplies > 0 && conversation.replyCount >= conversation.maxBotReplies) {
      conversation.status = "complete";
      this.addEvent(conversation, "Peer reached the configured max bot replies.");
      return;
    }
    if (!this.isAutoBotConversation(conversation)) {
      if (conversation.starter === "peer") {
        this.scheduleReply(conversation);
        return;
      }
      this.touchConversation(conversation);
      return;
    }
    this.scheduleReply(conversation);
  }
}

let manager: BotNetManager | null = null;

export function getBotNetManager(): BotNetManager {
  if (!manager) {
    manager = new BotNetManager();
  }
  return manager;
}
