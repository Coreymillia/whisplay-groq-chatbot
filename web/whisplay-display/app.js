const statusText = document.getElementById("statusText");
const emojiText = document.getElementById("emojiText");
const textContent = document.getElementById("textContent");
const batteryFill = document.getElementById("batteryFill");
const batteryText = document.getElementById("batteryText");
const dailyRequestsText = document.getElementById("dailyRequestsText");
const wifiIcon = document.getElementById("wifiIcon");
const vpnIcon = document.getElementById("vpnIcon");
const imageIcon = document.getElementById("imageIcon");
const ragIcon = document.getElementById("ragIcon");
const musicProgress = document.getElementById("musicProgress");
const musicFill = document.getElementById("musicFill");
const musicElapsed = document.getElementById("musicElapsed");
const musicTotal = document.getElementById("musicTotal");
const led = document.getElementById("led");
const ledText = document.getElementById("ledText");
const btn = document.getElementById("btn");
const btnText = document.getElementById("btnText");
const visionImageInput = document.getElementById("visionImageInput");
const visionUploadBtn = document.getElementById("visionUploadBtn");
const visionCaptureBtn = document.getElementById("visionCaptureBtn");
const visionAnalysisToggleBtn = document.getElementById("visionAnalysisToggleBtn");
const visionAnalysisWrap = document.getElementById("visionAnalysisWrap");
const visionAnalysisMeta = document.getElementById("visionAnalysisMeta");
const visionAnalysisText = document.getElementById("visionAnalysisText");
const visionPreview = document.getElementById("visionPreview");
const visionStatus = document.getElementById("visionStatus");
const savedPhotosList = document.getElementById("savedPhotosList");
const savedPhotosStatus = document.getElementById("savedPhotosStatus");
const savedPhotosToggleBtn = document.getElementById("savedPhotosToggleBtn");
const chatHistorySelect = document.getElementById("chatHistorySelect");
const loadChatBtn = document.getElementById("loadChatBtn");
const newChatBtn = document.getElementById("newChatBtn");
const chatStatus = document.getElementById("chatStatus");
const musicFileInput = document.getElementById("musicFileInput");
const musicUploadBtn = document.getElementById("musicUploadBtn");
const musicPlayBtn = document.getElementById("musicPlayBtn");
const musicStopBtn = document.getElementById("musicStopBtn");
const musicPrevBtn = document.getElementById("musicPrevBtn");
const musicNextBtn = document.getElementById("musicNextBtn");
const musicShuffleCheckbox = document.getElementById("musicShuffleCheckbox");
const musicTrackList = document.getElementById("musicTrackList");
const musicStatus = document.getElementById("musicStatus");
const groqKeyInput = document.getElementById("groqKeyInput");
const groqKeyHint = document.getElementById("groqKeyHint");
const geminiKeyInput = document.getElementById("geminiKeyInput");
const geminiKeyHint = document.getElementById("geminiKeyHint");
const personalityPresetSelect = document.getElementById("personalityPresetSelect");
const personalityInput = document.getElementById("personalityInput");
const personalityNameInput = document.getElementById("personalityNameInput");
const savePersonalityBtn = document.getElementById("savePersonalityBtn");
const voiceModeSelect = document.getElementById("voiceModeSelect");
const llmModelSelect = document.getElementById("llmModelSelect");
const volumeLevelSelect = document.getElementById("volumeLevelSelect");
const recordTimeSelect = document.getElementById("recordTimeSelect");
const scrollSpeedSelect = document.getElementById("scrollSpeedSelect");
const hatScrollSpeedSelect = document.getElementById("hatScrollSpeedSelect");
const hatFontSizeSelect = document.getElementById("hatFontSizeSelect");
const hatFontFamilySelect = document.getElementById("hatFontFamilySelect");
const hatTextColorSelect = document.getElementById("hatTextColorSelect");
const uiThemeSelect = document.getElementById("uiThemeSelect");
const cameraSourceSelect = document.getElementById("cameraSourceSelect");
const esp32CamUrlInput = document.getElementById("esp32CamUrlInput");
const esp32CamUrlWrap = document.getElementById("esp32CamUrlWrap");
const piCameraRotationSelect = document.getElementById("piCameraRotationSelect");
const esp32CamRotationSelect = document.getElementById("esp32CamRotationSelect");
const weatherLatitudeInput = document.getElementById("weatherLatitudeInput");
const weatherLongitudeInput = document.getElementById("weatherLongitudeInput");
const headerModeSelect = document.getElementById("headerModeSelect");
const screensaverModeSelect = document.getElementById("screensaverModeSelect");
const idleTimeoutSelect = document.getElementById("idleTimeoutSelect");
const screenBlankTimeoutSelect = document.getElementById("screenBlankTimeoutSelect");
const roomMonitorIntervalSelect = document.getElementById("roomMonitorIntervalSelect");
const saveSettingsBtn = document.getElementById("saveSettingsBtn");
const clearKeyBtn = document.getElementById("clearKeyBtn");
const shutdownBtn = document.getElementById("shutdownBtn");
const settingsStatus = document.getElementById("settingsStatus");
const roomMonitorStatus = document.getElementById("roomMonitorStatus");
const roomMonitorGalleryList = document.getElementById("roomMonitorGalleryList");
const roomMonitorToggleBtn = document.getElementById("roomMonitorToggleBtn");
const botNetEnabledCheckbox = document.getElementById("botNetEnabledCheckbox");
const botNetModeSelect = document.getElementById("botNetModeSelect");
const botNetModelSelect = document.getElementById("botNetModelSelect");
const botNetModeHint = document.getElementById("botNetModeHint");
const botNetTransportSelect = document.getElementById("botNetTransportSelect");
const botNetTransportHint = document.getElementById("botNetTransportHint");
const botNetNodeHandleInput = document.getElementById("botNetNodeHandleInput");
const botNetHubUrlInput = document.getElementById("botNetHubUrlInput");
const botNetHubUrlWrap = document.getElementById("botNetHubUrlWrap");
const botNetInviteCodeInput = document.getElementById("botNetInviteCodeInput");
const botNetInviteCodeWrap = document.getElementById("botNetInviteCodeWrap");
const botNetPeerUrlInput = document.getElementById("botNetPeerUrlInput");
const botNetPublicUrlInput = document.getElementById("botNetPublicUrlInput");
const botNetPeerUrlWrap = document.getElementById("botNetPeerUrlWrap");
const botNetPublicUrlWrap = document.getElementById("botNetPublicUrlWrap");
const botNetMaxRepliesInput = document.getElementById("botNetMaxRepliesInput");
const botNetReplyDelayInput = document.getElementById("botNetReplyDelayInput");
const botNetMaxRepliesWrap = document.getElementById("botNetMaxRepliesWrap");
const botNetReplyDelayWrap = document.getElementById("botNetReplyDelayWrap");
const botNetTopicInput = document.getElementById("botNetTopicInput");
const botNetTopicLabel = document.getElementById("botNetTopicLabel");
const botNetSaveBtn = document.getElementById("botNetSaveBtn");
const botNetTestBtn = document.getElementById("botNetTestBtn");
const botNetRegisterBtn = document.getElementById("botNetRegisterBtn");
const botNetConnectBtn = document.getElementById("botNetConnectBtn");
const botNetInviteBtn = document.getElementById("botNetInviteBtn");
const botNetRedeemBtn = document.getElementById("botNetRedeemBtn");
const botNetDisconnectBtn = document.getElementById("botNetDisconnectBtn");
const botNetStartBtn = document.getElementById("botNetStartBtn");
const botNetPeerStartBtn = document.getElementById("botNetPeerStartBtn");
const botNetStopBtn = document.getElementById("botNetStopBtn");
const botNetStatus = document.getElementById("botNetStatus");
const botNetTranscript = document.getElementById("botNetTranscript");
const dim = document.getElementById("dim");
const imageLayer = document.getElementById("imageLayer");
const imageDisplay = document.getElementById("imageDisplay");

let scrollTop = 0;
let scrollSpeed = 0;
let scrollSpeedFactor = 1;
let scrollTarget = null;
let scrollSyncStart = null;
let scrollSyncDuration = 0;
let scrollSyncFrom = 0;
let lastFrameTime = 0;
let maxScroll = 0;
let lastText = "";
let lastImageRevision = -1;
let isPressed = false;
let activePointerId = null;
let settingsLoaded = false;
let visionAnalysisVisible = false;
let latestVisionAnalysisStamp = 0;
let savedPhotos = [];
let showAllSavedPhotos = false;
let savedChatHistories = [];
let screenBlankTimeoutOptions = [];
let roomMonitorIntervalOptions = [];
let roomMonitorPhotos = [];
let showAllRoomMonitorPhotos = false;
let roomMonitorGalleryState = null;
let musicTracks = [];
let botNetState = null;
let botNetSettingsDirty = false;
let botNetSettingsLoaded = false;
const DEFAULT_UI_THEME = "default";
const DEFAULT_CAMERA_SOURCE = "pi-camera";
const DEFAULT_ESP32_CAM_URL = "http://esp32-cam.local";
const DEFAULT_HAT_TEXT_COLOR = "white";
const DEFAULT_HAT_FONT_FAMILY = "default";
const DEFAULT_CAMERA_ROTATION_DEG = "0";
const CUSTOM_PERSONALITY_PRESET_ID = "custom";
let personalityPresets = [];
let botNetModelOptions = [];
let llmModelOptions = [];
let volumeLevelOptions = [];
let recordTimeoutOptions = [];
let scrollSpeedOptions = [];
let hatScrollSpeedOptions = [];
let idleTimeoutOptions = [];

const DEFAULT_HEADER_MODE = "emoji";
const DEFAULT_SCREENSAVER_MODE = "retro-geometry";
const DEFAULT_IDLE_TIMEOUT_SEC = 120;

function setBotNetStatus(message, isError = false) {
  if (!botNetStatus) return;
  botNetStatus.textContent = message;
  botNetStatus.style.color = isError ? "#ff7b7b" : "";
}

function updateBotNetTransportUi(transportMode) {
  const currentTransport = transportMode === "online-hub" ? "online-hub" : "lan-direct";
  if (botNetTransportSelect) {
    botNetTransportSelect.value = currentTransport;
  }
  if (botNetTransportHint) {
    botNetTransportHint.textContent =
      currentTransport === "online-hub"
        ? "Online Hub keeps an outbound secure session to the hosted hub and uses relay delivery when needed."
        : "LAN Direct talks straight to a saved peer URL on your local network.";
  }
  if (botNetHubUrlWrap) {
    botNetHubUrlWrap.style.display = currentTransport === "online-hub" ? "block" : "none";
  }
  if (botNetInviteCodeWrap) {
    botNetInviteCodeWrap.style.display = currentTransport === "online-hub" ? "block" : "none";
  }
  if (botNetPeerUrlWrap) {
    botNetPeerUrlWrap.style.display = currentTransport === "lan-direct" ? "block" : "none";
  }
  if (botNetPublicUrlWrap) {
    botNetPublicUrlWrap.style.display = currentTransport === "lan-direct" ? "block" : "none";
  }
  if (botNetTestBtn) {
    botNetTestBtn.textContent =
      currentTransport === "online-hub" ? "Test Hub" : "Test Connection";
  }
  if (botNetInviteBtn) {
    botNetInviteBtn.disabled = currentTransport !== "online-hub";
  }
  if (botNetRedeemBtn) {
    botNetRedeemBtn.disabled = currentTransport !== "online-hub";
  }
}

