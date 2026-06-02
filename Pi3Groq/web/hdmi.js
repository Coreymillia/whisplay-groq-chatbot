const hdmiStatus = document.getElementById("hdmiStatus");
const hdmiMeta = document.getElementById("hdmiMeta");
const touchShell = document.querySelector(".touch-shell");
const chatView = document.getElementById("chatView");
const chatModeLabel = document.getElementById("chatModeLabel");
const chatStatusLine = document.getElementById("chatStatusLine");
const chatEmoji = document.getElementById("chatEmoji");
const chatText = document.getElementById("chatText");
const hdmiEmoji = document.getElementById("hdmiEmoji");
const hdmiText = document.getElementById("hdmiText");
const hdmiImage = document.getElementById("hdmiImage");
const hdmiImageEmpty = document.getElementById("hdmiImageEmpty");
const slideshowView = document.getElementById("slideshowView");
const mirrorView = document.getElementById("mirrorView");
const touchControls = document.getElementById("touchControls");
const slideshowImage = document.getElementById("slideshowImage");
const slideshowEmpty = document.getElementById("slideshowEmpty");
const slideshowCaption = document.getElementById("slideshowCaption");
const slideshowCounter = document.getElementById("slideshowCounter");
const slideshowModeLabel = document.getElementById("slideshowModeLabel");
const mirrorModeLabel = document.getElementById("mirrorModeLabel");
const prevSlideBtn = document.getElementById("prevSlideBtn");
const nextSlideBtn = document.getElementById("nextSlideBtn");
const refreshSlidesBtn = document.getElementById("refreshSlidesBtn");

let settings = null;
let statePollTimer = null;
let slideTimer = null;
let textScrollTimer = null;
let lastGeneratedRevision = "";
let lastRemoteImageUrl = "";
let generatedSlides = [];
let currentSlideIndex = 0;
let currentMode = "";
let lastActiveAt = 0;
let lastActivitySignature = "";

setSlideControlsEnabled(false);

function normalizeStatus(value) {
  return String(value || "").trim().toLowerCase();
}

function formatTime(value) {
  if (!value) {
    return "";
  }
  const date = new Date(Number(value) || value);
  if (Number.isNaN(date.getTime())) {
    return "";
  }
  return date.toLocaleTimeString([], {
    hour: "numeric",
    minute: "2-digit",
  });
}

function getPollIntervalMs() {
  const value = Number(settings?.pollIntervalMs || 2000);
  return Math.min(10000, Math.max(500, value));
}

function getSlideIntervalMs() {
  const value = Number(settings?.slideshowIntervalSec || 8);
  return Math.min(30000, Math.max(3000, value * 1000));
}

function getChatReturnTimeoutMs() {
  const value = Number(settings?.chatReturnTimeoutSec || 20);
  return Math.min(300000, Math.max(5000, value * 1000));
}

function getTouchDisplayMode() {
  return settings?.touchDisplayMode === "mirror" ? "mirror" : "slideshow-chat";
}

function getTouchViewportSize() {
  return {
    width: Math.max(120, Math.round(window.innerWidth || document.documentElement.clientWidth || 480)),
    height: Math.max(120, Math.round(window.innerHeight || document.documentElement.clientHeight || 320)),
  };
}

function buildTouchRenderUrl(baseUrl, revision = "") {
  if (!baseUrl) {
    return "";
  }
  const viewport = getTouchViewportSize();
  const url = new URL(baseUrl, window.location.origin);
  url.searchParams.set("frameWidth", String(viewport.width));
  url.searchParams.set("frameHeight", String(viewport.height));
  url.searchParams.set("frameVersion", `${viewport.width}x${viewport.height}${revision ? `-${revision}` : ""}`);
  return url.toString();
}

function setSlideControlsEnabled(enabled) {
  prevSlideBtn.disabled = !enabled;
  nextSlideBtn.disabled = !enabled;
}

function applyDisplayLayout() {
  const hybridMode = getTouchDisplayMode() !== "mirror";
  document.body.classList.toggle("touch-hybrid-mode", hybridMode);
  if (touchShell) {
    touchShell.dataset.touchView = currentMode || "mirror";
  }
}

