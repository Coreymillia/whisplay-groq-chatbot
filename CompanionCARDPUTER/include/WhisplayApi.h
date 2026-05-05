#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ArduinoWebsockets.h>

#include "Portal.h"

using namespace websockets;

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
  if (!buffer || bufferSize == 0 || !path || !cp_pi_host[0]) {
    return false;
  }
  snprintf(buffer, bufferSize, "http://%s:%u%s", cp_pi_host, cp_pi_port, path);
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
  http.setTimeout(10000);
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

static bool apiSendText(const char *text) {
  if (!text || !text[0]) {
    return false;
  }

  char wsUrl[192];
  snprintf(wsUrl, sizeof(wsUrl), "ws://%s:%u/ws", cp_pi_host, cp_pi_port);

  JsonDocument wsDoc;
  wsDoc["type"] = "text_input";
  wsDoc["text"] = text;
  String wsBody;
  serializeJson(wsDoc, wsBody);

  WebsocketsClient wsClient;
  if (wsClient.connect(wsUrl)) {
    bool sent = wsClient.send(wsBody);
    wsClient.poll();
    wsClient.close();
    if (sent) {
      return true;
    }
  }

  JsonDocument doc;
  doc["text"] = text;
  String body;
  serializeJson(doc, body);
  return apiPostJson("/api/input/text", body.c_str());
}

static bool apiRepeatLastAnswer() {
  return apiPostJson("/api/companion/action", "{\"action\":\"repeat\"}");
}

static bool apiResetChat() {
  return apiPostJson("/api/chat/reset", "{}");
}

static bool apiSendAudioWav(const uint8_t *data, size_t length, String &transcriptOut) {
  char url[192];
  if (!data || length == 0 || !apiBuildUrl(url, sizeof(url), "/api/input/audio")) {
    return false;
  }

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "audio/wav");
  http.setTimeout(45000);
  int code = http.sendRequest("POST", const_cast<uint8_t *>(data), length);
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    transcriptOut = response;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    transcriptOut = response;
    return false;
  }
  transcriptOut = String(doc["transcript"] | "");
  return (doc["ok"] | false) && transcriptOut.length() > 0;
}