function updateBotNetModeUi(mode) {
  const currentMode = mode === "persona-relay" ? "persona-relay" : "auto-bot";
  if (botNetModeSelect) {
    botNetModeSelect.value = currentMode;
  }
  if (botNetMaxRepliesWrap) {
    botNetMaxRepliesWrap.style.display = currentMode === "auto-bot" ? "block" : "none";
  }
  if (botNetReplyDelayWrap) {
    botNetReplyDelayWrap.style.display = currentMode === "auto-bot" ? "block" : "none";
  }
  if (botNetModeHint) {
    botNetModeHint.textContent =
      currentMode === "persona-relay"
        ? "Recommended. Whisplay rewrites one prompt in character, sends it once, and waits for you."
        : "Auto Conversation keeps both bots talking on their own until the reply limit stops them.";
  }
  if (botNetTopicLabel) {
    botNetTopicLabel.textContent =
      currentMode === "persona-relay" ? "Tell Whisplay what to send" : "Start Auto Conversation";
  }
  if (botNetTopicInput) {
    botNetTopicInput.placeholder =
      currentMode === "persona-relay"
        ? "Ask Whisplay to relay something to the peer in its own words..."
        : "Give the bots a topic, mood, or scenario for auto-chat...";
  }
  if (botNetPeerStartBtn) {
    botNetPeerStartBtn.style.display = currentMode === "auto-bot" ? "" : "none";
  }
}

function renderBotNetTranscript(conversation) {
  if (!botNetTranscript) return;
  botNetTranscript.innerHTML = "";
  if (!conversation || !Array.isArray(conversation.messages) || !conversation.messages.length) {
    botNetTranscript.innerHTML =
      '<div class="status-text vision-status">No BotNet conversation yet.</div>';
    return;
  }
  conversation.messages.slice(-12).forEach((message) => {
    const item = document.createElement("div");
    const speakerType = message.speakerType || "system";
    item.className = `botnet-transcript-item ${speakerType}`;

    const head = document.createElement("div");
    head.className = "botnet-transcript-head";
    const speaker = document.createElement("span");
    speaker.textContent = message.speakerName || speakerType;
    const stamp = document.createElement("span");
    const date = message.createdAt ? new Date(message.createdAt) : null;
    stamp.textContent =
      date && !Number.isNaN(date.getTime()) ? date.toLocaleTimeString() : "";
    head.appendChild(speaker);
    head.appendChild(stamp);

    const text = document.createElement("div");
    text.className = "botnet-transcript-text";
    text.textContent = message.text || "";

    item.appendChild(head);
    item.appendChild(text);
    botNetTranscript.appendChild(item);
  });
}

function applyBotNetSettings(settings, force = false) {
  if (botNetSettingsDirty && !force) {
    return;
  }
  updateBotNetModeUi(settings.botnetMode || "auto-bot");
  updateBotNetTransportUi(settings.transportMode || "lan-direct");
  populateBotNetModelOptions(settings.model || "");
  if (botNetEnabledCheckbox) {
    botNetEnabledCheckbox.checked = Boolean(settings.enabled);
  }
  if (botNetNodeHandleInput) {
    botNetNodeHandleInput.value = settings.nodeHandle || "Whisplay Bot";
  }
  if (botNetHubUrlInput) {
    botNetHubUrlInput.value = settings.hubUrl || "";
  }
  if (botNetPeerUrlInput) {
    botNetPeerUrlInput.value = settings.peerUrl || "";
  }
  if (botNetPublicUrlInput) {
    botNetPublicUrlInput.value = settings.publicUrl || "";
  }
  if (botNetMaxRepliesInput) {
    botNetMaxRepliesInput.value = String(
      Number.isFinite(settings.maxBotReplies) ? settings.maxBotReplies : 8,
    );
  }
  if (botNetReplyDelayInput) {
    botNetReplyDelayInput.value = String(
      Number.isFinite(settings.replyDelaySec) ? settings.replyDelaySec : 6,
    );
  }
  if (botNetModelSelect) {
    botNetModelSelect.value = settings.model || botNetModelOptions[0]?.id || "";
  }
  botNetSettingsDirty = false;
  botNetSettingsLoaded = true;
}

function applyBotNetState(state, forceSettings = false) {
  botNetState = state || null;
  botNetModelOptions = Array.isArray(state?.modelOptions)
    ? state.modelOptions
    : botNetModelOptions;
  const settings = state?.settings || {};
  const online = state?.online || null;
  if (!botNetSettingsLoaded || forceSettings) {
    applyBotNetSettings(settings, true);
  } else {
    applyBotNetSettings(settings, false);
  }
  const activeConversation = Array.isArray(state?.conversations)
    ? state.conversations.find((conversation) => conversation.status === "active")
    : null;
  const fallbackConversation =
    activeConversation || (Array.isArray(state?.conversations) ? state.conversations[0] : null);
  const connectionStatus = state?.connectionStatus || "Disconnected";
  const onlineSuffix =
    online && settings.transportMode === "online-hub"
      ? ` • ${online.nodeId ? `Node ${online.nodeId}` : "Unregistered"}${online.linkId ? ` • Link ${online.linkId}` : ""}`
      : "";
  setBotNetStatus(
    activeConversation
      ? `Active • ${connectionStatus} • ${activeConversation.topic || "Conversation"}`
      : `${connectionStatus}${settings.enabled ? "" : " • BotNet mode is off"}${onlineSuffix}`,
  );
  renderBotNetTranscript(fallbackConversation);
}

