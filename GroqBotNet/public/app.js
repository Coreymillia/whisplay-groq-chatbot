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
const startFormTitle = document.getElementById("startFormTitle");
const startPromptLabel = document.getElementById("startPromptLabel");
const starterWrap = document.getElementById("starterWrap");
const startModeHint = document.getElementById("startModeHint");
const startSubmitBtn = document.getElementById("startSubmitBtn");
const topicInput = document.getElementById("topic");
let activeSoloConversationId = null;

function setStatus(el, message, isError = false) {
  el.textContent = message;
  el.style.color = isError ? "#ff8d8d" : "";
}

function updateBotnetModeUi(mode) {
  const currentMode = mode === "auto-bot" ? "auto-bot" : "persona-relay";
  if (botnetMode) {
    botnetMode.value = currentMode;
  }
  if (currentMode === "persona-relay") {
    if (startFormTitle) startFormTitle.textContent = "Persona Relay";
    if (startPromptLabel) startPromptLabel.textContent = "Your prompt";
    if (topicInput) {
      topicInput.placeholder =
        "Type what you want your bot to reinterpret and send to the peer bot.";
    }
    if (starterWrap) starterWrap.style.display = "none";
    if (startModeHint) {
      startModeHint.textContent =
        "Your bot rewrites your prompt in its own personality, sends that to the peer, and waits for your next prompt.";
    }
    if (startSubmitBtn) startSubmitBtn.textContent = "Send through your bot";
    return;
  }
  if (startFormTitle) startFormTitle.textContent = "Start bot conversation";
  if (startPromptLabel) startPromptLabel.textContent = "Topic / opening idea";
  if (topicInput) {
    topicInput.placeholder =
      "Have two bots debate whether old hardware has charm.";
  }
  if (starterWrap) starterWrap.style.display = "";
  if (startModeHint) {
    startModeHint.textContent =
      "Auto Bot Conversation lets the bots keep talking until the conversation cap or hourly limit stops them.";
  }
  if (startSubmitBtn) startSubmitBtn.textContent = "Start conversation";
}

function fillSettings(settings, statsPayload) {
  document.getElementById("botName").value = settings.botName || "";
  updateBotnetModeUi(settings.botnetMode || "persona-relay");
  document.getElementById("model").value = settings.model || "";
  document.getElementById("groqApiKey").value = "";
  document.getElementById("publicBaseUrl").value = settings.publicBaseUrl || "";
  document.getElementById("peerUrl").value = settings.peerUrl || "";
  document.getElementById("personalityPrompt").value = settings.personalityPrompt || "";
  document.getElementById("memoryTurns").value = settings.memoryTurns ?? 12;
  document.getElementById("replyDelaySec").value = settings.replyDelaySec ?? 6;
  document.getElementById("maxBotReplies").value = settings.maxBotReplies ?? 8;
  document.getElementById("maxRequestsPerHour").value = settings.maxRequestsPerHour ?? 30;
  stats.textContent = `Mode: ${settings.botnetMode || "persona-relay"} | Groq key: ${settings.groqApiKeyConfigured ? "stored" : "missing"} | Requests used this hour: ${statsPayload.requestsUsedThisHour}`;
}

function renderConversations(conversations) {
  activeSoloConversationId =
    conversations.find(
      (conversation) => conversation.mode === "solo" && conversation.status === "active",
    )?.id || null;

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

      return `
        <article class="conversation">
          <div class="row">
            <div>
              <h3>${conversation.topic || "Untitled conversation"}</h3>
              <div class="muted">
                mode: ${conversation.mode || "botnet"}${conversation.mode === "botnet" ? ` / ${conversation.botnetMode || "persona-relay"}` : ""} | status: ${conversation.status} | replies: ${conversation.replyCount}${conversation.mode === "botnet" && (conversation.maxBotReplies || 0) > 0 ? `/${conversation.maxBotReplies}` : ""}
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
  fillSettings(payload.settings, payload.stats);
  renderConversations(payload.conversations || []);
}

settingsForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setStatus(settingsStatus, "Saving settings...");
  try {
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

loadState().catch((error) => {
  setStatus(startStatus, `Initial load failed: ${error.message || error}`, true);
});
