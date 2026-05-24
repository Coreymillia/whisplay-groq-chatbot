const settingsForm = document.getElementById("settingsForm");
const startForm = document.getElementById("startForm");
const soloForm = document.getElementById("soloForm");
const newSoloBtn = document.getElementById("newSoloBtn");
const conversationList = document.getElementById("conversationList");
const refreshBtn = document.getElementById("refreshBtn");
const settingsStatus = document.getElementById("settingsStatus");
const startStatus = document.getElementById("startStatus");
const soloStatus = document.getElementById("soloStatus");
const stats = document.getElementById("stats");
const botnetMode = document.getElementById("botnetMode");
const transportMode = document.getElementById("transportMode");
const peerUrl = document.getElementById("peerUrl");
const nodeHandle = document.getElementById("nodeHandle");
const hubUrl = document.getElementById("hubUrl");
const inviteCode = document.getElementById("inviteCode");
const startFormTitle = document.getElementById("startFormTitle");
const startPromptLabel = document.getElementById("startPromptLabel");
const starterWrap = document.getElementById("starterWrap");
const startModeHint = document.getElementById("startModeHint");
const startSubmitBtn = document.getElementById("startSubmitBtn");
const topicInput = document.getElementById("topic");
const maxBotRepliesWrap = document.getElementById("maxBotRepliesWrap");
const replyDelayWrap = document.getElementById("replyDelayWrap");
const soloMessage = document.getElementById("soloMessage");
const testHubBtn = document.getElementById("testHubBtn");
const registerHubBtn = document.getElementById("registerHubBtn");
const connectHubBtn = document.getElementById("connectHubBtn");
const createInviteBtn = document.getElementById("createInviteBtn");
const redeemInviteBtn = document.getElementById("redeemInviteBtn");
const disconnectHubBtn = document.getElementById("disconnectHubBtn");
const roomMonitorIntervalSec = document.getElementById("roomMonitorIntervalSec");
const roomMonitorStartTime = document.getElementById("roomMonitorStartTime");
const roomMonitorStopTime = document.getElementById("roomMonitorStopTime");
const roomMonitorFreeReserveGb = document.getElementById("roomMonitorFreeReserveGb");
const roomMonitorStatus = document.getElementById("roomMonitorStatus");
const roomMonitorCaptureBtn = document.getElementById("roomMonitorCaptureBtn");
const clearChatsBtn = document.getElementById("clearChatsBtn");

let activeSoloConversationId = null;
let currentOnline = null;

function setStatus(el, message, isError = false) {
  el.textContent = message;
  el.style.color = isError ? "#ff8d8d" : "";
}

function updateBotnetModeUi(mode) {
  const currentMode = mode === "auto-bot" ? "auto-bot" : "persona-relay";
  if (botnetMode) {
    botnetMode.value = currentMode;
  }
  if (maxBotRepliesWrap) {
    maxBotRepliesWrap.style.display = currentMode === "auto-bot" ? "" : "none";
  }
  if (replyDelayWrap) {
    replyDelayWrap.style.display = currentMode === "auto-bot" ? "" : "none";
  }
  if (currentMode === "persona-relay") {
    if (startFormTitle) startFormTitle.textContent = "One Message at a Time";
    if (startPromptLabel) startPromptLabel.textContent = "Your prompt";
    if (topicInput) {
      topicInput.placeholder = "Type what you want your bot to reinterpret and send to the peer bot.";
    }
    if (starterWrap) starterWrap.style.display = "none";
    if (startModeHint) {
      startModeHint.textContent =
        "Recommended. Your bot rewrites one prompt in its own personality, sends it once, and waits for your next prompt.";
    }
    if (startSubmitBtn) startSubmitBtn.textContent = "Send through your bot";
    return;
  }
  if (startFormTitle) startFormTitle.textContent = "Start auto conversation";
  if (startPromptLabel) startPromptLabel.textContent = "Topic / opening idea";
  if (topicInput) {
    topicInput.placeholder = "Have two bots debate whether old hardware has charm.";
  }
  if (starterWrap) starterWrap.style.display = "";
  if (startModeHint) {
    startModeHint.textContent =
      "Auto Conversation lets the bots keep talking until the reply cap or hourly limit stops them.";
  }
  if (startSubmitBtn) startSubmitBtn.textContent = "Start auto conversation";
}