function stopSlideTimer() {
  if (slideTimer) {
    window.clearTimeout(slideTimer);
    slideTimer = null;
  }
}

function stopTextScrollTimer() {
  if (textScrollTimer) {
    window.clearInterval(textScrollTimer);
    textScrollTimer = null;
  }
}

function startAutoScroll(element) {
  stopTextScrollTimer();
  if (!element) {
    return;
  }
  element.scrollTop = 0;
  window.requestAnimationFrame(() => {
    if (element.scrollHeight <= element.clientHeight + 8) {
      element.scrollTop = 0;
      return;
    }
    let direction = 1;
    textScrollTimer = window.setInterval(() => {
      const maxScrollTop = element.scrollHeight - element.clientHeight;
      if (maxScrollTop <= 0) {
        element.scrollTop = 0;
        return;
      }
      if (direction > 0 && element.scrollTop >= maxScrollTop) {
        direction = -1;
      } else if (direction < 0 && element.scrollTop <= 0) {
        direction = 1;
      }
      element.scrollTop = Math.max(0, Math.min(maxScrollTop, element.scrollTop + direction));
    }, 70);
  });
}

function setMode(mode) {
  if (mode === currentMode) {
    return;
  }
  currentMode = mode;
  slideshowView.hidden = mode !== "slideshow";
  chatView.hidden = mode !== "chat";
  mirrorView.hidden = mode !== "mirror";
  touchControls.hidden = mode !== "slideshow";
  applyDisplayLayout();
  setSlideControlsEnabled(mode === "slideshow" && generatedSlides.length > 1);
  if (mode !== "slideshow") {
    stopSlideTimer();
  }
}

function scheduleNextSlide() {
  stopSlideTimer();
  if (currentMode !== "slideshow" || generatedSlides.length <= 1) {
    return;
  }
  slideTimer = window.setTimeout(() => {
    showSlide(currentSlideIndex + 1);
  }, getSlideIntervalMs());
}

function showSlide(index) {
  if (!generatedSlides.length) {
    slideshowView.style.backgroundImage = "";
    slideshowImage.hidden = true;
    slideshowImage.removeAttribute("src");
    slideshowEmpty.hidden = false;
    slideshowCaption.textContent = "No AI slideshow images yet.";
    slideshowCounter.textContent = "0 / 0";
    setSlideControlsEnabled(false);
    stopSlideTimer();
    return;
  }

  currentSlideIndex = (index + generatedSlides.length) % generatedSlides.length;
  const slide = generatedSlides[currentSlideIndex];
  const imageUrl = slide.touchImageUrl
    ? buildTouchRenderUrl(slide.touchImageUrl, slide.updatedAt || Date.now())
    : (slide.fullscreenImageUrl || slide.companionImageUrl);
  const renderUrl = slide.touchImageUrl
    ? imageUrl
    : `${imageUrl}?ts=${slide.updatedAt || Date.now()}`;
  slideshowView.style.backgroundImage = slide.touchImageUrl ? "" : `url("${renderUrl}")`;
  slideshowImage.style.objectFit = slide.touchImageUrl ? "fill" : "contain";
  slideshowImage.src = renderUrl;
  slideshowImage.hidden = false;
  slideshowEmpty.hidden = true;
  slideshowCaption.textContent = formatTime(slide.updatedAt)
    ? `${slide.fileName} · ${formatTime(slide.updatedAt)}`
    : slide.fileName;
  slideshowCounter.textContent = `${currentSlideIndex + 1} / ${generatedSlides.length}`;
  setSlideControlsEnabled(currentMode === "slideshow" && generatedSlides.length > 1);
  scheduleNextSlide();
}

function shouldUseSlideshow(state) {
  if (!state || settings?.slideshowEnabled === false || getTouchDisplayMode() === "mirror") {
    return false;
  }
  if (!generatedSlides.length) {
    return false;
  }
  if (!lastActiveAt) {
    return true;
  }
  return Date.now() - lastActiveAt >= getChatReturnTimeoutMs();
}