async function loadBotNetState() {
  try {
    const response = await fetch("/api/botnet/state", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const payload = await response.json();
    applyBotNetState(payload, false);
  } catch (error) {
    console.error("Failed to load BotNet state:", error);
    setBotNetStatus("Failed to load BotNet state.", true);
  }
}

async function saveBotNetSettings(showSavedStatus = true) {
  const body = {
    enabled: Boolean(botNetEnabledCheckbox?.checked),
    botnetMode: botNetModeSelect?.value || "auto-bot",
    model: botNetModelSelect?.value || botNetModelOptions[0]?.id || "",
    transportMode: botNetTransportSelect?.value || "lan-direct",
    nodeHandle: (botNetNodeHandleInput?.value || "Whisplay Bot").trim(),
    hubUrl: (botNetHubUrlInput?.value || "").trim(),
    peerUrl: (botNetPeerUrlInput?.value || "").trim(),
    publicUrl: (botNetPublicUrlInput?.value || "").trim(),
    maxBotReplies: parseInt(botNetMaxRepliesInput?.value || "8", 10),
    replyDelaySec: parseInt(botNetReplyDelayInput?.value || "6", 10),
  };
  if (showSavedStatus) {
    setBotNetStatus("Saving BotNet settings...");
  }
  const response = await fetch("/api/botnet/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const payload = await response.json();
  if (!response.ok || payload.ok === false) {
    throw new Error(payload.error || `HTTP ${response.status}`);
  }
  applyBotNetState(
    {
      ok: true,
      ...(botNetState || {}),
      settings: payload.settings || body,
      modelOptions: botNetState?.modelOptions || [],
      connectionStatus: botNetState?.connectionStatus || "Disconnected",
      conversations: botNetState?.conversations || [],
    },
    true,
  );
  await loadBotNetState();
  if (showSavedStatus) {
    setBotNetStatus("BotNet settings saved.");
  }
}

async function testBotNetConnection() {
  const transportMode = botNetTransportSelect?.value || "lan-direct";
  const peerUrl = (botNetPeerUrlInput?.value || "").trim();
  const hubUrl = (botNetHubUrlInput?.value || "").trim();
  if (transportMode === "online-hub" && !hubUrl) {
    setBotNetStatus("Set Hub URL first.", true);
    return;
  }
  if (transportMode !== "online-hub" && !peerUrl) {
    setBotNetStatus("Set Connect to Bot first.", true);
    return;
  }
  try {
    setBotNetStatus(
      transportMode === "online-hub" ? "Testing hub connection..." : "Testing peer connection...",
    );
    await saveBotNetSettings(false);
    const response = await fetch("/api/botnet/test", { method: "POST" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.message || payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus(payload.message || "Peer bot is reachable.");
  } catch (error) {
    console.error("Failed to test BotNet connection:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to test BotNet connection.",
      true,
    );
  }
}

function validateBotNetConversationSetup() {
  const transportMode = botNetTransportSelect?.value || "lan-direct";
  if (transportMode === "online-hub") {
    if (!botNetState?.online?.registered) {
      setBotNetStatus("Register this node with the hub first.", true);
      return false;
    }
    if (!botNetState?.online?.connected) {
      setBotNetStatus("Connect the hub relay session first.", true);
      return false;
    }
    if (!botNetState?.online?.linkId || !botNetState?.online?.peerNodeId) {
      setBotNetStatus("No online peer link is active yet.", true);
      return false;
    }
    return true;
  }
  const peerUrl = (botNetPeerUrlInput?.value || "").trim();
  if (!peerUrl) {
    setBotNetStatus("Set Connect to Bot first.", true);
    return false;
  }
  const publicUrl = (botNetPublicUrlInput?.value || "").trim();
  if (!publicUrl) {
    setBotNetStatus("Set This Bot URL first.", true);
    return false;
  }
  return true;
}

async function registerBotNetNode() {
  try {
    setBotNetStatus("Registering this Whisplay node with the hub...");
    await saveBotNetSettings(false);
    const response = await fetch("/api/botnet/register", { method: "POST" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus("Whisplay node registered with the hub.");
  } catch (error) {
    console.error("Failed to register Whisplay node:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to register Whisplay node.",
      true,
    );
  }
}

async function connectBotNetHub() {
  try {
    setBotNetStatus("Connecting the Whisplay hub relay session...");
    await saveBotNetSettings(false);
    const response = await fetch("/api/botnet/connect", { method: "POST" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus("Whisplay is connected to the BotNet hub.");
  } catch (error) {
    console.error("Failed to connect Whisplay hub session:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to connect the hub session.",
      true,
    );
  }
}

async function disconnectBotNetHub() {
  try {
    setBotNetStatus("Disconnecting the Whisplay hub relay session...");
    const response = await fetch("/api/botnet/disconnect", { method: "POST" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus("Whisplay hub relay session disconnected.");
  } catch (error) {
    console.error("Failed to disconnect Whisplay hub session:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to disconnect the hub session.",
      true,
    );
  }
}

async function createBotNetInvite() {
  try {
    setBotNetStatus("Creating a BotNet invite...");
    await saveBotNetSettings(false);
    const response = await fetch("/api/botnet/invite", { method: "POST" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    if (botNetInviteCodeInput) {
      botNetInviteCodeInput.value = payload.invite?.inviteCode || "";
    }
    await loadBotNetState();
    setBotNetStatus(
      payload.invite?.inviteCode
        ? `Invite created: ${payload.invite.inviteCode}`
        : "BotNet invite created.",
    );
  } catch (error) {
    console.error("Failed to create BotNet invite:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to create a BotNet invite.",
      true,
    );
  }
}

async function redeemBotNetInvite() {
  const inviteCode = (botNetInviteCodeInput?.value || "").trim().toUpperCase();
  if (!inviteCode) {
    setBotNetStatus("Paste an invite code first.", true);
    return;
  }
  try {
    setBotNetStatus("Redeeming BotNet invite...");
    await saveBotNetSettings(false);
    const response = await fetch("/api/botnet/redeem", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ inviteCode }),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus("BotNet invite redeemed. Peer link is ready.");
  } catch (error) {
    console.error("Failed to redeem BotNet invite:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to redeem the BotNet invite.",
      true,
    );
  }
}

async function startBotNetConversation(starter = "self") {
  const topic = (botNetTopicInput?.value || "").trim();
  if (!topic) {
    setBotNetStatus("Enter a topic first.", true);
    return;
  }
  if (!validateBotNetConversationSetup()) {
    return;
  }
  try {
    setBotNetStatus(
      starter === "peer" ? "Asking peer bot to start..." : "Starting BotNet conversation...",
    );
    await saveBotNetSettings(false);
    const response = await fetch("/api/botnet/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        topic,
        starter,
        botnetMode: botNetModeSelect?.value || "auto-bot",
      }),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus(
      starter === "peer" ? "Peer start request sent." : "BotNet conversation started.",
    );
  } catch (error) {
    console.error("Failed to start BotNet conversation:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to start BotNet conversation.",
      true,
    );
  }
}

async function stopBotNetConversation() {
  try {
    setBotNetStatus("Stopping BotNet conversation...");
    const response = await fetch("/api/botnet/stop", { method: "POST" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadBotNetState();
    setBotNetStatus(payload.stopped ? "BotNet conversation stopped." : "No active BotNet conversation.");
  } catch (error) {
    console.error("Failed to stop BotNet conversation:", error);
    setBotNetStatus(
      error instanceof Error ? error.message : "Failed to stop BotNet conversation.",
      true,
    );
  }
}

function setIconVisible(iconEl, visible) {
  iconEl.style.display = visible ? "block" : "none";
}

const WIFI_LEVEL_SRC = {
  1: "/img/wifi-weak.png",
  2: "/img/wifi-medium.png",
  3: "/img/wifi-strong.png",
};

function updateWifiIcon(level) {
  const numeric = typeof level === "number" ? level : parseInt(level, 10);
  if (!Number.isFinite(numeric) || numeric <= 0) {
    return false;
  }
  const clamped = Math.min(3, Math.max(1, Math.round(numeric)));
  const src = WIFI_LEVEL_SRC[clamped];
  if (wifiIcon.getAttribute("src") !== src) {
    wifiIcon.setAttribute("src", src);
  }
  return true;
}

function rgb565ToRgb(color) {
  const r = (color >> 11) & 0x1f;
  const g = (color >> 5) & 0x3f;
  const b = color & 0x1f;
  return [
    Math.round((r * 255) / 31),
    Math.round((g * 255) / 63),
    Math.round((b * 255) / 31),
  ];
}

function normalizeColor(value) {
  if (typeof value === "number") {
    const rgb = rgb565ToRgb(value);
    return `rgb(${rgb[0]}, ${rgb[1]}, ${rgb[2]})`;
  }
  if (typeof value === "string" && value.length > 0) {
    return value.startsWith("#") ? value : `#${value}`;
  }
  return "#44f28a";
}

function applyScrollSync(text, sync, viewportHeight) {
  if (!sync || !text) {
    return;
  }
  const charEnd = Math.max(0, parseInt(sync.char_end || 0, 10));
  const duration = Math.max(1, parseInt(sync.duration_ms || 1, 10));
  const totalChars = text.length || 1;
  const ratio = Math.min(1, charEnd / totalChars);
  maxScroll = Math.max(0, textContent.offsetHeight - viewportHeight);
  scrollTarget = Math.max(scrollTop, Math.round(maxScroll * ratio));
  scrollSyncFrom = scrollTop;
  scrollSyncStart = performance.now();
  scrollSyncDuration = duration;
}

function updateText(text, sync, speed, speedFactor = 1) {
  const viewportHeight = document.querySelector(".text-viewport").offsetHeight;
  const nextText = text || "";
  const isRegressive =
    nextText.length > 0 && nextText.length < lastText.length && lastText.startsWith(nextText);
  const nextFactor = Number.isFinite(speedFactor) ? Math.max(0, speedFactor) : 1;

  if (isRegressive) {
    scrollSpeed = Math.max(0, parseInt(speed || 0, 10));
    scrollSpeedFactor = nextFactor;
    applyScrollSync(lastText, sync, viewportHeight);
    maxScroll = Math.max(0, textContent.offsetHeight - viewportHeight);
    return;
  }

  if (nextText !== lastText) {
    const isContinuation = nextText.startsWith(lastText);
    textContent.textContent = nextText;
    if (!isContinuation) {
      scrollTop = 0;
      scrollTarget = null;
      scrollSyncStart = null;
      scrollSyncDuration = 0;
      scrollSyncFrom = 0;
    }
    lastText = nextText;
  }

  scrollSpeed = Math.max(0, parseInt(speed || 0, 10));
  scrollSpeedFactor = nextFactor;
  applyScrollSync(lastText, sync, viewportHeight);
  maxScroll = Math.max(0, textContent.offsetHeight - viewportHeight);
}

function animateScroll(timestamp) {
  if (!lastFrameTime) {
    lastFrameTime = timestamp;
  }
  const deltaMs = timestamp - lastFrameTime;
  lastFrameTime = timestamp;

  if (scrollTarget !== null && scrollSyncStart !== null) {
    const elapsed = timestamp - scrollSyncStart;
    const progress = Math.min(1, elapsed / scrollSyncDuration);
    scrollTop = scrollSyncFrom + (scrollTarget - scrollSyncFrom) * progress;
    if (progress >= 1) {
      scrollTarget = null;
      scrollSyncStart = null;
    }
  } else if (scrollSpeed > 0 && scrollTop < maxScroll) {
    const speedPerSec = scrollSpeed * 5 * scrollSpeedFactor;
    scrollTop = Math.min(maxScroll, scrollTop + (speedPerSec * deltaMs) / 1000);
  }

  textContent.style.transform = `translateY(${-scrollTop}px)`;
  requestAnimationFrame(animateScroll);
}

let ws = null;
let reconnectTimer = null;
let cameraTimer = null;

function formatMs(ms) {
  const totalSec = Math.floor(ms / 1000);
  const min = Math.floor(totalSec / 60);
  const sec = totalSec % 60;
  return min + ":" + (sec < 10 ? "0" : "") + sec;
}

function formatDailyRequestsLabel(value) {
  const numeric = Number.isFinite(Number(value))
    ? Math.max(0, Math.round(Number(value)))
    : 0;
  return `RPD ${numeric}`;
}

function applyState(data) {
  if (!data || !data.ready) return;

  const status = data.status || "";
  statusText.textContent = status;
  emojiText.textContent = data.emoji || "";
  updateText(
    data.text || "",
    data.scroll_sync,
    data.scroll_speed,
    data.scroll_speed_factor,
  );
  updateTextInputState(data.text_input_enabled, status);

  const ledColor = normalizeColor(data.RGB);
  led.style.background = ledColor;
  led.style.boxShadow = `0 0 24px ${ledColor}`;
  ledText.textContent = ledColor;

  const batteryLevel = typeof data.battery_level === "number" ? data.battery_level : null;
  if (batteryLevel === null) {
    batteryText.textContent = "--%";
    batteryFill.style.width = "0%";
  } else {
    batteryText.textContent = `${batteryLevel}%`;
    batteryFill.style.width = `${Math.min(100, Math.max(0, batteryLevel))}%`;
  }
  batteryFill.style.background = normalizeColor(data.battery_color);
  if (dailyRequestsText) {
    dailyRequestsText.textContent = formatDailyRequestsLabel(
      data.groq_requests_today,
    );
  }

  setIconVisible(wifiIcon, updateWifiIcon(data.wifi_signal_level));
  setIconVisible(vpnIcon, Boolean(data.vpn_connected));
  setIconVisible(imageIcon, Boolean(data.image_icon_visible));
  setIconVisible(ragIcon, Boolean(data.rag_icon_visible));

  const progress = typeof data.music_progress === "number" ? data.music_progress : -1;
  const durationMs = typeof data.music_duration_ms === "number" ? data.music_duration_ms : 0;
  const showMusicProgress = status === "music" && progress >= 0 && durationMs > 0;
  if (showMusicProgress) {
    musicProgress.classList.add("visible");
    musicFill.style.width = (Math.min(1, Math.max(0, progress)) * 100).toFixed(1) + "%";
    musicElapsed.textContent = formatMs(durationMs * Math.min(1, Math.max(0, progress)));
    musicTotal.textContent = formatMs(durationMs);
  } else {
    musicProgress.classList.remove("visible");
    musicFill.style.width = "0%";
    musicElapsed.textContent = "0:00";
    musicTotal.textContent = "0:00";
  }

  const dimOpacity = Math.max(0, Math.min(1, (100 - (data.brightness ?? 100)) / 100));
  dim.style.opacity = dimOpacity.toFixed(2);

  if (data.camera_mode) {
    imageLayer.style.display = "flex";
    startCameraFeed();
    return;
  }

  stopCameraFeed();
  if (data.image && data.image_revision !== lastImageRevision) {
    lastImageRevision = data.image_revision;
    imageDisplay.src = `/image?rev=${lastImageRevision}`;
    imageLayer.style.display = "flex";
  } else if (!data.image) {
    imageLayer.style.display = "none";
  }
}

function startCameraFeed() {
  if (cameraTimer) return;
  cameraTimer = setInterval(() => {
    imageDisplay.src = `/camera?ts=${Date.now()}`;
  }, 200);
}

function stopCameraFeed() {
  if (!cameraTimer) return;
  clearInterval(cameraTimer);
  cameraTimer = null;
}

function connectWebSocket() {
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
    reconnectTimer = null;
  }
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const url = `${protocol}://${window.location.host}/ws`;
  ws = new WebSocket(url);

  ws.addEventListener("message", (event) => {
    let message = null;
    try {
      message = JSON.parse(event.data);
    } catch {
      return;
    }
    if (message.type === "state") {
      applyState(message.payload);
    } else if (message.type === "start_record") {
      startWebAudioRecording();
    } else if (message.type === "stop_record") {
      stopWebAudioRecording();
    } else if (message.type === "play_audio") {
      playWebAudio(message.data, message.format, message.duration, message.playId);
    } else if (message.type === "stop_audio") {
      stopWebAudio();
    } else if (message.type === "start_camera_stream") {
      startWebCameraStream();
    } else if (message.type === "stop_camera_stream") {
      stopWebCameraStream();
    } else if (message.type === "capture_photo") {
      sendWebCameraCapture();
    }
  });

  ws.addEventListener("close", () => {
    stopCameraFeed();
    reconnectTimer = setTimeout(connectWebSocket, 1000);
  });

  ws.addEventListener("error", () => {
    ws.close();
  });
}

function sendButton(action) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify({ type: "button", action }));
}

function applyTheme(theme) {
  const nextTheme = theme || DEFAULT_UI_THEME;
  document.documentElement.dataset.theme = nextTheme;
}

function setSettingsStatus(message, isError = false) {
  if (!settingsStatus) return;
  settingsStatus.textContent = message;
  settingsStatus.style.color = isError ? "#ff8a8a" : "";
}

function setVisionStatus(message, isError = false) {
  if (!visionStatus) return;
  visionStatus.textContent = message;
  visionStatus.style.color = isError ? "#ff8a8a" : "";
}

function setSavedPhotosStatus(message, isError = false) {
  if (!savedPhotosStatus) return;
  savedPhotosStatus.textContent = message;
  savedPhotosStatus.style.color = isError ? "#ff8a8a" : "";
}

function setChatStatus(message, isError = false) {
  if (!chatStatus) return;
  chatStatus.textContent = message;
  chatStatus.style.color = isError ? "#ff8a8a" : "";
}

function setVisionAnalysisVisible(visible) {
  visionAnalysisVisible = Boolean(visible);
  if (visionAnalysisWrap) {
    visionAnalysisWrap.style.display = visionAnalysisVisible ? "block" : "none";
  }
  if (visionAnalysisToggleBtn) {
    visionAnalysisToggleBtn.textContent = visionAnalysisVisible
      ? "Hide Gemini Output"
      : "Show Gemini Output";
  }
}

function renderVisionAnalysis(analysis) {
  if (!visionAnalysisMeta || !visionAnalysisText) return;
  if (!analysis) {
    latestVisionAnalysisStamp = 0;
    visionAnalysisMeta.textContent = "No Gemini analysis yet.";
    visionAnalysisText.textContent = "";
    return;
  }
  latestVisionAnalysisStamp = Number(analysis.updatedAt || 0);
  const updatedLabel = latestVisionAnalysisStamp
    ? new Date(latestVisionAnalysisStamp).toLocaleTimeString()
    : "just now";
  const question = analysis.question || "Vision question";
  const status = analysis.ok === false ? "error" : "ready";
  visionAnalysisMeta.textContent = `${status} · ${updatedLabel} · ${question}`;
  visionAnalysisText.textContent = analysis.rawResponse || "";
}

function renderSavedPhotos() {
  if (!savedPhotosList) return;
  savedPhotosList.innerHTML = "";
  if (!savedPhotos.length) {
    setSavedPhotosStatus("No saved photos yet.");
    const empty = document.createElement("div");
    empty.className = "saved-photos-empty";
    empty.textContent = "No saved photos yet.";
    savedPhotosList.appendChild(empty);
    if (savedPhotosToggleBtn) {
      savedPhotosToggleBtn.disabled = true;
      savedPhotosToggleBtn.textContent = "Gallery";
    }
    return;
  }
  const visiblePhotos = showAllSavedPhotos ? savedPhotos : savedPhotos.slice(0, 4);
  const hiddenCount = Math.max(0, savedPhotos.length - visiblePhotos.length);
  const countLabel = `${savedPhotos.length} saved photo${savedPhotos.length === 1 ? "" : "s"}.`;
  setSavedPhotosStatus(
    showAllSavedPhotos || hiddenCount === 0
      ? countLabel
      : `${countLabel} Showing ${visiblePhotos.length} recent.`,
  );
  if (savedPhotosToggleBtn) {
    savedPhotosToggleBtn.disabled = savedPhotos.length <= 4;
    savedPhotosToggleBtn.textContent =
      savedPhotos.length <= 4
        ? "Gallery"
        : showAllSavedPhotos
          ? "Recent Only"
          : `Gallery (${savedPhotos.length})`;
  }
  for (const photo of visiblePhotos) {
    const card = document.createElement("div");
    card.className = "saved-photo-card";

    const img = document.createElement("img");
    img.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
    img.alt = photo.fileName;

    const label = document.createElement("div");
    label.className = "saved-photo-name";
    label.textContent = photo.fileName;

    const meta = document.createElement("div");
    meta.className = "saved-photo-meta";
    meta.textContent = new Date(photo.updatedAt).toLocaleString();

    const actions = document.createElement("div");
    actions.className = "saved-photo-actions";

    const downloadLink = document.createElement("a");
    downloadLink.className = "button compact saved-photo-download";
    downloadLink.textContent = "Download";
    downloadLink.href = photo.imageUrl;
    downloadLink.download = photo.fileName;

    const deleteBtn = document.createElement("button");
    deleteBtn.type = "button";
    deleteBtn.className = "button secondary compact saved-photo-delete";
    deleteBtn.textContent = "Delete";
    deleteBtn.addEventListener("click", () => {
      deleteSavedPhoto(photo.fileName);
    });

    card.appendChild(img);
    card.appendChild(label);
    card.appendChild(meta);
    actions.appendChild(downloadLink);
    actions.appendChild(deleteBtn);
    card.appendChild(actions);
    savedPhotosList.appendChild(card);
  }
}

async function loadSavedPhotos() {
  try {
    const response = await fetch(`/api/photos?ts=${Date.now()}`, {
      cache: "no-store",
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const payload = await response.json();
    savedPhotos = Array.isArray(payload.photos) ? payload.photos : [];
    renderSavedPhotos();
  } catch (error) {
    console.error("Failed to load saved photos:", error);
    savedPhotos = [];
    renderSavedPhotos();
    setSavedPhotosStatus("Failed to load saved photos.", true);
  }
}

async function deleteSavedPhoto(fileName) {
  const confirmed = window.confirm(`Delete ${fileName}?`);
  if (!confirmed) {
    return;
  }
  try {
    const response = await fetch("/api/photos", {
      method: "DELETE",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ fileName }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadSavedPhotos();
  } catch (error) {
    console.error("Failed to delete saved photo:", error);
    const message = error instanceof Error ? error.message : String(error);
    setSavedPhotosStatus(`Delete failed: ${message}`, true);
  }
}

function updateCameraSourceUi() {
  const source = cameraSourceSelect?.value || DEFAULT_CAMERA_SOURCE;
  if (esp32CamUrlWrap) {
    esp32CamUrlWrap.style.display = source === "esp32-cam" ? "block" : "none";
  }
}

function formatChatHistoryOption(history) {
  const timeLabel = history.updatedAt
    ? new Date(history.updatedAt).toLocaleString()
    : history.fileName;
  const preview = history.preview ? ` - ${history.preview}` : "";
  return `${timeLabel}${preview}`;
}

function renderChatHistoryOptions() {
  if (!chatHistorySelect) return;
  chatHistorySelect.innerHTML = "";
  const placeholder = document.createElement("option");
  placeholder.value = "";
  placeholder.textContent = savedChatHistories.length ? "Saved chats" : "No saved chats";
  chatHistorySelect.appendChild(placeholder);
  savedChatHistories.forEach((history) => {
    const option = document.createElement("option");
    option.value = history.fileName;
    option.textContent = formatChatHistoryOption(history);
    chatHistorySelect.appendChild(option);
  });
  chatHistorySelect.value = "";
  if (loadChatBtn) {
    loadChatBtn.disabled = savedChatHistories.length === 0;
  }
}

async function loadChatHistories() {
  try {
    const response = await fetch(`/api/chat/histories?ts=${Date.now()}`, {
      cache: "no-store",
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const payload = await response.json();
    savedChatHistories = Array.isArray(payload.histories) ? payload.histories : [];
    renderChatHistoryOptions();
  } catch (error) {
    console.error("Failed to load chat histories:", error);
    savedChatHistories = [];
    renderChatHistoryOptions();
    setChatStatus("Failed to load saved chats.", true);
  }
}

async function resetChat() {
  setChatStatus("Ending current chat...");
  if (newChatBtn) {
    newChatBtn.disabled = true;
  }
  try {
    const response = await fetch("/api/chat/archive-reset", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({}),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    setChatStatus(
      payload.archived
        ? "Chat archived to Saved Chats. Fresh conversation ready."
        : "Fresh conversation ready.",
    );
    await loadChatHistories();
  } catch (error) {
    console.error("Failed to reset chat:", error);
    const message = error instanceof Error ? error.message : String(error);
    setChatStatus(`End chat failed: ${message}`, true);
  } finally {
    if (newChatBtn) {
      newChatBtn.disabled = false;
    }
  }
}

async function loadSelectedChatHistory() {
  const fileName = chatHistorySelect?.value || "";
  if (!fileName) {
    setChatStatus("Choose a saved chat first.", true);
    return;
  }
  setChatStatus("Loading saved chat...");
  if (loadChatBtn) {
    loadChatBtn.disabled = true;
  }
  try {
    const response = await fetch("/api/chat/load", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ fileName }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    const selectedHistory = savedChatHistories.find((history) => history.fileName === fileName);
    setChatStatus(
      selectedHistory?.preview
        ? `Loaded chat: ${selectedHistory.preview}`
        : "Loaded saved chat.",
    );
    await loadChatHistories();
  } catch (error) {
    console.error("Failed to load saved chat:", error);
    const message = error instanceof Error ? error.message : String(error);
    setChatStatus(`Load failed: ${message}`, true);
  } finally {
    if (loadChatBtn) {
      loadChatBtn.disabled = savedChatHistories.length === 0;
    }
  }
}

function populatePersonalityPresets(selectedId) {
  if (!personalityPresetSelect) return;
  personalityPresetSelect.innerHTML = "";
  const customOption = document.createElement("option");
  customOption.value = CUSTOM_PERSONALITY_PRESET_ID;
  customOption.textContent = "Custom";
  personalityPresetSelect.appendChild(customOption);
  personalityPresets.forEach((preset) => {
    const option = document.createElement("option");
    option.value = preset.id;
    option.textContent = preset.label;
    personalityPresetSelect.appendChild(option);
  });
  personalityPresetSelect.value = selectedId || CUSTOM_PERSONALITY_PRESET_ID;
}

function populateBotNetModelOptions(selectedValue) {
  if (!botNetModelSelect) return;
  botNetModelSelect.innerHTML = "";
  botNetModelOptions.forEach((item) => {
    const option = document.createElement("option");
    option.value = item.id;
    option.textContent = item.label;
    botNetModelSelect.appendChild(option);
  });
  const fallbackValue = selectedValue || botNetModelOptions[0]?.id || "";
  if (
    fallbackValue &&
    ![...botNetModelSelect.options].some((option) => option.value === fallbackValue)
  ) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = fallbackValue;
    botNetModelSelect.appendChild(option);
  }
  botNetModelSelect.value = fallbackValue;
}

function populateLlmModelOptions(selectedValue) {
  if (!llmModelSelect) return;
  llmModelSelect.innerHTML = "";
  llmModelOptions.forEach((item) => {
    const option = document.createElement("option");
    option.value = item.id;
    option.textContent = item.label;
    llmModelSelect.appendChild(option);
  });
  const fallbackValue = selectedValue || llmModelOptions[0]?.id || "";
  if (
    fallbackValue &&
    ![...llmModelSelect.options].some((option) => option.value === fallbackValue)
  ) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = fallbackValue;
    llmModelSelect.appendChild(option);
  }
  llmModelSelect.value = fallbackValue;
}

function populateRecordTimeoutOptions(selectedValue) {
  if (!recordTimeSelect) return;
  recordTimeSelect.innerHTML = "";
  recordTimeoutOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = `${value} seconds`;
    recordTimeSelect.appendChild(option);
  });
  const fallbackValue = String(selectedValue || 15);
  if (![...recordTimeSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = `${fallbackValue} seconds`;
    recordTimeSelect.appendChild(option);
  }
  recordTimeSelect.value = fallbackValue;
}

function formatVolumeLevelLabel(value) {
  return `${value}/10`;
}

function formatScrollSpeedLabel(value) {
  return `${value}/10`;
}

function populateVolumeLevelOptions(selectedValue) {
  if (!volumeLevelSelect) return;
  volumeLevelSelect.innerHTML = "";
  volumeLevelOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = formatVolumeLevelLabel(value);
    volumeLevelSelect.appendChild(option);
  });
  const fallbackValue = String(
    Number.isFinite(selectedValue) ? selectedValue : 9,
  );
  if (![...volumeLevelSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = formatVolumeLevelLabel(parseInt(fallbackValue, 10));
    volumeLevelSelect.appendChild(option);
  }
  volumeLevelSelect.value = fallbackValue;
}

function populateScrollSpeedOptions(selectedValue) {
  if (!scrollSpeedSelect) return;
  scrollSpeedSelect.innerHTML = "";
  scrollSpeedOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = formatScrollSpeedLabel(value);
    scrollSpeedSelect.appendChild(option);
  });
  const fallbackValue = String(
    Number.isFinite(selectedValue) ? selectedValue : 5,
  );
  if (![...scrollSpeedSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = formatScrollSpeedLabel(parseInt(fallbackValue, 10));
    scrollSpeedSelect.appendChild(option);
  }
  scrollSpeedSelect.value = fallbackValue;
}

function populateHatScrollSpeedOptions(selectedValue) {
  if (!hatScrollSpeedSelect) return;
  hatScrollSpeedSelect.innerHTML = "";
  hatScrollSpeedOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = `${value} - ${formatScrollSpeedLabel(value)}`;
    hatScrollSpeedSelect.appendChild(option);
  });
  const fallbackValue = String(
    Number.isFinite(selectedValue) ? selectedValue : 5,
  );
  if (![...hatScrollSpeedSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = `${fallbackValue} - ${formatScrollSpeedLabel(parseInt(fallbackValue, 10))}`;
    hatScrollSpeedSelect.appendChild(option);
  }
  hatScrollSpeedSelect.value = fallbackValue;
}

function populateHatFontSizeOptions(selectedValue) {
  if (!hatFontSizeSelect) return;
  // The select already has options in HTML, just set the value
  const fallbackValue = selectedValue || "medium";
  if (hatFontSizeSelect.querySelector(`option[value="${fallbackValue}"]`)) {
    hatFontSizeSelect.value = fallbackValue;
  } else {
    hatFontSizeSelect.value = "medium";
  }
}

function populateHatFontFamilyOptions(selectedValue) {
  if (!hatFontFamilySelect) return;
  const fallbackValue = selectedValue || DEFAULT_HAT_FONT_FAMILY;
  if (hatFontFamilySelect.querySelector(`option[value="${fallbackValue}"]`)) {
    hatFontFamilySelect.value = fallbackValue;
  } else {
    hatFontFamilySelect.value = DEFAULT_HAT_FONT_FAMILY;
  }
}

function formatIdleTimeoutLabel(value) {
  return value <= 0 ? "Off" : `${Math.round(value / 60)} minute${value === 60 ? "" : "s"}`;
}

function formatRoomMonitorIntervalLabel(value) {
  if (value <= 0) return "Off";
  if (value < 60) {
    return `${value} second${value === 1 ? "" : "s"}`;
  }
  const minutes = value / 60;
  const displayValue = Number.isInteger(minutes) ? String(minutes) : minutes.toFixed(1);
  return `${displayValue} minute${minutes === 1 ? "" : "s"}`;
}

function populateIdleTimeoutOptions(selectedValue) {
  if (!idleTimeoutSelect) return;
  idleTimeoutSelect.innerHTML = "";
  idleTimeoutOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = formatIdleTimeoutLabel(value);
    idleTimeoutSelect.appendChild(option);
  });
  const fallbackValue = String(
    Number.isFinite(selectedValue) ? selectedValue : DEFAULT_IDLE_TIMEOUT_SEC,
  );
  if (![...idleTimeoutSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = formatIdleTimeoutLabel(parseInt(fallbackValue, 10));
    idleTimeoutSelect.appendChild(option);
  }
  idleTimeoutSelect.value = fallbackValue;
}

function populateScreenBlankTimeoutOptions(selectedValue) {
  if (!screenBlankTimeoutSelect) return;
  screenBlankTimeoutSelect.innerHTML = "";
  screenBlankTimeoutOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = formatIdleTimeoutLabel(value);
    screenBlankTimeoutSelect.appendChild(option);
  });
  const fallbackValue = String(
    Number.isFinite(selectedValue) ? selectedValue : 0,
  );
  if (![...screenBlankTimeoutSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = formatIdleTimeoutLabel(parseInt(fallbackValue, 10));
    screenBlankTimeoutSelect.appendChild(option);
  }
  screenBlankTimeoutSelect.value = fallbackValue;
}

function populateRoomMonitorIntervalOptions(selectedValue) {
  if (!roomMonitorIntervalSelect) return;
  roomMonitorIntervalSelect.innerHTML = "";
  roomMonitorIntervalOptions.forEach((value) => {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = formatRoomMonitorIntervalLabel(value);
    roomMonitorIntervalSelect.appendChild(option);
  });
  const fallbackValue = String(Number.isFinite(selectedValue) ? selectedValue : 0);
  if (![...roomMonitorIntervalSelect.options].some((option) => option.value === fallbackValue)) {
    const option = document.createElement("option");
    option.value = fallbackValue;
    option.textContent = formatRoomMonitorIntervalLabel(parseInt(fallbackValue, 10));
    roomMonitorIntervalSelect.appendChild(option);
  }
  roomMonitorIntervalSelect.value = fallbackValue;
}

function syncPresetSelectionFromPrompt(prompt) {
  if (!personalityPresetSelect) return;
  const match = personalityPresets.find((preset) => preset.prompt === prompt);
  personalityPresetSelect.value = match?.id || CUSTOM_PERSONALITY_PRESET_ID;
}

function applySettings(settings) {
  if (!settings) return;
  populateLlmModelOptions(settings.llmModel || "");
  populatePersonalityPresets(settings.personalityPresetId);
  populateVolumeLevelOptions(settings.volumeLevel || 9);
  populateRecordTimeoutOptions(settings.manualRecordMaxSec || 15);
  populateScrollSpeedOptions(settings.scrollSpeedLevel || 5);
  populateHatScrollSpeedOptions(settings.hatScrollSpeedLevel || 5);
  populateHatFontSizeOptions(settings.hatFontSize || "medium");
  populateHatFontFamilyOptions(settings.hatFontFamily || DEFAULT_HAT_FONT_FAMILY);
  if (hatTextColorSelect) {
    hatTextColorSelect.value = settings.hatTextColor || DEFAULT_HAT_TEXT_COLOR;
  }
  if (personalityInput) {
    personalityInput.value = settings.personalityPrompt || "";
  }
  syncPresetSelectionFromPrompt(settings.personalityPrompt || "");
  const matchingPreset = personalityPresets.find(
    (preset) => preset.prompt === (settings.personalityPrompt || ""),
  );
  if (personalityNameInput) {
    personalityNameInput.value =
      matchingPreset?.id?.startsWith("saved-") ? matchingPreset.label : "";
  }
  if (voiceModeSelect) {
    voiceModeSelect.value = settings.voiceMode || "text-only";
  }
  if (llmModelSelect) {
    llmModelSelect.value = settings.llmModel || llmModelOptions[0]?.id || "";
  }
  if (musicShuffleCheckbox) {
    musicShuffleCheckbox.checked = Boolean(settings.musicShuffle);
  }
  if (uiThemeSelect) {
    uiThemeSelect.value = settings.uiTheme || DEFAULT_UI_THEME;
  }
  if (cameraSourceSelect) {
    cameraSourceSelect.value = settings.cameraSource || DEFAULT_CAMERA_SOURCE;
  }
  if (esp32CamUrlInput) {
    esp32CamUrlInput.value = settings.esp32CamUrl || DEFAULT_ESP32_CAM_URL;
  }
  if (piCameraRotationSelect) {
    piCameraRotationSelect.value = String(settings.piCameraRotationDeg ?? DEFAULT_CAMERA_ROTATION_DEG);
  }
  if (esp32CamRotationSelect) {
    esp32CamRotationSelect.value = String(settings.esp32CamRotationDeg ?? DEFAULT_CAMERA_ROTATION_DEG);
  }
  if (weatherLatitudeInput) {
    weatherLatitudeInput.value =
      settings.weatherLatitude === null || settings.weatherLatitude === undefined
        ? ""
        : String(settings.weatherLatitude);
  }
  if (weatherLongitudeInput) {
    weatherLongitudeInput.value =
      settings.weatherLongitude === null || settings.weatherLongitude === undefined
        ? ""
        : String(settings.weatherLongitude);
  }
  if (headerModeSelect) {
    headerModeSelect.value = settings.headerMode || DEFAULT_HEADER_MODE;
  }
  if (screensaverModeSelect) {
    screensaverModeSelect.value =
      settings.screensaverMode || DEFAULT_SCREENSAVER_MODE;
  }
  populateIdleTimeoutOptions(settings.idleTimeoutSec);
  populateScreenBlankTimeoutOptions(settings.screenBlankTimeoutSec);
  populateRoomMonitorIntervalOptions(settings.roomMonitorIntervalSec);
  applyTheme(settings.uiTheme || DEFAULT_UI_THEME);
  if (groqKeyHint) {
    groqKeyHint.textContent = settings.groqApiKeyConfigured
      ? "Groq key stored"
      : "No key stored";
  }
  if (geminiKeyHint) {
    geminiKeyHint.textContent = settings.geminiApiKeyConfigured
      ? "Gemini key stored"
      : "No key stored";
  }
  if (groqKeyInput) {
    groqKeyInput.value = "";
  }
  if (geminiKeyInput) {
    geminiKeyInput.value = "";
  }
  updateCameraSourceUi();
}

async function loadSettings() {
  try {
    const response = await fetch("/api/settings", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const payload = await response.json();
    personalityPresets = Array.isArray(payload.presets) ? payload.presets : [];
    volumeLevelOptions = Array.isArray(payload.volumeLevelOptions)
      ? payload.volumeLevelOptions
      : [];
    scrollSpeedOptions = Array.isArray(payload.scrollSpeedOptions)
      ? payload.scrollSpeedOptions
      : [];
    hatScrollSpeedOptions = Array.isArray(payload.hatScrollSpeedOptions)
      ? payload.hatScrollSpeedOptions
      : scrollSpeedOptions;
    recordTimeoutOptions = Array.isArray(payload.recordTimeoutOptions)
      ? payload.recordTimeoutOptions
      : [];
    idleTimeoutOptions = Array.isArray(payload.idleTimeoutOptions)
      ? payload.idleTimeoutOptions
      : [];
    screenBlankTimeoutOptions = Array.isArray(payload.screenBlankTimeoutOptions)
      ? payload.screenBlankTimeoutOptions
      : idleTimeoutOptions;
    roomMonitorIntervalOptions = Array.isArray(payload.roomMonitorIntervalOptions)
      ? payload.roomMonitorIntervalOptions
      : [];
    llmModelOptions = Array.isArray(payload.llmModelOptions)
      ? payload.llmModelOptions
      : [];
    applySettings(payload.settings || {});
    settingsLoaded = true;
    setSettingsStatus("Settings ready.");
  } catch (error) {
    console.error("Failed to load settings:", error);
    setSettingsStatus("Failed to load settings.", true);
  }
}

async function loadVisionPreview() {
  if (!visionPreview) return;
  try {
    const response = await fetch(`/api/vision/image?ts=${Date.now()}`, {
      cache: "no-store",
    });
    if (!response.ok) {
      visionPreview.style.display = "none";
      setVisionStatus("No image uploaded.");
      return;
    }
    visionPreview.src = `/api/vision/image?ts=${Date.now()}`;
    visionPreview.style.display = "block";
    setVisionStatus("Latest image ready for vision questions.");
  } catch (error) {
    console.error("Failed to load vision preview:", error);
    visionPreview.style.display = "none";
    setVisionStatus("Failed to load latest image.", true);
  }
}

async function loadVisionAnalysis() {
  try {
    const response = await fetch(`/api/vision/analysis?ts=${Date.now()}`, {
      cache: "no-store",
    });
    if (!response.ok) {
      return;
    }
    const payload = await response.json();
    const analysis = payload.analysis || null;
    const updatedAt = Number(analysis?.updatedAt || 0);
    if (!analysis && latestVisionAnalysisStamp === 0) {
      return;
    }
    if (!analysis || updatedAt !== latestVisionAnalysisStamp) {
      renderVisionAnalysis(analysis);
    }
  } catch (error) {
    console.error("Failed to load Gemini vision analysis:", error);
  }
}

function readFileAsDataUrl(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result || ""));
    reader.onerror = () => reject(reader.error || new Error("Failed to read file."));
    reader.readAsDataURL(file);
  });
}

function parseOptionalFloat(value) {
  const trimmed = String(value || "").trim();
  if (!trimmed) return null;
  const numeric = parseFloat(trimmed);
  return Number.isFinite(numeric) ? numeric : null;
}

function setMusicStatus(message, isError = false) {
  if (!musicStatus) return;
  musicStatus.textContent = message;
  musicStatus.style.color = isError ? "#ff8a8a" : "";
}

function formatBytes(value) {
  const units = ["B", "KB", "MB", "GB", "TB"];
  let size = Math.max(0, Number(value) || 0);
  let unitIndex = 0;
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex += 1;
  }
  return `${size >= 100 || unitIndex === 0 ? Math.round(size) : size.toFixed(1)} ${units[unitIndex]}`;
}

function setRoomMonitorGalleryStatus(message, isError = false) {
  if (!roomMonitorStatus) return;
  roomMonitorStatus.textContent = message;
  roomMonitorStatus.style.color = isError ? "#ff8a8a" : "";
}

function renderRoomMonitorPhotos(status = null) {
  roomMonitorGalleryState = status || roomMonitorGalleryState;
  const currentStatus = roomMonitorGalleryState;
  if (!roomMonitorGalleryList) return;
  roomMonitorGalleryList.innerHTML = "";

  if (!roomMonitorPhotos.length) {
    const intervalLabel = formatRoomMonitorIntervalLabel(currentStatus?.intervalSec || 0);
    const captureMessage = currentStatus?.enabled
      ? `Auto capture ${intervalLabel}. No room monitor images yet.`
      : "Room monitor is off. No room monitor images yet.";
    setRoomMonitorGalleryStatus(
      currentStatus?.lastError ? `${captureMessage} Last error: ${currentStatus.lastError}` : captureMessage,
      Boolean(currentStatus?.lastError),
    );
    const empty = document.createElement("div");
    empty.className = "saved-photos-empty";
    empty.textContent = "No room monitor images yet.";
    roomMonitorGalleryList.appendChild(empty);
    if (roomMonitorToggleBtn) {
      roomMonitorToggleBtn.disabled = true;
      roomMonitorToggleBtn.textContent = "Gallery";
    }
    return;
  }

  const visiblePhotos = showAllRoomMonitorPhotos
    ? roomMonitorPhotos
    : roomMonitorPhotos.slice(0, 4);
  const usageLabel = `${formatBytes(currentStatus?.totalSizeBytes || 0)} used`;
  const freeSpaceLabel = `${formatBytes(currentStatus?.freeSpaceBytes || 0)} free`;
  const reserveLabel = `keeps ${formatBytes(currentStatus?.freeSpaceReserveBytes || 0)} open`;
  const intervalLabel = formatRoomMonitorIntervalLabel(currentStatus?.intervalSec || 0);
  let summary = `${currentStatus?.enabled ? `Auto ${intervalLabel}` : "Auto off"} · ${roomMonitorPhotos.length} image${roomMonitorPhotos.length === 1 ? "" : "s"} · ${usageLabel} · ${freeSpaceLabel} · ${reserveLabel}`;
  if (currentStatus?.captureInProgress) {
    summary += " · Capturing now";
  } else if (currentStatus?.lastCaptureAt) {
    summary += ` · Last capture ${new Date(currentStatus.lastCaptureAt).toLocaleString()}`;
  }
  if (currentStatus?.lastError) {
    summary += ` · Last error: ${currentStatus.lastError}`;
  }
  setRoomMonitorGalleryStatus(summary, Boolean(currentStatus?.lastError));
  if (roomMonitorToggleBtn) {
    roomMonitorToggleBtn.disabled = roomMonitorPhotos.length <= 4;
    roomMonitorToggleBtn.textContent =
      roomMonitorPhotos.length <= 4
        ? "Gallery"
        : showAllRoomMonitorPhotos
          ? "Recent Only"
          : `Gallery (${roomMonitorPhotos.length})`;
  }

  for (const photo of visiblePhotos) {
    const card = document.createElement("div");
    card.className = "saved-photo-card";

    const img = document.createElement("img");
    img.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
    img.alt = photo.fileName;

    const label = document.createElement("div");
    label.className = "saved-photo-name";
    label.textContent = photo.fileName;

    const meta = document.createElement("div");
    meta.className = "saved-photo-meta";
    meta.textContent = `${new Date(photo.updatedAt).toLocaleString()} · ${formatBytes(photo.sizeBytes)}`;

    const actions = document.createElement("div");
    actions.className = "saved-photo-actions";

    const downloadLink = document.createElement("a");
    downloadLink.className = "button compact saved-photo-download";
    downloadLink.textContent = "Download";
    downloadLink.href = photo.imageUrl;
    downloadLink.download = photo.fileName;

    card.appendChild(img);
    card.appendChild(label);
    card.appendChild(meta);
    actions.appendChild(downloadLink);
    card.appendChild(actions);
    roomMonitorGalleryList.appendChild(card);
  }
}

async function loadRoomMonitorPhotos() {
  try {
    const response = await fetch(`/api/room-monitor/photos?ts=${Date.now()}`, {
      cache: "no-store",
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const payload = await response.json();
    roomMonitorPhotos = Array.isArray(payload.photos) ? payload.photos : [];
    renderRoomMonitorPhotos(payload.status || null);
  } catch (error) {
    console.error("Failed to load room monitor gallery:", error);
    roomMonitorPhotos = [];
    roomMonitorGalleryState = null;
    renderRoomMonitorPhotos(null);
    setRoomMonitorGalleryStatus("Failed to load room monitor gallery.", true);
  }
}

function renderMusicTracks() {
  if (!musicTrackList) return;
  if (!musicTracks.length) {
    musicTrackList.innerHTML = "";
    return;
  }
  musicTrackList.innerHTML = musicTracks
    .map(
      (track) => `
        <div class="music-track-row${track.current ? " current" : ""}">
          <span class="music-track-title">${track.title || track.fileName}</span>
          <span class="music-track-badge">${track.current ? "Now Playing" : "MP3"}</span>
        </div>
      `,
    )
    .join("");
}

function applyMusicPayload(payload) {
  const music = payload?.music || payload || {};
  musicTracks = Array.isArray(music.tracks) ? music.tracks : [];
  if (musicShuffleCheckbox) {
    musicShuffleCheckbox.checked = Boolean(music.musicShuffle);
  }
  renderMusicTracks();
  if (musicTracks.length === 0) {
    setMusicStatus("No uploaded MP3s yet.");
    return;
  }
  if (music.isPlaying && music.currentTrackTitle) {
    setMusicStatus(`Playing: ${music.currentTrackTitle}`);
    return;
  }
  if (music.currentTrackTitle) {
    setMusicStatus(`Ready: ${music.currentTrackTitle}`);
    return;
  }
  setMusicStatus(`${musicTracks.length} MP3${musicTracks.length === 1 ? "" : "s"} ready.`);
}

async function loadMusicLibrary() {
  try {
    const response = await fetch("/api/music/tracks", { cache: "no-store" });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    applyMusicPayload(payload);
  } catch (error) {
    console.error("Failed to load music library:", error);
    const message = error instanceof Error ? error.message : String(error);
    setMusicStatus(`Failed to load MP3 library: ${message}`, true);
  }
}

async function uploadMusicFile() {
  const file = musicFileInput?.files?.[0];
  if (!file) {
    setMusicStatus("Choose an MP3 first.", true);
    return;
  }
  if (!/\.mp3$/i.test(file.name) || !/^audio\/(?:mpeg|mp3)$/i.test(file.type || "audio/mpeg")) {
    setMusicStatus("Only MP3 files are supported.", true);
    return;
  }
  setMusicStatus("Uploading MP3...");
  if (musicUploadBtn) {
    musicUploadBtn.disabled = true;
  }
  try {
    const dataUrl = await readFileAsDataUrl(file);
    const response = await fetch("/api/music/upload", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        fileName: file.name,
        dataUrl,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    if (musicFileInput) {
      musicFileInput.value = "";
    }
    applyMusicPayload(payload);
    setMusicStatus(`Uploaded: ${payload.fileName || file.name}`);
  } catch (error) {
    console.error("Failed to upload MP3:", error);
    const message = error instanceof Error ? error.message : String(error);
    setMusicStatus(`MP3 upload failed: ${message}`, true);
  } finally {
    if (musicUploadBtn) {
      musicUploadBtn.disabled = false;
    }
  }
}

async function sendMusicControl(action) {
  try {
    const response = await fetch("/api/music/control", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ action }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || payload.message || `HTTP ${response.status}`);
    }
    applyMusicPayload(payload);
    setMusicStatus(payload.message || "Music updated.");
  } catch (error) {
    console.error("Failed to control music:", error);
    const message = error instanceof Error ? error.message : String(error);
    setMusicStatus(`Music control failed: ${message}`, true);
  }
}

async function uploadVisionImage() {
  const file = visionImageInput?.files?.[0];
  if (!file) {
    setVisionStatus("Choose an image first.", true);
    return;
  }
  setVisionStatus("Uploading image...");
  if (visionUploadBtn) {
    visionUploadBtn.disabled = true;
  }
  try {
    const dataUrl = await readFileAsDataUrl(file);
    const response = await fetch("/api/vision/upload", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        fileName: file.name,
        contentType: file.type,
        dataUrl,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    if (visionPreview) {
      visionPreview.src = payload.imageUrl || `/api/vision/image?ts=${Date.now()}`;
      visionPreview.style.display = "block";
    }
    if (visionImageInput) {
      visionImageInput.value = "";
    }
    renderVisionAnalysis(null);
    loadSavedPhotos();
    setVisionStatus("Image uploaded. Ask the bot what it sees.");
  } catch (error) {
    console.error("Failed to upload vision image:", error);
    const message = error instanceof Error ? error.message : String(error);
    setVisionStatus(`Upload failed: ${message}`, true);
  } finally {
    if (visionUploadBtn) {
      visionUploadBtn.disabled = false;
    }
  }
}

async function captureVisionImage() {
  setVisionStatus("Capturing image...");
  if (visionCaptureBtn) {
    visionCaptureBtn.disabled = true;
  }
  try {
    const response = await fetch("/api/vision/capture", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    if (visionPreview) {
      visionPreview.src = payload.imageUrl || `/api/vision/image?ts=${Date.now()}`;
      visionPreview.style.display = "block";
    }
    renderVisionAnalysis(null);
    loadSavedPhotos();
    setVisionStatus("Camera image captured. Ask the bot what it sees.");
  } catch (error) {
    console.error("Failed to capture camera image:", error);
    const message = error instanceof Error ? error.message : String(error);
    setVisionStatus(`Capture failed: ${message}`, true);
  } finally {
    if (visionCaptureBtn) {
      visionCaptureBtn.disabled = false;
    }
  }
}

async function saveSettings({ clearGroqApiKey = false } = {}) {
  if (!settingsLoaded) {
    setSettingsStatus("Settings are still loading.", true);
    return;
  }

  const body = {
    groqApiKey: clearGroqApiKey ? "" : (groqKeyInput?.value || "").trim(),
    clearGroqApiKey,
    geminiApiKey: (geminiKeyInput?.value || "").trim(),
    llmModel: llmModelSelect?.value || llmModelOptions[0]?.id || "",
    personalityPrompt: (personalityInput?.value || "").trim(),
    voiceMode: voiceModeSelect?.value || "text-only",
    musicShuffle: Boolean(musicShuffleCheckbox?.checked),
    volumeLevel: parseInt(volumeLevelSelect?.value || "9", 10),
    manualRecordMaxSec: parseInt(recordTimeSelect?.value || "15", 10),
    scrollSpeedLevel: parseInt(scrollSpeedSelect?.value || "5", 10),
    hatScrollSpeedLevel: parseInt(hatScrollSpeedSelect?.value || "5", 10),
    hatFontSize: hatFontSizeSelect?.value || "medium",
    hatFontFamily: hatFontFamilySelect?.value || DEFAULT_HAT_FONT_FAMILY,
    hatTextColor: hatTextColorSelect?.value || DEFAULT_HAT_TEXT_COLOR,
    uiTheme: uiThemeSelect?.value || DEFAULT_UI_THEME,
    cameraSource: cameraSourceSelect?.value || DEFAULT_CAMERA_SOURCE,
    esp32CamUrl: (esp32CamUrlInput?.value || DEFAULT_ESP32_CAM_URL).trim(),
    piCameraRotationDeg: parseInt(
      piCameraRotationSelect?.value || DEFAULT_CAMERA_ROTATION_DEG,
      10,
    ),
    esp32CamRotationDeg: parseInt(
      esp32CamRotationSelect?.value || DEFAULT_CAMERA_ROTATION_DEG,
      10,
    ),
    weatherLatitude: parseOptionalFloat(weatherLatitudeInput?.value),
    weatherLongitude: parseOptionalFloat(weatherLongitudeInput?.value),
    headerMode: headerModeSelect?.value || DEFAULT_HEADER_MODE,
    screensaverMode:
      screensaverModeSelect?.value || DEFAULT_SCREENSAVER_MODE,
    idleTimeoutSec: parseInt(
      idleTimeoutSelect?.value || String(DEFAULT_IDLE_TIMEOUT_SEC),
      10,
    ),
    screenBlankTimeoutSec: parseInt(
      screenBlankTimeoutSelect?.value || "0",
      10,
    ),
    roomMonitorIntervalSec: parseInt(
      roomMonitorIntervalSelect?.value || "0",
      10,
    ),
  };

  setSettingsStatus(clearGroqApiKey ? "Clearing key..." : "Saving settings...");

  try {
    const response = await fetch("/api/settings", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(body),
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const payload = await response.json();
    personalityPresets = Array.isArray(payload.presets)
      ? payload.presets
      : personalityPresets;
    applySettings(payload.settings || {});
    loadRoomMonitorPhotos();
    setSettingsStatus(
      clearGroqApiKey
        ? "Stored Groq key cleared."
        : "Settings saved. New replies will use the updated personality and stored keys.",
    );
  } catch (error) {
    console.error("Failed to save settings:", error);
    setSettingsStatus("Failed to save settings.", true);
  }
}

async function savePersonalityPreset() {
  if (!settingsLoaded) {
    setSettingsStatus("Settings are still loading.", true);
    return;
  }
  const name = (personalityNameInput?.value || "").trim();
  const prompt = (personalityInput?.value || "").trim();
  if (!name) {
    setSettingsStatus("Enter a personality name first.", true);
    return;
  }
  if (!prompt) {
    setSettingsStatus("Enter a personality prompt first.", true);
    return;
  }

  setSettingsStatus("Saving personality...");
  if (savePersonalityBtn) {
    savePersonalityBtn.disabled = true;
  }
  try {
    const response = await fetch("/api/settings/personality-save", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ name, prompt }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    personalityPresets = Array.isArray(payload.presets)
      ? payload.presets
      : personalityPresets;
    applySettings(payload.settings || {});
    setSettingsStatus(`Saved personality: ${name}.`);
  } catch (error) {
    console.error("Failed to save personality preset:", error);
    const message = error instanceof Error ? error.message : String(error);
    setSettingsStatus(message || "Failed to save personality.", true);
  } finally {
    if (savePersonalityBtn) {
      savePersonalityBtn.disabled = false;
    }
  }
}

async function requestShutdown() {
  if (!settingsLoaded) {
    setSettingsStatus("Settings are still loading.", true);
    return;
  }

  const confirmed = window.confirm(
    "Shut the Pi down now? Wait for the device to fully power off before unplugging it.",
  );
  if (!confirmed) {
    return;
  }

  setSettingsStatus("Requesting shutdown...");
  if (shutdownBtn) {
    shutdownBtn.disabled = true;
  }

  try {
    const response = await fetch("/api/system/shutdown", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    setSettingsStatus("Shutdown requested. The Pi should power off shortly.");
  } catch (error) {
    console.error("Failed to request shutdown:", error);
    const message = error instanceof Error ? error.message : String(error);
    if (error instanceof TypeError || /Failed to fetch/i.test(message)) {
      setSettingsStatus(
        "Shutdown may already be in progress. The browser connection dropped while the Pi was powering off.",
      );
      return;
    }
    setSettingsStatus(
      `Shutdown failed: ${message}`,
      true,
    );
    if (shutdownBtn) {
      shutdownBtn.disabled = false;
    }
  }
}

connectWebSocket();
loadSettings();
loadBotNetState();
loadMusicLibrary();
loadVisionPreview();
loadVisionAnalysis();
loadSavedPhotos();
loadRoomMonitorPhotos();
loadChatHistories();
setVisionAnalysisVisible(false);
requestAnimationFrame(animateScroll);
setInterval(loadVisionAnalysis, 3000);
setInterval(loadBotNetState, 5000);

personalityPresetSelect?.addEventListener("change", () => {
  const selectedId = personalityPresetSelect.value;
  if (selectedId === CUSTOM_PERSONALITY_PRESET_ID) {
    if (personalityNameInput) {
      personalityNameInput.value = "";
    }
    return;
  }
  const preset = personalityPresets.find((item) => item.id === selectedId);
  if (!preset || !personalityInput) {
    return;
  }
  personalityInput.value = preset.prompt;
  if (personalityNameInput) {
    personalityNameInput.value = preset.id.startsWith("saved-") ? preset.label : "";
  }
  setSettingsStatus(`${preset.label} preset loaded. Save to apply.`);
});

personalityInput?.addEventListener("input", () => {
  syncPresetSelectionFromPrompt(personalityInput.value.trim());
});

function setPressed(value) {
  isPressed = value;
  btnText.textContent = isPressed ? "pressed" : "released";
}

const press = () => {
  if (isPressed) return;
  setPressed(true);
  sendButton("press");
};
const release = () => {
  if (!isPressed) return;
  setPressed(false);
  sendButton("release");
};

btn.addEventListener("pointerdown", (event) => {
  event.preventDefault();
  activePointerId = event.pointerId;
  try {
    btn.setPointerCapture(event.pointerId);
  } catch {}
  press();
});

btn.addEventListener("pointerup", (event) => {
  if (activePointerId !== null && event.pointerId !== activePointerId) return;
  release();
  activePointerId = null;
});

btn.addEventListener("pointercancel", (event) => {
  if (activePointerId !== null && event.pointerId !== activePointerId) return;
  release();
  activePointerId = null;
});

btn.addEventListener("lostpointercapture", () => {
  release();
  activePointerId = null;
});

window.addEventListener("pointerup", (event) => {
  if (activePointerId !== null && event.pointerId !== activePointerId) return;
  release();
  activePointerId = null;
});

if (saveSettingsBtn) {
  saveSettingsBtn.addEventListener("click", () => {
    saveSettings();
  });
}

if (savePersonalityBtn) {
  savePersonalityBtn.addEventListener("click", () => {
    savePersonalityPreset();
  });
}

if (botNetSaveBtn) {
  botNetSaveBtn.addEventListener("click", () => {
    saveBotNetSettings().catch((error) => {
      console.error("Failed to save BotNet settings:", error);
      setBotNetStatus(
        error instanceof Error ? error.message : "Failed to save BotNet settings.",
        true,
      );
    });
  });
}

[
  botNetEnabledCheckbox,
  botNetModeSelect,
  botNetModelSelect,
  botNetTransportSelect,
  botNetNodeHandleInput,
  botNetHubUrlInput,
  botNetPeerUrlInput,
  botNetPublicUrlInput,
  botNetMaxRepliesInput,
  botNetReplyDelayInput,
].forEach((element) => {
  element?.addEventListener("input", () => {
    botNetSettingsDirty = true;
  });
  element?.addEventListener("change", () => {
    botNetSettingsDirty = true;
  });
});

if (botNetTransportSelect) {
  botNetTransportSelect.addEventListener("change", () => {
    updateBotNetTransportUi(botNetTransportSelect.value || "lan-direct");
  });
}

if (botNetTestBtn) {
  botNetTestBtn.addEventListener("click", () => {
    testBotNetConnection();
  });
}

if (botNetRegisterBtn) {
  botNetRegisterBtn.addEventListener("click", () => {
    registerBotNetNode();
  });
}

if (botNetConnectBtn) {
  botNetConnectBtn.addEventListener("click", () => {
    connectBotNetHub();
  });
}

if (botNetInviteBtn) {
  botNetInviteBtn.addEventListener("click", () => {
    createBotNetInvite();
  });
}

if (botNetRedeemBtn) {
  botNetRedeemBtn.addEventListener("click", () => {
    redeemBotNetInvite();
  });
}

if (botNetDisconnectBtn) {
  botNetDisconnectBtn.addEventListener("click", () => {
    disconnectBotNetHub();
  });
}

if (botNetStartBtn) {
  botNetStartBtn.addEventListener("click", () => {
    startBotNetConversation("self");
  });
}

if (botNetPeerStartBtn) {
  botNetPeerStartBtn.addEventListener("click", () => {
    startBotNetConversation("peer");
  });
}

if (botNetStopBtn) {
  botNetStopBtn.addEventListener("click", () => {
    stopBotNetConversation();
  });
}

if (visionUploadBtn) {
  visionUploadBtn.addEventListener("click", () => {
    uploadVisionImage();
  });
}

if (musicUploadBtn) {
  musicUploadBtn.addEventListener("click", () => {
    uploadMusicFile();
  });
}

if (musicPlayBtn) {
  musicPlayBtn.addEventListener("click", () => {
    sendMusicControl("play");
  });
}

if (musicStopBtn) {
  musicStopBtn.addEventListener("click", () => {
    sendMusicControl("stop");
  });
}

if (musicPrevBtn) {
  musicPrevBtn.addEventListener("click", () => {
    sendMusicControl("previous");
  });
}

if (musicNextBtn) {
  musicNextBtn.addEventListener("click", () => {
    sendMusicControl("next");
  });
}

if (musicShuffleCheckbox) {
  musicShuffleCheckbox.addEventListener("change", () => {
    setSettingsStatus("Shuffle changed. Save settings to keep it.");
  });
}

if (visionCaptureBtn) {
  visionCaptureBtn.addEventListener("click", () => {
    captureVisionImage();
  });
}

if (visionAnalysisToggleBtn) {
  visionAnalysisToggleBtn.addEventListener("click", () => {
    setVisionAnalysisVisible(!visionAnalysisVisible);
    if (visionAnalysisVisible) {
      loadVisionAnalysis();
    }
  });
}

if (savedPhotosToggleBtn) {
  savedPhotosToggleBtn.addEventListener("click", () => {
    if (savedPhotos.length <= 4) {
      return;
    }
    showAllSavedPhotos = !showAllSavedPhotos;
    renderSavedPhotos();
  });
}

if (roomMonitorToggleBtn) {
  roomMonitorToggleBtn.addEventListener("click", () => {
    if (roomMonitorPhotos.length <= 4) {
      return;
    }
    showAllRoomMonitorPhotos = !showAllRoomMonitorPhotos;
    renderRoomMonitorPhotos();
  });
}

if (newChatBtn) {
  newChatBtn.addEventListener("click", () => {
    resetChat();
  });
}

if (loadChatBtn) {
  loadChatBtn.addEventListener("click", () => {
    loadSelectedChatHistory();
  });
}

if (shutdownBtn) {
  shutdownBtn.addEventListener("click", () => {
    requestShutdown();
  });
}

if (uiThemeSelect) {
  uiThemeSelect.addEventListener("change", () => {
    applyTheme(uiThemeSelect.value || DEFAULT_UI_THEME);
  });
}

if (cameraSourceSelect) {
  cameraSourceSelect.addEventListener("change", () => {
    updateCameraSourceUi();
  });
}

if (clearKeyBtn) {
  clearKeyBtn.addEventListener("click", () => {
    saveSettings({ clearGroqApiKey: true });
  });
}

// ── Web Audio Recording ──────────────────────────────────────────────────────
// When WEB_AUDIO_ENABLED=true on the server, it sends "start_record" /
// "stop_record" commands here. The browser captures with MediaRecorder and
// streams binary frames (prefix byte 0x01) back to the server.

const FRAME_AUDIO   = 0x01;
const FRAME_CAM_LIVE = 0x02;
const FRAME_CAM_CAPTURE = 0x03;

let mediaRecorder = null;
let audioStream = null;
let audioStartInProgress = false;
let stopRequestedBeforeStart = false;
let pendingAudioChunkSends = [];

async function startWebAudioRecording() {
  if (audioStartInProgress) return;
  if (mediaRecorder && mediaRecorder.state !== "inactive") return;
  stopRequestedBeforeStart = false;
  audioStartInProgress = true;
  pendingAudioChunkSends = [];
  updateMicIndicator(true);
  try {
    audioStream = await navigator.mediaDevices.getUserMedia({ audio: true });

    if (stopRequestedBeforeStart) {
      if (audioStream) {
        audioStream.getTracks().forEach((t) => t.stop());
        audioStream = null;
      }
      updateMicIndicator(false);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: "record_complete" }));
      }
      return;
    }

    const preferredMimes = [
      "audio/webm;codecs=opus",
      "audio/ogg;codecs=opus",
      "audio/webm",
    ];
    const mimeType = preferredMimes.find((m) => MediaRecorder.isTypeSupported(m)) || "";
    const options = mimeType ? { mimeType } : {};

    mediaRecorder = new MediaRecorder(audioStream, options);

    mediaRecorder.ondataavailable = (event) => {
      if (!event.data || event.data.size === 0) return;
      if (!ws || ws.readyState !== WebSocket.OPEN) return;
      const sendTask = event.data.arrayBuffer().then((buf) => {
        const payload = new Uint8Array(1 + buf.byteLength);
        payload[0] = FRAME_AUDIO;
        payload.set(new Uint8Array(buf), 1);
        ws.send(payload);
      });
      pendingAudioChunkSends.push(sendTask);
      sendTask.finally(() => {
        pendingAudioChunkSends = pendingAudioChunkSends.filter(
          (task) => task !== sendTask,
        );
      });
    };

    mediaRecorder.onstop = () => {
      updateMicIndicator(false);
      if (audioStream) {
        audioStream.getTracks().forEach((t) => t.stop());
        audioStream = null;
      }
      mediaRecorder = null;
      Promise.allSettled(pendingAudioChunkSends).finally(() => {
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ type: "record_complete" }));
        }
      });
    };

    // Emit data chunks every 500 ms so the server can monitor progress.
    mediaRecorder.start(500);
    if (stopRequestedBeforeStart && mediaRecorder.state !== "inactive") {
      mediaRecorder.stop();
    }
  } catch (e) {
    console.error("[WebAudio] getUserMedia (audio) failed:", e);
    updateMicIndicator(false);
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "record_complete" }));
    }
  } finally {
    audioStartInProgress = false;
  }
}

