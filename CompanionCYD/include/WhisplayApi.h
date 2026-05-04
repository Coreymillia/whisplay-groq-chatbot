#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Portal.h"

struct CompanionState {
  bool ready = false;
  bool textInputEnabled = false;
  bool ragIconVisible = false;
  bool imageIconVisible = false;
  String status;
  String text;
  String emoji;
  uint32_t lastUpdateMs = 0;
};

struct CompanionSettings {
  bool loaded = false;
  String voiceMode = "text-only";
  String personalityPresetId = "custom";
};

static bool apiBuildUrl(char *buffer, size_t bufferSize, const char *path) {
  if (!buffer || bufferSize == 0 || !path || !cc_pi_host[0]) {
    return false;
  }
  snprintf(buffer, bufferSize, "http://%s:%u%s", cc_pi_host, cc_pi_port, path);
  return true;
}

static bool apiPostJson(const char *path, const char *jsonBody, String *responseOut = nullptr) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), path)) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);
  int code = http.POST(jsonBody ? jsonBody : "{}");
  if (responseOut) {
    *responseOut = http.getString();
  } else {
    http.getString();
  }
  http.end();
  return code >= 200 && code < 300;
}

static bool apiFetchState(CompanionState &state) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), "/api/state")) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    return false;
  }

  state.ready = doc["ready"] | false;
  state.status = String(doc["status"] | "");
  state.text = String(doc["text"] | "");
  state.emoji = String(doc["emoji"] | "");
  state.textInputEnabled = doc["text_input_enabled"] | false;
  state.ragIconVisible = doc["rag_icon_visible"] | false;
  state.imageIconVisible = doc["image_icon_visible"] | false;
  state.lastUpdateMs = millis();
  return true;
}

static bool apiFetchSettings(CompanionSettings &settings) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), "/api/settings")) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    return false;
  }
  JsonObject settingsObj = doc["settings"].as<JsonObject>();
  if (settingsObj.isNull()) {
    return false;
  }

  settings.voiceMode = String(settingsObj["voiceMode"] | "text-only");
  settings.personalityPresetId = String(settingsObj["personalityPresetId"] | "custom");
  settings.loaded = true;
  return true;
}

static const char *apiNextVoiceMode(const String &currentMode) {
  if (currentMode == "text-only") {
    return "speak-on-demand";
  }
  if (currentMode == "speak-on-demand") {
    return "voice-chat";
  }
  return "text-only";
}

static bool apiSetVoiceMode(const char *voiceMode) {
  JsonDocument doc;
  doc["voiceMode"] = voiceMode;
  String body;
  serializeJson(doc, body);
  return apiPostJson("/api/settings", body.c_str());
}

static bool apiCycleVoiceMode(CompanionSettings &settings) {
  const char *nextMode = apiNextVoiceMode(settings.voiceMode);
  if (!apiSetVoiceMode(nextMode)) {
    return false;
  }
  settings.voiceMode = String(nextMode);
  settings.loaded = true;
  return true;
}

static bool apiResetChat() {
  return apiPostJson("/api/chat/reset", "{}");
}

static bool apiRepeatLastAnswer() {
  return apiPostJson("/api/companion/action", "{\"action\":\"repeat\"}");
}

static bool apiCaptureVision() {
  return apiPostJson("/api/vision/capture", "{}");
}

static bool apiSendText(const char *text) {
  JsonDocument doc;
  doc["text"] = text;
  String body;
  serializeJson(doc, body);
  return apiPostJson("/api/input/text", body.c_str());
}
