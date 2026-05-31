#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>

#include "Portal.h"

static constexpr size_t COMPANION_MAX_PRESETS = 16;
static constexpr size_t COMPANION_MAX_PHOTOS = 96;
static constexpr size_t COMPANION_GENERATED_IMAGES_PAGE_SIZE = 12;

struct CompanionState {
  bool ready = false;
  bool textInputEnabled = false;
  bool ragIconVisible = false;
  bool imageIconVisible = false;
  String generatedImagesRevision;
  String status;
  String text;
  String emoji;
  uint32_t lastUpdateMs = 0;
};

struct CompanionPreset {
  String id;
  String label;
  String prompt;
};

struct CompanionSettings {
  bool loaded = false;
  String voiceMode = "text-only";
  String personalityPresetId = "custom";
  bool musicShuffle = false;
  int volumeLevel = 9;
  int scrollSpeedLevel = 5;
  int manualRecordMaxSec = 15;
  int idleTimeoutSec = 120;
  int screenBlankTimeoutSec = 0;
  int roomMonitorIntervalSec = 0;
  String uiTheme = "default";
  String cameraSource = "pi-camera";
  String headerMode = "emoji";
  String screensaverMode = "retro-geometry";
  size_t presetCount = 0;
  CompanionPreset presets[COMPANION_MAX_PRESETS];
};

struct CompanionPhoto {
  String fileName;
  String imageUrl;
  String companionImageUrl;
  uint32_t sizeBytes = 0;
};

struct CompanionPhotoLibrary {
  bool loaded = false;
  size_t count = 0;
  CompanionPhoto photos[COMPANION_MAX_PHOTOS];
  uint32_t lastUpdateMs = 0;
};

static bool apiBuildUrl(char *buffer, size_t bufferSize, const char *path) {
  if (!buffer || bufferSize == 0 || !path || !cc_pi_host[0]) {
    return false;
  }
  snprintf(buffer, bufferSize, "http://%s:%u%s", cc_pi_host, cc_pi_port, path);
  return true;
}

static bool apiBuildUrl(char *buffer, size_t bufferSize, const String &path) {
  return apiBuildUrl(buffer, bufferSize, path.c_str());
}

static bool apiPostJson(const char *path, const char *jsonBody, String *responseOut = nullptr) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), path)) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);
  int code = http.POST(jsonBody ? jsonBody : "{}");
  if (responseOut) {
    *responseOut = http.getString();
  } else {
    http.getString();
  }
  http.end();
  return code >= 200 && code < 300;
}

static bool apiFetchState(CompanionState &state) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), "/api/state")) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getStream())) {
    http.end();
    return false;
  }
  http.end();

  state.ready = doc["ready"] | false;
  state.status = String(doc["status"] | "");
  state.text = String(doc["text"] | "");
  state.emoji = String(doc["emoji"] | "");
  state.textInputEnabled = doc["text_input_enabled"] | false;
  state.ragIconVisible = doc["rag_icon_visible"] | false;
  state.imageIconVisible = doc["image_icon_visible"] | false;
  state.generatedImagesRevision = String(doc["generated_images_revision"] | "");
  state.lastUpdateMs = millis();
  return true;
}

static bool apiPostSettingsDocument(JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  return apiPostJson("/api/settings", body.c_str());
}

