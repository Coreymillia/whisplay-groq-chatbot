#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include "GroqPortal.h"

static const char GP_GROQ_CHAT_URL[] = "https://api.groq.com/openai/v1/chat/completions";
static const char GP_GROQ_WHISPER_HOST[] = "api.groq.com";
static const char GP_GROQ_WHISPER_PATH[] = "/openai/v1/audio/transcriptions";
static const char GP_GROQ_WHISPER_MODEL[] = "whisper-large-v3-turbo";
static const size_t GP_MAX_HISTORY_PAIRS = 12;
static const unsigned long GP_PEER_REPLY_TIMEOUT_MS = 45000;
static const unsigned long GP_PEER_POLL_INTERVAL_MS = 700;
static const unsigned long GP_WEATHER_CACHE_MS = 10UL * 60UL * 1000UL;

static String gp_chat_history = "[]";
static size_t gp_message_pairs = 0;
static String gp_cached_weather_key = "";
static String gp_cached_weather_summary = "";
static unsigned long gp_cached_weather_at_ms = 0;

static void gpLoadChatHistory() {
  Preferences prefs;
  prefs.begin("groqputer", true);
  gp_chat_history = prefs.getString("chatHistory", "[]");
  gp_message_pairs = prefs.getUInt("msgPairs", 0);
  prefs.end();
}

static void gpPersistChatHistory() {
  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putString("chatHistory", gp_chat_history);
  prefs.putUInt("msgPairs", gp_message_pairs);
  prefs.end();
}

static void gpResetChatHistory() {
  gp_chat_history = "[]";
  gp_message_pairs = 0;
  gpPersistChatHistory();
}

static void gpAppendChatHistoryPair(const String &userMessage, const String &replyMessage) {
  JsonDocument historyDoc;
  deserializeJson(historyDoc, gp_chat_history);
  JsonArray history = historyDoc.as<JsonArray>();
  if (history.isNull()) {
    history = historyDoc.to<JsonArray>();
  }

  JsonObject userEntry = history.add<JsonObject>();
  userEntry["role"] = "user";
  userEntry["content"] = userMessage;

  JsonObject assistantEntry = history.add<JsonObject>();
  assistantEntry["role"] = "assistant";
  assistantEntry["content"] = replyMessage;

  gp_message_pairs += 1;
  while (gp_message_pairs > GP_MAX_HISTORY_PAIRS && history.size() >= 2) {
    history.remove(0);
    history.remove(0);
    gp_message_pairs -= 1;
  }

  gp_chat_history = "";
  serializeJson(history, gp_chat_history);
  gpPersistChatHistory();
}

static String gpNormalizePeerUrl(const String &value) {
  String normalized = value;
  normalized.trim();
  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }
  return normalized;
}

static bool gpFetchPeerState(
  const String &peerBaseUrl,
  String &statusOut,
  String &textOut,
  String &errorOut
) {
  statusOut = "";
  textOut = "";
  errorOut = "";

  HTTPClient http;
  if (!http.begin(peerBaseUrl + "/api/state")) {
    errorOut = "Peer state request failed.";
    return false;
  }
  http.setTimeout(12000);
  int code = http.GET();
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("Peer state HTTP ") + code;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    errorOut = "Peer state parse failed.";
    return false;
  }

  bool ready = doc["ready"] | false;
  if (!ready) {
    errorOut = "Peer is not ready.";
    return false;
  }

  statusOut = String(doc["status"] | "");
  textOut = String(doc["text"] | "");
  textOut.trim();
  return true;
}

