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
static const size_t GP_MAX_HISTORY_PAIRS = 5;

static String gp_chat_history = "[]";
static size_t gp_message_pairs = 0;

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

  JsonDocument historyDoc;
  deserializeJson(historyDoc, gp_chat_history);
  JsonArray history = historyDoc.to<JsonArray>();

  JsonObject userEntry = history.add<JsonObject>();
  userEntry["role"] = "user";
  userEntry["content"] = userMessage;

  JsonObject assistantEntry = history.add<JsonObject>();
  assistantEntry["role"] = "assistant";
  assistantEntry["content"] = replyOut;

  gp_message_pairs += 1;
  while (gp_message_pairs > GP_MAX_HISTORY_PAIRS && history.size() >= 2) {
    history.remove(0);
    history.remove(0);
    gp_message_pairs -= 1;
  }

  gp_chat_history = "";
  serializeJson(history, gp_chat_history);
  gpPersistChatHistory();
  return true;
}