static bool apiFetchSettings(CompanionSettings &settings) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), "/api/settings")) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getStream())) {
    http.end();
    return false;
  }
  http.end();
  JsonObject settingsObj = doc["settings"].as<JsonObject>();
  if (settingsObj.isNull()) {
    return false;
  }

  settings.voiceMode = String(settingsObj["voiceMode"] | "text-only");
  settings.personalityPresetId = String(settingsObj["personalityPresetId"] | "custom");
  settings.musicShuffle = settingsObj["musicShuffle"] | false;
  settings.volumeLevel = settingsObj["volumeLevel"] | 9;
  settings.scrollSpeedLevel = settingsObj["scrollSpeedLevel"] | 5;
  settings.manualRecordMaxSec = settingsObj["manualRecordMaxSec"] | 15;
  settings.idleTimeoutSec = settingsObj["idleTimeoutSec"] | 120;
  settings.screenBlankTimeoutSec = settingsObj["screenBlankTimeoutSec"] | 0;
  settings.roomMonitorIntervalSec = settingsObj["roomMonitorIntervalSec"] | 0;
  settings.uiTheme = String(settingsObj["uiTheme"] | "default");
  settings.cameraSource = String(settingsObj["cameraSource"] | "pi-camera");
  settings.headerMode = String(settingsObj["headerMode"] | "emoji");
  settings.screensaverMode = String(settingsObj["screensaverMode"] | "retro-geometry");
  settings.presetCount = 0;

  JsonArray presetsArray = doc["presets"].as<JsonArray>();
  if (!presetsArray.isNull()) {
    for (JsonVariant presetVariant : presetsArray) {
      if (settings.presetCount >= COMPANION_MAX_PRESETS) {
        break;
      }
      JsonObject presetObject = presetVariant.as<JsonObject>();
      if (presetObject.isNull()) {
        continue;
      }
      CompanionPreset &preset = settings.presets[settings.presetCount++];
      preset.id = String(presetObject["id"] | "");
      preset.label = String(presetObject["label"] | "");
      preset.prompt = String(presetObject["prompt"] | "");
    }
  }
  settings.loaded = true;
  return true;
}

static bool apiFetchPhotos(CompanionPhotoLibrary &library, size_t maxPhotos = COMPANION_MAX_PHOTOS) {
  char url[192];
  if (!apiBuildUrl(url, sizeof(url), "/api/photos")) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getStream())) {
    http.end();
    return false;
  }
  http.end();

  JsonArray photosArray = doc["photos"].as<JsonArray>();
  if (photosArray.isNull()) {
    return false;
  }

  library.count = 0;
  for (JsonVariant photoVariant : photosArray) {
    if (library.count >= maxPhotos || library.count >= COMPANION_MAX_PHOTOS) {
      break;
    }
    JsonObject photoObject = photoVariant.as<JsonObject>();
    if (photoObject.isNull()) {
      continue;
    }
    CompanionPhoto &photo = library.photos[library.count++];
    photo.fileName = String(photoObject["fileName"] | "");
    photo.imageUrl = String(photoObject["imageUrl"] | "");
    photo.companionImageUrl = String(photoObject["companionImageUrl"] | "");
    photo.sizeBytes = photoObject["sizeBytes"] | 0;
  }

  library.loaded = true;
  library.lastUpdateMs = millis();
  return true;
}

static bool apiFetchGeneratedImagesPage(
  CompanionPhotoLibrary &library,
  size_t offset,
  size_t limit,
  bool *hasMoreOut = nullptr,
  size_t *totalCountOut = nullptr
) {
  library.count = 0;
  String requestPath = String("/api/generated-images?offset=") + String(offset) +
    "&limit=" + String(limit ? limit : COMPANION_GENERATED_IMAGES_PAGE_SIZE);
  char url[224];
  if (!apiBuildUrl(url, sizeof(url), requestPath)) {
    return false;
  }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getStream())) {
    http.end();
    return false;
  }
  http.end();

  JsonArray photosArray = doc["photos"].as<JsonArray>();
  if (photosArray.isNull()) {
    return false;
  }

  for (JsonVariant photoVariant : photosArray) {
    if (library.count >= COMPANION_MAX_PHOTOS) {
      break;
    }
    JsonObject photoObject = photoVariant.as<JsonObject>();
    if (photoObject.isNull()) {
      continue;
    }
    CompanionPhoto &photo = library.photos[library.count++];
    photo.fileName = String(photoObject["fileName"] | "");
    photo.imageUrl = String(photoObject["imageUrl"] | "");
    photo.companionImageUrl = String(photoObject["companionImageUrl"] | "");
    photo.sizeBytes = photoObject["sizeBytes"] | 0;
  }

  if (hasMoreOut) {
    *hasMoreOut = doc["hasMore"] | false;
  }
  if (totalCountOut) {
    *totalCountOut = doc["totalCount"] | library.count;
  }

  library.loaded = true;
  library.lastUpdateMs = millis();
  return true;
}

