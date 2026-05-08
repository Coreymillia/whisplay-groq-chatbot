const statusText = document.getElementById("statusText");
const metaText = document.getElementById("metaText");
const emojiText = document.getElementById("emojiText");
const textText = document.getElementById("textText");
const connectionText = document.getElementById("connectionText");
const imageCard = document.getElementById("imageCard");
const imageTitle = document.getElementById("imageTitle");
const imageSubtitle = document.getElementById("imageSubtitle");
const imageDisplay = document.getElementById("imageDisplay");

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
  if (!data || data.ready === false) {
    return;
  }

  statusText.textContent = data.status || "ready";
  emojiText.textContent = data.emoji || "🙂";
  textText.textContent = data.text || "Waiting for chat text…";
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