function stopWebAudioRecording() {
  stopRequestedBeforeStart = true;
  if (mediaRecorder && mediaRecorder.state !== "inactive") {
    mediaRecorder.stop();
  }
}

function updateMicIndicator(active) {
  const el = document.getElementById("micIndicator");
  if (el) el.style.display = active ? "block" : "none";
  refreshWebAudioCard();
}

function refreshWebAudioCard() {
  const card = document.getElementById("webAudioCard");
  if (!card) return;
  const mic = document.getElementById("micIndicator");
  const cam = document.getElementById("camIndicator");
  const anyActive =
    (mic && mic.style.display !== "none") ||
    (cam && cam.style.display !== "none");
  card.style.display = anyActive ? "block" : "none";
}

// ── Web Audio Playback (queued) ───────────────────────────────────────────────
// When WEB_AUDIO_ENABLED=true, the server sends "play_audio" with base64 data.
// We queue incoming audio and play chunks sequentially via Web Audio API.
// This prevents overlapping playback on browsers where onended is unreliable
// (e.g. Chromium on Raspberry Pi).

let audioCtx = null;
let currentAudioSource = null;
let audioQueue = [];
let isProcessingAudio = false;
let playbackFallbackTimer = null;
let currentPlayId = null;
let currentAudioResolver = null;
let audioPlaybackGeneration = 0;

