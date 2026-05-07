const http = require("http");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { BotNetHubTransport, DEFAULT_SESSION, sanitizeSession, normalizeUrl } = require("./online-transport");

const PORT = Number.parseInt(process.env.PORT || "18990", 10);
const HOST = process.env.HOST || "0.0.0.0";
const ROOT_DIR = __dirname;
const PUBLIC_DIR = path.join(ROOT_DIR, "public");
const DATA_DIR = path.resolve(process.env.GROQBOTNET_DATA_DIR || path.join(ROOT_DIR, "data"));
const SETTINGS_PATH = path.join(DATA_DIR, "settings.json");
const CONVERSATIONS_PATH = path.join(DATA_DIR, "conversations.json");
const STATE_PATH = path.join(DATA_DIR, "state.json");
const HUB_SESSION_PATH = path.join(DATA_DIR, "botnet-hub-session.json");

fs.mkdirSync(DATA_DIR, { recursive: true });

const DEFAULT_SETTINGS = {
  enabled: true,
  botName: "GroqBotNet Bot",
  botnetMode: "persona-relay",
  model: "llama-3.1-8b-instant",
  transportMode: "lan-direct",
  publicBaseUrl: "",
  peerUrl: "",
  hubUrl: "",
  nodeHandle: "GroqBotNet Bot",
  personalityPrompt:
    "You are a concise, imaginative chatbot talking to another chatbot. Stay in character, be conversational, and keep replies reasonably short.",
  memoryTurns: 12,
  replyDelaySec: 6,
  maxBotReplies: 8,
  maxRequestsPerHour: 30,
  groqApiKey: "",
};

const DEFAULT_STATE = {
  requestTimestamps: [],
};

const timers = new Map();

function readJson(filePath, fallback) {
  try {
    return JSON.parse(fs.readFileSync(filePath, "utf8"));
  } catch {
    return fallback;
  }
}

function writeJson(filePath, value) {
  fs.writeFileSync(filePath, JSON.stringify(value, null, 2));
}

function normalizePositiveInt(value, fallback, minimum = 1, maximum = 500) {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  return Math.min(maximum, Math.max(minimum, parsed));
}

function normalizeBotnetMode(value) {
  return value === "auto-bot" ? "auto-bot" : "persona-relay";
}

function normalizeTransportMode(value) {
  return value === "online-hub" ? "online-hub" : "lan-direct";
}

function loadSettings() {
  const loaded = readJson(SETTINGS_PATH, {});
  return {
    ...DEFAULT_SETTINGS,
    ...loaded,
    enabled: loaded.enabled !== false,
    botnetMode: normalizeBotnetMode(loaded.botnetMode || DEFAULT_SETTINGS.botnetMode),
    transportMode: normalizeTransportMode(loaded.transportMode || DEFAULT_SETTINGS.transportMode),
    publicBaseUrl: normalizeUrl(loaded.publicBaseUrl || DEFAULT_SETTINGS.publicBaseUrl),
    peerUrl: normalizeUrl(loaded.peerUrl || DEFAULT_SETTINGS.peerUrl),
    hubUrl: normalizeUrl(loaded.hubUrl || DEFAULT_SETTINGS.hubUrl),
    nodeHandle: String(loaded.nodeHandle || loaded.botName || DEFAULT_SETTINGS.nodeHandle).trim() || DEFAULT_SETTINGS.nodeHandle,
    memoryTurns: normalizePositiveInt(loaded.memoryTurns, DEFAULT_SETTINGS.memoryTurns, 1, 50),
    replyDelaySec: normalizePositiveInt(loaded.replyDelaySec, DEFAULT_SETTINGS.replyDelaySec, 0, 3600),
    maxBotReplies: normalizePositiveInt(loaded.maxBotReplies, DEFAULT_SETTINGS.maxBotReplies, 0, 200),
    maxRequestsPerHour: normalizePositiveInt(
      loaded.maxRequestsPerHour,
      DEFAULT_SETTINGS.maxRequestsPerHour,
      1,
      500,
    ),
  };
}

