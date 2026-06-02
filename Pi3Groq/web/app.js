const settingsForm = document.getElementById("settingsForm");
const modeSelect = document.getElementById("modeSelect");
const companionBaseUrlInput = document.getElementById("companionBaseUrlInput");
const pollIntervalSelect = document.getElementById("pollIntervalSelect");
const touchDisplayModeSelect = document.getElementById("touchDisplayModeSelect");
const touchDisplayRotationSelect = document.getElementById("touchDisplayRotationSelect");
const slideshowEnabledCheckbox = document.getElementById("slideshowEnabledCheckbox");
const slideshowIntervalSelect = document.getElementById("slideshowIntervalSelect");
const chatReturnTimeoutSelect = document.getElementById("chatReturnTimeoutSelect");
const settingsStatus = document.getElementById("settingsStatus");
const companionStatus = document.getElementById("companionStatus");
const conversationLog = document.getElementById("conversationLog");
const chatForm = document.getElementById("chatForm");
const chatInput = document.getElementById("chatInput");
const sendBtn = document.getElementById("sendBtn");
const openWhisplayBtn = document.getElementById("openWhisplayBtn");
const remoteStatusText = document.getElementById("remoteStatusText");
const remoteEmojiText = document.getElementById("remoteEmojiText");
const remoteReplyText = document.getElementById("remoteReplyText");
const remoteImage = document.getElementById("remoteImage");
const remoteImageEmpty = document.getElementById("remoteImageEmpty");
const llmModelBadge = document.getElementById("llmModelBadge");
const groqRequestsBadge = document.getElementById("groqRequestsBadge");
const geminiBalanceBadge = document.getElementById("geminiBalanceBadge");

let settings = null;
let pollTimer = null;
let pendingSend = false;
let lastReplyText = "";
let turns = [];