static bool gpSendPeerChatMessage(const String &userMessage, String &replyOut, String &errorOut) {
  replyOut = "";
  errorOut = "";
  if (!gp_peer_mode_enabled) {
    errorOut = "Connected device mode is off.";
    return false;
  }
  if (!gpPeerSettingsReady()) {
    errorOut = "Connected device URLs are not set.";
    return false;
  }

  const String peerBaseUrl = gpNormalizePeerUrl(String(gp_connected_device_url));
  if (!peerBaseUrl.length()) {
    errorOut = "Connected device URL is empty.";
    return false;
  }

  String initialStatus;
  String initialText;
  String stateError;
  gpFetchPeerState(peerBaseUrl, initialStatus, initialText, stateError);

  HTTPClient http;
  if (!http.begin(peerBaseUrl + "/api/input/text")) {
    errorOut = "Connected device request failed.";
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  JsonDocument requestDoc;
  requestDoc["text"] = userMessage;
  String payload;
  serializeJson(requestDoc, payload);
  int code = http.POST(payload);
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("Connected device HTTP ") + code;
    return false;
  }

  JsonDocument responseDoc;
  if (!deserializeJson(responseDoc, response) && (responseDoc["ok"] | true) == false) {
    errorOut = String(responseDoc["error"] | "Connected device rejected the message.");
    return false;
  }

  String latestReply;
  unsigned long deadline = millis() + GP_PEER_REPLY_TIMEOUT_MS;
  while (millis() < deadline) {
    delay(GP_PEER_POLL_INTERVAL_MS);

    String status;
    String text;
    if (!gpFetchPeerState(peerBaseUrl, status, text, stateError)) {
      continue;
    }

    if (text.length() && text != initialText) {
      latestReply = text;
    }

    if (latestReply.length() && !status.equalsIgnoreCase("answering")) {
      replyOut = latestReply;
      gpAppendChatHistoryPair(userMessage, replyOut);
      return true;
    }
  }

  if (latestReply.length()) {
    replyOut = latestReply;
    gpAppendChatHistoryPair(userMessage, replyOut);
    return true;
  }

  errorOut = stateError.length() ? stateError : "Connected device timed out.";
  return false;
}

static bool gpTranscribeWav(const uint8_t *wavData, size_t wavLength, String &transcriptOut, String &errorOut) {
  transcriptOut = "";
  errorOut = "";
  if (!wavData || wavLength == 0) {
    errorOut = "No audio data.";
    return false;
  }
  if (gp_groq_api_key[0] == '\0') {
    errorOut = "No Groq API key.";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);
  if (!client.connect(GP_GROQ_WHISPER_HOST, 443)) {
    errorOut = "Groq connection failed.";
    return false;
  }

  String boundary = "GroqputerBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  head += GP_GROQ_WHISPER_MODEL;
  head += "\r\n--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  size_t totalLen = head.length() + wavLength + tail.length();

  client.println(String("POST ") + GP_GROQ_WHISPER_PATH + " HTTP/1.1");
  client.println(String("Host: ") + GP_GROQ_WHISPER_HOST);
  client.println(String("Authorization: Bearer ") + gp_groq_api_key);
  client.println(String("Content-Type: multipart/form-data; boundary=") + boundary);
  client.println(String("Content-Length: ") + totalLen);
  client.println("Connection: close");
  client.println();

  client.print(head);
  for (size_t offset = 0; offset < wavLength; offset += 1024) {
    size_t chunk = min(static_cast<size_t>(1024), wavLength - offset);
    client.write(wavData + offset, chunk);
    delay(1);
  }
  client.print(tail);

  String raw;
  String body;
  bool headersDone = false;
  unsigned long timeoutAt = millis();
  while (client.connected() && millis() - timeoutAt < 30000) {
    while (client.available()) {
      char c = static_cast<char>(client.read());
      if (!headersDone) {
        raw += c;
        if (raw.endsWith("\r\n\r\n")) {
          headersDone = true;
          raw = "";
        }
      } else {
        body += c;
      }
      timeoutAt = millis();
    }
    delay(1);
  }
  client.stop();

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    errorOut = "Transcription parse failed.";
    return false;
  }
  transcriptOut = String(doc["text"] | "");
  if (!transcriptOut.length()) {
    String groqError = String(doc["error"]["message"] | "");
    errorOut = groqError.length() ? groqError : "Message not heard.";
    return false;
  }
  return true;
}