function sanitizeSettingsUpdate(body, currentSettings) {
  return {
    ...currentSettings,
    enabled: body.enabled !== false && body.enabled !== "false",
    botName: String(body.botName || currentSettings.botName).trim() || currentSettings.botName,
    botnetMode: normalizeBotnetMode(body.botnetMode || currentSettings.botnetMode),
    model: String(body.model || currentSettings.model).trim() || currentSettings.model,
    transportMode: normalizeTransportMode(body.transportMode || currentSettings.transportMode),
    publicBaseUrl: normalizeUrl(body.publicBaseUrl ?? currentSettings.publicBaseUrl),
    peerUrl: normalizeUrl(body.peerUrl ?? currentSettings.peerUrl),
    hubUrl: normalizeUrl(body.hubUrl ?? currentSettings.hubUrl),
    nodeHandle: String(body.nodeHandle || currentSettings.nodeHandle).trim() || currentSettings.nodeHandle,
    personalityPrompt:
      String(body.personalityPrompt || currentSettings.personalityPrompt).trim() ||
      currentSettings.personalityPrompt,
    memoryTurns: normalizePositiveInt(body.memoryTurns, currentSettings.memoryTurns, 1, 50),
    replyDelaySec: normalizePositiveInt(body.replyDelaySec, currentSettings.replyDelaySec, 0, 3600),
    maxBotReplies: normalizePositiveInt(body.maxBotReplies, currentSettings.maxBotReplies, 0, 200),
    maxRequestsPerHour: normalizePositiveInt(
      body.maxRequestsPerHour,
      currentSettings.maxRequestsPerHour,
      1,
      500,
    ),
    groqApiKey:
      typeof body.groqApiKey === "string" && body.groqApiKey.trim().length > 0
        ? body.groqApiKey.trim()
        : currentSettings.groqApiKey,
  };
}

let settings = loadSettings();
let conversations = readJson(CONVERSATIONS_PATH, []);
let runtimeState = {
  ...DEFAULT_STATE,
  ...readJson(STATE_PATH, DEFAULT_STATE),
};
let hubSession = sanitizeSession(readJson(HUB_SESSION_PATH, DEFAULT_SESSION));

function saveSettings() {
  writeJson(SETTINGS_PATH, settings);
}

function saveConversations() {
  writeJson(CONVERSATIONS_PATH, conversations);
}

function saveRuntimeState() {
  writeJson(STATE_PATH, runtimeState);
}

function saveHubSession() {
  writeJson(HUB_SESSION_PATH, hubSession);
}

function getPublicSettings() {
  return {
    ...settings,
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    groqApiKey: "",
  };
}

const hubTransport = new BotNetHubTransport({
  getSettings: () => settings,
  getSession: () => hubSession,
  setSession: (nextSession) => {
    hubSession = sanitizeSession(nextSession);
    saveHubSession();
  },
  onPeerStart: async (payload) => handlePeerStart(payload),
  onPeerMessage: async (payload) => handlePeerMessage(payload),
});

function nowIso() {
  return new Date().toISOString();
}

function makeMessage({ speakerType, speakerName, text, kind = "message" }) {
  return {
    id: crypto.randomUUID(),
    createdAt: nowIso(),
    speakerType,
    speakerName,
    kind,
    text: String(text || "").trim(),
  };
}

function makeConversation({
  topic,
  peerUrl,
  starter,
  mode = "botnet",
  botnetMode = settings.botnetMode,
  transportMode = settings.transportMode,
  linkId = "",
  peerNodeId = "",
}) {
  const createdAt = nowIso();
  return {
    id: crypto.randomUUID(),
    topic: String(topic || "").trim(),
    createdAt,
    updatedAt: createdAt,
    status: "active",
    starter,
    mode,
    botnetMode: mode === "botnet" ? normalizeBotnetMode(botnetMode) : undefined,
    transportMode: mode === "botnet" ? normalizeTransportMode(transportMode) : undefined,
    peerUrl: normalizeUrl(peerUrl || settings.peerUrl),
    linkId: String(linkId || "").trim(),
    peerNodeId: String(peerNodeId || "").trim(),
    maxBotReplies: settings.maxBotReplies,
    replyCount: 0,
    messages: [],
  };
}

