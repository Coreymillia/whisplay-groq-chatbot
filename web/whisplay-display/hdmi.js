const statusText = document.getElementById("statusText");
const metaText = document.getElementById("metaText");
const emojiText = document.getElementById("emojiText");
const connectionText = document.getElementById("connectionText");
const imageCard = document.getElementById("imageCard");
const imageTitle = document.getElementById("imageTitle");
const imageSubtitle = document.getElementById("imageSubtitle");
const imageDisplay = document.getElementById("imageDisplay");
const conversationLog = document.getElementById("conversationLog");
const conversationEmpty = document.getElementById("conversationEmpty");

let ws = null;
let reconnectTimer = null;
let cameraTimer = null;
let lastImageRevision = -1;

function formatConnectionMeta(data) {
  const parts = [];
  if (typeof data.battery_level === "number") {
    parts.push(`Battery ${Math.max(0, Math.min(100, data.battery_level))}%`);
  }
  if (typeof data.wifi_signal_level === "number") {
    parts.push(`Wi-Fi ${data.wifi_signal_level}/3`);
  }
  if (data.vpn_connected) {
    parts.push("VPN on");
  }
  if (typeof data.music_progress === "number" && typeof data.music_duration_ms === "number") {
    parts.push("Music active");
  }
  return parts.join(" · ") || "Live HDMI mirror";
}

function setImageVisible(visible) {
  imageCard.classList.toggle("hidden", !visible);
}

function startCameraFeed() {
  if (cameraTimer) return;
  cameraTimer = setInterval(() => {
    imageDisplay.src = `/camera?ts=${Date.now()}`;
  }, 250);
}

function stopCameraFeed() {
  if (!cameraTimer) return;
  clearInterval(cameraTimer);
  cameraTimer = null;
}

function applyState(data) {
  if (!data || data.ready === false) return;

  statusText.textContent = data.status || "ready";
  emojiText.textContent = data.emoji || "🙂";
  metaText.textContent = formatConnectionMeta(data);

  if (data.camera_mode) {
    imageTitle.textContent = "Live Camera";
    imageSubtitle.textContent = "Streaming the active capture view";
    setImageVisible(true);
    startCameraFeed();
    return;
  }

  stopCameraFeed();
  if (data.image) {
    setImageVisible(true);
    imageTitle.textContent = "Latest Capture";
    imageSubtitle.textContent = "Showing the most recent image sent to the HAT";
    if (data.image_revision !== lastImageRevision) {
      lastImageRevision = data.image_revision;
      imageDisplay.src = `/image?rev=${lastImageRevision}`;
    }
  } else {
    setImageVisible(false);
    imageDisplay.removeAttribute("src");
  }
}

function formatTime(timestamp) {
  const d = new Date(timestamp);
  const h = d.getHours().toString().padStart(2, "0");
  const m = d.getMinutes().toString().padStart(2, "0");
  const s = d.getSeconds().toString().padStart(2, "0");
  return `${h}:${m}:${s}`;
}

function appendTurn(turn) {
  if (conversationEmpty) conversationEmpty.style.display = "none";

  const bubble = document.createElement("div");
  bubble.className = `hdmi-bubble hdmi-bubble-${turn.role}`;

  const label = document.createElement("div");
  label.className = "hdmi-bubble-label";
  label.textContent = turn.role === "user" ? "You" : "Whisplay";

  const body = document.createElement("div");
  body.className = "hdmi-bubble-body";
  body.textContent = turn.text;

  const time = document.createElement("div");
  time.className = "hdmi-bubble-time";
  time.textContent = formatTime(turn.timestamp);

  bubble.appendChild(label);
  bubble.appendChild(body);
  bubble.appendChild(time);
  conversationLog.appendChild(bubble);

  conversationLog.scrollTop = conversationLog.scrollHeight;
}

function loadHistory(turns) {
  // Clear existing bubbles (keep the empty placeholder)
  const existing = conversationLog.querySelectorAll(".hdmi-bubble");
  existing.forEach((el) => el.remove());
  if (turns.length === 0) {
    if (conversationEmpty) conversationEmpty.style.display = "";
    return;
  }
  turns.forEach(appendTurn);
}

function connect() {
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
    reconnectTimer = null;
  }

  connectionText.textContent = "Connecting…";
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  ws = new WebSocket(`${protocol}://${window.location.host}/ws`);

  ws.addEventListener("open", () => {
    connectionText.textContent = "Connected";
  });

  ws.addEventListener("message", (event) => {
    let message = null;
    try {
      message = JSON.parse(event.data);
    } catch {
      return;
    }
    if (message.type === "state") {
      applyState(message.payload);
    } else if (message.type === "conversation_turn") {
      appendTurn(message.payload);
    } else if (message.type === "conversation_history") {
      loadHistory(message.payload);
    }
  });

  ws.addEventListener("close", () => {
    stopCameraFeed();
    connectionText.textContent = "Reconnecting…";
    reconnectTimer = setTimeout(connect, 1000);
  });

  ws.addEventListener("error", () => {
    ws.close();
  });
}

connect();
