const summaryText = document.getElementById("summaryText");
const metaText = document.getElementById("metaText");
const conversationTitle = document.getElementById("conversationTitle");
const messageList = document.getElementById("messageList");
const statusText = document.getElementById("statusText");

function formatTimestamp(value) {
  if (!value) return "";
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? "" : date.toLocaleString();
}

function pickConversation(conversations) {
  if (!Array.isArray(conversations) || conversations.length === 0) {
    return null;
  }
  return (
    conversations.find((conversation) => conversation.mode === "solo" && conversation.status === "active") ||
    conversations.find((conversation) => conversation.status === "active") ||
    conversations[0]
  );
}

function renderMessages(conversation) {
  if (!conversation) {
    conversationTitle.textContent = "Waiting for activity…";
    messageList.innerHTML = '<div class="hdmi-empty">No conversations yet. Open the control page and start a solo or botnet chat.</div>';
    return;
  }

  conversationTitle.textContent = conversation.topic || "Untitled conversation";
  const visibleMessages = Array.isArray(conversation.messages)
    ? conversation.messages.slice(-10)
    : [];
  if (!visibleMessages.length) {
    messageList.innerHTML = '<div class="hdmi-empty">No messages in this conversation yet.</div>';
    return;
  }

  messageList.innerHTML = visibleMessages
    .map((message) => {
      const kindClass = message.kind === "event" ? "event" : (message.speakerType || "assistant");
      return `
        <article class="hdmi-message ${kindClass}">
          <div class="hdmi-message-head">
            <strong>${message.speakerName || "Bot"}</strong>
            <span>${formatTimestamp(message.createdAt)}</span>
          </div>
          <div class="hdmi-message-body">${(message.text || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")}</div>
        </article>
      `;
    })
    .join("");
}

async function refreshState() {
  statusText.textContent = "Refreshing…";
  try {
    const response = await fetch(`/api/state?ts=${Date.now()}`, { cache: "no-store" });
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }

    const conversation = pickConversation(payload.conversations);
    renderMessages(conversation);

    const settings = payload.settings || {};
    const online = payload.online || {};
    const stats = payload.stats || {};
    summaryText.textContent =
      `${settings.botName || "GroqBotNet Bot"} · ${settings.botnetMode || "persona-relay"} · ${settings.transportMode || "lan-direct"}`;
    metaText.textContent =
      settings.transportMode === "online-hub"
        ? `Hub ${online.connected ? "connected" : online.registered ? "registered" : "offline"} · Requests this hour ${stats.requestsUsedThisHour ?? 0}`
        : `LAN Direct · Requests this hour ${stats.requestsUsedThisHour ?? 0}`;
    statusText.textContent = `Updated ${new Date().toLocaleTimeString()}`;
  } catch (error) {
    statusText.textContent = `Refresh failed: ${error.message || String(error)}`;
  }
}

refreshState();
setInterval(refreshState, 2000);
