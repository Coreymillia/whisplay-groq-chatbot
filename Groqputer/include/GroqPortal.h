#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

static const char GP_AP_SSID[] = "Groqputer-Setup";
static const char GP_DEFAULT_MODEL[] = "llama-3.1-8b-instant";
static const char GP_DEFAULT_PERSONALITY[] =
  "You are a compact, helpful Groq-powered Cardputer assistant. Keep replies concise, clear, and friendly.";
static const uint8_t GP_DEFAULT_RECORD_SECONDS = 5;
static const uint8_t GP_MIN_RECORD_SECONDS = 2;
static const uint8_t GP_MAX_RECORD_SECONDS = 15;

static char gp_wifi_ssid[64] = "";
static char gp_wifi_pass[64] = "";
static char gp_groq_api_key[128] = "";
static char gp_model[64] = "";
static uint8_t gp_record_seconds = GP_DEFAULT_RECORD_SECONDS;
static uint8_t gp_text_scale = 1;
static String gp_personality_prompt = GP_DEFAULT_PERSONALITY;
static bool gp_has_settings = false;
static unsigned long gp_last_wifi_retry_ms = 0;
static const unsigned long GP_WIFI_RETRY_INTERVAL_MS = 10000;

static WebServer *gp_portal_server = nullptr;
static DNSServer *gp_portal_dns = nullptr;

static uint8_t gpClampRecordSeconds(int value) {
  if (value < GP_MIN_RECORD_SECONDS) return GP_MIN_RECORD_SECONDS;
  if (value > GP_MAX_RECORD_SECONDS) return GP_MAX_RECORD_SECONDS;
  return static_cast<uint8_t>(value);
}