function isLiveActivityStatus(status) {
  const normalized = normalizeStatus(status);
  return [
    "wake_listening",
    "listening",
    "recognizing",
    "thinking",
    "answering",
    "external_answer",
    "camera_mode",
  ].includes(normalized);
}

function buildActivitySignature(state) {
  if (!state) {
    return "";
  }
  return [
    normalizeStatus(state.status),
    state.text || "",
    state.emoji || "",
    state.touch_image_proxy_url || "",
    state.remote_image_proxy_url || "",
    state.image_revision || "",
  ].join("|");
}

function shouldUseChat(state) {
  if (!state || getTouchDisplayMode() === "mirror") {
    return false;
  }
  return !shouldUseSlideshow(state);
}

function renderChat(state) {
  if (!state) {
    chatEmoji.textContent = "!";
    chatStatusLine.textContent = "Companion not connected";
    chatText.textContent = "Waiting for Whisplay state.";
    return;
  }

  chatModeLabel.textContent = "Chat Text";
  chatEmoji.textContent = state.emoji || "🙂";
  chatStatusLine.textContent = state.status || "Connected";
  chatText.textContent = state.text || "No Whisplay reply text yet.";
  startAutoScroll(chatText);
}

function renderMirror(state) {
  if (!state) {
    hdmiEmoji.textContent = "!";
    hdmiText.textContent = "Waiting for Whisplay state.";
    hdmiImage.hidden = true;
    hdmiImageEmpty.hidden = false;
    lastRemoteImageUrl = "";
    return;
  }

  hdmiEmoji.textContent = state.emoji || "🙂";
  hdmiText.textContent = state.text || "No Whisplay reply text yet.";
  startAutoScroll(hdmiText);

  const remoteImageUrl = state.touch_image_proxy_url
    ? buildTouchRenderUrl(state.touch_image_proxy_url, state.image_revision || "")
    : state.remote_image_proxy_url;
  if (remoteImageUrl) {
    if (remoteImageUrl !== lastRemoteImageUrl) {
      lastRemoteImageUrl = remoteImageUrl;
      hdmiImage.src = remoteImageUrl;
    }
    hdmiImage.hidden = false;
    hdmiImageEmpty.hidden = true;
  } else {
    lastRemoteImageUrl = "";
    hdmiImage.hidden = true;
    hdmiImageEmpty.hidden = false;
  }
}

function updateHeader(state, errorMessage = "") {
  if (!state) {
    hdmiStatus.textContent = errorMessage ? "Companion not connected" : "Waiting for Whisplay...";
    hdmiMeta.textContent = errorMessage || "Save the Whisplay URL in the local Pi3Groq browser UI.";
    return;
  }

  hdmiStatus.textContent = state.status || "Connected";
  const modeLabel = currentMode === "slideshow"
    ? "AI slideshow"
    : currentMode === "chat"
      ? "chat text"
      : "live mirror";
  const remoteBaseUrl = state.remoteBaseUrl || settings?.companionBaseUrl || "";
  const detailParts = [remoteBaseUrl, state.llm_model || "no model", modeLabel].filter(Boolean);
  hdmiMeta.textContent = detailParts.join(" · ");
}

async function loadSettings() {
  const response = await fetch("/api/settings", { cache: "no-store" });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to load Pi3Groq settings.");
  }
  settings = payload.settings || {};
}

function applyRuntimeSettings(nextSettings) {
  if (!nextSettings) {
    return;
  }
  const previousPollInterval = getPollIntervalMs();
  settings = nextSettings;
  applyDisplayLayout();
  if (getPollIntervalMs() !== previousPollInterval) {
    restartStatePolling();
  }
}

