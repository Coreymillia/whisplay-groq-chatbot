const hdmiStatus = document.getElementById("hdmiStatus");
const hdmiMeta = document.getElementById("hdmiMeta");
const hdmiEmoji = document.getElementById("hdmiEmoji");
const hdmiText = document.getElementById("hdmiText");
const hdmiImage = document.getElementById("hdmiImage");
const hdmiImageEmpty = document.getElementById("hdmiImageEmpty");

async function refreshHdmi() {
  try {
    const response = await fetch("/api/companion/state", { cache: "no-store" });
    const payload = await response.json();
    if (!payload.ok || !payload.companion) {
      hdmiStatus.textContent = "Companion not connected";
      hdmiMeta.textContent = payload.error || "Save the Whisplay URL in the local Pi3Groq browser UI.";
      hdmiEmoji.textContent = "!";
      hdmiText.textContent = payload.error || "Waiting for Whisplay state.";
      hdmiImage.hidden = true;
      hdmiImageEmpty.hidden = false;
      return;
    }

    const state = payload.companion;
    hdmiStatus.textContent = state.status || "Connected";
    hdmiMeta.textContent = `${state.remoteBaseUrl || payload.settings.companionBaseUrl || ""} · ${state.llm_model || "no model"}`;
    hdmiEmoji.textContent = state.emoji || "🙂";
    hdmiText.textContent = state.text || "No Whisplay reply text yet.";

    if (state.remote_image_proxy_url) {
      hdmiImage.src = state.remote_image_proxy_url;
      hdmiImage.hidden = false;
      hdmiImageEmpty.hidden = true;
    } else {
      hdmiImage.hidden = true;
      hdmiImageEmpty.hidden = false;
    }
  } catch (error) {
    hdmiStatus.textContent = "State request failed";
    hdmiMeta.textContent = error instanceof Error ? error.message : "Unknown error";
  }
}

window.addEventListener("load", () => {
  refreshHdmi();
  window.setInterval(refreshHdmi, 2000);
});