static String gpEscapeHtml(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("\"", "&quot;");
  escaped.replace("'", "&#39;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

static void gpLoadSettings() {
  Preferences prefs;
  prefs.begin("groqputer", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String apiKey = prefs.getString("groqKey", "");
  String model = prefs.getString("model", GP_DEFAULT_MODEL);
  String personality = prefs.getString("personality", GP_DEFAULT_PERSONALITY);
  gp_record_seconds = gpClampRecordSeconds(
    static_cast<int>(prefs.getUChar("recordSec", GP_DEFAULT_RECORD_SECONDS))
  );
  gp_text_scale = prefs.getUChar("txtsz", 1);
  prefs.end();

  ssid.toCharArray(gp_wifi_ssid, sizeof(gp_wifi_ssid));
  pass.toCharArray(gp_wifi_pass, sizeof(gp_wifi_pass));
  apiKey.toCharArray(gp_groq_api_key, sizeof(gp_groq_api_key));
  model.toCharArray(gp_model, sizeof(gp_model));
  if (gp_model[0] == '\0') {
    strlcpy(gp_model, GP_DEFAULT_MODEL, sizeof(gp_model));
  }
  gp_personality_prompt = personality.length() ? personality : GP_DEFAULT_PERSONALITY;
  if (gp_text_scale < 1) {
    gp_text_scale = 1;
  } else if (gp_text_scale > 3) {
    gp_text_scale = 3;
  }
  gp_has_settings = gp_wifi_ssid[0] != '\0' && gp_groq_api_key[0] != '\0';
}

static void gpSaveSettings(
  const char *ssid,
  const char *pass,
  const char *apiKey,
  const char *model,
  const String &personality,
  uint8_t recordSeconds
) {
  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putString("ssid", ssid ? ssid : "");
  prefs.putString("pass", pass ? pass : "");
  prefs.putString("groqKey", apiKey ? apiKey : "");
  prefs.putString("model", model && model[0] ? model : GP_DEFAULT_MODEL);
  prefs.putString("personality", personality.length() ? personality : GP_DEFAULT_PERSONALITY);
  prefs.putUChar("recordSec", gpClampRecordSeconds(recordSeconds));
  prefs.end();

  strlcpy(gp_wifi_ssid, ssid ? ssid : "", sizeof(gp_wifi_ssid));
  strlcpy(gp_wifi_pass, pass ? pass : "", sizeof(gp_wifi_pass));
  strlcpy(gp_groq_api_key, apiKey ? apiKey : "", sizeof(gp_groq_api_key));
  strlcpy(gp_model, model && model[0] ? model : GP_DEFAULT_MODEL, sizeof(gp_model));
  gp_personality_prompt = personality.length() ? personality : GP_DEFAULT_PERSONALITY;
  gp_record_seconds = gpClampRecordSeconds(recordSeconds);
  gp_has_settings = gp_wifi_ssid[0] != '\0' && gp_groq_api_key[0] != '\0';
}

static void gpSetTextScale(uint8_t scale) {
  if (scale < 1) {
    scale = 1;
  } else if (scale > 3) {
    scale = 3;
  }
  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putUChar("txtsz", scale);
  prefs.end();
  gp_text_scale = scale;
}

static String gpModelOptionHtml(const char *value, const char *label) {
  String html = "<option value='";
  html += value;
  html += "'";
  if (strcmp(gp_model, value) == 0) {
    html += " selected";
  }
  html += ">";
  html += label;
  html += "</option>";
  return html;
}

static String gpBuildPortalHtml() {
  String html;
  html.reserve(3500);
  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Groqputer Setup</title><style>";
  html += "body{background:#0b1018;color:#d7ecff;font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:20px;}";
  html += "h1{color:#66f0ff;}label{display:block;margin-top:14px;font-weight:bold;color:#a4d3ff;}";
  html += "input,select,textarea{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#111a24;color:#f2fbff;}";
  html += "textarea{min-height:120px;resize:vertical;}";
  html += "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;}";
  html += ".hint{font-size:.95em;color:#9ab4c9;}";
  html += "</style></head><body>";
  html += "<h1>Groqputer Setup</h1>";
  html += "<p class='hint'>Join this AP, save Wi-Fi + Groq settings, then the Cardputer will reboot into standalone chat mode.</p>";
  html += "<form method='post' action='/save'>";
  html += "<label>WiFi SSID</label><input name='ssid' value='" + gpEscapeHtml(String(gp_wifi_ssid)) + "' maxlength='63' required>";
  html += "<label>WiFi Password</label><input name='pass' type='password' value='" + gpEscapeHtml(String(gp_wifi_pass)) + "' maxlength='63'>";
  html += "<label>Groq API Key</label><input name='groqKey' value='" + gpEscapeHtml(String(gp_groq_api_key)) + "' maxlength='127' required>";
  html += "<label>Chat Model</label><select name='model'>";
  html += gpModelOptionHtml("llama-3.1-8b-instant", "llama-3.1-8b-instant");
  html += gpModelOptionHtml("llama-3.3-70b-versatile", "llama-3.3-70b-versatile");
  html += gpModelOptionHtml("qwen/qwen3-32b", "qwen/qwen3-32b");
  html += gpModelOptionHtml("groq/compound-mini", "groq/compound-mini");
  html += gpModelOptionHtml("openai/gpt-oss-20b", "openai/gpt-oss-20b");
  html += "</select>";
  html += "<label>Max Record Seconds</label><input name='recordSec' type='number' value='";
  html += String(gp_record_seconds);
  html += "' min='2' max='15' required>";
  html += "<label>Personality Prompt</label><textarea name='personality' required>";
  html += gpEscapeHtml(gp_personality_prompt);
  html += "</textarea>";
  html += "<button type='submit'>Save & Reboot</button></form></body></html>";
  return html;
}

static void gpHandlePortalRoot() {
  gp_portal_server->send(200, "text/html", gpBuildPortalHtml());
}

static void gpHandlePortalSave() {
  String ssid = gp_portal_server->arg("ssid");
  String pass = gp_portal_server->arg("pass");
  String apiKey = gp_portal_server->arg("groqKey");
  String model = gp_portal_server->arg("model");
  String personality = gp_portal_server->arg("personality");
  uint8_t recordSec = gpClampRecordSeconds(gp_portal_server->arg("recordSec").toInt());

  gpSaveSettings(
    ssid.c_str(),
    pass.c_str(),
    apiKey.c_str(),
    model.c_str(),
    personality,
    recordSec
  );

  gp_portal_server->send(
    200,
    "text/html",
    "<html><body style='background:#0b1018;color:#d7ecff;font-family:Arial;padding:24px'>"
    "<h2>Saved. Rebooting...</h2></body></html>"
  );
  delay(1200);
  ESP.restart();
}

static void gpRunPortal() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(GP_AP_SSID);

  if (!gp_portal_dns) {
    gp_portal_dns = new DNSServer();
  }
  if (!gp_portal_server) {
    gp_portal_server = new WebServer(80);
  }

  gp_portal_dns->start(53, "*", WiFi.softAPIP());
  gp_portal_server->on("/", gpHandlePortalRoot);
  gp_portal_server->on("/save", HTTP_POST, gpHandlePortalSave);
  gp_portal_server->onNotFound(gpHandlePortalRoot);
  gp_portal_server->begin();

  while (true) {
    gp_portal_dns->processNextRequest();
    gp_portal_server->handleClient();
    delay(2);
  }
}

static void gpStartWifiStation() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(gp_wifi_ssid, gp_wifi_pass);
  gp_last_wifi_retry_ms = millis();
}

static bool gpEnsureWifiConnected(bool forceRetry = false) {
  if (gp_wifi_ssid[0] == '\0') {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  unsigned long now = millis();
  if (forceRetry || gp_last_wifi_retry_ms == 0 || now - gp_last_wifi_retry_ms >= GP_WIFI_RETRY_INTERVAL_MS) {
    WiFi.disconnect(false, true);
    delay(100);
    gpStartWifiStation();
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool gpConnect(bool forcePortal = false) {
  gpLoadSettings();
  if (forcePortal || !gp_has_settings) {
    gpRunPortal();
  }

  gpStartWifiStation();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}