static String gpCompactWhitespace(const String &value) {
  String output;
  output.reserve(value.length());
  bool previousWasSpace = false;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '\r' || c == '\n' || c == '\t') {
      c = ' ';
    }
    if (c == ' ') {
      if (!previousWasSpace) {
        output += c;
      }
      previousWasSpace = true;
      continue;
    }
    previousWasSpace = false;
    output += c;
  }
  output.trim();
  return output;
}

static bool gpNwsGet(const String &url, String &bodyOut, String &errorOut) {
  bodyOut = "";
  errorOut = "";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);

  HTTPClient http;
  if (!http.begin(client, url)) {
    errorOut = "NWS request setup failed.";
    return false;
  }
  http.addHeader("User-Agent", "Groqputer/1.0 weather feature");
  http.addHeader("Accept", "application/geo+json, application/json;q=0.9");
  http.setTimeout(15000);
  int code = http.GET();
  bodyOut = http.getString();
  http.end();
  client.stop();

  if (code < 200 || code >= 300) {
    String detail = gpCompactWhitespace(bodyOut);
    if (!detail.length()) {
      detail = String("HTTP ") + code;
    }
    errorOut = detail;
    return false;
  }
  if (!bodyOut.length()) {
    errorOut = "NWS returned an empty response.";
    return false;
  }
  return true;
}

static String gpSummarizeForecastPeriods(JsonArray periods) {
  String summary;
  size_t count = 0;
  for (JsonVariant periodValue : periods) {
    if (count >= 4) break;
    JsonObject period = periodValue.as<JsonObject>();
    String name = String(period["name"] | "Forecast");
    String details = gpCompactWhitespace(String(period["detailedForecast"] | ""));
    int temperature = period["temperature"] | 0;
    String temperatureUnit = String(period["temperatureUnit"] | "F");
    String windSpeed = gpCompactWhitespace(String(period["windSpeed"] | ""));
    String windDirection = gpCompactWhitespace(String(period["windDirection"] | ""));
    String line = name + ": " + String(temperature) + "\xB0" + temperatureUnit + ".";
    if (details.length()) {
      line += " " + details;
    }
    if (windSpeed.length()) {
      line += " Wind " + windSpeed;
      if (windDirection.length()) {
        line += " " + windDirection;
      }
      line += ".";
    }
    if (summary.length()) summary += "\n";
    summary += line;
    count += 1;
  }
  return summary;
}

static String gpSummarizeAlerts(JsonArray features) {
  size_t alertCount = 0;
  String summary;
  for (JsonVariant featureValue : features) {
    if (alertCount >= 3) break;
    JsonObject properties = featureValue["properties"].as<JsonObject>();
    String event = gpCompactWhitespace(String(properties["event"] | "Alert"));
    String headline = gpCompactWhitespace(String(properties["headline"] | ""));
    String urgency = gpCompactWhitespace(String(properties["urgency"] | ""));
    String certainty = gpCompactWhitespace(String(properties["certainty"] | ""));
    String description = gpCompactWhitespace(String(properties["description"] | ""));
    if (description.length() > 220) {
      description = description.substring(0, 220);
      description += "...";
    }
    String line = event;
    if (headline.length()) line += " - " + headline;
    if (urgency.length()) line += " [" + urgency + "]";
    if (certainty.length()) line += " (" + certainty + ")";
    if (description.length()) line += ". " + description;
    if (summary.length()) summary += "\n";
    summary += line;
    alertCount += 1;
  }
  if (!alertCount) {
    return "No active weather alerts.";
  }
  return summary;
}