static bool apiFetchGeneratedImages(CompanionPhotoLibrary &library, size_t maxPhotos = COMPANION_MAX_PHOTOS) {
  library.count = 0;
  size_t offset = 0;
  const size_t clampedMax = maxPhotos > COMPANION_MAX_PHOTOS ? COMPANION_MAX_PHOTOS : maxPhotos;
  static CompanionPhotoLibrary page;

  while (library.count < clampedMax) {
    page.loaded = false;
    page.count = 0;
    page.lastUpdateMs = 0;
    bool hasMore = false;
    if (!apiFetchGeneratedImagesPage(page, offset, COMPANION_GENERATED_IMAGES_PAGE_SIZE, &hasMore, nullptr)) {
      return false;
    }
    if (page.count == 0) {
      break;
    }
    for (size_t i = 0; i < page.count && library.count < clampedMax; i++) {
      library.photos[library.count++] = page.photos[i];
    }
    if (!hasMore) {
      break;
    }
    offset += page.count;
  }

  library.loaded = true;
  library.lastUpdateMs = millis();
  return true;
}

static bool apiDownloadFile(
  const String &requestPath,
  fs::FS &fileSystem,
  const char *localPath,
  String *errorOut = nullptr
) {
  char url[256];
  if (!apiBuildUrl(url, sizeof(url), requestPath)) {
    if (errorOut) {
      *errorOut = "url build";
    }
    return false;
  }

  HTTPClient http;
  http.begin(url);
  http.setTimeout(15000);
  int code = http.GET();
  if (code != 200) {
    if (errorOut) {
      *errorOut = "http " + String(code);
    }
    http.end();
    return false;
  }

  fileSystem.remove(localPath);
  File file = fileSystem.open(localPath, "w");
  if (!file) {
    if (errorOut) {
      *errorOut = "open write";
    }
    http.end();
    return false;
  }

  int expectedSize = http.getSize();
  int written = http.writeToStream(&file);
  file.close();
  http.end();
  size_t finalSize = 0;
  File verifyFile = fileSystem.open(localPath, "r");
  if (verifyFile) {
    finalSize = verifyFile.size();
    verifyFile.close();
  }
  bool ok = written >= 0 && finalSize > 0;
  if (expectedSize >= 0) {
    ok = ok && written == expectedSize && static_cast<int>(finalSize) == expectedSize;
  }
  if (!ok) {
    if (errorOut) {
      *errorOut = "write " + String(written) + "/" + String(expectedSize) + "/" + String(finalSize);
    }
    fileSystem.remove(localPath);
  } else if (errorOut) {
    *errorOut = "";
  }
  return ok;
}

static const char *apiNextVoiceMode(const String &currentMode) {
  if (currentMode == "text-only") {
    return "speak-on-demand";
  }
  if (currentMode == "speak-on-demand") {
    return "voice-chat";
  }
  return "text-only";
}

static bool apiSetVoiceMode(const char *voiceMode) {
  JsonDocument doc;
  doc["voiceMode"] = voiceMode;
  return apiPostSettingsDocument(doc);
}

static bool apiCycleVoiceMode(CompanionSettings &settings) {
  const char *nextMode = apiNextVoiceMode(settings.voiceMode);
  if (!apiSetVoiceMode(nextMode)) {
    return false;
  }
  settings.voiceMode = String(nextMode);
  settings.loaded = true;
  return true;
}

static bool apiSetStringSetting(const char *key, const String &value) {
  JsonDocument doc;
  doc[key] = value;
  return apiPostSettingsDocument(doc);
}

static bool apiSetIntSetting(const char *key, int value) {
  JsonDocument doc;
  doc[key] = value;
  return apiPostSettingsDocument(doc);
}

static bool apiSetBoolSetting(const char *key, bool value) {
  JsonDocument doc;
  doc[key] = value;
  return apiPostSettingsDocument(doc);
}

static bool apiSetPersonalityPreset(const CompanionPreset &preset) {
  if (!preset.prompt.length()) {
    return false;
  }
  JsonDocument doc;
  doc["personalityPrompt"] = preset.prompt;
  return apiPostSettingsDocument(doc);
}

static bool apiResetChat() {
  return apiPostJson("/api/chat/reset", "{}");
}

static bool apiRepeatLastAnswer() {
  return apiPostJson("/api/companion/action", "{\"action\":\"repeat\"}");
}

static bool apiCaptureVision() {
  return apiPostJson("/api/vision/capture", "{}");
}

static bool apiSendText(const char *text) {
  JsonDocument doc;
  doc["text"] = text;
  String body;
  serializeJson(doc, body);
  return apiPostJson("/api/input/text", body.c_str());
}