function escapeHtml(value) {
  return String(value || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

function formatTime(value) {
  return new Date(value).toLocaleTimeString([], {
    hour: "numeric",
    minute: "2-digit",
  });
}

function pushTurn(role, text) {
  const trimmed = String(text || "").trim();
  if (!trimmed) {
    return;
  }
  const latest = turns[turns.length - 1];
  if (latest && latest.role === role && latest.text === trimmed) {
    return;
  }
  turns.push({ role, text: trimmed, timestamp: Date.now() });
  if (turns.length > 120) {
    turns = turns.slice(turns.length - 120);
  }
  renderConversation();
}

function renderConversation() {
  if (!turns.length) {
    conversationLog.innerHTML = '<div class="muted">No local chat turns yet. Type here to send through Whisplay.</div>';
    return;
  }
  conversationLog.innerHTML = turns
    .map(
      (turn) => `
        <article class="turn ${turn.role}">
          <div class="turn-meta">
            <strong>${turn.role === "user" ? "You" : "Whisplay"}</strong>
            <span>${formatTime(turn.timestamp)}</span>
          </div>
          <pre class="turn-text">${escapeHtml(turn.text)}</pre>
        </article>
      `,
    )
    .join("");
  conversationLog.scrollTop = conversationLog.scrollHeight;
}

function updateSettingsForm(nextSettings) {
  settings = nextSettings;
  modeSelect.value = nextSettings.mode || "companion";
  companionBaseUrlInput.value = nextSettings.companionBaseUrl || "";
  pollIntervalSelect.value = String(nextSettings.pollIntervalMs || 2000);
  touchDisplayModeSelect.value = nextSettings.touchDisplayMode || "slideshow-chat";
  touchDisplayRotationSelect.value = String(nextSettings.touchDisplayRotationDeg ?? 270);
  slideshowEnabledCheckbox.checked = nextSettings.slideshowEnabled !== false;
  slideshowIntervalSelect.value = String(nextSettings.slideshowIntervalSec || 8);
  chatReturnTimeoutSelect.value = String(nextSettings.chatReturnTimeoutSec || 20);
  const baseUrl = (nextSettings.companionBaseUrl || "").trim();
  openWhisplayBtn.href = baseUrl ? baseUrl : "#";
  openWhisplayBtn.setAttribute("aria-disabled", baseUrl ? "false" : "true");
}

async function loadSettings() {
  const response = await fetch("/api/settings", { cache: "no-store" });
  const payload = await response.json();
  if (!payload.ok) {
    throw new Error(payload.error || "Failed to load Pi3Groq settings.");
  }
  updateSettingsForm(payload.settings);
}

function setSaveStatus(message, isError = false) {
  settingsStatus.textContent = message;
  settingsStatus.style.color = isError ? "var(--danger)" : "";
}

function renderRemoteState(payload) {
  const state = payload.companion;
  if (!payload.ok || !state) {
    companionStatus.textContent = payload.error || "Whisplay companion is not connected.";
    remoteStatusText.textContent = "--";
    remoteEmojiText.textContent = "!";
    remoteReplyText.textContent = payload.error || "Waiting for a saved Whisplay URL.";
    llmModelBadge.textContent = "No model";
    groqRequestsBadge.textContent = "RPD --";
    geminiBalanceBadge.textContent = "$ --";
    remoteImage.hidden = true;
    remoteImageEmpty.hidden = false;
    return;
  }

  companionStatus.textContent = state.ready
    ? `Connected to ${state.remoteBaseUrl || payload.settings.companionBaseUrl}`
    : `Whisplay reachable but not ready: ${state.status || "unknown status"}`;
  remoteStatusText.textContent = state.status || "--";
  remoteEmojiText.textContent = state.emoji || "🙂";
  remoteReplyText.textContent = state.text || "No reply text yet.";
  llmModelBadge.textContent = state.llm_model || "No model";
  groqRequestsBadge.textContent = `RPD ${state.groq_requests_today ?? "--"}`;
  geminiBalanceBadge.textContent = state.gemini_low_tier_image_balance_text || "$ --";

  if (state.text && state.text !== lastReplyText) {
    lastReplyText = state.text;
    pushTurn("bot", state.text);
  }

  if (state.remote_image_proxy_url) {
    remoteImage.src = state.remote_image_proxy_url;
    remoteImage.hidden = false;
    remoteImageEmpty.hidden = true;
  } else {
    remoteImage.hidden = true;
    remoteImageEmpty.hidden = false;
  }
}

async function pollState() {
  try {
    const response = await fetch("/api/companion/state", { cache: "no-store" });
    const payload = await response.json();
    renderRemoteState(payload);
  } catch (error) {
    renderRemoteState({
      ok: false,
      error: error instanceof Error ? error.message : "State request failed.",
      companion: null,
      settings: settings || {},
    });
  }
}

function startPolling() {
  if (pollTimer) {
    window.clearInterval(pollTimer);
  }
  const interval = Number(settings?.pollIntervalMs || 2000);
  pollTimer = window.setInterval(pollState, interval);
}

async function saveSettings(event) {
  event.preventDefault();
  setSaveStatus("Saving companion settings...");
  try {
    const response = await fetch("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        mode: modeSelect.value,
        companionBaseUrl: companionBaseUrlInput.value.trim(),
        pollIntervalMs: Number(pollIntervalSelect.value),
        touchDisplayMode: touchDisplayModeSelect.value,
        touchDisplayRotationDeg: Number(touchDisplayRotationSelect.value),
        slideshowEnabled: slideshowEnabledCheckbox.checked,
        slideshowIntervalSec: Number(slideshowIntervalSelect.value),
        chatReturnTimeoutSec: Number(chatReturnTimeoutSelect.value),
      }),
    });
    const payload = await response.json();
    if (!response.ok || !payload.ok) {
      throw new Error(payload.error || "Failed to save settings.");
    }
    updateSettingsForm(payload.settings);
    setSaveStatus(
      payload.touchDisplayRotationApplyPending
        ? "Saved. Pi3Groq is rebooting to apply the new touch display rotation."
        : "Saved. Pi3Groq will now poll the updated Whisplay URL.",
    );
    startPolling();
    await pollState();
  } catch (error) {
    setSaveStatus(error instanceof Error ? error.message : "Failed to save settings.", true);
  }
}

async function sendMessage(event) {
  event.preventDefault();
  if (pendingSend) {
    return;
  }
  const text = chatInput.value.trim();
  if (!text) {
    return;
  }
  pendingSend = true;
  sendBtn.disabled = true;
  pushTurn("user", text);
  chatInput.value = "";
  try {
    const response = await fetch("/api/companion/input", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ text }),
    });
    const payload = await response.json();
    if (!response.ok || !payload.ok) {
      throw new Error(payload.error || "Failed to send message to Whisplay.");
    }
    await pollState();
  } catch (error) {
    pushTurn("bot", `Send failed: ${error instanceof Error ? error.message : "unknown error"}`);
  } finally {
    pendingSend = false;
    sendBtn.disabled = false;
    chatInput.focus();
  }
}

settingsForm.addEventListener("submit", saveSettings);
chatForm.addEventListener("submit", sendMessage);

window.addEventListener("load", async () => {
  renderConversation();
  try {
    await loadSettings();
    startPolling();
    await pollState();
  } catch (error) {
    setSaveStatus(error instanceof Error ? error.message : "Failed to load settings.", true);
  }
});