function ensureAudioContext() {
  if (!audioCtx || audioCtx.state === "closed") {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  }
  return audioCtx;
}

function sendPlayComplete(playId) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const msg = { type: "play_complete" };
    if (playId !== undefined && playId !== null) msg.playId = playId;
    ws.send(JSON.stringify(msg));
  }
}

// Stop the current source WITHOUT sending play_complete (internal use).
function stopWebAudioSilent() {
  if (playbackFallbackTimer) {
    clearTimeout(playbackFallbackTimer);
    playbackFallbackTimer = null;
  }
  if (currentAudioSource) {
    // Remove onended BEFORE stop() to prevent spurious play_complete.
    currentAudioSource.onended = null;
    try { currentAudioSource.stop(); } catch {}
    try { currentAudioSource.disconnect(); } catch {}
    currentAudioSource = null;
  }
}

function playWebAudio(base64Data, format, duration, playId) {
  audioQueue.push({ base64Data, format, duration, playId });
  if (!isProcessingAudio) {
    processAudioQueue();
  }
}

async function processAudioQueue() {
  if (isProcessingAudio) return;
  isProcessingAudio = true;
  while (audioQueue.length > 0) {
    const item = audioQueue.shift();
    await playWebAudioItem(item.base64Data, item.format, item.duration, item.playId);
  }
  isProcessingAudio = false;
}

