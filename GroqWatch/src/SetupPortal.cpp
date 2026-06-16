#include "SetupPortal.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "AppSettings.h"
#include "AppModes.h"
#include "pin_config.h"

namespace GroqWatch {
namespace {

static constexpr const char *kApSsid = "GroqWatch-Setup";
static WebServer *gServer = nullptr;
static DNSServer *gDns = nullptr;
static Arduino_GFX *gGfx = nullptr;
static AppSettings gSettings;

struct TzEntry {
    const char *label;
    const char *value;
};

static const TzEntry kTimezones[] = {
    {"Eastern (EST5EDT)", "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"Central (CST6CDT)", "CST6CDT,M3.2.0/2,M11.1.0/2"},
    {"Mountain (MST7MDT)", "MST7MDT,M3.2.0/2,M11.1.0/2"},
    {"Mountain no DST (MST7)", "MST7"},
    {"Pacific (PST8PDT)", "PST8PDT,M3.2.0/2,M11.1.0/2"},
    {"Alaska (AKST9AKDT)", "AKST9AKDT,M3.2.0/2,M11.1.0/2"},
    {"Hawaii (HST10)", "HST10"},
};

static const char *kModels[] = {
    "llama-3.1-8b-instant",
    "llama-3.3-70b-versatile",
    "qwen/qwen3-32b",
    "groq/compound-mini",
    "openai/gpt-oss-20b",
};
static constexpr int kModelCount = 5;

String htmlEscape(const String &value) {
    String escaped = value;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

void drawPortalScreen(const char *statusLine = "Open 192.168.4.1") {
    if (!gGfx) return;
    gGfx->fillScreen(RGB565_BLACK);
    gGfx->setTextSize(2);
    gGfx->setTextColor(RGB565_CYAN);
    gGfx->setCursor(28, 24);
    gGfx->print("GroqWatch Setup");

    gGfx->setTextSize(1);
    gGfx->setTextColor(RGB565_WHITE);
    gGfx->setCursor(18, 76);
    gGfx->print("1. Join WiFi: ");
    gGfx->print(kApSsid);

    gGfx->setTextSize(2);
    gGfx->setTextColor(RGB565_GREEN);
    gGfx->setCursor(66, 100);
    gGfx->print("192.168.4.1");

    gGfx->setTextSize(1);
    gGfx->setTextColor(RGB565_WHITE);
    gGfx->setCursor(18, 154);
    gGfx->print("2. Fill web form");

    gGfx->setCursor(18, 178);
    gGfx->print("3. Save & Reboot");
    gGfx->setTextColor(RGB565_CYAN);
    gGfx->setCursor(18, 240);
    gGfx->print(statusLine);

    if (gSettings.wifiSsid[0]) {
        gGfx->setTextColor(RGB565_WHITE);
        gGfx->setCursor(18, 280);
        gGfx->print("SSID: ");
        gGfx->print(gSettings.wifiSsid);
    }
}

void handleRoot() {
    String tzOpts;
    for (const auto &tz : kTimezones) {
        tzOpts += "<option value='" + String(tz.value) + "'";
        if (String(gSettings.timezone) == tz.value) tzOpts += " selected";
        tzOpts += ">" + String(tz.label) + "</option>";
    }

    String modelOpts;
    for (int i = 0; i < kModelCount; i++) {
        modelOpts += "<option value='" + String(kModels[i]) + "'";
        if (String(gSettings.model) == kModels[i]) modelOpts += " selected";
        modelOpts += ">" + String(kModels[i]) + "</option>";
    }

    String bootOpts;
    bootOpts += "<option value='watch'";
    if (String(gSettings.bootMode) == "watch") bootOpts += " selected";
    bootOpts += ">Watch</option>";
    bootOpts += "<option value='bot'";
    if (String(gSettings.bootMode) == "bot") bootOpts += " selected";
    bootOpts += ">Bot</option>";
    bootOpts += "<option value='ai'";
    if (String(gSettings.bootMode) == "ai") bootOpts += " selected";
    bootOpts += ">AI Screensaver</option>";

    String html;
    html.reserve(8192);
    html = "<!DOCTYPE html><html><head>"
           "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>GroqWatch Setup</title>"
           "<style>"
           "body{background:#081018;color:#dff;font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:20px;}"
           "h1{color:#63f3ff;}label{display:block;margin-top:14px;color:#9cf;font-weight:bold;}"
           "input,select,textarea{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#102030;color:#dff;}"
           "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;font-size:1.05em;}"
           ".hint{font-size:.92em;color:#9ab;}"
           "</style></head><body>"
           "<h1>GroqWatch Setup</h1>"
           "<p class='hint'>WiFi is required for all online modes.</p>"
           "<form method='post' action='/save'>"
           "<label>WiFi SSID</label>"
           "<input name='ssid' maxlength='63' required value='" + htmlEscape(String(gSettings.wifiSsid)) + "'>"
           "<label>WiFi Password</label>"
           "<input name='pass' type='password' maxlength='63' value='" + htmlEscape(String(gSettings.wifiPass)) + "'>"
           "<label>Timezone</label><select name='tz'>" + tzOpts + "</select>"
           "<hr style='border-color:#35516f;margin:22px 0'>"
           "<h3 style='color:#0ff'>Whisplay Companion</h3>"
           "<label>Whisplay URL</label>"
           "<input name='whisplayUrl' maxlength='159' value='" + htmlEscape(String(gSettings.whisplayUrl)) + "' placeholder='http://10.160.0.136:17880'>"
           "<hr style='border-color:#35516f;margin:22px 0'>"
           "<details>"
           "<summary style='color:#9ab;cursor:pointer;margin-bottom:10px'>\u26a0\ufe0f Local Groq bot (experimental — click to show)</summary>"
           "<label>Groq API Key</label>"
           "<input name='groqKey' maxlength='127' value='" + htmlEscape(String(gSettings.groqApiKey)) + "' placeholder='Needed for bot mode'>"
           "<label>Chat Model</label><select name='model'>" + modelOpts + "</select>"
           "<label>Bot Personality</label>"
           "<textarea name='persona' rows='3'>" + htmlEscape(String(gSettings.personaPrompt)) + "</textarea>"
           "</details>"
           "<hr style='border-color:#35516f;margin:22px 0'>"
           "<h3 style='color:#0ff'>AI Screensaver</h3>"
           "<p class='hint'>Uses the Whisplay URL above for image feeds.</p>"
           "<label>Latitude</label>"
           "<input name='lat' maxlength='31' value='" + htmlEscape(String(gSettings.latitude)) + "'>"
           "<label>Longitude</label>"
           "<input name='lon' maxlength='31' value='" + htmlEscape(String(gSettings.longitude)) + "'>"
           "<hr style='border-color:#35516f;margin:22px 0'>"
           "<label>Default Boot Mode</label><select name='bootMode'>" + bootOpts + "</select>"
           "<p class='hint'>Hold BOOT at startup or tap Setup tile in Settings to reopen.</p>"
           "<button type='submit'>Save &amp; Reboot</button>"
           "</form></body></html>";

    gServer->send(200, "text/html", html);
}

void handleSave() {
    AppSettings saved = gSettings;
    gServer->arg("ssid").toCharArray(saved.wifiSsid, sizeof(saved.wifiSsid));
    gServer->arg("pass").toCharArray(saved.wifiPass, sizeof(saved.wifiPass));
    gServer->arg("tz").toCharArray(saved.timezone, sizeof(saved.timezone));
    gServer->arg("groqKey").toCharArray(saved.groqApiKey, sizeof(saved.groqApiKey));
    gServer->arg("whisplayUrl").toCharArray(saved.whisplayUrl, sizeof(saved.whisplayUrl));
    gServer->arg("lat").toCharArray(saved.latitude, sizeof(saved.latitude));
    gServer->arg("lon").toCharArray(saved.longitude, sizeof(saved.longitude));
    gServer->arg("model").toCharArray(saved.model, sizeof(saved.model));
    gServer->arg("persona").toCharArray(saved.personaPrompt, sizeof(saved.personaPrompt));
    gServer->arg("bootMode").toCharArray(saved.bootMode, sizeof(saved.bootMode));

    if (!saved.wifiSsid[0]) {
        gServer->send(400, "text/plain", "SSID is required.");
        return;
    }
    if (!saved.timezone[0]) strlcpy(saved.timezone, "CST6CDT,M3.2.0/2,M11.1.0/2", sizeof(saved.timezone));
    if (!saved.model[0]) strlcpy(saved.model, kDefaultModel, sizeof(saved.model));
    if (!saved.bootMode[0]) strlcpy(saved.bootMode, "watch", sizeof(saved.bootMode));

    saveSettings(saved);
    drawPortalScreen("Saved. Rebooting...");
    gServer->send(200, "text/html",
                  "<html><body style='font-family:Arial;background:#081018;color:#dff;padding:24px'><h2>Saved. Rebooting...</h2></body></html>");
    delay(1200);
    ESP.restart();
    while (true) { delay(1000); }
}

}  // namespace

bool watchBootButtonHeld() {
    pinMode(WATCH_BOOT_BUTTON_PIN, INPUT_PULLUP);
    delay(20);
    if (digitalRead(WATCH_BOOT_BUTTON_PIN) != LOW) return false;
    delay(600);
    return digitalRead(WATCH_BOOT_BUTTON_PIN) == LOW;
}

[[noreturn]] void runSetupPortalModal(Arduino_GFX *gfx, const AppSettings &currentSettings) {
    gGfx = gfx;
    gSettings = currentSettings;

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid, "");
    delay(300);

    gDns = new DNSServer();
    gServer = new WebServer(80);
    gDns->start(53, "*", WiFi.softAPIP());
    gServer->on("/", handleRoot);
    gServer->on("/save", HTTP_POST, handleSave);
    gServer->onNotFound(handleRoot);
    gServer->begin();

    Serial.printf("[setup] AP SSID=%s IP=%s\n", kApSsid, WiFi.softAPIP().toString().c_str());
    drawPortalScreen();

    while (true) {
        gDns->processNextRequest();
        gServer->handleClient();
        delay(2);
    }
}

}  // namespace GroqWatch