static bool gpFetchWeatherSummary(String &weatherSummaryOut, String &errorOut, bool forceRefresh = false) {
  weatherSummaryOut = "";
  errorOut = "";

  double latitude = 0.0;
  double longitude = 0.0;
  if (!gpGetWeatherCoordinates(latitude, longitude)) {
    errorOut = "Set weather latitude and longitude in setup first.";
    return false;
  }

  char latBuffer[24];
  char lonBuffer[24];
  snprintf(latBuffer, sizeof(latBuffer), "%.4f", latitude);
  snprintf(lonBuffer, sizeof(lonBuffer), "%.4f", longitude);
  String cacheKey = String(latBuffer) + "," + String(lonBuffer);
  unsigned long now = millis();
  if (
    !forceRefresh &&
    gp_cached_weather_summary.length() &&
    gp_cached_weather_key == cacheKey &&
    now - gp_cached_weather_at_ms < GP_WEATHER_CACHE_MS
  ) {
    weatherSummaryOut = gp_cached_weather_summary;
    return true;
  }

  String pointsBody;
  if (!gpNwsGet(String("https://api.weather.gov/points/") + latBuffer + "," + lonBuffer, pointsBody, errorOut)) {
    if (errorOut.startsWith("HTTP 404")) {
      errorOut = "NWS has no forecast data for the saved latitude/longitude.";
    } else if (errorOut.length()) {
      errorOut = "NWS points lookup failed: " + errorOut;
    } else {
      errorOut = "NWS points lookup failed.";
    }
    return false;
  }

  JsonDocument pointsDoc;
  if (deserializeJson(pointsDoc, pointsBody)) {
    errorOut = "NWS points response parse failed.";
    return false;
  }
  String forecastUrl = String(pointsDoc["properties"]["forecast"] | "");
  forecastUrl.trim();
  if (!forecastUrl.length()) {
    errorOut = "NWS forecast URL was unavailable for this location.";
    return false;
  }

  String city = gpCompactWhitespace(String(pointsDoc["properties"]["relativeLocation"]["properties"]["city"] | ""));
  String state = gpCompactWhitespace(String(pointsDoc["properties"]["relativeLocation"]["properties"]["state"] | ""));
  String locationLabel = city.length() ? city : cacheKey;
  if (state.length()) {
    locationLabel += ", " + state;
  }

  String forecastBody;
  if (!gpNwsGet(forecastUrl, forecastBody, errorOut)) {
    errorOut = errorOut.length() ? "NWS forecast lookup failed: " + errorOut : "NWS forecast lookup failed.";
    return false;
  }
  JsonDocument forecastDoc;
  if (deserializeJson(forecastDoc, forecastBody)) {
    errorOut = "NWS forecast response parse failed.";
    return false;
  }
  JsonArray periods = forecastDoc["properties"]["periods"].as<JsonArray>();
  if (periods.isNull() || periods.size() == 0) {
    errorOut = "NWS forecast periods were unavailable.";
    return false;
  }

  String alertsBody;
  if (!gpNwsGet(String("https://api.weather.gov/alerts/active?point=") + latBuffer + "," + lonBuffer, alertsBody, errorOut)) {
    errorOut = errorOut.length() ? "NWS alerts lookup failed: " + errorOut : "NWS alerts lookup failed.";
    return false;
  }
  JsonDocument alertsDoc;
  if (deserializeJson(alertsDoc, alertsBody)) {
    errorOut = "NWS alerts response parse failed.";
    return false;
  }
  JsonArray alertFeatures = alertsDoc["features"].as<JsonArray>();

  String forecastText = gpSummarizeForecastPeriods(periods);
  String alertsText = gpSummarizeAlerts(alertFeatures);
  weatherSummaryOut =
    "Location: " + locationLabel +
    "\n\nForecast:\n" + forecastText +
    "\n\nAlerts:\n" + alertsText;

  gp_cached_weather_key = cacheKey;
  gp_cached_weather_summary = weatherSummaryOut;
  gp_cached_weather_at_ms = now;
  return true;
}

