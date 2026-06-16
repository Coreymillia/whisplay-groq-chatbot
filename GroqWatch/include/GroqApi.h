#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "AppSettings.h"

static const char RD_GROQ_CHAT_URL[] = "https://api.groq.com/openai/v1/chat/completions";
static const char RD_GROQ_WHISPER_HOST[] = "api.groq.com";
static const char RD_GROQ_WHISPER_PATH[] = "/openai/v1/audio/transcriptions";
static const char RD_GROQ_WHISPER_MODEL[] = "whisper-large-v3-turbo";

static String rd_chat_history = "[]";
static size_t rd_message_pairs = 0;
static String rd_last_user_message;
static String rd_last_reply_message;

static void rdRefreshLastMessages() {
    rd_last_user_message = "";
    rd_last_reply_message = "";
    if (!rd_chat_history.length()) return;
    JsonDocument doc;
    if (deserializeJson(doc, rd_chat_history) || !doc.is<JsonArray>()) return;
    for (JsonVariant v : doc.as<JsonArray>()) {
        String role = String(v["role"] | "");
        String content = String(v["content"] | "");
        if (!content.length()) continue;
        if (role == "user") rd_last_user_message = content;
        else if (role == "assistant") rd_last_reply_message = content;
    }
}

static void rdResetChatHistory() {
    rd_chat_history = "[]";
    rd_message_pairs = 0;
    rd_last_user_message = "";
    rd_last_reply_message = "";
}

static void rdAppendHistory(const String &userMsg, const String &replyMsg) {
    JsonDocument doc;
    deserializeJson(doc, rd_chat_history);
    JsonArray arr = doc.to<JsonArray>();
    JsonObject u = arr.add<JsonObject>();
    u["role"] = "user";
    u["content"] = userMsg;
    JsonObject a = arr.add<JsonObject>();
    a["role"] = "assistant";
    a["content"] = replyMsg;
    rd_message_pairs++;
    while (rd_message_pairs > 5 && arr.size() >= 2) {
        arr.remove(0); arr.remove(0);
        rd_message_pairs--;
    }
    rd_chat_history = "";
    serializeJson(arr, rd_chat_history);
    rd_last_user_message = userMsg;
    rd_last_reply_message = replyMsg;
}

static bool rdTranscribeWav(const uint8_t *wavData, size_t wavLen,
                            const char *apiKey, String &transcript, String &error) {
    transcript = "";
    error = "";
    if (!wavData || !wavLen) { error = "No audio data."; return false; }
    if (!apiKey || !apiKey[0]) { error = "No Groq key."; return false; }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30);
    if (!client.connect(RD_GROQ_WHISPER_HOST, 443)) {
        error = "Groq connect failed."; return false;
    }

    String boundary = "GroqWatchBoundary";
    String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n";
    head += RD_GROQ_WHISPER_MODEL;
    head += "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = head.length() + wavLen + tail.length();

    client.println(String("POST ") + RD_GROQ_WHISPER_PATH + " HTTP/1.1");
    client.println(String("Host: ") + RD_GROQ_WHISPER_HOST);
    client.println(String("Authorization: Bearer ") + apiKey);
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(totalLen));
    client.println("Connection: close");
    client.println();
    client.print(head);
    for (size_t off = 0; off < wavLen; off += 1024) {
        size_t chunk = min((size_t)1024, wavLen - off);
        client.write(wavData + off, chunk); delay(1);
    }
    client.print(tail);

    String raw, body;
    bool headersDone = false;
    unsigned long t = millis();
    while (client.connected() && millis() - t < 30000) {
        while (client.available()) {
            char c = (char)client.read();
            if (!headersDone) { raw += c; if (raw.endsWith("\r\n\r\n")) { headersDone = true; raw = ""; } }
            else body += c;
            t = millis();
        }
        delay(1);
    }
    client.stop();

    JsonDocument doc;
    if (deserializeJson(doc, body)) { error = "Parse failed."; return false; }
    transcript = String(doc["text"] | "");
    transcript.trim();
    if (!transcript.length()) {
        error = String(doc["error"]["message"] | "");
        if (!error.length()) error = "Message not heard.";
        return false;
    }
    return true;
}

static bool rdSendChat(const String &userMsg, const String &apiKey,
                       const String &model, const String &persona,
                       String &reply, String &error) {
    reply = "";
    error = "";
    if (!userMsg.length()) { error = "Empty message."; return false; }
    if (!apiKey.length()) { error = "No Groq key."; return false; }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30);
    HTTPClient http;
    http.begin(client, RD_GROQ_CHAT_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + apiKey);
    http.setTimeout(30000);

    JsonDocument doc;
    JsonArray msgs = doc["messages"].to<JsonArray>();
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = persona.length() ? persona : "You are a helpful wrist-worn assistant. Keep replies short.";

    if (rd_chat_history.length() && rd_chat_history != "[]") {
        JsonDocument hist;
        if (!deserializeJson(hist, rd_chat_history) && hist.is<JsonArray>())
            for (JsonVariant v : hist.as<JsonArray>()) msgs.add(v);
    }
    JsonObject usr = msgs.add<JsonObject>();
    usr["role"] = "user";
    usr["content"] = userMsg;
    doc["model"] = model.length() ? model : "llama-3.1-8b-instant";
    doc["temperature"] = 0.7;
    doc["max_tokens"] = 256;

    String payload;
    serializeJson(doc, payload);
    int code = http.POST(payload);
    String resp = http.getString();
    http.end();
    client.stop();

    if (code < 200 || code >= 300) { error = resp.length() ? resp : "HTTP " + String(code); return false; }

    JsonDocument rdoc;
    if (deserializeJson(rdoc, resp)) { error = "Parse failed."; return false; }
    reply = String(rdoc["choices"][0]["message"]["content"] | "");
    reply.trim();
    if (!reply.length()) { error = "Empty reply."; return false; }

    rdAppendHistory(userMsg, reply);
    return true;
}