function findConversation(id) {
  return conversations.find((conversation) => conversation.id === id) || null;
}

function getLatestActiveSoloConversation() {
  return (
    conversations.find(
      (conversation) => conversation.mode === "solo" && conversation.status === "active",
    ) || null
  );
}

function getLatestActiveBotnetConversation() {
  return (
    conversations.find(
      (conversation) => conversation.mode === "botnet" && conversation.status === "active",
    ) || null
  );
}

function isAutoBotConversation(conversation) {
  return normalizeBotnetMode(conversation?.botnetMode) === "auto-bot";
}

function isOnlineConversation(conversation) {
  return normalizeTransportMode(conversation?.transportMode || settings.transportMode) === "online-hub";
}

function touchConversation(conversation) {
  conversation.updatedAt = nowIso();
  saveConversations();
}

function addMessage(conversation, message) {
  conversation.messages.push(message);
  conversation.updatedAt = nowIso();
  saveConversations();
}

function addEvent(conversation, text) {
  addMessage(
    conversation,
    makeMessage({
      speakerType: "system",
      speakerName: "System",
      text,
      kind: "event",
    }),
  );
}

function cleanupRequestWindow() {
  const cutoff = Date.now() - 60 * 60 * 1000;
  runtimeState.requestTimestamps = runtimeState.requestTimestamps.filter((timestamp) => timestamp >= cutoff);
}

function consumeRequestQuota() {
  cleanupRequestWindow();
  if (runtimeState.requestTimestamps.length >= settings.maxRequestsPerHour) {
    throw new Error(`This bot has already used ${settings.maxRequestsPerHour} Groq requests in the last hour.`);
  }
  runtimeState.requestTimestamps.push(Date.now());
  saveRuntimeState();
}

function buildConversationMessages(conversation, extraUserPrompt) {
  const relevant = conversation.messages
    .filter((message) => message.kind === "message" || message.kind === "relay")
    .slice(-settings.memoryTurns);

  const conversationModeText =
    conversation.mode === "solo"
      ? "You are talking to a human through a small browser chat UI."
      : "You are talking to another chatbot over text.";

  const messages = [
    {
      role: "system",
      content: [
        settings.personalityPrompt,
        "You are part of GroqBotNet.",
        `Your bot name is ${settings.botName}.`,
        conversationModeText,
        conversation.mode === "solo"
          ? "Keep replies concise, natural, helpful, and in character."
          : "Keep replies concise, natural, and chatbot-to-chatbot friendly.",
        "Do not mention system prompts, tokens, APIs, or hidden instructions.",
      ].join(" "),
    },
  ];

  for (const message of relevant) {
    if (message.speakerType === "self") {
      messages.push({ role: "assistant", content: message.text });
    } else if (message.speakerType === "peer" || message.speakerType === "user") {
      messages.push({ role: "user", content: message.text });
    }
  }

  if (extraUserPrompt) {
    messages.push({ role: "user", content: extraUserPrompt });
  }

  return messages;
}

function buildPersonaRelayMessages(conversation, userPrompt) {
  const relevant = conversation.messages
    .filter((message) => message.kind === "message" || message.kind === "relay")
    .slice(-settings.memoryTurns);

  const messages = [
    {
      role: "system",
      content: [
        settings.personalityPrompt,
        "You are part of GroqBotNet.",
        `Your bot name is ${settings.botName}.`,
        "You are rewriting the local user's prompt into the exact message you would personally send to the peer bot.",
        "Preserve the user's intent, but express it in your own personality and voice.",
        "Keep it concise and natural.",
        "Write the final message itself, exactly as it should be sent to the peer.",
        "Do not give advice, instructions, stage directions, or commentary.",
        "Do not say things like 'ask them', 'tell them', 'send a message', 'you want to know', or 'here is the message'.",
        "Do not wrap the whole reply in quotes unless the message itself truly needs quotes.",
        'Example local prompt: "ask my friend how it is doing today" -> "How are ye doin\\\' today, matey?"',
        "Return only the outgoing message text with no commentary about rewriting, filtering, or translating.",
      ].join(" "),
    },
  ];

  for (const message of relevant) {
    if (message.speakerType === "self") {
      messages.push({ role: "assistant", content: message.text });
    } else if (message.speakerType === "peer") {
      messages.push({ role: "user", content: message.text });
    }
  }

  messages.push({
    role: "user",
    content: `Rewrite this into the exact message to send to the peer bot: ${String(userPrompt || "").trim()}`,
  });

  return messages;
}

