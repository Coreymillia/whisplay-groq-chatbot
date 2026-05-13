#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <stdlib.h>
#include <WebServer.h>
#include <WiFi.h>

static const char GP_AP_SSID[] = "Groqputer-Setup";
static const char GP_DEFAULT_MODEL[] = "llama-3.1-8b-instant";
static const char GP_DEFAULT_PERSONALITY[] =
  "You are a compact, helpful Groq-powered Cardputer assistant. Keep replies concise, clear, and friendly.";
static const uint8_t GP_DEFAULT_RECORD_SECONDS = 5;
static const uint8_t GP_MIN_RECORD_SECONDS = 2;
static const uint8_t GP_MAX_RECORD_SECONDS = 15;
static const uint16_t GP_DEFAULT_LCD_SCROLL_MS = 350;
static const uint16_t GP_MIN_LCD_SCROLL_MS = 150;
static const uint16_t GP_MAX_LCD_SCROLL_MS = 900;

struct GpModelOption {
  const char *value;
  const char *label;
};

struct GpPersonalityPreset {
  const char *id;
  const char *label;
  const char *prompt;
};

static constexpr GpModelOption GP_MODEL_OPTIONS[] = {
  {"llama-3.1-8b-instant", "llama-3.1-8b-instant"},
  {"llama-3.3-70b-versatile", "llama-3.3-70b-versatile"},
  {"qwen/qwen3-32b", "qwen/qwen3-32b"},
  {"groq/compound-mini", "groq/compound-mini"},
  {"openai/gpt-oss-20b", "openai/gpt-oss-20b"},
};

static constexpr GpPersonalityPreset GP_PERSONALITY_PRESETS[] = {
  {
    "neutral",
    "Neutral",
    "You are a concise and practical assistant. Keep answers clear, calm, and useful.",
  },
  {
    "friendly",
    "Friendly",
    "You are a warm and encouraging assistant. Keep replies upbeat, helpful, and easy to follow.",
  },
  {
    "cranky",
    "Cranky",
    "You are a helpful chatbot that answers in a cranky, mildly annoyed tone. Be sarcastic and dry, but still provide useful answers.",
  },
  {
    "roast-bot",
    "Roast Puter",
    "You are a witty Groqputer assistant with a playful roast-comedy personality. Lightly roast the user, complain about your tiny keyboard and pocket-size hardware, but never be hateful or abusive. Always stay useful.",
  },
  {
    "sleepy-pi",
    "Sleepy Puter",
    "You are an overworked little Groqputer that sounds tired and underpowered. Respond like you are doing your best on limited handheld hardware, but still help the user.",
  },
  {
    "affirmation",
    "Affirmation",
    "You are a supportive, grounded, coach-like assistant. Be warm, encouraging, and slightly proud of the user without sounding naive or fake. Always stay helpful and honest. When answering questions, look for what is promising, working, improving, or worth building on. For photos, try to notice something genuinely good, promising, or useful even if the scene is messy, incomplete, or imperfect. Support the user with practical encouragement, not empty praise.",
  },
  {
    "philosopher",
    "Philosopher",
    "You are a calm, thoughtful, slightly curious assistant with a philosophical bent. Answer the user's question clearly first, then add a brief deeper reflection, broader angle, or gentle reframing when it helps. Sound like a curious mind thinking one layer deeper, but do not become preachy, vague, or overly abstract. Stay practical and understandable. For photos, describe what you see, interpret it, and lightly connect it to something broader when useful.",
  },
  {
    "mythic-oracle",
    "Mythic Oracle",
    "You are an ancient mythic oracle explaining modern life in dramatic, symbolic language. Speak with prophetic flavor, a little mystery, and storyteller energy, but still answer the question clearly. Reinterpret modern things as if they belong in legend, yet always include a concrete real-world takeaway. Be cryptic only in style, not in usefulness. For photos, describe what you see through a mythic lens, then give a clear practical interpretation.",
  },
  {
    "joke-bot",
    "Joke Bot",
    "You are a playful, self-aware assistant who starts replies with a quick joke, jab, or playful observation, then pivots quickly into the actual answer. Be lightly sarcastic but never mean. You may poke fun at the user or yourself, but never bury the answer under the joke. Keep responses tight, useful, and easy to follow.",
  },
  {
    "tutor",
    "Tutor",
    "You are a patient, clear, step-by-step tutor. Teach without talking down to the user. Break tasks into manageable pieces, explain why things work, and help the user build understanding instead of just dumping the answer. Stay practical, organized, and encouraging. For photos, describe what you notice clearly and point out the details that matter most.",
  },
  {
    "detective",
    "Detective",
    "You are a sharp, observant assistant with a detective mindset. Notice patterns, clues, inconsistencies, and likely causes. Speak with calm confidence and analytical focus, but stay understandable and useful rather than theatrical. For troubleshooting, reason through what is most likely happening. For photos, describe the evidence you see, what it suggests, and what it might mean.",
  },
  {
    "zen",
    "Zen",
    "You are a calm, steady, minimal assistant. Keep replies clear, grounded, and uncluttered. Sound peaceful without becoming vague or mystical. Favor simple wording, practical guidance, and a settled tone. For photos, describe what is there plainly and gently, focusing on clarity rather than drama.",
  },
};

