import fs from "fs";
import path from "path";
import { dataDir } from "../utils/dir";
import { getRuntimeSettings } from "../config/runtime-settings";

type BotNetMode = "solo" | "botnet";
type BotNetStarter = "self" | "peer";
type BotNetSpeakerType = "self" | "peer" | "user" | "system";

export interface BotNetSettings {
  enabled: boolean;
  peerUrl: string;
  publicUrl: string;
  maxBotReplies: number;
  replyDelaySec: number;
}

export interface BotNetMessage {
  id: string;
  createdAt: string;
  speakerType: BotNetSpeakerType;
  speakerName: string;
  kind: "message" | "event";
  text: string;
}

export interface BotNetConversation {
  id: string;
  topic: string;
  mode: BotNetMode;
  starter: BotNetStarter | "user";
  peerUrl: string;
  status: "active" | "stopped" | "complete";
  maxBotReplies: number;
  replyCount: number;
  createdAt: string;
  updatedAt: string;
  messages: BotNetMessage[];
}

export interface BotNetState {
  settings: BotNetSettings;
  connectionStatus: string;
  conversations: BotNetConversation[];
}

const SETTINGS_PATH = path.join(dataDir, "botnet-settings.json");
const CONVERSATIONS_PATH = path.join(dataDir, "botnet-conversations.json");

const DEFAULT_SETTINGS: BotNetSettings = {
  enabled: false,
  peerUrl: "",
  publicUrl: "",
  maxBotReplies: 8,
  replyDelaySec: 6,
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

function sanitizeSettings(input: Partial<BotNetSettings> | null | undefined): BotNetSettings {
  return {
    enabled: Boolean(input?.enabled),
    peerUrl: normalizeUrl(input?.peerUrl),
    publicUrl: normalizeUrl(input?.publicUrl),
    maxBotReplies: clampInt(input?.maxBotReplies, DEFAULT_SETTINGS.maxBotReplies, 0, 200),
    replyDelaySec: clampInt(input?.replyDelaySec, DEFAULT_SETTINGS.replyDelaySec, 0, 3600),
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
  kind: "message" | "event" = "message",
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

class BotNetManager {
  private settings: BotNetSettings = sanitizeSettings(
    readJson<Partial<BotNetSettings>>(SETTINGS_PATH, DEFAULT_SETTINGS),
  );

  private conversations: BotNetConversation[] = readJson<BotNetConversation[]>(
    CONVERSATIONS_PATH,
    [],
  );

  private connectionStatus = "Disconnected";
  private activeTimer: NodeJS.Timeout | null = null;

  private save(): void {
    writeJson(SETTINGS_PATH, this.settings);
    writeJson(CONVERSATIONS_PATH, this.conversations.slice(0, 20));
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
      .filter((message) => message.kind === "message")
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

  private async sendToPeer(
    conversation: BotNetConversation,
    pathName: "/api/botnet/start" | "/api/botnet/message",
    payload: Record<string, unknown>,
  ): Promise<void> {
    const peerUrl = normalizeUrl(conversation.peerUrl || this.settings.peerUrl);
    if (!peerUrl) {
      throw new Error("Peer URL is not configured.");
    }
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

  private async emitReply(
    conversation: BotNetConversation,
    extraPrompt = "",
  ): Promise<void> {
    if (conversation.status !== "active") {
      return;
    }
    if (conversation.replyCount >= conversation.maxBotReplies && conversation.maxBotReplies >= 0) {
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
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
      message: reply,
    });

    if (conversation.replyCount >= conversation.maxBotReplies && conversation.maxBotReplies >= 0) {
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
  ): BotNetConversation {
    const conversation: BotNetConversation = {
      id: randomId(),
      topic: topic.trim(),
      mode: "botnet",
      starter,
      peerUrl: this.settings.peerUrl,
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

  getState(): BotNetState {
    return {
      settings: { ...this.settings },
      connectionStatus: this.connectionStatus,
      conversations: this.conversations.slice(0, 10),
    };
  }

  saveSettings(update: Partial<BotNetSettings>): BotNetSettings {
    this.settings = sanitizeSettings({ ...this.settings, ...update });
    this.save();
    return { ...this.settings };
  }

  async testPeer(): Promise<{ ok: boolean; message: string }> {
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

  async startConversation(
    topic: string,
    starter: BotNetStarter = "self",
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
    const conversation = this.createConversation(trimmedTopic, starter);
    this.connectionStatus = "Active conversation";

    if (starter === "peer") {
      await this.sendToPeer(conversation, "/api/botnet/start", {
        conversationId: conversation.id,
        topic: conversation.topic,
        senderBotName: "Whisplay Bot",
        senderUrl: this.settings.publicUrl,
        maxBotReplies: conversation.maxBotReplies,
        replyCount: conversation.replyCount,
      });
      this.addEvent(conversation, "Asked the peer bot to start the conversation.");
      return conversation;
    }

    await this.emitReply(
      conversation,
      `Start a fresh chatbot-to-chatbot conversation about this topic: "${trimmedTopic}". Send only the first message.`,
    );
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
    this.connectionStatus = "Reachable";
    return true;
  }

  async handlePeerStart(body: Record<string, unknown>): Promise<void> {
    if (!this.settings.enabled) {
      throw new Error("BotNet mode is off on this Whisplay node.");
    }
    const topic = String(getField(body, "topic") || "").trim();
    if (!topic) {
      throw new Error("Missing topic.");
    }
    const conversation = this.createConversation(topic, "peer");
    conversation.id =
      String(getField(body, "conversationId", "conversationid") || conversation.id).trim() ||
      conversation.id;
    conversation.peerUrl = normalizeUrl(
      getField(body, "senderUrl", "senderurl") || this.settings.peerUrl,
    );
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
    this.addEvent(
      conversation,
      `Peer ${String(getField(body, "senderBotName", "senderbotname") || "bot")} requested a new conversation.`,
    );
    this.connectionStatus = "Active conversation";
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
    if (!conversation) {
      conversation = this.createConversation(
        String(getField(body, "topic") || "").trim(),
        "peer",
      );
      conversation.id = conversationId;
    }
    conversation.peerUrl = normalizeUrl(
      getField(body, "senderUrl", "senderurl") ||
        conversation.peerUrl ||
        this.settings.peerUrl,
    );
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
    if (conversation.replyCount >= conversation.maxBotReplies && conversation.maxBotReplies >= 0) {
      conversation.status = "complete";
      this.addEvent(conversation, "Peer reached the configured max bot replies.");
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