function playWebAudioItem(base64Data, _format, duration, playId) {
  return new Promise(async (resolve) => {
    const generation = audioPlaybackGeneration;
    currentPlayId = playId;
    let finished = false;
    const finish = (notifyServer) => {
      if (finished) return;
      finished = true;
      if (currentAudioResolver === cancelPlayback) {
        currentAudioResolver = null;
      }
      if (playbackFallbackTimer) {
        clearTimeout(playbackFallbackTimer);
        playbackFallbackTimer = null;
      }
      if (currentAudioSource) {
        try { currentAudioSource.onended = null; } catch {}
        try { currentAudioSource.disconnect(); } catch {}
        currentAudioSource = null;
      }
      if (notifyServer && generation === audioPlaybackGeneration) {
        sendPlayComplete(playId);
      }
      resolve();
    };
    const cancelPlayback = () => finish(false);
    currentAudioResolver = cancelPlayback;
    try {
      const ctx = ensureAudioContext();
      if (ctx.state === "suspended") {
        await ctx.resume();
      }
      const binary = atob(base64Data);
      const bytes = new Uint8Array(binary.length);
      for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
      const decoded = await ctx.decodeAudioData(bytes.buffer.slice(0));

      if (generation !== audioPlaybackGeneration) {
        finish(false);
        return;
      }

      stopWebAudioSilent();
      currentAudioSource = ctx.createBufferSource();
      currentAudioSource.buffer = decoded;
      currentAudioSource.connect(ctx.destination);

      const onFinished = () => {
        finish(true);
      };

      currentAudioSource.onended = onFinished;

      // Fallback timer: use decoded buffer duration (most accurate) with a margin.
      // This covers browsers where onended does not fire reliably.
      const bufferMs = decoded.duration * 1000;
      const fallbackMs = Math.max(bufferMs, duration || 0) + 2000;
      playbackFallbackTimer = setTimeout(() => {
        console.warn("[WebAudio] Fallback timer fired — onended did not fire");
        playbackFallbackTimer = null;
        if (!finished) {
          stopWebAudioSilent();
          finish(true);
        }
      }, fallbackMs);

      currentAudioSource.start(0);
    } catch (e) {
      console.error("[WebAudio] Playback failed:", e);
      finish(true);
    }
  });
}

