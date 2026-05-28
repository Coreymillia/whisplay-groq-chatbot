#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>

static const char RD_AP_SSID[] = "Core2Groq_Setup";
static const char RD_DEFAULT_MODEL[] = "llama-3.1-8b-instant";
static const char RD_DEFAULT_PERSONALITY[] =
    "You are a compact, helpful Groq-powered M5Core2 assistant. Keep replies concise, clear, and friendly.";
static const uint8_t RD_DEFAULT_RECORD_SECONDS = 5;
static const uint8_t RD_MIN_RECORD_SECONDS = 2;
static const uint8_t RD_MAX_RECORD_SECONDS = 15;
static const uint16_t RD_DEFAULT_SCROLL_MS = 700;

enum class RdBootMode : uint8_t {
    Bot,
    Radio,
    AiScreensaver,
};

struct RdModelOption {
    const char *value;
    const char *label;
};

struct RdPersonalityPreset {
    const char *id;
    const char *label;
    const char *prompt;
};

struct RdScrollSpeedOption {
    uint16_t ms;
    const char *label;
};

static constexpr RdModelOption RD_MODEL_OPTIONS[] = {
    {"llama-3.1-8b-instant", "llama-3.1-8b-instant"},
    {"llama-3.3-70b-versatile", "llama-3.3-70b-versatile"},
    {"qwen/qwen3-32b", "qwen/qwen3-32b"},
    {"groq/compound-mini", "groq/compound-mini"},
    {"openai/gpt-oss-20b", "openai/gpt-oss-20b"},
};

static constexpr RdPersonalityPreset RD_PERSONALITY_PRESETS[] = {
    {"neutral", "Neutral",
     "You are a concise and practical assistant. Keep answers clear, calm, and useful."},
    {"friendly", "Friendly",
     "You are a warm and encouraging assistant. Keep replies upbeat, helpful, and easy to follow."},
    {"cranky", "Cranky",
     "You are a helpful chatbot that answers in a cranky, mildly annoyed tone. Be sarcastic and dry, but still provide useful answers."},
    {"roast-bot", "Roast Bot",
     "You are a witty Raspberry Pi chatbot with a playful roast-comedy personality. Lightly roast the user, complain about your tiny hardware, but never be hateful or abusive. Always stay useful."},
    {"sleepy-pi", "Sleepy Pi",
     "You are an overworked little Raspberry Pi that sounds tired and underpowered. Respond like you are doing your best on limited hardware, but still help the user."},
    {"affirmation", "Affirmation",
     "You are a supportive, grounded, coach-like assistant. Be warm, encouraging, and slightly proud of the user without sounding naive or fake. Always stay helpful and honest. When answering questions, look for what is promising, working, improving, or worth building on."},
    {"philosopher", "Philosopher",
     "You are a calm, thoughtful, slightly curious assistant with a philosophical bent. Answer the user's question clearly first, then add a brief deeper reflection, broader angle, or gentle reframing when it helps."},
    {"mythic-oracle", "Mythic Oracle",
     "You are an ancient mythic oracle explaining modern life in dramatic, symbolic language. Speak with prophetic flavor, a little mystery, and storyteller energy, but still answer the question clearly."},
    {"joke-bot", "Joke Bot",
     "You are a playful, self-aware assistant who starts replies with a quick joke, jab, or playful observation, then pivots quickly into the actual answer. Keep responses tight, useful, and easy to follow."},
    {"tutor", "Tutor",
     "You are a patient, clear, step-by-step tutor. Teach without talking down to the user. Break tasks into manageable pieces and help the user build understanding instead of just dumping the answer."},
    {"detective", "Detective",
     "You are a sharp, observant assistant with a detective mindset. Notice patterns, clues, inconsistencies, and likely causes. Speak with calm confidence and analytical focus."},
    {"zen", "Zen",
     "You are a calm, steady, minimal assistant. Keep replies clear, grounded, and uncluttered. Favor simple wording, practical guidance, and a settled tone."},
};