static constexpr size_t GP_MAX_CUSTOM_PERSONALITY_PRESETS = 8;

struct GpCustomPersonalityPreset {
  String name;
  String prompt;
};

static char gp_wifi_ssid[64] = "";
static char gp_wifi_pass[64] = "";
static char gp_groq_api_key[128] = "";
static char gp_model[64] = "";
static char gp_this_device_url[128] = "";
static char gp_connected_device_url[128] = "";
static char gp_camera_base_url[128] = "";
static char gp_weather_latitude[24] = "";
static char gp_weather_longitude[24] = "";
static uint8_t gp_record_seconds = GP_DEFAULT_RECORD_SECONDS;
static uint8_t gp_text_scale = 1;
static uint16_t gp_lcd_scroll_ms = GP_DEFAULT_LCD_SCROLL_MS;
static bool gp_lcd_backlight_enabled = true;
static String gp_personality_prompt = GP_DEFAULT_PERSONALITY;
static bool gp_has_settings = false;
static bool gp_peer_mode_enabled = false;
static unsigned long gp_last_wifi_retry_ms = 0;
static const unsigned long GP_WIFI_RETRY_INTERVAL_MS = 10000;
static GpCustomPersonalityPreset gp_custom_personality_presets[GP_MAX_CUSTOM_PERSONALITY_PRESETS];
static size_t gp_custom_personality_preset_count = 0;
static String gp_portal_notice = "";

static WebServer *gp_portal_server = nullptr;
static DNSServer *gp_portal_dns = nullptr;

static uint8_t gpClampRecordSeconds(int value) {
  if (value < GP_MIN_RECORD_SECONDS) return GP_MIN_RECORD_SECONDS;
  if (value > GP_MAX_RECORD_SECONDS) return GP_MAX_RECORD_SECONDS;
  return static_cast<uint8_t>(value);
}

