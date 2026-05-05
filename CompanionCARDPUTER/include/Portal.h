#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

static const char CP_DEFAULT_HOST[] = "10.160.0.136";
static const uint16_t CP_DEFAULT_PORT = 17880;

static char cp_wifi_ssid[64] = "";
static char cp_wifi_pass[64] = "";
static char cp_pi_host[64] = "";
static uint16_t cp_pi_port = CP_DEFAULT_PORT;
static uint8_t cp_text_scale = 1;
static bool cp_has_settings = false;
static unsigned long cp_last_wifi_retry_ms = 0;
static const unsigned long CP_WIFI_RETRY_INTERVAL_MS = 10000;

static WebServer *cp_portal_server = nullptr;
static DNSServer *cp_portal_dns = nullptr;

static void cpLoadSettings() {
  Preferences prefs;
  prefs.begin("compcard", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String host = prefs.getString("host", "");
  cp_pi_port = static_cast<uint16_t>(prefs.getUInt("port", CP_DEFAULT_PORT));
  cp_text_scale = prefs.getUChar("txtsz", 1);
  prefs.end();

  ssid.toCharArray(cp_wifi_ssid, sizeof(cp_wifi_ssid));
  pass.toCharArray(cp_wifi_pass, sizeof(cp_wifi_pass));
  host.toCharArray(cp_pi_host, sizeof(cp_pi_host));
  if (cp_pi_host[0] == '\0') {
    strlcpy(cp_pi_host, CP_DEFAULT_HOST, sizeof(cp_pi_host));
  }
  if (cp_pi_port == 0) {
    cp_pi_port = CP_DEFAULT_PORT;
  }
  if (cp_text_scale < 1) {
    cp_text_scale = 1;
  } else if (cp_text_scale > 3) {
    cp_text_scale = 3;
  }
  cp_has_settings = cp_wifi_ssid[0] != '\0';
}

static void cpSaveSettings(
  const char *ssid,
  const char *pass,
  const char *host,
  uint16_t port
) {
  Preferences prefs;
  prefs.begin("compcard", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("host", host);
  prefs.putUInt("port", port);
  prefs.end();

  strlcpy(cp_wifi_ssid, ssid, sizeof(cp_wifi_ssid));
  strlcpy(cp_wifi_pass, pass, sizeof(cp_wifi_pass));
  strlcpy(cp_pi_host, host, sizeof(cp_pi_host));
  cp_pi_port = port;
  cp_has_settings = true;
}

static void cpSetTextScale(uint8_t scale) {
  if (scale < 1) {
    scale = 1;
  } else if (scale > 3) {
    scale = 3;
  }

  Preferences prefs;
  prefs.begin("compcard", false);
  prefs.putUChar("txtsz", scale);
  prefs.end();
  cp_text_scale = scale;
}

static bool cpProbeApiReachable(uint8_t attempts = 3, uint16_t delayMs = 1200) {
  char url[160];
  snprintf(url, sizeof(url), "http://%s:%u/api/state", cp_pi_host, cp_pi_port);

  for (uint8_t attempt = 0; attempt < attempts; attempt++) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);
    int code = http.GET();
    http.end();
    if (code == 200) {
      return true;
    }
    delay(delayMs);
  }
  return false;
}

static String cpBuildPortalHtml() {
  return String()
    + "<!DOCTYPE html><html><head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Whisplay Cardputer Setup</title>"
      "<style>"
      "body{background:#0b1018;color:#d7ecff;font-family:Arial,sans-serif;max-width:480px;margin:auto;padding:20px;}"
      "h1{color:#66f0ff;}label{display:block;margin-top:14px;font-weight:bold;color:#a4d3ff;}"
      "input{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#111a24;color:#f2fbff;}"
      "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;}"
      ".hint{font-size:.95em;color:#9ab4c9;}"
      "</style></head><body>"
      "<h1>Whisplay Cardputer Setup</h1>"
      "<p class='hint'>Join this AP, then point the Cardputer at your WhisplayGroqHat Pi.</p>"
      "<form method='post' action='/save'>"
      "<label>WiFi SSID</label><input name='ssid' value='" + String(cp_wifi_ssid) + "' maxlength='63' required>"
      "<label>WiFi Password</label><input name='pass' type='password' value='" + String(cp_wifi_pass) + "' maxlength='63'>"
      "<label>Pi Host / IP</label><input name='host' value='" + String(cp_pi_host) + "' maxlength='63' required>"
      "<label>Pi Port</label><input name='port' type='number' value='" + String(cp_pi_port) + "' min='1' max='65535' required>"
      "<button type='submit'>Save & Connect</button>"
      "</form></body></html>";
}

static void cpHandlePortalRoot() {
  cp_portal_server->send(200, "text/html", cpBuildPortalHtml());
}

static void cpHandlePortalSave() {
  String ssid = cp_portal_server->arg("ssid");
  String pass = cp_portal_server->arg("pass");
  String host = cp_portal_server->arg("host");
  uint16_t port = static_cast<uint16_t>(cp_portal_server->arg("port").toInt());
  if (port == 0) {
    port = CP_DEFAULT_PORT;
  }
  cpSaveSettings(ssid.c_str(), pass.c_str(), host.c_str(), port);
  cp_portal_server->send(
    200,
    "text/html",
    "<html><body style='background:#0b1018;color:#d7ecff;font-family:Arial;padding:24px'>"
    "<h2>Saved. Rebooting...</h2></body></html>"
  );
  delay(1200);
  ESP.restart();
}

static void cpRunPortal() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WhisplayCardputer-Setup");

  if (!cp_portal_dns) {
    cp_portal_dns = new DNSServer();
  }
  if (!cp_portal_server) {
    cp_portal_server = new WebServer(80);
  }

  cp_portal_dns->start(53, "*", WiFi.softAPIP());
  cp_portal_server->on("/", cpHandlePortalRoot);
  cp_portal_server->on("/save", HTTP_POST, cpHandlePortalSave);
  cp_portal_server->onNotFound(cpHandlePortalRoot);
  cp_portal_server->begin();

  while (true) {
    cp_portal_dns->processNextRequest();
    cp_portal_server->handleClient();
    delay(2);
  }
}

static void cpStartWifiStation() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cp_wifi_ssid, cp_wifi_pass);
  cp_last_wifi_retry_ms = millis();
}

static bool cpEnsureWifiConnected(bool forceRetry = false) {
  if (!cp_has_settings || cp_wifi_ssid[0] == '\0') {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  unsigned long now = millis();
  if (forceRetry || cp_last_wifi_retry_ms == 0 || now - cp_last_wifi_retry_ms >= CP_WIFI_RETRY_INTERVAL_MS) {
    WiFi.disconnect(false, true);
    delay(100);
    cpStartWifiStation();
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool cpConnect(bool forcePortal = false) {
  cpLoadSettings();
  if (forcePortal || !cp_has_settings || cp_wifi_ssid[0] == '\0') {
    cpRunPortal();
  }

  cpStartWifiStation();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  cpProbeApiReachable();
  return WiFi.status() == WL_CONNECTED;
}