function updateTransportUi(mode) {
  const currentMode = mode === "online-hub" ? "online-hub" : "lan-direct";
  if (transportMode) {
    transportMode.value = currentMode;
  }
  const onlineMode = currentMode === "online-hub";
  if (peerUrl) peerUrl.disabled = onlineMode;
  if (nodeHandle) nodeHandle.disabled = !onlineMode;
  if (hubUrl) hubUrl.disabled = !onlineMode;
  if (inviteCode) inviteCode.disabled = !onlineMode;
  if (testHubBtn) testHubBtn.disabled = !onlineMode;
  if (registerHubBtn) registerHubBtn.disabled = !onlineMode;
  if (connectHubBtn) connectHubBtn.disabled = !onlineMode;
  if (createInviteBtn) createInviteBtn.disabled = !onlineMode;
  if (redeemInviteBtn) redeemInviteBtn.disabled = !onlineMode;
  if (disconnectHubBtn) disconnectHubBtn.disabled = !onlineMode;
}

function fillSettings(settings, online, statsPayload) {
  document.getElementById("botName").value = settings.botName || "";
  updateBotnetModeUi(settings.botnetMode || "persona-relay");
  updateTransportUi(settings.transportMode || "lan-direct");
  document.getElementById("model").value = settings.model || "";
  document.getElementById("groqApiKey").value = "";
  document.getElementById("publicBaseUrl").value = settings.publicBaseUrl || "";
  document.getElementById("peerUrl").value = settings.peerUrl || "";
  document.getElementById("nodeHandle").value = settings.nodeHandle || "";
  document.getElementById("hubUrl").value = settings.hubUrl || "";
  document.getElementById("personalityPrompt").value = settings.personalityPrompt || "";
  document.getElementById("memoryTurns").value = settings.memoryTurns ?? 12;
  document.getElementById("replyDelaySec").value = settings.replyDelaySec ?? 6;
  document.getElementById("maxBotReplies").value = settings.maxBotReplies ?? 8;
  document.getElementById("maxRequestsPerHour").value = settings.maxRequestsPerHour ?? 30;
  if (roomMonitorIntervalSec) {
    roomMonitorIntervalSec.value = settings.roomMonitorIntervalSec ?? 0;
  }
  if (roomMonitorStartTime) {
    roomMonitorStartTime.value = settings.roomMonitorStartTime || "";
  }
  if (roomMonitorStopTime) {
    roomMonitorStopTime.value = settings.roomMonitorStopTime || "";
  }
  if (roomMonitorFreeReserveGb) {
    roomMonitorFreeReserveGb.value = String(settings.roomMonitorFreeReserveGb ?? 8);
  }
  currentOnline = online || null;

  const transportSummary =
    settings.transportMode === "online-hub"
      ? `Online hub: ${online?.connected ? "connected" : online?.registered ? "registered" : "offline"}`
      : "LAN direct";
  const linkSummary =
    settings.transportMode === "online-hub"
      ? ` | link: ${online?.linkId ? `${online.peerHandle || online.peerNodeId || "paired"} (${online.peerOnline ? "peer online" : "peer offline"})` : "not paired"}`
      : "";
  const errorSummary = online?.lastError ? ` | hub: ${online.lastError}` : "";
  stats.textContent =
    `Mode: ${settings.botnetMode || "persona-relay"} | Transport: ${transportSummary}${linkSummary} | Groq key: ${settings.groqApiKeyConfigured ? "stored" : "missing"} | Requests used this hour: ${statsPayload.requestsUsedThisHour}${errorSummary}`;
}

function formatTimestamp(value) {
  if (!value) return "";
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? "" : date.toLocaleString();
}

function renderRoomMonitor(roomMonitor) {
  if (!roomMonitorStatus) {
    return;
  }

  const statusBits = [
    roomMonitor?.enabled
      ? `Running every ${roomMonitor.intervalSec}s`
      : "Disabled",
    roomMonitor?.startTime && roomMonitor?.stopTime
      ? `window: ${roomMonitor.startTime}-${roomMonitor.stopTime}`
      : "window: all day",
    Number.isFinite(roomMonitor?.freeReserveGb)
      ? `reserve: ${roomMonitor.freeReserveGb} GB`
      : null,
    Number.isFinite(roomMonitor?.freeSpaceBytes)
      ? `free: ${Math.max(0, Math.round(roomMonitor.freeSpaceBytes / (1024 * 1024 * 1024) * 10) / 10)} GB`
      : null,
    typeof roomMonitor?.activeNow === "boolean"
      ? roomMonitor.activeNow
        ? "active now"
        : "outside active hours"
      : null,
    roomMonitor?.captureInProgress ? "capturing now" : null,
    roomMonitor?.detectedCamera ? `camera: ${roomMonitor.detectedCamera}` : null,
    roomMonitor?.cameraCommand ? `tool: ${roomMonitor.cameraCommand}` : null,
    Number.isFinite(roomMonitor?.totalCount) ? `images: ${roomMonitor.totalCount}` : null,
    roomMonitor?.lastCaptureAt
      ? `last capture: ${formatTimestamp(roomMonitor.lastCaptureAt)}`
      : null,
    roomMonitor?.lastError ? `error: ${roomMonitor.lastError}` : null,
  ].filter(Boolean);

  roomMonitorStatus.textContent = statusBits.join(" | ") || "Room monitor is idle.";
}

