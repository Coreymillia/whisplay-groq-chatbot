#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

extern Arduino_GFX *gfx;

static const char CC_DEFAULT_HOST[] = "10.160.0.136";
static const uint16_t CC_DEFAULT_PORT = 17880;

static char cc_wifi_ssid[64] = "";
static char cc_wifi_pass[64] = "";
static char cc_pi_host[64] = "";
static uint16_t cc_pi_port = CC_DEFAULT_PORT;
static uint8_t cc_brightness = 220;
static uint8_t cc_chat_text_scale = 1;
static uint8_t cc_chat_color_mode = 0;
static bool cc_has_settings = false;
static unsigned long cc_last_wifi_retry_ms = 0;
static const unsigned long CC_WIFI_RETRY_INTERVAL_MS = 10000;

static WebServer *cc_portal_server = nullptr;
static DNSServer *cc_portal_dns = nullptr;

static void ccLoadSettings() {
  Preferences prefs;
  prefs.begin("compcyd", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String host = prefs.getString("host", "");
  cc_pi_port = static_cast<uint16_t>(prefs.getUInt("port", CC_DEFAULT_PORT));
  cc_brightness = prefs.getUChar("bright", 220);
  cc_chat_text_scale = prefs.getUChar("chatscale", 1);
  cc_chat_color_mode = prefs.getUChar("chatcolor", 0);
  prefs.end();

  ssid.toCharArray(cc_wifi_ssid, sizeof(cc_wifi_ssid));
  pass.toCharArray(cc_wifi_pass, sizeof(cc_wifi_pass));
  host.toCharArray(cc_pi_host, sizeof(cc_pi_host));
  if (cc_pi_host[0] == '\0') {
    strlcpy(cc_pi_host, CC_DEFAULT_HOST, sizeof(cc_pi_host));
  }
  if (cc_pi_port == 0) {
    cc_pi_port = CC_DEFAULT_PORT;
  }
  if (cc_brightness < 10) {
    cc_brightness = 10;
  }
  if (cc_chat_text_scale < 1 || cc_chat_text_scale > 3) {
    cc_chat_text_scale = 1;
  }
  if (cc_chat_color_mode > 7) {
    cc_chat_color_mode = 0;
  }
  cc_has_settings = cc_wifi_ssid[0] != '\0';
}

static void ccSaveSettings(
  const char *ssid,
  const char *pass,
  const char *host,
  uint16_t port,
  uint8_t brightness
) {
  Preferences prefs;
  prefs.begin("compcyd", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("host", host);
  prefs.putUInt("port", port);
  prefs.putUChar("bright", brightness);
  prefs.end();

  strlcpy(cc_wifi_ssid, ssid, sizeof(cc_wifi_ssid));
  strlcpy(cc_wifi_pass, pass, sizeof(cc_wifi_pass));
  strlcpy(cc_pi_host, host, sizeof(cc_pi_host));
  cc_pi_port = port;
  cc_brightness = brightness;
  cc_has_settings = true;
}

static void ccSaveLocalUiSettings(uint8_t chatTextScale, uint8_t chatColorMode) {
  if (chatTextScale < 1) {
    chatTextScale = 1;
  } else if (chatTextScale > 3) {
    chatTextScale = 3;
  }
  if (chatColorMode > 7) {
    chatColorMode = 0;
  }

  Preferences prefs;
  prefs.begin("compcyd", false);
  prefs.putUChar("chatscale", chatTextScale);
  prefs.putUChar("chatcolor", chatColorMode);
  prefs.end();

  cc_chat_text_scale = chatTextScale;
  cc_chat_color_mode = chatColorMode;
}

static bool ccProbeApiReachable(uint8_t attempts = 4, uint16_t delay_ms = 1200) {
  char url[160];
  snprintf(url, sizeof(url), "http://%s:%u/api/state", cc_pi_host, cc_pi_port);

  for (uint8_t attempt = 0; attempt < attempts; attempt++) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);
    int code = http.GET();
    http.end();
    if (code == 200) {
      return true;
    }
    delay(delay_ms);
  }
  return false;
}