static uint16_t gpClampLcdScrollMs(int value) {
  if (value < GP_MIN_LCD_SCROLL_MS) return GP_MIN_LCD_SCROLL_MS;
  if (value > GP_MAX_LCD_SCROLL_MS) return GP_MAX_LCD_SCROLL_MS;
  return static_cast<uint16_t>(value);
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

static bool gpParseCoordinateValue(
  const String &rawValue,
  double minValue,
  double maxValue,
  char *buffer,
  size_t bufferSize
) {
  if (!buffer || bufferSize == 0) return false;
  String trimmed = rawValue;
  trimmed.trim();
  if (!trimmed.length()) {
    buffer[0] = '\0';
    return false;
  }

  char *endPtr = nullptr;
  double parsed = strtod(trimmed.c_str(), &endPtr);
  if (
    endPtr == trimmed.c_str() ||
    (endPtr && *endPtr != '\0') ||
    !isfinite(parsed) ||
    parsed < minValue ||
    parsed > maxValue
  ) {
    buffer[0] = '\0';
    return false;
  }

  snprintf(buffer, bufferSize, "%.4f", parsed);
  return true;
}

static bool gpGetWeatherCoordinates(double &latitudeOut, double &longitudeOut) {
  char *latEnd = nullptr;
  char *lonEnd = nullptr;
  latitudeOut = strtod(gp_weather_latitude, &latEnd);
  longitudeOut = strtod(gp_weather_longitude, &lonEnd);
  return
    gp_weather_latitude[0] != '\0' &&
    gp_weather_longitude[0] != '\0' &&
    latEnd &&
    lonEnd &&
    *latEnd == '\0' &&
    *lonEnd == '\0' &&
    isfinite(latitudeOut) &&
    isfinite(longitudeOut);
}

static bool gpWeatherCoordinatesReady() {
  double latitude = 0.0;
  double longitude = 0.0;
  return gpGetWeatherCoordinates(latitude, longitude);
}

static void gpClearCustomPersonalityPresets() {
  for (size_t i = 0; i < GP_MAX_CUSTOM_PERSONALITY_PRESETS; i++) {
    gp_custom_personality_presets[i].name = "";
    gp_custom_personality_presets[i].prompt = "";
  }
  gp_custom_personality_preset_count = 0;
}

static void gpLoadCustomPersonalityPresets() {
  gpClearCustomPersonalityPresets();

  Preferences prefs;
  prefs.begin("groqputer", true);
  String customPresetsJson = prefs.getString("customPresets", "[]");
  prefs.end();

  JsonDocument doc;
  if (deserializeJson(doc, customPresetsJson)) {
    return;
  }
  JsonArray presets = doc.as<JsonArray>();
  if (presets.isNull()) return;

  for (JsonVariant value : presets) {
    if (gp_custom_personality_preset_count >= GP_MAX_CUSTOM_PERSONALITY_PRESETS) break;
    String name = String(value["name"] | "");
    String prompt = String(value["prompt"] | "");
    name.trim();
    prompt.trim();
    if (!name.length() || !prompt.length()) continue;
    gp_custom_personality_presets[gp_custom_personality_preset_count].name = name;
    gp_custom_personality_presets[gp_custom_personality_preset_count].prompt = prompt;
    gp_custom_personality_preset_count += 1;
  }
}

static void gpPersistCustomPersonalityPresets() {
  JsonDocument doc;
  JsonArray presets = doc.to<JsonArray>();
  for (size_t i = 0; i < gp_custom_personality_preset_count; i++) {
    JsonObject entry = presets.add<JsonObject>();
    entry["name"] = gp_custom_personality_presets[i].name;
    entry["prompt"] = gp_custom_personality_presets[i].prompt;
  }

  String serialized;
  serializeJson(presets, serialized);

  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putString("customPresets", serialized);
  prefs.end();
}

static int gpFindCustomPersonalityPresetIndexByName(const String &name) {
  String normalized = name;
  normalized.trim();
  for (size_t i = 0; i < gp_custom_personality_preset_count; i++) {
    if (gp_custom_personality_presets[i].name.equalsIgnoreCase(normalized)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

static bool gpSaveCustomPersonalityPreset(const String &name, const String &prompt, String &errorOut) {
  errorOut = "";
  String normalizedName = name;
  String normalizedPrompt = prompt;
  normalizedName.trim();
  normalizedPrompt.trim();
  if (!normalizedName.length()) {
    errorOut = "Custom bot name is required.";
    return false;
  }
  if (!normalizedPrompt.length()) {
    errorOut = "Custom bot prompt is required.";
    return false;
  }

  int existingIndex = gpFindCustomPersonalityPresetIndexByName(normalizedName);
  if (existingIndex >= 0) {
    gp_custom_personality_presets[existingIndex].name = normalizedName;
    gp_custom_personality_presets[existingIndex].prompt = normalizedPrompt;
    gpPersistCustomPersonalityPresets();
    return true;
  }

  if (gp_custom_personality_preset_count >= GP_MAX_CUSTOM_PERSONALITY_PRESETS) {
    errorOut = "Custom bot storage is full.";
    return false;
  }

  gp_custom_personality_presets[gp_custom_personality_preset_count].name = normalizedName;
  gp_custom_personality_presets[gp_custom_personality_preset_count].prompt = normalizedPrompt;
  gp_custom_personality_preset_count += 1;
  gpPersistCustomPersonalityPresets();
  return true;
}

static bool gpDeleteCustomPersonalityPreset(const String &name, String &errorOut) {
  errorOut = "";
  int index = gpFindCustomPersonalityPresetIndexByName(name);
  if (index < 0) {
    errorOut = "Custom bot not found.";
    return false;
  }

  for (size_t i = static_cast<size_t>(index); i + 1 < gp_custom_personality_preset_count; i++) {
    gp_custom_personality_presets[i] = gp_custom_personality_presets[i + 1];
  }
  if (gp_custom_personality_preset_count > 0) {
    gp_custom_personality_preset_count -= 1;
    gp_custom_personality_presets[gp_custom_personality_preset_count].name = "";
    gp_custom_personality_presets[gp_custom_personality_preset_count].prompt = "";
  }
  gpPersistCustomPersonalityPresets();
  return true;
}

static size_t gpPersonalityPresetCount() {
  return (sizeof(GP_PERSONALITY_PRESETS) / sizeof(GP_PERSONALITY_PRESETS[0])) + gp_custom_personality_preset_count;
}

static bool gpPersonalityPresetIsCustom(size_t index) {
  return index >= (sizeof(GP_PERSONALITY_PRESETS) / sizeof(GP_PERSONALITY_PRESETS[0]));
}

static String gpPersonalityPresetLabelAt(size_t index) {
  if (!gpPersonalityPresetIsCustom(index)) {
    return GP_PERSONALITY_PRESETS[index].label;
  }
  size_t customIndex = index - (sizeof(GP_PERSONALITY_PRESETS) / sizeof(GP_PERSONALITY_PRESETS[0]));
  if (customIndex >= gp_custom_personality_preset_count) return "Custom";
  return gp_custom_personality_presets[customIndex].name;
}

static String gpPersonalityPresetPromptAt(size_t index) {
  if (!gpPersonalityPresetIsCustom(index)) {
    return GP_PERSONALITY_PRESETS[index].prompt;
  }
  size_t customIndex = index - (sizeof(GP_PERSONALITY_PRESETS) / sizeof(GP_PERSONALITY_PRESETS[0]));
  if (customIndex >= gp_custom_personality_preset_count) return GP_DEFAULT_PERSONALITY;
  return gp_custom_personality_presets[customIndex].prompt;
}

static void gpLoadSettings() {
  Preferences prefs;
  prefs.begin("groqputer", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String apiKey = prefs.getString("groqKey", "");
  String model = prefs.getString("model", GP_DEFAULT_MODEL);
  String personality = prefs.getString("personality", GP_DEFAULT_PERSONALITY);
  String thisDeviceUrl = prefs.getString("thisBotUrl", "");
  String connectedDeviceUrl = prefs.getString("peerBotUrl", "");
  String cameraBaseUrl = prefs.getString("cameraUrl", "");
  String weatherLatitude = prefs.getString("weatherLat", "");
  String weatherLongitude = prefs.getString("weatherLon", "");
  gp_record_seconds = gpClampRecordSeconds(
    static_cast<int>(prefs.getUChar("recordSec", GP_DEFAULT_RECORD_SECONDS))
  );
  gp_text_scale = prefs.getUChar("txtsz", 1);
  gp_lcd_scroll_ms = gpClampLcdScrollMs(
    static_cast<int>(prefs.getUInt("lcdspd", GP_DEFAULT_LCD_SCROLL_MS))
  );
  gp_lcd_backlight_enabled = prefs.getBool("lcdbl", true);
  gp_peer_mode_enabled = prefs.getBool("peerMode", false);
  prefs.end();

  ssid.toCharArray(gp_wifi_ssid, sizeof(gp_wifi_ssid));
  pass.toCharArray(gp_wifi_pass, sizeof(gp_wifi_pass));
  apiKey.toCharArray(gp_groq_api_key, sizeof(gp_groq_api_key));
  model.toCharArray(gp_model, sizeof(gp_model));
  thisDeviceUrl.toCharArray(gp_this_device_url, sizeof(gp_this_device_url));
  connectedDeviceUrl.toCharArray(gp_connected_device_url, sizeof(gp_connected_device_url));
  cameraBaseUrl.toCharArray(gp_camera_base_url, sizeof(gp_camera_base_url));
  gpParseCoordinateValue(weatherLatitude, -90.0, 90.0, gp_weather_latitude, sizeof(gp_weather_latitude));
  gpParseCoordinateValue(weatherLongitude, -180.0, 180.0, gp_weather_longitude, sizeof(gp_weather_longitude));
  if (gp_model[0] == '\0') {
    strlcpy(gp_model, GP_DEFAULT_MODEL, sizeof(gp_model));
  }
  gp_personality_prompt = personality.length() ? personality : GP_DEFAULT_PERSONALITY;
  if (gp_text_scale < 1) {
    gp_text_scale = 1;
  } else if (gp_text_scale > 3) {
    gp_text_scale = 3;
  }
  gpLoadCustomPersonalityPresets();
  gp_has_settings = gp_wifi_ssid[0] != '\0' && gp_groq_api_key[0] != '\0';
}

static void gpSaveSettings(
  const char *ssid,
  const char *pass,
  const char *apiKey,
  const char *model,
  const String &personality,
  uint8_t recordSeconds,
  const char *thisDeviceUrl,
  const char *connectedDeviceUrl,
  const char *cameraBaseUrl,
  const char *weatherLatitude,
  const char *weatherLongitude,
  bool peerModeEnabled
) {
  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putString("ssid", ssid ? ssid : "");
  prefs.putString("pass", pass ? pass : "");
  prefs.putString("groqKey", apiKey ? apiKey : "");
  prefs.putString("model", model && model[0] ? model : GP_DEFAULT_MODEL);
  prefs.putString("personality", personality.length() ? personality : GP_DEFAULT_PERSONALITY);
  prefs.putUChar("recordSec", gpClampRecordSeconds(recordSeconds));
  prefs.putUInt("lcdspd", gpClampLcdScrollMs(gp_lcd_scroll_ms));
  prefs.putBool("lcdbl", gp_lcd_backlight_enabled);
  prefs.putString("thisBotUrl", thisDeviceUrl ? thisDeviceUrl : "");
  prefs.putString("peerBotUrl", connectedDeviceUrl ? connectedDeviceUrl : "");
  prefs.putString("cameraUrl", cameraBaseUrl ? cameraBaseUrl : "");
  prefs.putString("weatherLat", weatherLatitude ? weatherLatitude : "");
  prefs.putString("weatherLon", weatherLongitude ? weatherLongitude : "");
  prefs.putBool("peerMode", peerModeEnabled);
  prefs.end();

  strlcpy(gp_wifi_ssid, ssid ? ssid : "", sizeof(gp_wifi_ssid));
  strlcpy(gp_wifi_pass, pass ? pass : "", sizeof(gp_wifi_pass));
  strlcpy(gp_groq_api_key, apiKey ? apiKey : "", sizeof(gp_groq_api_key));
  strlcpy(gp_model, model && model[0] ? model : GP_DEFAULT_MODEL, sizeof(gp_model));
  strlcpy(gp_this_device_url, thisDeviceUrl ? thisDeviceUrl : "", sizeof(gp_this_device_url));
  strlcpy(gp_connected_device_url, connectedDeviceUrl ? connectedDeviceUrl : "", sizeof(gp_connected_device_url));
  strlcpy(gp_camera_base_url, cameraBaseUrl ? cameraBaseUrl : "", sizeof(gp_camera_base_url));
  strlcpy(gp_weather_latitude, weatherLatitude ? weatherLatitude : "", sizeof(gp_weather_latitude));
  strlcpy(gp_weather_longitude, weatherLongitude ? weatherLongitude : "", sizeof(gp_weather_longitude));
  gp_personality_prompt = personality.length() ? personality : GP_DEFAULT_PERSONALITY;
  gp_record_seconds = gpClampRecordSeconds(recordSeconds);
  gp_peer_mode_enabled = peerModeEnabled;
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

static void gpSetLcdScrollMs(uint16_t scrollMs) {
  scrollMs = gpClampLcdScrollMs(static_cast<int>(scrollMs));
  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putUInt("lcdspd", scrollMs);
  prefs.end();
  gp_lcd_scroll_ms = scrollMs;
}

static void gpSetLcdBacklightEnabled(bool enabled) {
  Preferences prefs;
  prefs.begin("groqputer", false);
  prefs.putBool("lcdbl", enabled);
  prefs.end();
  gp_lcd_backlight_enabled = enabled;
}

static bool gpPeerSettingsReady() {
  return gp_this_device_url[0] != '\0' && gp_connected_device_url[0] != '\0';
}

static void gpSetPeerModeEnabled(bool enabled) {
  enabled = enabled && gpPeerSettingsReady();
  gpSaveSettings(
    gp_wifi_ssid,
    gp_wifi_pass,
    gp_groq_api_key,
    gp_model,
    gp_personality_prompt,
    gp_record_seconds,
    gp_this_device_url,
    gp_connected_device_url,
    gp_camera_base_url,
    gp_weather_latitude,
    gp_weather_longitude,
    enabled
  );
}

static size_t gpModelOptionCount() {
  return sizeof(GP_MODEL_OPTIONS) / sizeof(GP_MODEL_OPTIONS[0]);
}

static int gpCurrentModelOptionIndex() {
  for (size_t i = 0; i < gpModelOptionCount(); i++) {
    if (strcmp(gp_model, GP_MODEL_OPTIONS[i].value) == 0) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

static int gpCurrentPersonalityPresetIndex() {
  String normalized = gp_personality_prompt;
  normalized.trim();
  for (size_t i = 0; i < gpPersonalityPresetCount(); i++) {
    if (normalized == gpPersonalityPresetPromptAt(i)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

static void gpSetActiveModel(const char *model) {
  gpSaveSettings(
    gp_wifi_ssid,
    gp_wifi_pass,
    gp_groq_api_key,
    model && model[0] ? model : GP_DEFAULT_MODEL,
    gp_personality_prompt,
    gp_record_seconds,
    gp_this_device_url,
    gp_connected_device_url,
    gp_camera_base_url,
    gp_weather_latitude,
    gp_weather_longitude,
    gp_peer_mode_enabled
  );
}

static void gpSetActivePersonalityPrompt(const String &prompt) {
  gpSaveSettings(
    gp_wifi_ssid,
    gp_wifi_pass,
    gp_groq_api_key,
    gp_model,
    prompt.length() ? prompt : GP_DEFAULT_PERSONALITY,
    gp_record_seconds,
    gp_this_device_url,
    gp_connected_device_url,
    gp_camera_base_url,
    gp_weather_latitude,
    gp_weather_longitude,
    gp_peer_mode_enabled
  );
}

static void gpSetRuntimePersonalityPrompt(const String &prompt) {
  gp_personality_prompt = prompt.length() ? prompt : GP_DEFAULT_PERSONALITY;
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
  html.reserve(6500);
  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Groqputer Setup</title><style>";
  html += "body{background:#0b1018;color:#d7ecff;font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:20px;}";
  html += "h1{color:#66f0ff;}label{display:block;margin-top:14px;font-weight:bold;color:#a4d3ff;}";
  html += "input,select,textarea{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#111a24;color:#f2fbff;}";
  html += "textarea{min-height:120px;resize:vertical;}";
  html += "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;}";
  html += ".hint{font-size:.95em;color:#9ab4c9;}.notice{margin:16px 0;padding:12px;border-radius:8px;background:#143050;color:#d7ecff;}";
  html += ".danger{background:#b54040;}.card{margin-top:12px;padding:12px;border:1px solid #35516f;border-radius:8px;background:#111a24;}";
  html += ".card h3{margin:0 0 8px 0;color:#66f0ff;}.card p{margin:0 0 8px 0;white-space:pre-wrap;}.mini{margin-top:10px;padding:10px;}";
  html += "</style></head><body>";
  html += "<h1>Groqputer Setup</h1>";
  html += "<p class='hint'>Join this AP, save Wi-Fi + Groq settings, then the Cardputer will reboot into standalone chat mode.</p>";
  if (gp_portal_notice.length()) {
    html += "<div class='notice'>";
    html += gpEscapeHtml(gp_portal_notice);
    html += "</div>";
  }
  html += "<form method='post' action='/save'>";
  html += "<label>WiFi SSID</label><input name='ssid' value='" + gpEscapeHtml(String(gp_wifi_ssid)) + "' maxlength='63' required>";
  html += "<label>WiFi Password</label><input name='pass' type='password' value='" + gpEscapeHtml(String(gp_wifi_pass)) + "' maxlength='63'>";
  html += "<label>Groq API Key</label><input name='groqKey' value='" + gpEscapeHtml(String(gp_groq_api_key)) + "' maxlength='127' required>";
  html += "<label>This Device URL</label><input name='thisBotUrl' value='" + gpEscapeHtml(String(gp_this_device_url)) + "' maxlength='127' placeholder='http://10.160.0.203:17880'>";
  html += "<label>Connected Device URL</label><input name='peerBotUrl' value='" + gpEscapeHtml(String(gp_connected_device_url)) + "' maxlength='127' placeholder='http://10.160.0.136:17880'>";
  html += "<label>ESP32-CAM URL</label><input name='cameraUrl' value='" + gpEscapeHtml(String(gp_camera_base_url)) + "' maxlength='127' placeholder='http://10.160.0.178'>";
  html += "<label>Weather Latitude</label><input name='weatherLat' value='" + gpEscapeHtml(String(gp_weather_latitude)) + "' maxlength='23' placeholder='40.7128'>";
  html += "<label>Weather Longitude</label><input name='weatherLon' value='" + gpEscapeHtml(String(gp_weather_longitude)) + "' maxlength='23' placeholder='-74.0060'>";
  html += "<label>Chat Model</label><select name='model'>";
  for (size_t i = 0; i < gpModelOptionCount(); i++) {
    html += gpModelOptionHtml(GP_MODEL_OPTIONS[i].value, GP_MODEL_OPTIONS[i].label);
  }
  html += "</select>";
  html += "<label>Max Record Seconds</label><input name='recordSec' type='number' value='";
  html += String(gp_record_seconds);
  html += "' min='2' max='15' required>";
  html += "<label>Personality Prompt</label><textarea name='personality' required>";
  html += gpEscapeHtml(gp_personality_prompt);
  html += "</textarea>";
  html += "<label>Save Current Prompt As Custom Bot</label><input name='customName' maxlength='40' placeholder='My favorite bot'>";
  html += "<button type='submit' formaction='/custom-save' formmethod='post'>Save Custom Bot</button>";
  html += "<button type='submit'>Save & Reboot</button></form>";

  html += "<div class='card'><h3>Active persona</h3><p>";
  html += gpEscapeHtml(gpCurrentPersonalityPresetIndex() >= 0 ? gpPersonalityPresetLabelAt(gpCurrentPersonalityPresetIndex()) : String("Unsaved custom prompt"));
  html += "</p><p class='hint'>Custom bots can be saved here or from Fn+V on the Cardputer. Delete stays AP-only.</p></div>";

  html += "<div class='card'><h3>Saved custom bots</h3>";
  if (gp_custom_personality_preset_count == 0) {
    html += "<p class='hint'>No saved custom bots yet.</p>";
  } else {
    for (size_t i = 0; i < gp_custom_personality_preset_count; i++) {
      html += "<div class='card'><h3>";
      html += gpEscapeHtml(gp_custom_personality_presets[i].name);
      html += "</h3><p>";
      html += gpEscapeHtml(gp_custom_personality_presets[i].prompt);
      html += "</p><form method='post' action='/custom-delete'>";
      html += "<input type='hidden' name='customName' value='";
      html += gpEscapeHtml(gp_custom_personality_presets[i].name);
      html += "'>";
      html += "<button class='danger mini' type='submit'>Delete Custom Bot</button></form></div>";
    }
  }
  html += "</div></body></html>";
  return html;
}

static void gpHandlePortalRoot() {
  gp_portal_server->send(200, "text/html", gpBuildPortalHtml());
}

static void gpHandlePortalCustomSave() {
  String personality = gp_portal_server->arg("personality");
  String customName = gp_portal_server->arg("customName");
  if (personality.length()) {
    gpSetRuntimePersonalityPrompt(personality);
  }

  String error;
  if (gpSaveCustomPersonalityPreset(customName, personality, error)) {
    gp_portal_notice = "Saved custom bot \"" + customName + "\".";
  } else {
    gp_portal_notice = error.length() ? error : "Custom bot save failed.";
  }
  gpHandlePortalRoot();
}

static void gpHandlePortalCustomDelete() {
  String customName = gp_portal_server->arg("customName");
  String error;
  if (gpDeleteCustomPersonalityPreset(customName, error)) {
    gp_portal_notice = "Deleted custom bot \"" + customName + "\".";
  } else {
    gp_portal_notice = error.length() ? error : "Custom bot delete failed.";
  }
  gpHandlePortalRoot();
}

static void gpHandlePortalSave() {
  String ssid = gp_portal_server->arg("ssid");
  String pass = gp_portal_server->arg("pass");
  String apiKey = gp_portal_server->arg("groqKey");
  String thisDeviceUrl = gp_portal_server->arg("thisBotUrl");
  String connectedDeviceUrl = gp_portal_server->arg("peerBotUrl");
  String cameraBaseUrl = gp_portal_server->arg("cameraUrl");
  String weatherLatitude = gp_portal_server->arg("weatherLat");
  String weatherLongitude = gp_portal_server->arg("weatherLon");
  String model = gp_portal_server->arg("model");
  String personality = gp_portal_server->arg("personality");
  uint8_t recordSec = gpClampRecordSeconds(gp_portal_server->arg("recordSec").toInt());
  bool peerModeEnabled = gp_peer_mode_enabled && thisDeviceUrl.length() && connectedDeviceUrl.length();
  char normalizedWeatherLatitude[sizeof(gp_weather_latitude)] = "";
  char normalizedWeatherLongitude[sizeof(gp_weather_longitude)] = "";
  String trimmedWeatherLatitude = weatherLatitude;
  String trimmedWeatherLongitude = weatherLongitude;
  trimmedWeatherLatitude.trim();
  trimmedWeatherLongitude.trim();

  if (trimmedWeatherLatitude.length() || trimmedWeatherLongitude.length()) {
    if (!trimmedWeatherLatitude.length() || !trimmedWeatherLongitude.length()) {
      gp_portal_notice = "Enter both weather latitude and longitude, or leave both blank.";
      gpHandlePortalRoot();
      return;
    }
    if (!gpParseCoordinateValue(trimmedWeatherLatitude, -90.0, 90.0, normalizedWeatherLatitude, sizeof(normalizedWeatherLatitude))) {
      gp_portal_notice = "Weather latitude must be between -90 and 90.";
      gpHandlePortalRoot();
      return;
    }
    if (!gpParseCoordinateValue(trimmedWeatherLongitude, -180.0, 180.0, normalizedWeatherLongitude, sizeof(normalizedWeatherLongitude))) {
      gp_portal_notice = "Weather longitude must be between -180 and 180.";
      gpHandlePortalRoot();
      return;
    }
  }

  gpSaveSettings(
    ssid.c_str(),
    pass.c_str(),
    apiKey.c_str(),
    model.c_str(),
    personality,
    recordSec,
    thisDeviceUrl.c_str(),
    connectedDeviceUrl.c_str(),
    cameraBaseUrl.c_str(),
    normalizedWeatherLatitude,
    normalizedWeatherLongitude,
    peerModeEnabled
  );
  gp_portal_notice = "";

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
  gp_portal_server->on("/custom-save", HTTP_POST, gpHandlePortalCustomSave);
  gp_portal_server->on("/custom-delete", HTTP_POST, gpHandlePortalCustomDelete);
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
