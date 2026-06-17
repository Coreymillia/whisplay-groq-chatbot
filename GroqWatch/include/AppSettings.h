#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace GroqWatch {

static constexpr const char *kPrefsNamespace = "groqwatch";
static constexpr const char *kKeySsid = "ssid";
static constexpr const char *kKeyPass = "pass";
static constexpr const char *kKeyTimezone = "tz";
static constexpr const char *kKeyGroqKey = "groqKey";
static constexpr const char *kKeyWhisplayUrl = "whisplay";
static constexpr const char *kKeyLat = "lat";
static constexpr const char *kKeyLon = "lon";
static constexpr const char *kKeyModel = "model";
static constexpr const char *kKeyPersona = "persona";
static constexpr const char *kKeyBootMode = "bootMode";
static constexpr const char *kKeyStyle = "style";
static constexpr const char *kKeyWatchFace = "wface";
static constexpr const char *kKeyScreenTimeout = "scrto";
static constexpr const char *kKeyWakeMethod = "wakem";

static constexpr const char *kDefaultModel = "llama-3.1-8b-instant";
static constexpr const char *kDefaultPersona =
    "You are a concise, practical wrist-worn assistant. Keep replies short and helpful.";

static constexpr const char *kWatchFaceNames[] = {"Particles", "NWS", "Stub"};
static constexpr uint8_t kWatchFaceCount = 3;

struct AppSettings {
    char wifiSsid[64];
    char wifiPass[64];
    char timezone[64];
    char groqApiKey[128];
    char whisplayUrl[160];
    char latitude[32];
    char longitude[32];
    char model[64];
    char personaPrompt[512];
    char bootMode[12];
    uint8_t watchStyle;
    uint8_t watchFace;
    uint16_t screenTimeoutSec;  // 0 = never
    uint8_t wakeMethod;         // 0 = button, 1 = shake, 2 = both
};

enum class AppMode : uint8_t {
    Watch,
    AiScreensaver,
    Bot,
    Tetris,
    Settings,
};

inline void defaultSettings(AppSettings &settings) {
    memset(&settings, 0, sizeof(settings));
    strlcpy(settings.timezone, "CST6CDT,M3.2.0/2,M11.1.0/2", sizeof(settings.timezone));
    strlcpy(settings.model, kDefaultModel, sizeof(settings.model));
    strlcpy(settings.personaPrompt, kDefaultPersona, sizeof(settings.personaPrompt));
    strlcpy(settings.bootMode, "watch", sizeof(settings.bootMode));
    settings.watchStyle = 0;
    settings.watchFace  = 0;
    settings.screenTimeoutSec = 30;
    settings.wakeMethod = 0;  // button only
}

inline bool hasWiFi(const AppSettings &settings) {
    return settings.wifiSsid[0] != '\0';
}

inline bool hasGroqKey(const AppSettings &settings) {
    return settings.groqApiKey[0] != '\0';
}

inline bool hasWhisplayUrl(const AppSettings &settings) {
    return settings.whisplayUrl[0] != '\0';
}

inline bool hasNwsLocation(const AppSettings &settings) {
    return settings.latitude[0] != '\0' && settings.longitude[0] != '\0';
}

inline void loadSettings(AppSettings &settings) {
    defaultSettings(settings);

    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    String ssid = prefs.getString(kKeySsid, "");
    String pass = prefs.getString(kKeyPass, "");
    String tz = prefs.getString(kKeyTimezone, settings.timezone);
    String groqKey = prefs.getString(kKeyGroqKey, "");
    String whisplayUrl = prefs.getString(kKeyWhisplayUrl, "");
    String lat = prefs.getString(kKeyLat, "");
    String lon = prefs.getString(kKeyLon, "");
    String model = prefs.getString(kKeyModel, kDefaultModel);
    String persona = prefs.getString(kKeyPersona, kDefaultPersona);
    String bootMode = prefs.getString(kKeyBootMode, "watch");
    settings.watchStyle  = prefs.getUChar(kKeyStyle, 0);
    settings.watchFace   = prefs.getUChar(kKeyWatchFace, 0);
    settings.screenTimeoutSec = prefs.getUShort(kKeyScreenTimeout, 30);
    settings.wakeMethod  = prefs.getUChar(kKeyWakeMethod, 0);
    prefs.end();

    ssid.toCharArray(settings.wifiSsid, sizeof(settings.wifiSsid));
    pass.toCharArray(settings.wifiPass, sizeof(settings.wifiPass));
    tz.toCharArray(settings.timezone, sizeof(settings.timezone));
    groqKey.toCharArray(settings.groqApiKey, sizeof(settings.groqApiKey));
    whisplayUrl.toCharArray(settings.whisplayUrl, sizeof(settings.whisplayUrl));
    lat.toCharArray(settings.latitude, sizeof(settings.latitude));
    lon.toCharArray(settings.longitude, sizeof(settings.longitude));
    model.toCharArray(settings.model, sizeof(settings.model));
    persona.toCharArray(settings.personaPrompt, sizeof(settings.personaPrompt));
    bootMode.toCharArray(settings.bootMode, sizeof(settings.bootMode));

    if (settings.watchStyle > 2) settings.watchStyle = 0;
    if (settings.watchFace  > 2) settings.watchFace  = 0;
    if (settings.screenTimeoutSec > 3600) settings.screenTimeoutSec = 30;
    if (settings.wakeMethod > 2) settings.wakeMethod = 0;
    if (settings.bootMode[0] == '\0') strlcpy(settings.bootMode, "watch", sizeof(settings.bootMode));
}

inline void saveSettings(const AppSettings &settings) {
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putString(kKeySsid, settings.wifiSsid);
    prefs.putString(kKeyPass, settings.wifiPass);
    prefs.putString(kKeyTimezone, settings.timezone);
    prefs.putString(kKeyGroqKey, settings.groqApiKey);
    prefs.putString(kKeyWhisplayUrl, settings.whisplayUrl);
    prefs.putString(kKeyLat, settings.latitude);
    prefs.putString(kKeyLon, settings.longitude);
    prefs.putString(kKeyModel, settings.model);
    prefs.putString(kKeyPersona, settings.personaPrompt);
    prefs.putString(kKeyBootMode, settings.bootMode);
    prefs.putUChar(kKeyStyle, settings.watchStyle);
    prefs.putUChar(kKeyWatchFace, settings.watchFace);
    prefs.putUShort(kKeyScreenTimeout, settings.screenTimeoutSec);
    prefs.putUChar(kKeyWakeMethod, settings.wakeMethod);
    prefs.end();
}

inline void clearSettings() {
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.clear();
    prefs.end();
}

}  // namespace GroqWatch