async function loadGeneratedImages() {
  if (!lastGeneratedRevision) {
    return;
  }
  const response = await fetch("/api/companion/generated-images?limit=200", {
    cache: "no-store",
  });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to load AI slideshow images.");
  }

  const previousFileName = generatedSlides[currentSlideIndex]?.fileName || "";
  generatedSlides = Array.isArray(payload.photos) ? payload.photos : [];

  const previousIndex = generatedSlides.findIndex((photo) => photo.fileName === previousFileName);
  currentSlideIndex = previousIndex >= 0 ? previousIndex : 0;
  slideshowModeLabel.textContent = `AI Slideshow · ${payload.totalCount || generatedSlides.length}`;
  showSlide(currentSlideIndex);
}

async function refreshHdmi() {
  try {
    const response = await fetch("/api/companion/state", { cache: "no-store" });
    const payload = await response.json();
    if (!response.ok || !payload.ok || !payload.companion) {
      throw new Error(payload.error || "Companion state request failed.");
    }

    applyRuntimeSettings(payload.settings || settings);
    const state = payload.companion;
    const normalizedStatus = normalizeStatus(state.status);
    const nextActivitySignature = buildActivitySignature(state);
    if (isLiveActivityStatus(normalizedStatus) || nextActivitySignature !== lastActivitySignature) {
      lastActiveAt = Date.now();
    }
    lastActivitySignature = nextActivitySignature;

    const nextRevision = String(state.generated_images_revision || "");
    if ((nextRevision && nextRevision !== lastGeneratedRevision) || (!generatedSlides.length && nextRevision)) {
      const previousRevision = lastGeneratedRevision;
      lastGeneratedRevision = nextRevision;
      try {
        await loadGeneratedImages();
      } catch (error) {
        lastGeneratedRevision = previousRevision;
        throw error;
      }
    }

    const previousMode = currentMode;
    let nextMode = "mirror";
    if (getTouchDisplayMode() === "mirror") {
      nextMode = "mirror";
    } else if (shouldUseChat(state)) {
      nextMode = "chat";
    } else if (shouldUseSlideshow(state)) {
      nextMode = "slideshow";
    } else {
      nextMode = "chat";
    }
    setMode(nextMode);
    mirrorModeLabel.textContent = `Mirror Mode · ${state.status || "connected"}`;
    chatModeLabel.textContent = `Chat Text · ${state.status || "connected"}`;
    renderChat(state);
    renderMirror(state);
    if (nextMode === "slideshow" && previousMode !== "slideshow") {
      showSlide(currentSlideIndex);
    }
    updateHeader(state);
  } catch (error) {
    const fallbackMode = getTouchDisplayMode() === "mirror" ? "mirror" : "chat";
    setMode(fallbackMode);
    renderChat(null);
    renderMirror(null);
    updateHeader(null, error instanceof Error ? error.message : "Unknown state request error.");
  }
}

function restartStatePolling() {
  if (statePollTimer) {
    window.clearInterval(statePollTimer);
  }
  statePollTimer = window.setInterval(() => {
    void refreshHdmi();
  }, getPollIntervalMs());
}

prevSlideBtn.addEventListener("click", () => {
  if (!generatedSlides.length) {
    return;
  }
  showSlide(currentSlideIndex - 1);
});

nextSlideBtn.addEventListener("click", () => {
  if (!generatedSlides.length) {
    return;
  }
  showSlide(currentSlideIndex + 1);
});

refreshSlidesBtn.addEventListener("click", async () => {
  try {
    lastGeneratedRevision = "";
    await refreshHdmi();
    if (currentMode === "slideshow") {
      showSlide(currentSlideIndex);
    }
  } catch (error) {
    hdmiMeta.textContent = error instanceof Error ? error.message : "Failed to refresh slideshow.";
  }
});

window.addEventListener("load", async () => {
  try {
    await loadSettings();
    restartStatePolling();
    await refreshHdmi();
  } catch (error) {
    updateHeader(null, error instanceof Error ? error.message : "Failed to load touch display.");
  }
});

window.addEventListener("resize", () => {
  if (currentMode === "slideshow" && generatedSlides.length) {
    showSlide(currentSlideIndex);
    return;
  }
  if (currentMode === "mirror") {
    void refreshHdmi();
  }
});