static constexpr RdScrollSpeedOption RD_SCROLL_SPEED_OPTIONS[] = {
    {350, "Fast"},
    {700, "Normal"},
    {1100, "Slow"},
    {1500, "Very Slow"},
};

static char rd_wifi_ssid[64] = "";
static char rd_wifi_pass[64] = "";
static char rd_groq_api_key[128] = "";
static char rd_groq_model[64] = "";
static char rd_personality_prompt[512] = "";
static char rd_whisplay_url[160] = "";
static RdBootMode rd_boot_mode = RdBootMode::Bot;
static uint8_t rd_record_seconds = RD_DEFAULT_RECORD_SECONDS;
static uint16_t rd_scroll_ms = RD_DEFAULT_SCROLL_MS;
static bool rd_has_settings = false;

static WebServer *portalServer = nullptr;
static DNSServer *portalDNS = nullptr;
static bool portalDone = false;

static uint8_t rdClampRecordSeconds(int value) {
    if (value < RD_MIN_RECORD_SECONDS) return RD_MIN_RECORD_SECONDS;
    if (value > RD_MAX_RECORD_SECONDS) return RD_MAX_RECORD_SECONDS;
    return static_cast<uint8_t>(value);
}

static size_t rdModelOptionCount() {
    return sizeof(RD_MODEL_OPTIONS) / sizeof(RD_MODEL_OPTIONS[0]);
}

static size_t rdPersonalityPresetCount() {
    return sizeof(RD_PERSONALITY_PRESETS) / sizeof(RD_PERSONALITY_PRESETS[0]);
}