function renderConversations(conversations) {
  activeSoloConversationId =
    conversations.find((conversation) => conversation.mode === "solo" && conversation.status === "active")?.id || null;

  if (!conversations.length) {
    conversationList.innerHTML = '<div class="muted">No conversations yet.</div>';
    return;
  }

  conversationList.innerHTML = conversations
    .map((conversation) => {
      const messages = conversation.messages
        .map((message) => {
          const kindClass = message.kind === "event" ? "event" : message.speakerType;
          return `
            <div class="message ${kindClass}">
              <div class="message-head">
                <strong>${message.speakerName}</strong>
                <span>${new Date(message.createdAt).toLocaleString()}</span>
              </div>
              <div>${message.text}</div>
            </div>
          `;
        })
        .join("");

      const transportInfo =
        conversation.mode === "botnet"
          ? ` | transport: ${conversation.transportMode || "lan-direct"}${
              conversation.transportMode === "online-hub" && conversation.linkId ? ` / ${conversation.linkId}` : ""
            }`
          : "";

      return `
        <article class="conversation">
          <div class="row">
            <div>
              <h3>${conversation.topic || "Untitled conversation"}</h3>
              <div class="muted">
                mode: ${conversation.mode || "botnet"}${conversation.mode === "botnet" ? ` / ${conversation.botnetMode || "persona-relay"}` : ""}${transportInfo} | status: ${conversation.status} | replies: ${conversation.replyCount}${conversation.mode === "botnet" && (conversation.maxBotReplies || 0) > 0 ? `/${conversation.maxBotReplies}` : ""}
              </div>
            </div>
            ${conversation.status === "active" ? `<button data-stop="${conversation.id}" class="secondary">Stop</button>` : ""}
          </div>
          <div class="messages">${messages || '<div class="muted">No messages yet.</div>'}</div>
        </article>
      `;
    })
    .join("");

  document.querySelectorAll("[data-stop]").forEach((button) => {
    button.addEventListener("click", async () => {
      await fetch("/api/conversations/stop", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ conversationId: button.dataset.stop }),
      });
      await loadState();
    });
  });
}

async function loadState() {
  const response = await fetch("/api/state", { cache: "no-store" });
  const payload = await response.json();
  if (!response.ok || payload.ok === false) {
    throw new Error(payload.error || `HTTP ${response.status}`);
  }
  fillSettings(payload.settings, payload.online, payload.stats);
  renderRoomMonitor(payload.roomMonitor || {});
  renderConversations(payload.conversations || []);
}

function validateBotnetSetup() {
  const currentTransport = transportMode?.value || "lan-direct";
  if (currentTransport === "online-hub") {
    if (!hubUrl?.value.trim()) {
      throw new Error("Hub URL is required for Online Hub mode.");
    }
    if (!nodeHandle?.value.trim()) {
      throw new Error("Node handle is required for Online Hub mode.");
    }
    if (!currentOnline?.connected) {
      throw new Error("Connect to the hub before starting an online BotNet conversation.");
    }
    if (!currentOnline?.linkId) {
      throw new Error("No online peer link is active yet.");
    }
    return;
  }
  if (!peerUrl?.value.trim()) {
    throw new Error("Peer URL is required for LAN Direct mode.");
  }
}

async function saveSettingsFromForm() {
  const formData = new FormData(settingsForm);
  const body = Object.fromEntries(formData.entries());
  const response = await fetch("/api/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const payload = await response.json();
  if (!response.ok || payload.ok === false) {
    throw new Error(payload.error || `HTTP ${response.status}`);
  }
}

async function postAction(url, body, statusEl, successMessage) {
  setStatus(statusEl, "Working...");
  try {
    await saveSettingsFromForm();
    const response = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body || {}),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    setStatus(statusEl, successMessage);
    await loadState();
  } catch (error) {
    setStatus(statusEl, error.message || String(error), true);
  }
}

async function createInvite() {
  await postAction("/api/botnet/invite", {}, settingsStatus, "Invite created.");
}