// Stop playback, clear queue, and notify server (used by "stop_audio" command).
function stopWebAudio() {
  audioPlaybackGeneration += 1;
  audioQueue.length = 0;
  stopWebAudioSilent();
  if (currentAudioResolver) {
    const resolveCurrentAudio = currentAudioResolver;
    currentAudioResolver = null;
    resolveCurrentAudio();
  }
}

// ── Web Camera Streaming ─────────────────────────────────────────────────────
// When WEB_CAMERA_ENABLED=true, the server sends "start_camera_stream" and
// "stop_camera_stream" commands. We capture from getUserMedia and stream JPEG
// frames (prefix byte 0x02). For single captures, prefix byte 0x03 is used.

let webCamStream = null;
let webCamVideo = null;
let webCamCanvas = null;
let webCamSendTimer = null;

async function startWebCameraStream() {
  if (webCamStream) return;
  try {
    webCamStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: "environment", width: { ideal: 640 }, height: { ideal: 480 } },
    });
    if (!webCamVideo) {
      webCamVideo = document.createElement("video");
      webCamVideo.autoplay = true;
      webCamVideo.muted = true;
      webCamVideo.playsInline = true;
      webCamCanvas = document.createElement("canvas");
    }
    webCamVideo.srcObject = webCamStream;
    await webCamVideo.play().catch(() => {});
    updateCamIndicator(true);
    webCamSendTimer = setInterval(() => sendWebCameraFrameInternal(false), 200);
  } catch (e) {
    console.error("[WebCamera] getUserMedia (video) failed:", e);
    webCamStream = null;
  }
}