static bool gpSendWeatherChatMessage(
  const String &userPrompt,
  const String &weatherSummary,
  String &replyOut,
  String &errorOut
) {
  replyOut = "";
  errorOut = "";
  if (!userPrompt.length()) {
    errorOut = "Empty weather prompt.";
    return false;
  }
  if (gp_groq_api_key[0] == '\0') {
    errorOut = "No Groq API key.";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  HTTPClient http;
  http.begin(client, GP_GROQ_CHAT_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + gp_groq_api_key);
  http.setTimeout(30000);

  JsonDocument doc;
  JsonArray messages = doc["messages"].to<JsonArray>();

  JsonObject system = messages.add<JsonObject>();
  system["role"] = "system";
  system["content"] =
    String(gp_personality_prompt.length() ? gp_personality_prompt : GP_DEFAULT_PERSONALITY) + "\n" +
    "You are answering a weather question using supplied NOAA/NWS forecast data. " +
    "Stay concise, practical, and in character. Use only the supplied weather data. " +
    "If alerts exist, mention them clearly. Do not invent extra forecast details.";

  JsonObject user = messages.add<JsonObject>();
  user["role"] = "user";
  user["content"] = "Weather data:\n" + weatherSummary + "\n\nUser question: " + userPrompt;

  doc["model"] = gp_model[0] ? gp_model : GP_DEFAULT_MODEL;
  doc["temperature"] = 0.7;
  doc["max_tokens"] = 300;

  String payload;
  serializeJson(doc, payload);
  int code = http.POST(payload);
  String response = http.getString();
  http.end();
  client.stop();
  delay(100);

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("HTTP ") + code;
    return false;
  }

  JsonDocument responseDoc;
  if (deserializeJson(responseDoc, response)) {
    errorOut = "Groq weather response parse failed.";
    return false;
  }
  replyOut = String(responseDoc["choices"][0]["message"]["content"] | "");
  if (!replyOut.length()) {
    errorOut = "Empty Groq weather reply.";
    return false;
  }

  gpAppendChatHistoryPair(userPrompt, replyOut);
  return true;
}

static bool gpSendChatMessage(const String &userMessage, String &replyOut, String &errorOut) {
  replyOut = "";
  errorOut = "";
  if (!userMessage.length()) {
    errorOut = "Empty message.";
    return false;
  }
  if (gp_groq_api_key[0] == '\0') {
    errorOut = "No Groq API key.";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  HTTPClient http;
  http.begin(client, GP_GROQ_CHAT_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + gp_groq_api_key);
  http.setTimeout(30000);

  JsonDocument doc;
  JsonArray messages = doc["messages"].to<JsonArray>();

  JsonObject system = messages.add<JsonObject>();
  system["role"] = "system";
  system["content"] = gp_personality_prompt.length() ? gp_personality_prompt : GP_DEFAULT_PERSONALITY;

  if (gp_chat_history.length()) {
    JsonDocument historyDoc;
    if (!deserializeJson(historyDoc, gp_chat_history) && historyDoc.is<JsonArray>()) {
      for (JsonVariant value : historyDoc.as<JsonArray>()) {
        messages.add(value);
      }
    }
  }

  JsonObject user = messages.add<JsonObject>();
  user["role"] = "user";
  user["content"] = userMessage;

  doc["model"] = gp_model[0] ? gp_model : GP_DEFAULT_MODEL;
  doc["temperature"] = 0.7;
  doc["max_tokens"] = 300;

  String payload;
  serializeJson(doc, payload);
  int code = http.POST(payload);
  String response = http.getString();
  http.end();
  client.stop();
  delay(100);

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("HTTP ") + code;
    return false;
  }

  JsonDocument responseDoc;
  if (deserializeJson(responseDoc, response)) {
    errorOut = "Groq response parse failed.";
    return false;
  }
  replyOut = String(responseDoc["choices"][0]["message"]["content"] | "");
  if (!replyOut.length()) {
    errorOut = "Empty Groq reply.";
    return false;
  }

  gpAppendChatHistoryPair(userMessage, replyOut);
  return true;
}
