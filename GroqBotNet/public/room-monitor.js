const roomMonitorMeta = document.getElementById("roomMonitorMeta");
const roomMonitorStatus = document.getElementById("roomMonitorStatus");
const captureCountText = document.getElementById("captureCountText");
const roomMonitorGallery = document.getElementById("roomMonitorGallery");
const captureNowBtn = document.getElementById("captureNowBtn");
const refreshRoomMonitorBtn = document.getElementById("refreshRoomMonitorBtn");

function formatTimestamp(value) {
  if (!value) return "";
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? "" : date.toLocaleString();
}

function setRoomMonitorStatus(message, isError = false) {
  if (!roomMonitorStatus) return;
  roomMonitorStatus.textContent = message;
  roomMonitorStatus.style.color = isError ? "#ff8d8d" : "";
}

function renderRoomMonitor(roomMonitor) {
  const captures = Array.isArray(roomMonitor?.captures) ? roomMonitor.captures : [];
  if (roomMonitorMeta) {
    roomMonitorMeta.textContent = roomMonitor?.detectedCamera
      ? `${roomMonitor.detectedCamera} · ${roomMonitor.cameraCommand || "camera tool"}`
      : "No Raspberry Pi camera detected yet.";
  }

  const statusBits = [
    roomMonitor?.enabled ? `Running every ${roomMonitor.intervalSec}s` : "Disabled",
    roomMonitor?.startTime && roomMonitor?.stopTime
      ? `Window ${roomMonitor.startTime}-${roomMonitor.stopTime}`
      : "Window all day",
    Number.isFinite(roomMonitor?.freeReserveGb)
      ? `reserve ${roomMonitor.freeReserveGb} GB`
      : null,
    Number.isFinite(roomMonitor?.freeSpaceBytes)
      ? `free ${Math.max(0, Math.round(roomMonitor.freeSpaceBytes / (1024 * 1024 * 1024) * 10) / 10)} GB`
      : null,
    typeof roomMonitor?.autoBrightnessEnabled === "boolean"
      ? roomMonitor.autoBrightnessEnabled
        ? "auto brightness on"
        : "auto brightness off"
      : null,
    typeof roomMonitor?.activeNow === "boolean"
      ? roomMonitor.activeNow
        ? "active now"
        : "outside active hours"
      : null,
    roomMonitor?.captureInProgress ? "capturing now" : null,
    roomMonitor?.lastCaptureAt ? `last capture ${formatTimestamp(roomMonitor.lastCaptureAt)}` : null,
    roomMonitor?.lastBrightnessSummary || null,
    roomMonitor?.lastError ? `error: ${roomMonitor.lastError}` : null,
  ].filter(Boolean);
  setRoomMonitorStatus(statusBits.join(" | ") || "Room monitor is idle.");

  if (captureCountText) {
    captureCountText.textContent = `${captures.length} image${captures.length === 1 ? "" : "s"}`;
  }

  if (!roomMonitorGallery) {
    return;
  }
  if (!captures.length) {
    roomMonitorGallery.innerHTML = '<div class="muted">No room monitor images yet.</div>';
    return;
  }

  roomMonitorGallery.innerHTML = captures
    .map(
      (capture) => `
        <article class="conversation">
          <div class="row">
            <div>
              <h3 style="margin:0">${capture.fileName}</h3>
              <div class="muted">${formatTimestamp(capture.capturedAt)} · ${Math.max(1, Math.round((capture.sizeBytes || 0) / 1024))} KB</div>
            </div>
            <div class="actions">
              <a class="secondary" href="${capture.url}" target="_blank" rel="noopener">View</a>
              <a class="secondary" href="${capture.downloadUrl}">Download</a>
            </div>
          </div>
        </article>
      `,
    )
    .join("");
}

async function loadRoomMonitor() {
  setRoomMonitorStatus("Refreshing...");
  const response = await fetch("/api/room-monitor/images", { cache: "no-store" });
  const payload = await response.json();
  if (!response.ok || payload.ok === false) {
    throw new Error(payload.error || `HTTP ${response.status}`);
  }
  renderRoomMonitor(payload.roomMonitor || {});
}

captureNowBtn?.addEventListener("click", async () => {
  setRoomMonitorStatus("Capturing...");
  try {
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
  } catch (error) {
    setRoomMonitorStatus(error.message || String(error), true);
  }
});

refreshRoomMonitorBtn?.addEventListener("click", async () => {
  try {
    await loadRoomMonitor();
  } catch (error) {
    setRoomMonitorStatus(error.message || String(error), true);
  }
});

loadRoomMonitor().catch((error) => {
  setRoomMonitorStatus(error.message || String(error), true);
});