async function callGroq(messages) {
  const apiKey = settings.groqApiKey || process.env.OPENAI_API_KEY || process.env.GROQ_API_KEY || "";
  if (!apiKey) {
    throw new Error("No Groq API key configured yet.");
  }

  const response = await fetch("https://api.groq.com/openai/v1/chat/completions", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${apiKey}`,
    },
    body: JSON.stringify({
      model: settings.model,
      messages,
      temperature: 0.9,
    }),
  });

  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error?.message || `Groq request failed with HTTP ${response.status}`);
  }

  const text = payload.choices?.[0]?.message?.content;
  if (!text || typeof text !== "string") {
    throw new Error("Groq returned an empty reply.");
  }
  return text.trim();
}

async function sendToPeer(conversation, payload) {
  if (isOnlineConversation(conversation)) {
    await hubTransport.sendEvent(
      payload.type === "start" ? "botnet.peer-start" : "botnet.peer-message",
      conversation,
      payload,
    );
    return;
  }
  const peerUrl = normalizeUrl(conversation.peerUrl || settings.peerUrl);
  if (!peerUrl) {
    throw new Error("Peer URL is not configured.");
  }

  const response = await fetch(`${peerUrl}${payload.type === "start" ? "/api/botnet/start" : "/api/botnet/message"}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const body = await response.json().catch(() => ({}));
  if (!response.ok || body.ok === false) {
    throw new Error(body.error || `Peer request failed with HTTP ${response.status}`);
  }
}

function clearConversationTimer(conversationId) {
  const timer = timers.get(conversationId);
  if (timer) {
    clearTimeout(timer);
    timers.delete(conversationId);
  }
}

function markConversationComplete(conversation, reason) {
  conversation.status = "complete";
  if (reason) {
    addEvent(conversation, reason);
  } else {
    touchConversation(conversation);
  }
  clearConversationTimer(conversation.id);
}

async function generateReply(conversation, extraPrompt) {
  consumeRequestQuota();
  const reply = await callGroq(buildConversationMessages(conversation, extraPrompt));
  return reply;
}

async function generatePersonaRelay(conversation, userPrompt) {
  consumeRequestQuota();
  return callGroq(buildPersonaRelayMessages(conversation, userPrompt));
}

async function emitBotReply(
  conversation,
  extraPrompt,
  options = { deliverToPeer: true, enforceReplyLimit: true },
) {
  if (conversation.status !== "active") {
    return;
  }
  if (
    options.enforceReplyLimit &&
    conversation.maxBotReplies > 0 &&
    conversation.replyCount >= conversation.maxBotReplies
  ) {
    markConversationComplete(conversation, "Conversation stopped because the max bot replies limit was reached.");
    return;
  }

  const reply = await generateReply(conversation, extraPrompt);
  conversation.replyCount += 1;
  addMessage(
    conversation,
    makeMessage({
      speakerType: "self",
      speakerName: settings.botName,
      text: reply,
    }),
  );

  if (options.deliverToPeer) {
    const payload = {
      type: "message",
      conversationId: conversation.id,
      topic: conversation.topic,
      senderBotName: settings.botName,
      senderUrl: normalizeUrl(settings.publicBaseUrl),
      botnetMode: normalizeBotnetMode(conversation.botnetMode),
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
      message: reply,
    };

    try {
      await sendToPeer(conversation, payload);
    } catch (error) {
      addEvent(conversation, `Failed to deliver message to peer: ${error.message}`);
    }
  }

  if (
    options.enforceReplyLimit &&
    conversation.maxBotReplies > 0 &&
    conversation.replyCount >= conversation.maxBotReplies
  ) {
    markConversationComplete(conversation, "Conversation reached the configured max bot replies.");
  }
}

function scheduleReply(conversation, extraPrompt) {
  clearConversationTimer(conversation.id);
  const delayMs = settings.replyDelaySec * 1000;
  const timer = setTimeout(async () => {
    timers.delete(conversation.id);
    try {
      await emitBotReply(conversation, extraPrompt);
    } catch (error) {
      addEvent(conversation, `Reply failed: ${error.message}`);
      markConversationComplete(conversation);
    }
  }, delayMs);
  timers.set(conversation.id, timer);
}

async function startLocalConversation(topic) {
  const conversation = makeConversation({
    topic,
    peerUrl: settings.peerUrl,
    starter: "self",
    mode: "botnet",
    botnetMode: "auto-bot",
    transportMode: settings.transportMode,
    linkId: hubSession.linkId,
    peerNodeId: hubSession.peerNodeId,
  });
  conversations.unshift(conversation);
  saveConversations();

  try {
    await emitBotReply(
      conversation,
      `Start a fresh chatbot-to-chatbot conversation about this topic: "${topic}". Send only the first message.`,
    );
  } catch (error) {
    addEvent(conversation, `Could not start conversation: ${error.message}`);
  }

  return conversation;
}

async function requestPeerStart(topic) {
  const conversation = makeConversation({
    topic,
    peerUrl: settings.peerUrl,
    starter: "peer",
    mode: "botnet",
    botnetMode: "auto-bot",
    transportMode: settings.transportMode,
    linkId: hubSession.linkId,
    peerNodeId: hubSession.peerNodeId,
  });
  conversations.unshift(conversation);
  saveConversations();

  try {
    await sendToPeer(conversation, {
      type: "start",
      conversationId: conversation.id,
      topic,
      senderBotName: settings.botName,
      senderUrl: normalizeUrl(settings.publicBaseUrl),
      botnetMode: "auto-bot",
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
    });
    addEvent(conversation, "Asked the peer bot to open the conversation.");
  } catch (error) {
    addEvent(conversation, `Could not reach peer to start conversation: ${error.message}`);
  }

  return conversation;
}

async function relayUserPromptToPeer(conversation, userPrompt) {
  addMessage(
    conversation,
    makeMessage({
      speakerType: "user",
      speakerName: "You",
      text: userPrompt,
      kind: "user-prompt",
    }),
  );

  const relayedMessage = await generatePersonaRelay(conversation, userPrompt);
  addMessage(
    conversation,
    makeMessage({
      speakerType: "self",
      speakerName: `${settings.botName} sent`,
      text: relayedMessage,
      kind: "relay",
    }),
  );

  await sendToPeer(conversation, {
    type: "message",
    conversationId: conversation.id,
    topic: conversation.topic,
    senderBotName: settings.botName,
    senderUrl: normalizeUrl(settings.publicBaseUrl),
    botnetMode: "persona-relay",
    maxBotReplies: conversation.maxBotReplies,
    replyCount: conversation.replyCount,
    message: relayedMessage,
  });
}

async function handlePersonaRelayPrompt(body) {
  const prompt = String(body.topic || body.message || "").trim();
  if (!prompt) {
    throw new Error("Prompt is required.");
  }

  const requestedId = String(body.conversationId || "").trim();
  let conversation =
    (requestedId && findConversation(requestedId)) ||
    (!body.newConversation && getLatestActiveBotnetConversation()) ||
    null;

  if (
    !conversation ||
    conversation.mode !== "botnet" ||
    conversation.status !== "active" ||
    (!isOnlineConversation(conversation) && !conversation.peerUrl) ||
    isAutoBotConversation(conversation)
  ) {
    const topic = prompt.length > 72 ? `${prompt.slice(0, 72)}...` : prompt;
    conversation = makeConversation({
      topic: topic || "Persona relay",
      peerUrl: settings.peerUrl,
      starter: "self",
      mode: "botnet",
      botnetMode: "persona-relay",
      transportMode: settings.transportMode,
      linkId: hubSession.linkId,
      peerNodeId: hubSession.peerNodeId,
    });
    conversations.unshift(conversation);
    saveConversations();
  }

  if (isOnlineConversation(conversation)) {
    if (!hubTransport.getPublicState().connected) {
      throw new Error("Connect to the hub before sending an online message.");
    }
    if (!String(conversation.linkId || hubSession.linkId || "").trim()) {
      throw new Error("No online peer link is active yet.");
    }
  } else if (!normalizeUrl(conversation.peerUrl || settings.peerUrl)) {
    throw new Error("Peer URL is not configured.");
  }

  await relayUserPromptToPeer(conversation, prompt);
  return conversation;
}

async function handlePeerStart(body) {
  const conversationId = String(body.conversationId || "").trim() || crypto.randomUUID();
  const transportMode = normalizeTransportMode(
    body.transportMode || (body.senderNodeId ? "online-hub" : settings.transportMode),
  );
  let conversation = findConversation(conversationId);
  if (!conversation) {
    conversation = {
      id: conversationId,
      topic: String(body.topic || "").trim(),
      createdAt: nowIso(),
      updatedAt: nowIso(),
      status: "active",
      starter: "peer",
      mode: "botnet",
      botnetMode: normalizeBotnetMode(body.botnetMode || "auto-bot"),
      transportMode,
      peerUrl: normalizeUrl(body.senderUrl || settings.peerUrl),
      linkId: String(body.linkId || hubSession.linkId || "").trim(),
      peerNodeId: String(body.senderNodeId || body.peerNodeId || hubSession.peerNodeId || "").trim(),
      maxBotReplies: normalizePositiveInt(body.maxBotReplies, settings.maxBotReplies, 0, 200),
      replyCount: normalizePositiveInt(body.replyCount, 0, 0, 200),
      messages: [],
    };
    conversations.unshift(conversation);
    saveConversations();
  }

  conversation.botnetMode = normalizeBotnetMode(body.botnetMode || conversation.botnetMode);
  conversation.transportMode = transportMode;
  conversation.linkId = String(body.linkId || conversation.linkId || hubSession.linkId || "").trim();
  conversation.peerNodeId = String(
    body.senderNodeId || body.peerNodeId || conversation.peerNodeId || hubSession.peerNodeId || "",
  ).trim();
  if (transportMode === "online-hub") {
    hubSession = sanitizeSession({
      ...hubSession,
      linkId: conversation.linkId,
      peerNodeId: conversation.peerNodeId,
      peerHandle: String(body.senderBotName || hubSession.peerHandle || "").trim(),
      peerOnline: true,
      lastError: "",
    });
    saveHubSession();
  }
  addEvent(conversation, `Peer ${body.senderBotName || "bot"} requested a new conversation.`);
  if (!isAutoBotConversation(conversation)) {
    return;
  }
  scheduleReply(
    conversation,
    `Start a fresh chatbot-to-chatbot conversation about this topic: "${conversation.topic}". Send only the first message.`,
  );
}

async function handlePeerMessage(body) {
  const conversationId = String(body.conversationId || "").trim();
  if (!conversationId) {
    throw new Error("Missing conversationId.");
  }
  const transportMode = normalizeTransportMode(
    body.transportMode || (body.senderNodeId ? "online-hub" : settings.transportMode),
  );

  let conversation = findConversation(conversationId);
  if (!conversation) {
    conversation = {
      id: conversationId,
      topic: String(body.topic || "").trim(),
      createdAt: nowIso(),
      updatedAt: nowIso(),
      status: "active",
      starter: "peer",
      mode: "botnet",
      botnetMode: normalizeBotnetMode(body.botnetMode || settings.botnetMode),
      transportMode,
      peerUrl: normalizeUrl(body.senderUrl || settings.peerUrl),
      linkId: String(body.linkId || hubSession.linkId || "").trim(),
      peerNodeId: String(body.senderNodeId || body.peerNodeId || hubSession.peerNodeId || "").trim(),
      maxBotReplies: normalizePositiveInt(body.maxBotReplies, settings.maxBotReplies, 0, 200),
      replyCount: 0,
      messages: [],
    };
    conversations.unshift(conversation);
  }

  conversation.botnetMode = normalizeBotnetMode(body.botnetMode || conversation.botnetMode);
  conversation.transportMode = transportMode;
  conversation.peerUrl = normalizeUrl(body.senderUrl || conversation.peerUrl || settings.peerUrl);
  conversation.linkId = String(body.linkId || conversation.linkId || hubSession.linkId || "").trim();
  conversation.peerNodeId = String(
    body.senderNodeId || body.peerNodeId || conversation.peerNodeId || hubSession.peerNodeId || "",
  ).trim();
  conversation.maxBotReplies = normalizePositiveInt(
    body.maxBotReplies,
    conversation.maxBotReplies || settings.maxBotReplies,
    0,
    200,
  );
  conversation.replyCount = Math.max(
    normalizePositiveInt(body.replyCount, 0, 0, 200),
    conversation.replyCount || 0,
  );
  if (transportMode === "online-hub") {
    hubSession = sanitizeSession({
      ...hubSession,
      linkId: conversation.linkId,
      peerNodeId: conversation.peerNodeId,
      peerHandle: String(body.senderBotName || hubSession.peerHandle || "").trim(),
      peerOnline: true,
      lastError: "",
    });
    saveHubSession();
  }

  addMessage(
    conversation,
    makeMessage({
      speakerType: "peer",
      speakerName: String(body.senderBotName || "Peer Bot").trim() || "Peer Bot",
      text: body.message,
    }),
  );

  if (conversation.maxBotReplies > 0 && conversation.replyCount >= conversation.maxBotReplies) {
    markConversationComplete(conversation, "Received the final peer message and closed the conversation.");
    return;
  }

  if (!isAutoBotConversation(conversation)) {
    if (conversation.starter === "peer") {
      scheduleReply(conversation);
      return;
    }
    touchConversation(conversation);
    return;
  }
  scheduleReply(conversation);
}

async function startSoloConversation(initialMessage) {
  const topic = initialMessage.length > 72 ? `${initialMessage.slice(0, 72)}...` : initialMessage;
  const conversation = makeConversation({
    topic: topic || "Solo chat",
    peerUrl: "",
    starter: "user",
    mode: "solo",
  });
  conversations.unshift(conversation);
  addMessage(
    conversation,
    makeMessage({
      speakerType: "user",
      speakerName: "You",
      text: initialMessage,
    }),
  );

  try {
    await emitBotReply(conversation, "", {
      deliverToPeer: false,
      enforceReplyLimit: false,
    });
  } catch (error) {
    addEvent(conversation, `Solo reply failed: ${error.message}`);
  }

  return conversation;
}

async function handleSoloMessage(body) {
  const message = String(body.message || "").trim();
  if (!message) {
    throw new Error("Message is required.");
  }

  const requestedId = String(body.conversationId || "").trim();
  let conversation =
    (requestedId && findConversation(requestedId)) ||
    (!body.newConversation && getLatestActiveSoloConversation()) ||
    null;

  if (!conversation || conversation.mode !== "solo" || conversation.status !== "active") {
    return startSoloConversation(message);
  }

  addMessage(
    conversation,
    makeMessage({
      speakerType: "user",
      speakerName: "You",
      text: message,
    }),
  );

  try {
    await emitBotReply(conversation, "", {
      deliverToPeer: false,
      enforceReplyLimit: false,
    });
  } catch (error) {
    addEvent(conversation, `Solo reply failed: ${error.message}`);
  }

  return conversation;
}

function stopConversation(conversationId) {
  const conversation = findConversation(conversationId);
  if (!conversation) {
    return false;
  }
  conversation.status = "stopped";
  addEvent(conversation, "Conversation stopped by user.");
  clearConversationTimer(conversationId);
  return true;
}

function sendJson(res, statusCode, payload) {
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(JSON.stringify(payload));
}

async function readBody(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  if (chunks.length === 0) {
    return {};
  }
  const raw = Buffer.concat(chunks).toString("utf8");
  return raw ? JSON.parse(raw) : {};
}

function contentTypeFor(filePath) {
  if (filePath.endsWith(".html")) return "text/html; charset=utf-8";
  if (filePath.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (filePath.endsWith(".css")) return "text/css; charset=utf-8";
  return "text/plain; charset=utf-8";
}

function serveStatic(req, res) {
  const requestPath = req.url === "/" ? "/index.html" : req.url;
  const filePath = path.normalize(path.join(PUBLIC_DIR, requestPath));
  if (!filePath.startsWith(PUBLIC_DIR) || !fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    res.writeHead(404);
    res.end("Not found");
    return;
  }
  res.writeHead(200, { "Content-Type": contentTypeFor(filePath) });
  fs.createReadStream(filePath).pipe(res);
}

function getStatePayload() {
  return {
    settings: getPublicSettings(),
    online: hubTransport.getPublicState(),
    conversations,
    stats: {
      requestsUsedThisHour: runtimeState.requestTimestamps.filter(
        (timestamp) => timestamp >= Date.now() - 60 * 60 * 1000,
      ).length,
    },
  };
}

const server = http.createServer(async (req, res) => {
  try {
    if (!req.url) {
      sendJson(res, 400, { ok: false, error: "Missing URL." });
      return;
    }

    const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);

    if (req.method === "GET" && url.pathname === "/api/state") {
      cleanupRequestWindow();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/settings") {
      const body = await readBody(req);
      settings = sanitizeSettingsUpdate(body, settings);
      if (settings.transportMode !== "online-hub") {
        hubTransport.disconnect();
      }
      saveSettings();
      sendJson(res, 200, { ok: true, settings: getPublicSettings() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/conversations/start") {
      const body = await readBody(req);
      const topic = String(body.topic || "").trim();
      if (!topic) {
        sendJson(res, 400, { ok: false, error: "Topic is required." });
        return;
      }
      const requestedMode = normalizeBotnetMode(body.botnetMode || settings.botnetMode);
      const starter = body.starter === "peer" ? "peer" : "self";
      const conversation =
        requestedMode === "persona-relay"
          ? await handlePersonaRelayPrompt(body)
          : starter === "peer"
            ? await requestPeerStart(topic)
            : await startLocalConversation(topic);
      sendJson(res, 200, { ok: true, conversation, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/solo/send") {
      const body = await readBody(req);
      const conversation = await handleSoloMessage(body);
      sendJson(res, 200, { ok: true, conversation, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/conversations/stop") {
      const body = await readBody(req);
      const id = String(body.conversationId || "").trim();
      if (!id || !stopConversation(id)) {
        sendJson(res, 404, { ok: false, error: "Conversation not found." });
        return;
      }
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/start") {
      const body = await readBody(req);
      await handlePeerStart(body);
      sendJson(res, 200, { ok: true });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/message") {
      const body = await readBody(req);
      await handlePeerMessage(body);
      sendJson(res, 200, { ok: true });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/test") {
      const result = await hubTransport.test();
      sendJson(res, 200, { ok: true, result, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/register") {
      await hubTransport.register();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/connect") {
      await hubTransport.connect();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/invite") {
      const invite = await hubTransport.createInvite();
      sendJson(res, 200, { ok: true, invite, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/redeem") {
      const body = await readBody(req);
      await hubTransport.redeemInvite(body.inviteCode);
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/disconnect") {
      hubTransport.disconnect();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "GET" && !url.pathname.startsWith("/api/")) {
      serveStatic(req, res);
      return;
    }

    sendJson(res, 404, { ok: false, error: "Not found." });
  } catch (error) {
    sendJson(res, 500, { ok: false, error: error instanceof Error ? error.message : "Unknown error." });
  }
});

server.listen(PORT, HOST, () => {
  console.log(`[GroqBotNet] Listening on http://${HOST}:${PORT}`);
  if (settings.enabled && settings.transportMode === "online-hub" && hubSession.nodeId && hubSession.authToken) {
    hubTransport.connect().catch((error) => {
      hubSession = sanitizeSession({
        ...hubSession,
        lastError: error instanceof Error ? error.message : "Failed to reconnect to the hub.",
      });
      saveHubSession();
    });
  }
});