function stopWebCameraStream() {
  if (webCamSendTimer) { clearInterval(webCamSendTimer); webCamSendTimer = null; }
  if (webCamStream) { webCamStream.getTracks().forEach((t) => t.stop()); webCamStream = null; }
  updateCamIndicator(false);
}

function sendWebCameraCapture() {
  sendWebCameraFrameInternal(true);
}

function sendWebCameraFrameInternal(isCapture) {
  if (!webCamVideo || !webCamCanvas || !ws || ws.readyState !== WebSocket.OPEN) return;
  const w = webCamVideo.videoWidth || 640;
  const h = webCamVideo.videoHeight || 480;
  webCamCanvas.width = w;
  webCamCanvas.height = h;
  const ctx2d = webCamCanvas.getContext("2d");
  ctx2d.drawImage(webCamVideo, 0, 0, w, h);
  const quality = isCapture ? 0.95 : 0.75;
  webCamCanvas.toBlob(
    (blob) => {
      if (!blob || !ws || ws.readyState !== WebSocket.OPEN) return;
      blob.arrayBuffer().then((buf) => {
        const prefixByte = isCapture ? FRAME_CAM_CAPTURE : FRAME_CAM_LIVE;
        const payload = new Uint8Array(1 + buf.byteLength);
        payload[0] = prefixByte;
        payload.set(new Uint8Array(buf), 1);
        ws.send(payload);
      });
    },
    "image/jpeg",
    quality,
  );
}

function updateCamIndicator(active) {
  const el = document.getElementById("camIndicator");
  if (el) el.style.display = active ? "block" : "none";
  refreshWebAudioCard();
}

// ── Text Input ───────────────────────────────────────────────────────────────
// When the device is in "idle" (sleep) state, allow the user to type a message
// and send it directly to the LLM, bypassing ASR.

const textInput = document.getElementById("textInput");
const textSendBtn = document.getElementById("textSendBtn");
let currentDeviceStatus = "";

function updateTextInputState(enabled, status) {
  currentDeviceStatus = status || "";
  const isEnabled =
    typeof enabled === "boolean"
      ? enabled
      : currentDeviceStatus === "idle" || currentDeviceStatus === "starting";
  textInput.disabled = !isEnabled;
  textSendBtn.disabled = !isEnabled;
}

async function sendTextInput() {
  const text = (textInput.value || "").trim();
  if (!text) return;
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify({ type: "text_input", text }));
  textInput.value = "";
}

textSendBtn.addEventListener("click", sendTextInput);
textInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey && !e.isComposing) {
    e.preventDefault();
    sendTextInput();
  }
});

updateTextInputState(false, "");

// Unlock AudioContext on first user interaction (required by browsers).
document.addEventListener("click", () => { try { ensureAudioContext(); } catch {} }, { once: true });
document.addEventListener("touchstart", () => { try { ensureAudioContext(); } catch {} }, { once: true });