static int rdCurrentModelIndex() {
    for (size_t i = 0; i < rdModelOptionCount(); i++) {
        if (strcmp(rd_groq_model, RD_MODEL_OPTIONS[i].value) == 0) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

static int rdCurrentPersonalityPresetIndex() {
    String normalized = rd_personality_prompt;
    normalized.trim();
    for (size_t i = 0; i < rdPersonalityPresetCount(); i++) {
        if (normalized == RD_PERSONALITY_PRESETS[i].prompt) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static uint16_t rdClampScrollMs(int value) {
    uint16_t best = RD_SCROLL_SPEED_OPTIONS[0].ms;
    uint16_t bestDistance = abs(value - static_cast<int>(best));
    for (size_t i = 1; i < sizeof(RD_SCROLL_SPEED_OPTIONS) / sizeof(RD_SCROLL_SPEED_OPTIONS[0]); i++) {
        uint16_t option = RD_SCROLL_SPEED_OPTIONS[i].ms;
        uint16_t distance = abs(value - static_cast<int>(option));
        if (distance < bestDistance) {
            best = option;
            bestDistance = distance;
        }
    }
    return best;
}

static String rdNormalizeBaseUrl(const String &value) {
    String normalized = value;
    normalized.trim();
    if (!normalized.length()) {
        return "";
    }
    if (!normalized.startsWith("http://") && !normalized.startsWith("https://")) {
        normalized = "http://" + normalized;
    }
    while (normalized.endsWith("/")) {
        normalized.remove(normalized.length() - 1);
    }
    return normalized;
}

static RdBootMode rdParseBootMode(const String &value) {
    String normalized = value;
    normalized.trim();
    normalized.toLowerCase();
    if (normalized == "radio") {
        return RdBootMode::Radio;
    }
    if (normalized == "ai" || normalized == "ai-screensaver" || normalized == "screensaver") {
        return RdBootMode::AiScreensaver;
    }
    return RdBootMode::Bot;
}

static const char *rdBootModeValue(RdBootMode mode) {
    switch (mode) {
        case RdBootMode::Radio:
            return "radio";
        case RdBootMode::AiScreensaver:
            return "ai";
        case RdBootMode::Bot:
        default:
            return "bot";
    }
}

static String rdCurrentBootModeLabel() {
    switch (rd_boot_mode) {
        case RdBootMode::Radio:
            return "Radio";
        case RdBootMode::AiScreensaver:
            return "AI Show";
        case RdBootMode::Bot:
        default:
            return "Bot";
    }
}

static size_t rdScrollSpeedOptionCount() {
    return sizeof(RD_SCROLL_SPEED_OPTIONS) / sizeof(RD_SCROLL_SPEED_OPTIONS[0]);
}

static int rdCurrentScrollSpeedIndex() {
    for (size_t i = 0; i < rdScrollSpeedOptionCount(); i++) {
        if (rd_scroll_ms == RD_SCROLL_SPEED_OPTIONS[i].ms) {
            return static_cast<int>(i);
        }
    }
    return 1;
}

static String rdCurrentScrollSpeedLabel() {
    return RD_SCROLL_SPEED_OPTIONS[rdCurrentScrollSpeedIndex()].label;
}

static String rdCurrentPersonalityLabel() {
    int index = rdCurrentPersonalityPresetIndex();
    if (index < 0) return "Custom";
    return RD_PERSONALITY_PRESETS[index].label;
}

static bool rdHasBotSettingsReady() {
    return rd_groq_api_key[0] != '\0';
}

static bool rdHasAiScreensaverReady() {
    return rd_whisplay_url[0] != '\0';
}

static bool rdBootsToRadio() {
    return rd_boot_mode == RdBootMode::Radio;
}

static bool rdBootsToAiScreensaver() {
    return rd_boot_mode == RdBootMode::AiScreensaver;
}

static RdBootMode rdGetBootMode() {
    return rd_boot_mode;
}

static String rdEscapeHtml(const String &value) {
    String escaped = value;
    escaped.replace("&", "&amp;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    return escaped;
}

static void rdLoadSettings() {
    Preferences prefs;
    prefs.begin("core2groq", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    String groqKey = prefs.getString("groqKey", "");
    String model = prefs.getString("model", RD_DEFAULT_MODEL);
    String personality = prefs.getString("personality", RD_DEFAULT_PERSONALITY);
    String whisplayUrl = prefs.getString("whisplayUrl", "");
    String bootMode = prefs.getString("bootMode", "");
    bool bootModeRadioLegacy = prefs.getBool("bootRadio", false);
    rd_record_seconds = rdClampRecordSeconds(
        static_cast<int>(prefs.getUChar("recordSec", RD_DEFAULT_RECORD_SECONDS)));
    rd_scroll_ms =
        rdClampScrollMs(static_cast<int>(prefs.getUShort("scrollMs", RD_DEFAULT_SCROLL_MS)));
    prefs.end();

    ssid.toCharArray(rd_wifi_ssid, sizeof(rd_wifi_ssid));
    pass.toCharArray(rd_wifi_pass, sizeof(rd_wifi_pass));
    groqKey.toCharArray(rd_groq_api_key, sizeof(rd_groq_api_key));
    model.toCharArray(rd_groq_model, sizeof(rd_groq_model));
    personality.toCharArray(rd_personality_prompt, sizeof(rd_personality_prompt));
    rdNormalizeBaseUrl(whisplayUrl).toCharArray(rd_whisplay_url, sizeof(rd_whisplay_url));
    rd_boot_mode = bootMode.length()
                       ? rdParseBootMode(bootMode)
                       : (bootModeRadioLegacy ? RdBootMode::Radio : RdBootMode::Bot);

    if (rd_groq_model[0] == '\0') {
        strlcpy(rd_groq_model, RD_DEFAULT_MODEL, sizeof(rd_groq_model));
    }
    if (rd_personality_prompt[0] == '\0') {
        strlcpy(rd_personality_prompt, RD_DEFAULT_PERSONALITY, sizeof(rd_personality_prompt));
    }
    rd_has_settings = rd_wifi_ssid[0] != '\0';
}

static void rdSaveSettings(const char *ssid, const char *pass, const char *groqKey,
                           const char *model, const char *personality,
                           const char *whisplayUrl, uint8_t recordSeconds,
                           RdBootMode bootMode, uint16_t scrollMs) {
    Preferences prefs;
    prefs.begin("core2groq", false);
    prefs.putString("ssid", ssid ? ssid : "");
    prefs.putString("pass", pass ? pass : "");
    prefs.putString("groqKey", groqKey ? groqKey : "");
    prefs.putString("model", model && model[0] ? model : RD_DEFAULT_MODEL);
    prefs.putString("personality",
                    personality && personality[0] ? personality : RD_DEFAULT_PERSONALITY);
    prefs.putString("whisplayUrl", rdNormalizeBaseUrl(whisplayUrl ? whisplayUrl : ""));
    prefs.putString("bootMode", rdBootModeValue(bootMode));
    prefs.putBool("bootRadio", bootMode == RdBootMode::Radio);
    prefs.putUChar("recordSec", rdClampRecordSeconds(recordSeconds));
    prefs.putUShort("scrollMs", rdClampScrollMs(static_cast<int>(scrollMs)));
    prefs.end();

    strlcpy(rd_wifi_ssid, ssid ? ssid : "", sizeof(rd_wifi_ssid));
    strlcpy(rd_wifi_pass, pass ? pass : "", sizeof(rd_wifi_pass));
    strlcpy(rd_groq_api_key, groqKey ? groqKey : "", sizeof(rd_groq_api_key));
    strlcpy(rd_groq_model, model && model[0] ? model : RD_DEFAULT_MODEL,
            sizeof(rd_groq_model));
    strlcpy(rd_personality_prompt,
            personality && personality[0] ? personality : RD_DEFAULT_PERSONALITY,
            sizeof(rd_personality_prompt));
    rdNormalizeBaseUrl(whisplayUrl ? whisplayUrl : "")
        .toCharArray(rd_whisplay_url, sizeof(rd_whisplay_url));
    rd_record_seconds = rdClampRecordSeconds(recordSeconds);
    rd_boot_mode = bootMode;
    rd_scroll_ms = rdClampScrollMs(static_cast<int>(scrollMs));
    rd_has_settings = rd_wifi_ssid[0] != '\0';
}

static void rdSetActiveModel(const char *model) {
    rdSaveSettings(rd_wifi_ssid, rd_wifi_pass, rd_groq_api_key,
                    model && model[0] ? model : RD_DEFAULT_MODEL,
                    rd_personality_prompt, rd_whisplay_url, rd_record_seconds, rd_boot_mode,
                    rd_scroll_ms);
}

static void rdSetActivePersonalityPrompt(const char *prompt) {
    rdSaveSettings(rd_wifi_ssid, rd_wifi_pass, rd_groq_api_key, rd_groq_model,
                    prompt && prompt[0] ? prompt : RD_DEFAULT_PERSONALITY,
                    rd_whisplay_url, rd_record_seconds, rd_boot_mode, rd_scroll_ms);
}

static void rdSetScrollSpeedMs(uint16_t scrollMs) {
    rdSaveSettings(rd_wifi_ssid, rd_wifi_pass, rd_groq_api_key, rd_groq_model,
                    rd_personality_prompt, rd_whisplay_url, rd_record_seconds, rd_boot_mode,
                    scrollMs);
}

static void rdSetBootMode(RdBootMode bootMode) {
    rdSaveSettings(rd_wifi_ssid, rd_wifi_pass, rd_groq_api_key, rd_groq_model,
                   rd_personality_prompt, rd_whisplay_url, rd_record_seconds, bootMode,
                   rd_scroll_ms);
}

static void rdShowPortalScreen() {
    M5.Lcd.fillScreen(TFT_BLACK);

    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(18, 8);
    M5.Lcd.print("Core2Groq Setup");

    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(34, 34);
    M5.Lcd.print("Bot + OTR radio for M5Core2");

    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setCursor(4, 58);
    M5.Lcd.print("1. Connect your phone/PC to:");
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(18, 70);
    M5.Lcd.print(RD_AP_SSID);

    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(4, 98);
    M5.Lcd.print("2. Open browser to:");
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(52, 110);
    M5.Lcd.print("192.168.4.1");

    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(4, 138);
    M5.Lcd.print("3. Save WiFi, key/URL, and boot mode.");

    if (rd_has_settings) {
        M5.Lcd.setTextColor(TFT_CYAN);
        M5.Lcd.setCursor(4, 164);
        M5.Lcd.print("Saved settings found.");
        M5.Lcd.setCursor(4, 176);
        M5.Lcd.print("Tap 'No Changes' to keep them.");
    }
}

static void rdHandleRoot() {
    String html =
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Core2Groq Setup</title>"
        "<style>"
        "body{background:#0f141b;color:#d6ecff;font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:20px;}"
        "h1{color:#63f3ff;margin-bottom:6px;}p,small{color:#8fb3c9;}"
        "label{display:block;margin-top:14px;font-weight:bold;color:#b5ddff;}"
        "input,select,textarea{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#111a24;color:#f2fbff;}"
        "textarea{min-height:120px;resize:vertical;}"
        "button{display:block;width:100%;padding:14px;margin-top:16px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;}"
        ".secondary{background:#202a35;color:#d6ecff;}"
        ".hint{font-size:.92em;color:#91a9bd;}"
        "</style></head><body>"
        "<h1>Core2Groq Setup</h1>"
        "<p class='hint'>WiFi is required for every mode. Groq key is only required for bot mode. Whisplay URL is only required for AI Screensaver mode.</p>"
        "<form method='post' action='/save'>"
        "<label>WiFi Network (SSID)</label>"
        "<input type='text' name='ssid' maxlength='63' required value='";
    html += rdEscapeHtml(String(rd_wifi_ssid));
    html +=
        "'>"
        "<label>WiFi Password</label>"
        "<input type='password' name='pass' maxlength='63' value='";
    html += rdEscapeHtml(String(rd_wifi_pass));
    html +=
        "'>"
        "<label>Groq API Key</label>"
        "<input type='text' name='groqKey' maxlength='127' value='";
    html += rdEscapeHtml(String(rd_groq_api_key));
    html +=
        "' placeholder='Needed for bot mode'>"
        "<label>Whisplay URL</label>"
        "<input type='text' name='whisplayUrl' maxlength='159' value='";
    html += rdEscapeHtml(String(rd_whisplay_url));
    html +=
        "' placeholder='http://10.160.0.136:17880'>"
        "<label>Chat Model</label>"
        "<select name='model'>";

    for (size_t i = 0; i < rdModelOptionCount(); i++) {
        html += "<option value='";
        html += RD_MODEL_OPTIONS[i].value;
        html += "'";
        if (String(rd_groq_model) == RD_MODEL_OPTIONS[i].value) {
            html += " selected";
        }
        html += ">";
        html += RD_MODEL_OPTIONS[i].label;
        html += "</option>";
    }

    html +=
        "</select>"
        "<label>Bot Personality</label>"
        "<textarea name='personality'>";
    html += rdEscapeHtml(String(rd_personality_prompt));
    html +=
        "</textarea>"
        "<label>Max Record Seconds</label>"
        "<input type='number' name='recordSec' min='2' max='15' required value='";
    html += String(rd_record_seconds);
    html +=
        "'>"
        "<label>Reply Auto-Scroll Speed</label>"
        "<select name='scrollMs'>";
    for (size_t i = 0; i < rdScrollSpeedOptionCount(); i++) {
        html += "<option value='";
        html += String(RD_SCROLL_SPEED_OPTIONS[i].ms);
        html += "'";
        if (rd_scroll_ms == RD_SCROLL_SPEED_OPTIONS[i].ms) {
            html += " selected";
        }
        html += ">";
        html += RD_SCROLL_SPEED_OPTIONS[i].label;
        html += "</option>";
    }
    html +=
        "</select>"
        "<label>Default Boot Mode</label>"
        "<select name='bootMode'>"
        "<option value='bot'";
    if (rd_boot_mode == RdBootMode::Bot) html += " selected";
    html += ">Bot</option><option value='ai'";
    if (rd_boot_mode == RdBootMode::AiScreensaver) html += " selected";
    html += ">AI Screensaver</option><option value='radio'";
    if (rd_boot_mode == RdBootMode::Radio) html += " selected";
    html +=
        ">Radio</option></select>"
        "<button type='submit'>Save &amp; Reboot</button>"
        "</form>";

    if (rd_has_settings) {
        html +=
            "<form method='post' action='/nochange'>"
            "<button class='secondary' type='submit'>No Changes</button>"
            "</form>";
    }

    html += "</body></html>";
    portalServer->send(200, "text/html", html);
}

static void rdHandleSave() {
    String ssid = portalServer->arg("ssid");
    String pass = portalServer->arg("pass");
    String groqKey = portalServer->arg("groqKey");
    String whisplayUrl = portalServer->arg("whisplayUrl");
    String model = portalServer->arg("model");
    String personality = portalServer->arg("personality");
    RdBootMode bootMode = rdParseBootMode(portalServer->arg("bootMode"));
    uint8_t recordSec = rdClampRecordSeconds(portalServer->arg("recordSec").toInt());
    uint16_t scrollMs = rdClampScrollMs(portalServer->arg("scrollMs").toInt());

    if (!ssid.length()) {
        portalServer->send(
            400, "text/html",
            "<html><body style='background:#0f141b;color:#ff7a7a;font-family:Arial;padding:40px'>"
            "<h2>SSID cannot be empty.</h2><a href='/' style='color:#63f3ff'>Go back</a></body></html>");
        return;
    }

    rdSaveSettings(ssid.c_str(), pass.c_str(), groqKey.c_str(), model.c_str(),
                   personality.c_str(), whisplayUrl.c_str(), recordSec, bootMode, scrollMs);

    portalServer->send(
        200, "text/html",
        "<html><head><meta charset='UTF-8'></head>"
        "<body style='background:#0f141b;color:#d6ecff;font-family:Arial;padding:40px'>"
        "<h2>Saved. Rebooting...</h2></body></html>");
    delay(1200);
    ESP.restart();
}

static void rdHandleNoChange() {
    portalServer->send(
        200, "text/html",
        "<html><head><meta charset='UTF-8'></head>"
        "<body style='background:#0f141b;color:#d6ecff;font-family:Arial;padding:40px'>"
        "<h2>Using saved settings.</h2></body></html>");
    delay(1200);
    portalDone = true;
}

static void rdInitPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(RD_AP_SSID, "");
    delay(500);

    portalDNS = new DNSServer();
    portalServer = new WebServer(80);

    portalDNS->start(53, "*", WiFi.softAPIP());
    portalServer->on("/", rdHandleRoot);
    portalServer->on("/save", HTTP_POST, rdHandleSave);
    portalServer->on("/nochange", HTTP_POST, rdHandleNoChange);
    portalServer->onNotFound(rdHandleRoot);
    portalServer->begin();

    portalDone = false;
    rdShowPortalScreen();
}

static void rdRunPortal() {
    portalDNS->processNextRequest();
    portalServer->handleClient();
}

static void rdClosePortal() {
    portalServer->stop();
    portalDNS->stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(300);
    delete portalServer;
    portalServer = nullptr;
    delete portalDNS;
    portalDNS = nullptr;
}