async function redeemInvite() {
  const code = inviteCode?.value.trim().toUpperCase() || "";
  if (!code) {
    setStatus(settingsStatus, "Invite code is required.", true);
    return;
  }
  await postAction("/api/botnet/redeem", { inviteCode: code }, settingsStatus, "Invite redeemed.");
}

settingsForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setStatus(settingsStatus, "Saving settings...");
  try {
    await saveSettingsFromForm();
    setStatus(settingsStatus, "Settings saved.");
    await loadState();
  } catch (error) {
    setStatus(settingsStatus, error.message || String(error), true);
  }
});

startForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setStatus(startStatus, "Starting conversation...");
  try {
    validateBotnetSetup();
    const formData = new FormData(startForm);
    const body = Object.fromEntries(formData.entries());
    body.botnetMode = botnetMode?.value || "persona-relay";
    const response = await fetch("/api/conversations/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    setStatus(startStatus, "Conversation started.");
    startForm.reset();
    updateBotnetModeUi(botnetMode?.value || "persona-relay");
    updateTransportUi(transportMode?.value || "lan-direct");
    document.getElementById("starter").value = "self";
    await loadState();
  } catch (error) {
    setStatus(startStatus, error.message || String(error), true);
  }
});

soloForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setStatus(soloStatus, "Sending message...");
  try {
    const message = document.getElementById("soloMessage").value.trim();
    if (!message) {
      throw new Error("Message is required.");
    }
    const response = await fetch("/api/solo/send", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        message,
        conversationId: activeSoloConversationId || "",
      }),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    document.getElementById("soloMessage").value = "";
    setStatus(soloStatus, "Bot replied.");
    await loadState();
  } catch (error) {
    setStatus(soloStatus, error.message || String(error), true);
  }
});

topicInput?.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey && !event.isComposing) {
    event.preventDefault();
    startForm.requestSubmit();
  }
});

soloMessage?.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey && !event.isComposing) {
    event.preventDefault();
    soloForm.requestSubmit();
  }
});

newSoloBtn.addEventListener("click", () => {
  activeSoloConversationId = null;
  setStatus(soloStatus, "Next message will start a fresh solo chat.");
  document.getElementById("soloMessage").focus();
});

refreshBtn.addEventListener("click", async () => {
  await loadState();
});

botnetMode?.addEventListener("change", () => {
  updateBotnetModeUi(botnetMode.value);
});

transportMode?.addEventListener("change", () => {
  updateTransportUi(transportMode.value);
});

testHubBtn?.addEventListener("click", async () => {
  await postAction("/api/botnet/test", {}, settingsStatus, "Hub test succeeded.");
});

registerHubBtn?.addEventListener("click", async () => {
  await postAction("/api/botnet/register", {}, settingsStatus, "Node registered.");
});

connectHubBtn?.addEventListener("click", async () => {
  await postAction("/api/botnet/connect", {}, settingsStatus, "Hub connected.");
});

createInviteBtn?.addEventListener("click", async () => {
  try {
    setStatus(settingsStatus, "Creating invite...");
    await saveSettingsFromForm();
    const response = await fetch("/api/botnet/invite", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({}),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    if (inviteCode) {
      inviteCode.value = payload.invite?.inviteCode || "";
    }
    setStatus(
      settingsStatus,
      payload.invite?.inviteCode
        ? `Invite created: ${payload.invite.inviteCode}`
        : "Invite created.",
    );
    await loadState();
  } catch (error) {
    setStatus(settingsStatus, error.message || String(error), true);
  }
});

redeemInviteBtn?.addEventListener("click", async () => {
  await redeemInvite();
});

disconnectHubBtn?.addEventListener("click", async () => {
  await postAction("/api/botnet/disconnect", {}, settingsStatus, "Hub disconnected.");
});

roomMonitorCaptureBtn?.addEventListener("click", async () => {
  setStatus(roomMonitorStatus, "Capturing...");
  try {
    await saveSettingsFromForm();
    const response = await fetch("/api/room-monitor/capture", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({}),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    renderRoomMonitor(payload.roomMonitor || {});
    await loadState();
  } catch (error) {
    setStatus(roomMonitorStatus, error.message || String(error), true);
  }
});

clearChatsBtn?.addEventListener("click", async () => {
  setStatus(settingsStatus, "Clearing chat...");
  try {
    const response = await fetch("/api/conversations/clear", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({}),
    });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    activeSoloConversationId = null;
    setStatus(settingsStatus, "Chat cleared.");
    await loadState();
  } catch (error) {
    setStatus(settingsStatus, error.message || String(error), true);
  }
});

loadState().catch((error) => {
  setStatus(startStatus, `Initial load failed: ${error.message || error}`, true);
});