static void ccShowPortalScreen() {
  if (!gfx) {
    return;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(0x07FF);
  gfx->setTextSize(2);
  gfx->setCursor(16, 10);
  gfx->print("Whisplay CYD");
  gfx->setCursor(78, 30);
  gfx->print("Setup");

  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(10, 58);
  gfx->print("1. Join WiFi: WhisplayCYD-Setup");
  gfx->setCursor(10, 74);
  gfx->print("2. Open: http://192.168.4.1");
  gfx->setCursor(10, 90);
  gfx->print("3. Save your WiFi + Pi address");

  gfx->setTextColor(0xFFE0);
  gfx->setCursor(10, 122);
  gfx->print("Pi host:");
  gfx->setTextColor(0x07E0);
  gfx->setCursor(70, 122);
  gfx->print(cc_pi_host);

  char portBuf[16];
  snprintf(portBuf, sizeof(portBuf), "%u", cc_pi_port);
  gfx->setTextColor(0xFFE0);
  gfx->setCursor(10, 138);
  gfx->print("Pi port:");
  gfx->setTextColor(0x07E0);
  gfx->setCursor(70, 138);
  gfx->print(portBuf);

  if (cc_has_settings) {
    gfx->setTextColor(0x07E0);
    gfx->setCursor(10, 176);
    gfx->print("Existing settings are loaded.");
    gfx->setCursor(10, 192);
    gfx->print("Save to replace them.");
  }
}

static void ccHandlePortalRoot() {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Whisplay CYD Setup</title>"
    "<style>"
    "body{background:#081018;color:#8ff;font-family:Arial,sans-serif;max-width:480px;margin:auto;padding:20px;}"
    "h1{color:#0ff;}label{display:block;margin-top:14px;color:#9cf;font-weight:bold;}"
    "input{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #1d5f7a;background:#102030;color:#dff;}"
    "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#0d6efd;color:#fff;font-weight:bold;}"
    ".hint{font-size:.9em;color:#9ab;}"
    ".rng{display:flex;align-items:center;gap:8px;margin-top:8px;}"
    ".rng input[type=range]{flex:1;accent-color:#0ff;}"
    "</style></head><body>"
    "<h1>Whisplay CYD Setup</h1>"
    "<p class='hint'>Point this companion to your WhisplayGroqHat Pi web UI.</p>"
    "<form method='post' action='/save'>"
    "<label>WiFi SSID</label><input name='ssid' value='" + String(cc_wifi_ssid) + "' maxlength='63' required>"
    "<label>WiFi Password</label><input name='pass' type='password' value='" + String(cc_wifi_pass) + "' maxlength='63'>"
    "<label>Pi Host / IP</label><input name='host' value='" + String(cc_pi_host) + "' maxlength='63' required>"
    "<label>Pi Port</label><input name='port' type='number' value='" + String(cc_pi_port) + "' min='1' max='65535' required>"
    "<label>Brightness</label>"
    "<div class='rng'><input type='range' name='bright' min='10' max='255' value='" + String(cc_brightness) +
    "' oninput='this.nextElementSibling.value=this.value'><output>" + String(cc_brightness) + "</output></div>"
    "<button type='submit'>Save & Connect</button>"
    "</form></body></html>";
  cc_portal_server->send(200, "text/html", html);
}

static void ccHandlePortalSave() {
  String ssid = cc_portal_server->arg("ssid");
  String pass = cc_portal_server->arg("pass");
  String host = cc_portal_server->arg("host");
  uint16_t port = static_cast<uint16_t>(cc_portal_server->arg("port").toInt());
  uint8_t brightness = static_cast<uint8_t>(cc_portal_server->arg("bright").toInt());
  if (port == 0) {
    port = CC_DEFAULT_PORT;
  }
  if (brightness < 10) {
    brightness = 10;
  }
  ccSaveSettings(ssid.c_str(), pass.c_str(), host.c_str(), port, brightness);
  cc_portal_server->send(
    200,
    "text/html",
    "<html><body style='background:#081018;color:#8ff;font-family:Arial;padding:24px'>"
    "<h2>Saved. Rebooting...</h2></body></html>"
  );
  delay(1200);
  ESP.restart();
}

static void ccRunPortal() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WhisplayCYD-Setup");

  if (!cc_portal_dns) {
    cc_portal_dns = new DNSServer();
  }
  if (!cc_portal_server) {
    cc_portal_server = new WebServer(80);
  }

  cc_portal_dns->start(53, "*", WiFi.softAPIP());
  cc_portal_server->on("/", ccHandlePortalRoot);
  cc_portal_server->on("/save", HTTP_POST, ccHandlePortalSave);
  cc_portal_server->onNotFound(ccHandlePortalRoot);
  cc_portal_server->begin();

  ccShowPortalScreen();

  while (true) {
    cc_portal_dns->processNextRequest();
    cc_portal_server->handleClient();
    delay(2);
  }
}

static void ccStartWifiStation() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cc_wifi_ssid, cc_wifi_pass);
  cc_last_wifi_retry_ms = millis();
}

static bool ccEnsureWifiConnected(bool forceRetry = false) {
  if (!cc_has_settings || cc_wifi_ssid[0] == '\0') {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  unsigned long now = millis();
  if (forceRetry || cc_last_wifi_retry_ms == 0 || now - cc_last_wifi_retry_ms >= CC_WIFI_RETRY_INTERVAL_MS) {
    WiFi.disconnect(false, true);
    delay(100);
    ccStartWifiStation();
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool ccConnect(bool forcePortal = false) {
  ccLoadSettings();
  if (forcePortal || !cc_has_settings || cc_wifi_ssid[0] == '\0') {
    ccRunPortal();
  }

  ccStartWifiStation();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  ccProbeApiReachable();
  return WiFi.status() == WL_CONNECTED;
}
