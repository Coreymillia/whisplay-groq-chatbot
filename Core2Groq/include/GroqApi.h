#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include "Portal.h"

static const char RD_GROQ_CHAT_URL[] = "https://api.groq.com/openai/v1/chat/completions";
static const char RD_GROQ_WHISPER_HOST[] = "api.groq.com";
static const char RD_GROQ_WHISPER_PATH[] = "/openai/v1/audio/transcriptions";
static const char RD_GROQ_WHISPER_MODEL[] = "whisper-large-v3-turbo";
static const size_t RD_MAX_HISTORY_PAIRS = 5;

static String rd_chat_history = "[]";
static size_t rd_message_pairs = 0;
static String rd_last_user_message;
static String rd_last_reply_message;

static void rdRefreshLastMessagesFromHistory() {
    rd_last_user_message = "";
    rd_last_reply_message = "";
    if (!rd_chat_history.length()) return;

    JsonDocument doc;
    if (deserializeJson(doc, rd_chat_history) || !doc.is<JsonArray>()) {
        return;
    }

    for (JsonVariant value : doc.as<JsonArray>()) {
        String role = String(value["role"] | "");
        String content = String(value["content"] | "");
        if (!content.length()) continue;
        if (role == "user") {
            rd_last_user_message = content;
        } else if (role == "assistant") {
            rd_last_reply_message = content;
        }
    }
}

static void rdLoadChatHistory() {
    Preferences prefs;
    prefs.begin("core2groq", true);
    rd_chat_history = prefs.getString("chatHistory", "[]");
    rd_message_pairs = prefs.getUInt("msgPairs", 0);
    prefs.end();
    rdRefreshLastMessagesFromHistory();
}

static void rdPersistChatHistory() {
    Preferences prefs;
    prefs.begin("core2groq", false);
    prefs.putString("chatHistory", rd_chat_history);
    prefs.putUInt("msgPairs", rd_message_pairs);
    prefs.end();
}

static void rdResetChatHistory() {
    rd_chat_history = "[]";
    rd_message_pairs = 0;
    rd_last_user_message = "";
    rd_last_reply_message = "";
    rdPersistChatHistory();
}

static void rdAppendChatHistoryPair(const String &userMessage, const String &replyMessage) {
    JsonDocument historyDoc;
    deserializeJson(historyDoc, rd_chat_history);
    JsonArray history = historyDoc.to<JsonArray>();

    JsonObject userEntry = history.add<JsonObject>();
    userEntry["role"] = "user";
    userEntry["content"] = userMessage;

    JsonObject assistantEntry = history.add<JsonObject>();
    assistantEntry["role"] = "assistant";
    assistantEntry["content"] = replyMessage;

    rd_message_pairs += 1;
    while (rd_message_pairs > RD_MAX_HISTORY_PAIRS && history.size() >= 2) {
        history.remove(0);
        history.remove(0);
        rd_message_pairs -= 1;
    }

    rd_chat_history = "";
    serializeJson(history, rd_chat_history);
    rd_last_user_message = userMessage;
    rd_last_reply_message = replyMessage;
    rdPersistChatHistory();
}

static bool rdTranscribeWav(const uint8_t *wavData, size_t wavLength,
                            String &transcriptOut, String &errorOut) {
    transcriptOut = "";
    errorOut = "";
    if (!wavData || wavLength == 0) {
        errorOut = "No audio data.";
        return false;
    }
    if (!rdHasBotSettingsReady()) {
        errorOut = "Add Groq key in setup.";
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30);
    if (!client.connect(RD_GROQ_WHISPER_HOST, 443)) {
        errorOut = "Groq connection failed.";
        return false;
    }

    String boundary = "Core2GroqBoundary";
    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    head += RD_GROQ_WHISPER_MODEL;
    head += "\r\n--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
    head += "Content-Type: audio/wav\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = head.length() + wavLength + tail.length();

    client.println(String("POST ") + RD_GROQ_WHISPER_PATH + " HTTP/1.1");
    client.println(String("Host: ") + RD_GROQ_WHISPER_HOST);
    client.println(String("Authorization: Bearer ") + rd_groq_api_key);
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
    transcriptOut.trim();
    if (!transcriptOut.length()) {
        String groqError = String(doc["error"]["message"] | "");
        errorOut = groqError.length() ? groqError : "Message not heard.";
        return false;
    }
    return true;
}

static bool rdSendChatMessage(const String &userMessage, String &replyOut, String &errorOut) {
    replyOut = "";
    errorOut = "";
    if (!userMessage.length()) {
        errorOut = "Empty message.";
        return false;
    }
    if (!rdHasBotSettingsReady()) {
        errorOut = "Add Groq key in setup.";
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30);

    HTTPClient http;
    http.begin(client, RD_GROQ_CHAT_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + rd_groq_api_key);
    http.setTimeout(30000);

    JsonDocument doc;
    JsonArray messages = doc["messages"].to<JsonArray>();

    JsonObject system = messages.add<JsonObject>();
    system["role"] = "system";
    system["content"] =
        rd_personality_prompt[0] ? rd_personality_prompt : RD_DEFAULT_PERSONALITY;

    if (rd_chat_history.length()) {
        JsonDocument historyDoc;
        if (!deserializeJson(historyDoc, rd_chat_history) && historyDoc.is<JsonArray>()) {
            for (JsonVariant value : historyDoc.as<JsonArray>()) {
                messages.add(value);
            }
        }
    }

    JsonObject user = messages.add<JsonObject>();
    user["role"] = "user";
    user["content"] = userMessage;

    doc["model"] = rd_groq_model[0] ? rd_groq_model : RD_DEFAULT_MODEL;
    doc["temperature"] = 0.7;
    doc["max_tokens"] = 320;

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
    replyOut.trim();
    if (!replyOut.length()) {
        errorOut = "Empty Groq reply.";
        return false;
    }

    rdAppendChatHistoryPair(userMessage, replyOut);
    return true;
}
