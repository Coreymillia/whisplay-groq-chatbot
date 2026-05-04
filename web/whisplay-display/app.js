const statusText = document.getElementById("statusText");
const emojiText = document.getElementById("emojiText");
const textContent = document.getElementById("textContent");
const batteryFill = document.getElementById("batteryFill");
const batteryText = document.getElementById("batteryText");
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
const groqKeyInput = document.getElementById("groqKeyInput");
const groqKeyHint = document.getElementById("groqKeyHint");
const geminiKeyInput = document.getElementById("geminiKeyInput");
const geminiKeyHint = document.getElementById("geminiKeyHint");
const personalityPresetSelect = document.getElementById("personalityPresetSelect");
const personalityInput = document.getElementById("personalityInput");
const voiceModeSelect = document.getElementById("voiceModeSelect");
const volumeLevelSelect = document.getElementById("volumeLevelSelect");
const recordTimeSelect = document.getElementById("recordTimeSelect");
const uiThemeSelect = document.getElementById("uiThemeSelect");
const cameraSourceSelect = document.getElementById("cameraSourceSelect");
const esp32CamUrlInput = document.getElementById("esp32CamUrlInput");
const esp32CamUrlWrap = document.getElementById("esp32CamUrlWrap");
const headerModeSelect = document.getElementById("headerModeSelect");
const screensaverModeSelect = document.getElementById("screensaverModeSelect");
const idleTimeoutSelect = document.getElementById("idleTimeoutSelect");
const saveSettingsBtn = document.getElementById("saveSettingsBtn");
const clearKeyBtn = document.getElementById("clearKeyBtn");
const shutdownBtn = document.getElementById("shutdownBtn");
const settingsStatus = document.getElementById("settingsStatus");
const dim = document.getElementById("dim");
const imageLayer = document.getElementById("imageLayer");
const imageDisplay = document.getElementById("imageDisplay");

let scrollTop = 0;
let scrollSpeed = 0;
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
const DEFAULT_UI_THEME = "default";
const DEFAULT_CAMERA_SOURCE = "pi-camera";
const DEFAULT_ESP32_CAM_URL = "http://esp32-cam.local";
const CUSTOM_PERSONALITY_PRESET_ID = "custom";
let personalityPresets = [];
let volumeLevelOptions = [];
let recordTimeoutOptions = [];
let idleTimeoutOptions = [];

const DEFAULT_HEADER_MODE = "emoji";
const DEFAULT_SCREENSAVER_MODE = "retro-geometry";
const DEFAULT_IDLE_TIMEOUT_SEC = 120;

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

function updateText(text, sync, speed) {
  const viewportHeight = document.querySelector(".text-viewport").offsetHeight;
  const nextText = text || "";
  const isRegressive =
    nextText.length > 0 && nextText.length < lastText.length && lastText.startsWith(nextText);

  if (isRegressive) {
    scrollSpeed = Math.max(0, parseInt(speed || 0, 10));
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
    const speedPerSec = scrollSpeed * 5;
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

function applyState(data) {
  if (!data || !data.ready) return;

  const status = data.status || "";
  statusText.textContent = status;
  emojiText.textContent = data.emoji || "";
  updateText(data.text || "", data.scroll_sync, data.scroll_speed);
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
    card.appendChild(deleteBtn);
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

function formatIdleTimeoutLabel(value) {
  return value <= 0 ? "Off" : `${Math.round(value / 60)} minute${value === 60 ? "" : "s"}`;
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

function syncPresetSelectionFromPrompt(prompt) {
  if (!personalityPresetSelect) return;
  const match = personalityPresets.find((preset) => preset.prompt === prompt);
  personalityPresetSelect.value = match?.id || CUSTOM_PERSONALITY_PRESET_ID;
}

function applySettings(settings) {
  if (!settings) return;
  populatePersonalityPresets(settings.personalityPresetId);
  populateVolumeLevelOptions(settings.volumeLevel || 9);
  populateRecordTimeoutOptions(settings.manualRecordMaxSec || 15);
  if (personalityInput) {
    personalityInput.value = settings.personalityPrompt || "";
  }
  syncPresetSelectionFromPrompt(settings.personalityPrompt || "");
  if (voiceModeSelect) {
    voiceModeSelect.value = settings.voiceMode || "text-only";
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
  if (headerModeSelect) {
    headerModeSelect.value = settings.headerMode || DEFAULT_HEADER_MODE;
  }
  if (screensaverModeSelect) {
    screensaverModeSelect.value =
      settings.screensaverMode || DEFAULT_SCREENSAVER_MODE;
  }
  populateIdleTimeoutOptions(settings.idleTimeoutSec);
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
    recordTimeoutOptions = Array.isArray(payload.recordTimeoutOptions)
      ? payload.recordTimeoutOptions
      : [];
    idleTimeoutOptions = Array.isArray(payload.idleTimeoutOptions)
      ? payload.idleTimeoutOptions
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
    personalityPrompt: (personalityInput?.value || "").trim(),
    voiceMode: voiceModeSelect?.value || "text-only",
    volumeLevel: parseInt(volumeLevelSelect?.value || "9", 10),
    manualRecordMaxSec: parseInt(recordTimeSelect?.value || "15", 10),
    uiTheme: uiThemeSelect?.value || DEFAULT_UI_THEME,
    cameraSource: cameraSourceSelect?.value || DEFAULT_CAMERA_SOURCE,
    esp32CamUrl: (esp32CamUrlInput?.value || DEFAULT_ESP32_CAM_URL).trim(),
    headerMode: headerModeSelect?.value || DEFAULT_HEADER_MODE,
    screensaverMode:
      screensaverModeSelect?.value || DEFAULT_SCREENSAVER_MODE,
    idleTimeoutSec: parseInt(
      idleTimeoutSelect?.value || String(DEFAULT_IDLE_TIMEOUT_SEC),
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
    applySettings(payload.settings || {});
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
loadVisionPreview();
loadVisionAnalysis();
loadSavedPhotos();
setVisionAnalysisVisible(false);
requestAnimationFrame(animateScroll);
setInterval(loadVisionAnalysis, 3000);

personalityPresetSelect?.addEventListener("change", () => {
  const selectedId = personalityPresetSelect.value;
  if (selectedId === CUSTOM_PERSONALITY_PRESET_ID) {
    return;
  }
  const preset = personalityPresets.find((item) => item.id === selectedId);
  if (!preset || !personalityInput) {
    return;
  }
  personalityInput.value = preset.prompt;
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

if (visionUploadBtn) {
  visionUploadBtn.addEventListener("click", () => {
    uploadVisionImage();
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

function sendTextInput() {
  const text = (textInput.value || "").trim();
  if (!text) return;
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify({ type: "text_input", text }));
  textInput.value = "";
}

textSendBtn.addEventListener("click", sendTextInput);
textInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && (e.metaKey || e.ctrlKey) && !e.isComposing) {
    e.preventDefault();
    sendTextInput();
  }
});

updateTextInputState(false, "");

// Unlock AudioContext on first user interaction (required by browsers).
document.addEventListener("click", () => { try { ensureAudioContext(); } catch {} }, { once: true });
document.addEventListener("touchstart", () => { try { ensureAudioContext(); } catch {} }, { once: true });
