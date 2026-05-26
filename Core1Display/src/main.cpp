#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <M5Stack.h>
#include <math.h>
#include <Preferences.h>
#include <SD.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>

static const char CORE1_AP_SSID[] = "Core1Display-Setup";
static const char CORE1_DEFAULT_HOST[] = "http://10.160.0.203";
static const char CORE1_DEFAULT_WHISPLAY_URL[] = "http://10.160.0.136:17880";
static const uint16_t CORE1_DEFAULT_POLL_MS = 1500;
static const uint16_t CORE1_MIN_POLL_MS = 500;
static const uint16_t CORE1_MAX_POLL_MS = 5000;
static const unsigned long CORE1_WIFI_RETRY_MS = 10000;
static const unsigned long CORE1_LONG_PRESS_MS = 1200;
static const uint16_t CORE1_DEFAULT_AUTO_SCROLL_MS = 0;
static const char CORE1_WHISPLAY_IMAGE_CACHE_PATH[] = "/whisplay-latest.jpg";
static const uint16_t CORE1_WHISPLAY_IMAGE_CACHE_WIDTH = 320;
static const uint16_t CORE1_WHISPLAY_IMAGE_CACHE_HEIGHT = 240;
static const uint16_t CORE1_AI_IMAGE_WIDTH = 320;
static const uint16_t CORE1_AI_IMAGE_HEIGHT = 240;
static const unsigned long CORE1_AI_POLL_MS = 45000;
static const unsigned long CORE1_AI_SLIDE_MS = 30000;
static const char CORE1_AI_SD_DIR[] = "/core1-ai";
static const char CORE1_AI_SPIFFS_PREFIX[] = "/ai-show-";
static const char CORE1_AI_CACHE_VERSION[] = "v1";
static const size_t CORE1_AI_MAX_IMAGES = 96;
static const size_t CORE1_AI_SPIFFS_MAX_IMAGES = 64;
static const uint8_t CORE1_SETTINGS_VISIBLE_ROWS = 6;

enum class DisplayMode : uint8_t {
  Split = 0,
  FullReply = 1,
};

enum class ScreenMode : uint8_t {
  Viewer,
  Settings,
  Screensaver,
};

enum class BackendMode : uint8_t {
  Auto = 0,
  Groqputer = 1,
  Whisplay = 2,
};

enum class BackendSource : uint8_t {
  None = 0,
  Groqputer = 1,
  Whisplay = 2,
};

enum class SettingsField : uint8_t {
  BorderColor,
  FontColor,
  AutoScroll,
  FontSize,
  FontFamily,
  ScreensaverMode,
  ScreensaverOn,
  ScreenOff,
  DisplayMode,
};

enum class ScreensaverMode : uint8_t {
  Off = 0,
  Matrix = 1,
  Ripple = 2,
  Entropy = 3,
  AiSlideshow = 4,
};

struct ColorTheme {
  const char *label;
  uint16_t accent;
  uint16_t title;
  uint16_t body;
};

static constexpr ColorTheme CORE1_COLOR_THEMES[] = {
  {"Green", TFT_GREEN, TFT_CYAN, TFT_WHITE},
  {"Amber", TFT_YELLOW, TFT_ORANGE, TFT_WHITE},
  {"Ice", TFT_CYAN, TFT_BLUE, TFT_WHITE},
  {"Rose", TFT_MAGENTA, TFT_PINK, TFT_WHITE},
};

struct FontColorOption {
  const char *label;
  uint16_t color;
};

struct BodyFontOption {
  const char *label;
  const GFXfont *font;
  uint8_t charWidth;
  uint8_t lineHeight;
  uint8_t baseline;
};

static constexpr FontColorOption CORE1_FONT_COLORS[] = {
  {"White", TFT_WHITE},
  {"Green", TFT_GREEN},
  {"Amber", TFT_YELLOW},
  {"Cyan", TFT_CYAN},
  {"Pink", TFT_MAGENTA},
};

static const BodyFontOption CORE1_BODY_FONTS[] = {
  {"Built-in", nullptr, 6, 10, 0},
  {"Sans", &FreeSans9pt7b, 8, 18, 13},
  {"SansBold", &FreeSansBold9pt7b, 9, 18, 13},
  {"Mono", &FreeMono9pt7b, 10, 17, 13},
  {"MonoBold", &FreeMonoBold9pt7b, 10, 17, 13},
};

static constexpr uint16_t CORE1_AUTO_SCROLL_OPTIONS[] = {0, 400, 700, 1000, 1500, 2200};
static constexpr uint32_t CORE1_SAVER_IDLE_OPTIONS[] = {0, 15000, 30000, 60000, 120000};
static constexpr uint32_t CORE1_SCREEN_OFF_OPTIONS[] = {0, 30000, 60000, 120000, 300000};
static constexpr char CORE1_MATRIX_CHARS[] = "01ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char core1_wifi_ssid[64] = "";
static char core1_wifi_pass[64] = "";
static char core1_groqputer_url[128] = "";
static char core1_whisplay_url[128] = "";
static uint16_t core1_poll_ms = CORE1_DEFAULT_POLL_MS;
static uint16_t core1_auto_scroll_ms = CORE1_DEFAULT_AUTO_SCROLL_MS;
static uint8_t core1_font_scale = 1;
static uint8_t core1_color_theme_index = 0;
static uint8_t core1_font_color_index = 0;
static uint8_t core1_font_family_index = 0;
static ScreensaverMode core1_screensaver_mode = ScreensaverMode::Off;
static uint32_t core1_screensaver_idle_ms = 0;
static uint32_t core1_screen_off_ms = 0;
static DisplayMode core1_display_mode = DisplayMode::Split;
static BackendMode core1_backend_mode = BackendMode::Auto;
static bool core1_has_settings = false;
static unsigned long core1_last_wifi_retry_ms = 0;

static DNSServer *core1_portal_dns = nullptr;
static WebServer *core1_portal_server = nullptr;
static String core1_portal_notice = "";

enum class FocusPane : uint8_t {
  Prompt,
  Reply,
};

static ScreenMode activeScreen = ScreenMode::Viewer;
static SettingsField activeSettingsField = SettingsField::BorderColor;
static FocusPane activePane = FocusPane::Reply;
static String latestUserMessage = "";
static String latestReplyMessage = "";
static String latestModelTag = "BOT";
static String latestPersona = "Unknown";
static String latestStatus = "idle";
static String latestPollError = "";
static unsigned long latestUpdatedAtMs = 0;
static unsigned long lastPollAttemptMs = 0;
static unsigned long lastSuccessfulPollMs = 0;
static int promptScrollOffset = 0;
static int replyScrollOffset = 0;
static bool uiDirty = true;
static bool footerDirty = true;
static wl_status_t lastDrawnWifiStatus = WL_IDLE_STATUS;
static long lastDrawnSyncAgeSeconds = -1;
static unsigned long lastAutoScrollMs = 0;
static unsigned long autoScrollPauseStartedMs = 0;
static BackendSource activeBackendSource = BackendSource::None;
static unsigned long lastInteractionMs = 0;
static unsigned long screensaverStartedMs = 0;
static unsigned long lastScreensaverFrameMs = 0;
static bool screenBlanked = false;
static bool core1_image_cache_ready = false;
static bool core1_sd_ready = false;

struct MatrixColumn {
  float y;
  float speed;
  uint8_t headChar;
};

struct EntropyParticle {
  float x;
  float y;
  float vx;
  float vy;
  uint16_t color;
};

static constexpr int CORE1_MATRIX_COLUMN_COUNT = 18;
static constexpr int CORE1_ENTROPY_PARTICLE_COUNT = 28;
static MatrixColumn matrixColumns[CORE1_MATRIX_COLUMN_COUNT];
static EntropyParticle entropyParticles[CORE1_ENTROPY_PARTICLE_COUNT];

struct Core1AiImage {
  String fileName = "";
  String remoteUrl = "";
  String cachePath = "";
  bool useSd = false;
  uint32_t updatedAt = 0;
  bool cached = false;
};

static Core1AiImage core1_ai_images[CORE1_AI_MAX_IMAGES];
static size_t core1_ai_image_count = 0;
static size_t core1_ai_slide_index = 0;
static size_t core1_ai_prefetch_index = 0;
static unsigned long core1_last_ai_poll_ms = 0;
static unsigned long core1_last_ai_slide_ms = 0;
static unsigned long core1_last_ai_prefetch_ms = 0;
static bool core1_ai_dirty = true;
static String core1_ai_error = "";

struct CompanionSnapshot {
  bool configured = false;
  bool ready = false;
  bool reachable = false;
  String host = "";
  String userMessage = "";
  String replyMessage = "";
  String modelTag = "BOT";
  String persona = "Unknown";
  String status = "idle";
  String error = "";
  String imagePath = "";
  String imageCachePath = "";
  uint32_t imageRevision = 0;
  bool imageAvailable = false;
  unsigned long changedAtMs = 0;
  unsigned long lastSuccessMs = 0;
};

static CompanionSnapshot groqputerSnapshot;
static CompanionSnapshot whisplaySnapshot;

static bool ensureWifiConnected(bool forceRetry);

static fs::FS &core1StorageFs(bool useSd) {
  return useSd ? static_cast<fs::FS &>(SD) : static_cast<fs::FS &>(SPIFFS);
}

static String normalizeBaseUrl(const String &value) {
  String normalized = value;
  normalized.trim();
  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }
  return normalized;
}

static bool ensureCore1SdReady() {
  if (core1_sd_ready) {
    return true;
  }
  core1_sd_ready = SD.begin(TFCARD_CS_PIN, SPI, 40000000);
  if (core1_sd_ready) {
    SD.mkdir(CORE1_AI_SD_DIR);
  }
  return core1_sd_ready;
}

static String core1AiCacheKey(const String &fileName) {
  String safe = fileName;
  int dot = safe.lastIndexOf('.');
  if (dot > 0) {
    safe = safe.substring(0, dot);
  }
  safe.replace("/", "-");
  safe.replace("\\", "-");
  safe.replace(" ", "-");
  for (size_t i = 0; i < safe.length(); i++) {
    char c = safe.charAt(i);
    bool ok =
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '-' || c == '_';
    if (!ok) {
      safe.setCharAt(i, '-');
    }
  }
  while (safe.indexOf("--") >= 0) {
    safe.replace("--", "-");
  }
  if (!safe.length()) {
    safe = "ai-image";
  }
  safe += "-";
  safe += CORE1_AI_CACHE_VERSION;
  return safe;
}

static String core1AiCachePath(const String &fileName, bool useSd) {
  String key = core1AiCacheKey(fileName);
  if (useSd) {
    return String(CORE1_AI_SD_DIR) + "/" + key + ".jpg";
  }
  return String(CORE1_AI_SPIFFS_PREFIX) + key + ".jpg";
}

static String core1LeafName(const String &path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

static size_t core1AiImageLimit() {
  return ensureCore1SdReady() ? CORE1_AI_MAX_IMAGES : CORE1_AI_SPIFFS_MAX_IMAGES;
}

static bool core1AiKnownLeaf(const String &leaf, bool useSd) {
  for (size_t i = 0; i < core1_ai_image_count; i++) {
    if (core1LeafName(core1AiCachePath(core1_ai_images[i].fileName, useSd)) == leaf) {
      return true;
    }
  }
  return false;
}

static void clearAiScreensaverCache() {
  if (core1_image_cache_ready) {
    File dir = SPIFFS.open("/");
    if (dir && dir.isDirectory()) {
      while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
          break;
        }
        String name = core1LeafName(String(entry.name()));
        entry.close();
        if (name.startsWith(core1LeafName(CORE1_AI_SPIFFS_PREFIX)) && !core1AiKnownLeaf(name, false)) {
          SPIFFS.remove(String("/") + name);
        }
      }
      dir.close();
    }
  }
  if (ensureCore1SdReady()) {
    File dir = SD.open(CORE1_AI_SD_DIR);
    if (dir && dir.isDirectory()) {
      while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
          break;
        }
        String name = core1LeafName(String(entry.name()));
        entry.close();
        if (!core1AiKnownLeaf(name, true)) {
          SD.remove(String(CORE1_AI_SD_DIR) + "/" + name);
        }
      }
      dir.close();
    }
  }
}

static bool downloadUrlToFile(const String &url, fs::FS &fileSystem, const char *cachePath) {
  if (!ensureWifiConnected(false)) {
    return false;
  }
  HTTPClient http;
  if (!http.begin(url)) {
    return false;
  }
  http.setTimeout(15000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  fileSystem.remove(cachePath);
  File file = fileSystem.open(cachePath, FILE_WRITE);
  if (!file) {
    http.end();
    return false;
  }

  int expectedSize = http.getSize();
  int written = http.writeToStream(&file);
  file.close();
  http.end();

  File verifyFile = fileSystem.open(cachePath, FILE_READ);
  size_t finalSize = 0;
  if (verifyFile) {
    finalSize = verifyFile.size();
    verifyFile.close();
  }
  bool ok = written > 0 && finalSize > 0;
  if (expectedSize >= 0) {
    ok = ok && written == expectedSize && static_cast<int>(finalSize) == expectedSize;
  }
  if (!ok) {
    fileSystem.remove(cachePath);
  }
  return ok;
}

static bool fileExistsOnStorage(const String &path, bool useSd) {
  if (!path.length()) {
    return false;
  }
  return core1StorageFs(useSd).exists(path.c_str());
}

static void clearWhisplayImageCache() {
  if (!core1_image_cache_ready) {
    return;
  }
  if (SPIFFS.exists(CORE1_WHISPLAY_IMAGE_CACHE_PATH)) {
    SPIFFS.remove(CORE1_WHISPLAY_IMAGE_CACHE_PATH);
  }
}

static void resetSnapshotImageState(CompanionSnapshot &snapshot) {
  clearWhisplayImageCache();
  snapshot.imagePath = "";
  snapshot.imageCachePath = "";
  snapshot.imageRevision = 0;
  snapshot.imageAvailable = false;
}

static bool downloadUrlToSPIFFS(const String &url, const char *cachePath) {
  if (!core1_image_cache_ready || !ensureWifiConnected(false)) {
    Serial.printf("[Core1] SPIFFS download skipped path=%s wifi=%d cache=%d\n",
      cachePath,
      WiFi.status() == WL_CONNECTED,
      core1_image_cache_ready);
    return false;
  }
  HTTPClient http;
  if (!http.begin(url)) {
    Serial.printf("[Core1] SPIFFS begin failed path=%s url=%s\n", cachePath, url.c_str());
    return false;
  }
  http.setTimeout(15000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[Core1] SPIFFS GET failed path=%s code=%d url=%s\n", cachePath, code, url.c_str());
    http.end();
    return false;
  }

  if (SPIFFS.exists(cachePath)) {
    SPIFFS.remove(cachePath);
  }
  File file = SPIFFS.open(cachePath, FILE_WRITE);
  if (!file) {
    Serial.printf("[Core1] SPIFFS open failed path=%s\n", cachePath);
    http.end();
    return false;
  }

  int written = http.writeToStream(&file);
  int expectedSize = http.getSize();
  file.close();
  http.end();

  if (written <= 0) {
    SPIFFS.remove(cachePath);
    Serial.printf("[Core1] SPIFFS write failed path=%s written=%d expected=%d\n",
      cachePath, written, expectedSize);
    return false;
  }
  if (expectedSize >= 0 && written != expectedSize) {
    SPIFFS.remove(cachePath);
    Serial.printf("[Core1] SPIFFS size mismatch path=%s written=%d expected=%d\n",
      cachePath, written, expectedSize);
    return false;
  }
  Serial.printf("[Core1] SPIFFS cached path=%s bytes=%d url=%s\n", cachePath, written, url.c_str());
  return true;
}

static void refreshWhisplayImageCache(CompanionSnapshot &snapshot) {
  if (!snapshot.imagePath.length() || !core1_image_cache_ready) {
    snapshot.imageCachePath = "";
    snapshot.imageAvailable = false;
    return;
  }

  String imageUrl = normalizeBaseUrl(String(core1_whisplay_url)) +
    "/api/vision/image/companion?width=" + String(CORE1_WHISPLAY_IMAGE_CACHE_WIDTH) +
    "&height=" + String(CORE1_WHISPLAY_IMAGE_CACHE_HEIGHT) +
    "&rev=" + String(snapshot.imageRevision);
  Serial.printf("[Core1] Refresh whisplay image rev=%lu path=%s\n",
    static_cast<unsigned long>(snapshot.imageRevision),
    snapshot.imagePath.c_str());
  if (downloadUrlToSPIFFS(imageUrl, CORE1_WHISPLAY_IMAGE_CACHE_PATH)) {
    snapshot.imageCachePath = CORE1_WHISPLAY_IMAGE_CACHE_PATH;
    snapshot.imageAvailable = true;
    Serial.printf("[Core1] Whisplay image ready cache=%s\n", snapshot.imageCachePath.c_str());
    return;
  }

  snapshot.imageCachePath = "";
  snapshot.imageAvailable = false;
  Serial.println("[Core1] Whisplay image refresh failed");
}

static Core1AiImage *currentAiImage() {
  if (core1_ai_image_count == 0) {
    return nullptr;
  }
  if (core1_ai_slide_index >= core1_ai_image_count) {
    core1_ai_slide_index = 0;
  }
  return &core1_ai_images[core1_ai_slide_index];
}

static bool ensureAiImageCached(Core1AiImage &image) {
  if (!core1_image_cache_ready || !image.remoteUrl.length()) {
    return false;
  }

  if (image.cachePath.length() && fileExistsOnStorage(image.cachePath, image.useSd)) {
    image.cached = true;
    return true;
  }

  if (ensureCore1SdReady()) {
    String sdPath = core1AiCachePath(image.fileName, true);
    if (fileExistsOnStorage(sdPath, true)) {
      image.cachePath = sdPath;
      image.useSd = true;
      image.cached = true;
      return true;
    }
  }

  String spiffsPath = core1AiCachePath(image.fileName, false);
  if (fileExistsOnStorage(spiffsPath, false)) {
    image.cachePath = spiffsPath;
    image.useSd = false;
    image.cached = true;
    return true;
  }

  if (downloadUrlToFile(image.remoteUrl, SPIFFS, spiffsPath.c_str())) {
    image.cachePath = spiffsPath;
    image.useSd = false;
    image.cached = true;
    if (ensureCore1SdReady()) {
      String sdPath = core1AiCachePath(image.fileName, true);
      downloadUrlToFile(image.remoteUrl, SD, sdPath.c_str());
    }
    return true;
  }

  if (ensureCore1SdReady()) {
    String sdPath = core1AiCachePath(image.fileName, true);
    if (downloadUrlToFile(image.remoteUrl, SD, sdPath.c_str())) {
      image.cachePath = sdPath;
      image.useSd = true;
      image.cached = true;
      return true;
    }
  }

  image.cachePath = "";
  image.cached = false;
  return false;
}

static bool ensureCurrentAiImageCached() {
  Core1AiImage *image = currentAiImage();
  if (!image) {
    return false;
  }
  return ensureAiImageCached(*image);
}

static void prefetchAiImages() {
  if (!core1_ai_image_count || millis() - core1_last_ai_prefetch_ms < 4000UL) {
    return;
  }
  core1_last_ai_prefetch_ms = millis();
  size_t attempts = min(core1_ai_image_count, static_cast<size_t>(4));
  for (size_t offset = 0; offset < attempts; offset++) {
    size_t index = (core1_ai_prefetch_index + offset) % core1_ai_image_count;
    Core1AiImage &image = core1_ai_images[index];
    if (!image.cached || !fileExistsOnStorage(image.cachePath, image.useSd)) {
      ensureAiImageCached(image);
      core1_ai_prefetch_index = (index + 1) % core1_ai_image_count;
      return;
    }
  }
  core1_ai_prefetch_index = (core1_ai_prefetch_index + attempts) % core1_ai_image_count;
}

static bool fetchCore1AiImages(bool force = false) {
  if (!whisplaySnapshot.configured || !core1_image_cache_ready) {
    return false;
  }
  unsigned long now = millis();
  if (!force &&
      core1_last_ai_poll_ms != 0 &&
      now - core1_last_ai_poll_ms < CORE1_AI_POLL_MS) {
    return false;
  }
  core1_last_ai_poll_ms = now;

  if (!ensureWifiConnected(false)) {
    core1_ai_error = "WiFi disconnected";
    return false;
  }

  HTTPClient http;
  String endpoint = normalizeBaseUrl(String(core1_whisplay_url)) + "/api/generated-images";
  Serial.printf("[Core1] Fetch AI images force=%d endpoint=%s\n", force, endpoint.c_str());
  if (!http.begin(endpoint)) {
    core1_ai_error = "HTTP begin failed";
    Serial.println("[Core1] AI image list begin failed");
    return false;
  }
  http.setTimeout(12000);
  int code = http.GET();
  String response = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    core1_ai_error = response.length() ? response : String("HTTP ") + code;
    Serial.printf("[Core1] AI image list failed code=%d body=%s\n", code, response.c_str());
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    core1_ai_error = "JSON parse failed";
    Serial.println("[Core1] AI image list JSON parse failed");
    return false;
  }
  JsonArray photos = doc["photos"].as<JsonArray>();
  if (photos.isNull()) {
    core1_ai_error = "No AI photos";
    Serial.println("[Core1] AI image list missing photos array");
    return false;
  }

  size_t limit = min(core1AiImageLimit(), static_cast<size_t>(CORE1_AI_MAX_IMAGES));
  size_t nextCount = 0;
  for (JsonVariant photoVariant : photos) {
    if (nextCount >= limit) {
      break;
    }
    JsonObject photo = photoVariant.as<JsonObject>();
    if (photo.isNull()) {
      continue;
    }
    String fileName = String(photo["fileName"] | "");
    String companionPath = String(photo["companionImageUrl"] | photo["imageUrl"] | "");
    if (!fileName.length() || !companionPath.length()) {
      continue;
    }
    String remoteUrl = companionPath;
    if (remoteUrl.startsWith("/")) {
      remoteUrl = normalizeBaseUrl(String(core1_whisplay_url)) + remoteUrl;
    }
    if (remoteUrl.indexOf('?') >= 0) {
      remoteUrl += "&width=" + String(CORE1_AI_IMAGE_WIDTH) +
        "&height=" + String(CORE1_AI_IMAGE_HEIGHT) +
        "&v=" + String(CORE1_AI_CACHE_VERSION);
    } else {
      remoteUrl += "?width=" + String(CORE1_AI_IMAGE_WIDTH) +
        "&height=" + String(CORE1_AI_IMAGE_HEIGHT) +
        "&v=" + String(CORE1_AI_CACHE_VERSION);
    }
    Core1AiImage &entry = core1_ai_images[nextCount++];
    entry.fileName = fileName;
    entry.remoteUrl = remoteUrl;
    entry.updatedAt = photo["updatedAt"] | 0;
    entry.cachePath = "";
    entry.useSd = false;
    entry.cached = false;
  }

  bool changed = true;
  core1_ai_image_count = nextCount;
  Serial.printf("[Core1] AI image list count=%u limit=%u\n",
    static_cast<unsigned>(core1_ai_image_count),
    static_cast<unsigned>(limit));
  if (core1_ai_slide_index >= core1_ai_image_count) {
    core1_ai_slide_index = 0;
  }
  core1_ai_prefetch_index = 0;
  clearAiScreensaverCache();
  if (ensureCurrentAiImageCached()) {
    core1_ai_error = "";
  } else if (core1_ai_image_count == 0) {
    core1_ai_error = "No AI images";
  } else if (!core1_ai_error.length()) {
    core1_ai_error = "AI cache failed";
  }
  if (changed || force) {
    core1_ai_dirty = true;
  }
  return nextCount > 0;
}

static bool activeWhisplayImageVisible() {
  return activeBackendSource == BackendSource::Whisplay &&
    whisplaySnapshot.imageAvailable &&
    whisplaySnapshot.imageCachePath.length() > 0;
}

static bool isActiveChatStatus(const String &status) {
  return status.length() &&
    !status.equalsIgnoreCase("idle") &&
    !status.equalsIgnoreCase("last reply");
}

static uint16_t clampPollMs(int value) {
  if (value < CORE1_MIN_POLL_MS) return CORE1_MIN_POLL_MS;
  if (value > CORE1_MAX_POLL_MS) return CORE1_MAX_POLL_MS;
  return static_cast<uint16_t>(value);
}

static uint16_t clampAutoScrollMs(int value) {
  if (value < 0) return CORE1_DEFAULT_AUTO_SCROLL_MS;
  return static_cast<uint16_t>(value);
}

static uint8_t clampFontScale(int value) {
  if (value < 1) return 1;
  if (value > 3) return 3;
  return static_cast<uint8_t>(value);
}

static uint8_t clampThemeIndex(int value) {
  if (value < 0) return 0;
  if (value >= static_cast<int>(sizeof(CORE1_COLOR_THEMES) / sizeof(CORE1_COLOR_THEMES[0]))) {
    return static_cast<uint8_t>((sizeof(CORE1_COLOR_THEMES) / sizeof(CORE1_COLOR_THEMES[0])) - 1);
  }
  return static_cast<uint8_t>(value);
}

static uint8_t clampFontColorIndex(int value) {
  if (value < 0) return 0;
  if (value >= static_cast<int>(sizeof(CORE1_FONT_COLORS) / sizeof(CORE1_FONT_COLORS[0]))) {
    return static_cast<uint8_t>((sizeof(CORE1_FONT_COLORS) / sizeof(CORE1_FONT_COLORS[0])) - 1);
  }
  return static_cast<uint8_t>(value);
}

static uint8_t clampFontFamilyIndex(int value) {
  if (value < 0) return 0;
  if (value >= static_cast<int>(sizeof(CORE1_BODY_FONTS) / sizeof(CORE1_BODY_FONTS[0]))) {
    return static_cast<uint8_t>((sizeof(CORE1_BODY_FONTS) / sizeof(CORE1_BODY_FONTS[0])) - 1);
  }
  return static_cast<uint8_t>(value);
}

static BackendMode clampBackendMode(int value) {
  if (value < static_cast<int>(BackendMode::Auto)) {
    return BackendMode::Auto;
  }
  if (value > static_cast<int>(BackendMode::Whisplay)) {
    return BackendMode::Whisplay;
  }
  return static_cast<BackendMode>(value);
}

static ScreensaverMode clampScreensaverMode(int value) {
  if (value < static_cast<int>(ScreensaverMode::Off)) {
    return ScreensaverMode::Off;
  }
  if (value > static_cast<int>(ScreensaverMode::AiSlideshow)) {
    return ScreensaverMode::AiSlideshow;
  }
  return static_cast<ScreensaverMode>(value);
}

static uint32_t clampOptionValue(uint32_t value, const uint32_t *options, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (options[i] == value) {
      return value;
    }
  }
  return options[0];
}

static ColorTheme currentTheme() {
  return CORE1_COLOR_THEMES[clampThemeIndex(core1_color_theme_index)];
}

static uint16_t currentFontColor() {
  return CORE1_FONT_COLORS[clampFontColorIndex(core1_font_color_index)].color;
}

static BodyFontOption currentBodyFont() {
  return CORE1_BODY_FONTS[clampFontFamilyIndex(core1_font_family_index)];
}

static int currentBodyCharWidth() {
  return currentBodyFont().charWidth * core1_font_scale;
}

static int currentBodyLineHeight() {
  return currentBodyFont().lineHeight * core1_font_scale;
}

static int currentBodyBaseline() {
  return currentBodyFont().baseline * core1_font_scale;
}

static void applyBodyFont() {
  BodyFontOption font = currentBodyFont();
  if (font.font) {
    M5.Lcd.setFont(font.font);
  } else {
    M5.Lcd.setFont();
  }
  M5.Lcd.setTextSize(core1_font_scale);
}

static void restoreBuiltinBodyFont() {
  M5.Lcd.setFont();
  M5.Lcd.setTextFont(1);
  M5.Lcd.setTextSize(1);
}

static const char *screensaverModeLabel(ScreensaverMode mode);

static String escapeHtml(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("\"", "&quot;");
  escaped.replace("'", "&#39;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

static void loadSettings() {
  Preferences prefs;
  prefs.begin("core1disp", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String groqputerUrl = prefs.getString("groqputerUrl", CORE1_DEFAULT_HOST);
  String whisplayUrl = prefs.getString("whisplayUrl", "");
  core1_poll_ms = clampPollMs(static_cast<int>(prefs.getUInt("pollMs", CORE1_DEFAULT_POLL_MS)));
  core1_auto_scroll_ms = clampAutoScrollMs(static_cast<int>(prefs.getUInt("autoScrollMs", CORE1_DEFAULT_AUTO_SCROLL_MS)));
  core1_font_scale = clampFontScale(static_cast<int>(prefs.getUChar("fontScale", 1)));
  core1_color_theme_index = clampThemeIndex(static_cast<int>(prefs.getUChar("themeIdx", 0)));
  core1_font_color_index = clampFontColorIndex(static_cast<int>(prefs.getUChar("fontColorIdx", 0)));
  core1_font_family_index = clampFontFamilyIndex(static_cast<int>(prefs.getUChar("fontFamilyIdx", 0)));
  core1_screensaver_mode = clampScreensaverMode(static_cast<int>(prefs.getUChar("saverMode", 0)));
  core1_screensaver_idle_ms = clampOptionValue(
    prefs.getUInt("saverIdleMs", 0),
    CORE1_SAVER_IDLE_OPTIONS,
    sizeof(CORE1_SAVER_IDLE_OPTIONS) / sizeof(CORE1_SAVER_IDLE_OPTIONS[0])
  );
  core1_screen_off_ms = clampOptionValue(
    prefs.getUInt("screenOffMs", 0),
    CORE1_SCREEN_OFF_OPTIONS,
    sizeof(CORE1_SCREEN_OFF_OPTIONS) / sizeof(CORE1_SCREEN_OFF_OPTIONS[0])
  );
  core1_display_mode = prefs.getBool("fullReply", false) ? DisplayMode::FullReply : DisplayMode::Split;
  core1_backend_mode = clampBackendMode(static_cast<int>(prefs.getUChar("backendMode", 0)));
  prefs.end();

  ssid.toCharArray(core1_wifi_ssid, sizeof(core1_wifi_ssid));
  pass.toCharArray(core1_wifi_pass, sizeof(core1_wifi_pass));
  normalizeBaseUrl(groqputerUrl).toCharArray(core1_groqputer_url, sizeof(core1_groqputer_url));
  normalizeBaseUrl(whisplayUrl).toCharArray(core1_whisplay_url, sizeof(core1_whisplay_url));
  groqputerSnapshot.configured = core1_groqputer_url[0] != '\0';
  groqputerSnapshot.host = String(core1_groqputer_url);
  whisplaySnapshot.configured = core1_whisplay_url[0] != '\0';
  whisplaySnapshot.host = String(core1_whisplay_url);
  core1_has_settings = core1_wifi_ssid[0] != '\0' &&
    (groqputerSnapshot.configured || whisplaySnapshot.configured);
}

static void saveSettings(
  const String &ssid,
  const String &pass,
  const String &groqputerUrl,
  const String &whisplayUrl,
  uint16_t pollMs
) {
  String normalizedUrl = normalizeBaseUrl(groqputerUrl);
  String normalizedWhisplayUrl = normalizeBaseUrl(whisplayUrl);
  Preferences prefs;
  prefs.begin("core1disp", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("groqputerUrl", normalizedUrl);
  prefs.putString("whisplayUrl", normalizedWhisplayUrl);
  prefs.putUInt("pollMs", clampPollMs(pollMs));
  prefs.putUInt("autoScrollMs", core1_auto_scroll_ms);
  prefs.putUChar("fontScale", core1_font_scale);
  prefs.putUChar("themeIdx", core1_color_theme_index);
  prefs.putUChar("fontColorIdx", core1_font_color_index);
  prefs.putUChar("fontFamilyIdx", core1_font_family_index);
  prefs.putUChar("saverMode", static_cast<uint8_t>(core1_screensaver_mode));
  prefs.putUInt("saverIdleMs", core1_screensaver_idle_ms);
  prefs.putUInt("screenOffMs", core1_screen_off_ms);
  prefs.putBool("fullReply", core1_display_mode == DisplayMode::FullReply);
  prefs.putUChar("backendMode", static_cast<uint8_t>(core1_backend_mode));
  prefs.end();

  ssid.toCharArray(core1_wifi_ssid, sizeof(core1_wifi_ssid));
  pass.toCharArray(core1_wifi_pass, sizeof(core1_wifi_pass));
  normalizedUrl.toCharArray(core1_groqputer_url, sizeof(core1_groqputer_url));
  normalizedWhisplayUrl.toCharArray(core1_whisplay_url, sizeof(core1_whisplay_url));
  core1_poll_ms = clampPollMs(pollMs);
  groqputerSnapshot.configured = core1_groqputer_url[0] != '\0';
  groqputerSnapshot.host = String(core1_groqputer_url);
  whisplaySnapshot.configured = core1_whisplay_url[0] != '\0';
  whisplaySnapshot.host = String(core1_whisplay_url);
  core1_has_settings = core1_wifi_ssid[0] != '\0' &&
    (groqputerSnapshot.configured || whisplaySnapshot.configured);
}

static void persistViewerSettings() {
  Preferences prefs;
  prefs.begin("core1disp", false);
  prefs.putUInt("autoScrollMs", core1_auto_scroll_ms);
  prefs.putUChar("fontScale", core1_font_scale);
  prefs.putUChar("themeIdx", core1_color_theme_index);
  prefs.putUChar("fontColorIdx", core1_font_color_index);
  prefs.putUChar("fontFamilyIdx", core1_font_family_index);
  prefs.putUChar("saverMode", static_cast<uint8_t>(core1_screensaver_mode));
  prefs.putUInt("saverIdleMs", core1_screensaver_idle_ms);
  prefs.putUInt("screenOffMs", core1_screen_off_ms);
  prefs.putBool("fullReply", core1_display_mode == DisplayMode::FullReply);
  prefs.putUChar("backendMode", static_cast<uint8_t>(core1_backend_mode));
  prefs.end();
}

static String buildPortalHtml() {
  String html;
  html.reserve(3400);
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Core1 Display Setup</title><style>";
  html += "body{background:#081018;color:#d9f0ff;font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:20px;}";
  html += "h1{color:#67f056;}label{display:block;margin-top:14px;font-weight:bold;color:#9fd2ff;}";
  html += "input{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#111a24;color:#f2fbff;}";
  html += "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;}";
  html += ".hint{font-size:.95em;color:#9ab4c9;}.notice{margin:16px 0;padding:12px;border-radius:8px;background:#143050;}";
  html += "</style></head><body>";
  html += "<h1>Core1 Display Setup</h1>";
  html += "<p class='hint'>Connect the Core1 to Wi-Fi and point it at one or both chat backends.</p>";
  if (core1_portal_notice.length()) {
    html += "<div class='notice'>";
    html += escapeHtml(core1_portal_notice);
    html += "</div>";
  }
  html += "<form method='post' action='/save'>";
  html += "<label>WiFi SSID</label><input name='ssid' maxlength='63' required value='" + escapeHtml(String(core1_wifi_ssid)) + "'>";
  html += "<label>WiFi Password</label><input name='pass' type='password' maxlength='63' value='" + escapeHtml(String(core1_wifi_pass)) + "'>";
  html += "<label>Groqputer Base URL</label><input name='groqputerUrl' maxlength='127' placeholder='http://10.160.0.203' value='" + escapeHtml(String(core1_groqputer_url[0] ? core1_groqputer_url : CORE1_DEFAULT_HOST)) + "'>";
  html += "<label>Whisplay Base URL</label><input name='whisplayUrl' maxlength='127' placeholder='" + String(CORE1_DEFAULT_WHISPLAY_URL) + "' value='" + escapeHtml(String(core1_whisplay_url)) + "'>";
  html += "<label>Poll Interval (ms)</label><input name='pollMs' type='number' min='500' max='5000' required value='" + String(core1_poll_ms) + "'>";
  html += "<button type='submit'>Save & Reboot</button></form>";
  html += "<p class='hint'>Groqputer uses <code>/api/companion/chat</code>. Whisplay uses <code>/api/state</code> (and optional settings metadata) on its own base URL.</p>";
  html += "</body></html>";
  return html;
}

static void handlePortalRoot() {
  core1_portal_server->send(200, "text/html", buildPortalHtml());
}

static void handlePortalSave() {
  String ssid = core1_portal_server->arg("ssid");
  String pass = core1_portal_server->arg("pass");
  String groqputerUrl = core1_portal_server->arg("groqputerUrl");
  String whisplayUrl = core1_portal_server->arg("whisplayUrl");
  uint16_t pollMs = clampPollMs(core1_portal_server->arg("pollMs").toInt());
  ssid.trim();
  groqputerUrl.trim();
  whisplayUrl.trim();
  if (!ssid.length()) {
    core1_portal_notice = "WiFi SSID is required.";
    handlePortalRoot();
    return;
  }
  if (!groqputerUrl.length() && !whisplayUrl.length()) {
    core1_portal_notice = "Enter at least one backend URL.";
    handlePortalRoot();
    return;
  }

  saveSettings(ssid, pass, groqputerUrl, whisplayUrl, pollMs);
  core1_portal_server->send(200, "text/html", "<html><body style='background:#081018;color:#d9f0ff;font-family:Arial;padding:24px'><h2>Saved. Rebooting...</h2></body></html>");
  delay(1200);
  ESP.restart();
}

static void runSetupPortal() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(CORE1_AP_SSID);

  if (!core1_portal_dns) {
    core1_portal_dns = new DNSServer();
  }
  if (!core1_portal_server) {
    core1_portal_server = new WebServer(80);
  }

  core1_portal_dns->start(53, "*", WiFi.softAPIP());
  core1_portal_server->on("/", handlePortalRoot);
  core1_portal_server->on("/save", HTTP_POST, handlePortalSave);
  core1_portal_server->onNotFound(handlePortalRoot);
  core1_portal_server->begin();

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setCursor(16, 20);
  M5.Lcd.print("Core1 Display Setup");
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setCursor(16, 56);
  M5.Lcd.print("Connect to:");
  M5.Lcd.setCursor(16, 80);
  M5.Lcd.print(CORE1_AP_SSID);
  M5.Lcd.setCursor(16, 116);
  M5.Lcd.print("Open:");
  M5.Lcd.setCursor(16, 140);
  M5.Lcd.print("http://192.168.4.1");

  while (true) {
    core1_portal_dns->processNextRequest();
    core1_portal_server->handleClient();
    delay(2);
  }
}

static void startWifiStation() {
  if (core1_wifi_ssid[0] == '\0') {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(core1_wifi_ssid, core1_wifi_pass);
  core1_last_wifi_retry_ms = millis();
}

static bool ensureWifiConnected(bool forceRetry = false) {
  if (core1_wifi_ssid[0] == '\0') {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  unsigned long now = millis();
  if (forceRetry || now - core1_last_wifi_retry_ms >= CORE1_WIFI_RETRY_MS) {
    WiFi.disconnect();
    WiFi.begin(core1_wifi_ssid, core1_wifi_pass);
    core1_last_wifi_retry_ms = now;
  }
  return false;
}

static String normalizeMessage(const String &value, const String &fallback) {
  String normalized = value;
  normalized.replace("\r", " ");
  normalized.replace("\n", " ");
  normalized.trim();
  return normalized.length() ? normalized : fallback;
}

static const char *backendModeLabel(BackendMode mode) {
  switch (mode) {
    case BackendMode::Groqputer:
      return "Groq";
    case BackendMode::Whisplay:
      return "Whis";
    case BackendMode::Auto:
    default:
      return "Auto";
  }
}

static const char *backendSourceLabel(BackendSource source) {
  switch (source) {
    case BackendSource::Groqputer:
      return "Groqputer";
    case BackendSource::Whisplay:
      return "Whisplay";
    case BackendSource::None:
    default:
      return "No source";
  }
}

static BackendMode cycleBackendMode(BackendMode current) {
  switch (current) {
    case BackendMode::Auto:
      return BackendMode::Groqputer;
    case BackendMode::Groqputer:
      return BackendMode::Whisplay;
    case BackendMode::Whisplay:
    default:
      return BackendMode::Auto;
  }
}

static bool snapshotHasVisibleContent(const CompanionSnapshot &snapshot) {
  return snapshot.replyMessage.length() > 0 || snapshot.userMessage.length() > 0;
}

static bool fetchGroqputerSnapshot(CompanionSnapshot &snapshot, String &errorOut) {
  errorOut = "";
  snapshot.configured = core1_groqputer_url[0] != '\0';
  snapshot.host = String(core1_groqputer_url);
  if (!snapshot.configured) {
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.error = "Groqputer URL not set";
    errorOut = snapshot.error;
    return false;
  }
  if (!ensureWifiConnected()) {
    errorOut = "WiFi disconnected";
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "offline";
    snapshot.error = errorOut;
    return false;
  }

  HTTPClient http;
  String endpoint = normalizeBaseUrl(String(core1_groqputer_url)) + "/api/companion/chat";
  if (!http.begin(endpoint)) {
    errorOut = "HTTP begin failed";
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "error";
    snapshot.error = errorOut;
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("HTTP ") + code;
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "error";
    snapshot.error = errorOut;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    errorOut = "JSON parse failed";
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "error";
    snapshot.error = errorOut;
    return false;
  }

  String nextStatus = String(doc["status"] | "idle");
  String nextModelTag = normalizeMessage(String(doc["modelTag"] | "BOT"), "BOT");
  String nextPersona = normalizeMessage(String(doc["persona"] | "Unknown"), "Unknown");
  String nextUserMessage = normalizeMessage(
    String(doc["latestUser"] | ""),
    "Waiting for the first prompt."
  );
  String nextReplyMessage = normalizeMessage(
    String(doc["latestReply"] | ""),
    "Waiting for the first reply."
  );
  bool ready = doc["ready"] | false;
  snapshot.reachable = true;
  snapshot.ready = ready;
  if (snapshot.status != nextStatus ||
      snapshot.modelTag != nextModelTag ||
      snapshot.persona != nextPersona ||
      snapshot.userMessage != nextUserMessage ||
      snapshot.replyMessage != nextReplyMessage) {
    snapshot.changedAtMs = millis();
  }
  snapshot.status = nextStatus;
  snapshot.modelTag = nextModelTag;
  snapshot.persona = nextPersona;
  snapshot.userMessage = nextUserMessage;
  snapshot.replyMessage = nextReplyMessage;
  snapshot.lastSuccessMs = millis();
  snapshot.error = "";
  if (!snapshot.ready) {
    errorOut = "Groqputer not ready";
    snapshot.error = errorOut;
    return false;
  }
  return true;
}

static bool fetchWhisplaySnapshot(CompanionSnapshot &snapshot, String &errorOut) {
  errorOut = "";
  snapshot.configured = core1_whisplay_url[0] != '\0';
  snapshot.host = String(core1_whisplay_url);
  if (!snapshot.configured) {
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.error = "Whisplay URL not set";
    errorOut = snapshot.error;
    return false;
  }
  if (!ensureWifiConnected()) {
    errorOut = "WiFi disconnected";
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "offline";
    snapshot.error = errorOut;
    return false;
  }

  HTTPClient http;
  String endpoint = normalizeBaseUrl(String(core1_whisplay_url)) + "/api/state";
  if (!http.begin(endpoint)) {
    errorOut = "HTTP begin failed";
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "error";
    snapshot.error = errorOut;
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("HTTP ") + code;
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "error";
    snapshot.error = errorOut;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    errorOut = "JSON parse failed";
    snapshot.ready = false;
    snapshot.reachable = false;
    snapshot.status = "error";
    snapshot.error = errorOut;
    return false;
  }

  bool ready = doc["ready"] | false;
  String nextStatus = normalizeMessage(String(doc["status"] | "idle"), "idle");
  String nextReplyMessage = normalizeMessage(
    String(doc["text"] | ""),
    ready ? "Waiting for the next reply." : "Whisplay not ready."
  );
  String nextUserMessage = normalizeMessage(
    String(doc["emoji"] | "") + " " + nextStatus,
    "Whisplay state"
  );
  String nextModelTag = "W-HAT";
  String nextPersona = "Whisplay";
  String nextImagePath = String(doc["image"] | "");
  uint32_t nextImageRevision = doc["image_revision"] | 0;
  bool imageMetadataChanged =
    snapshot.imagePath != nextImagePath ||
    snapshot.imageRevision != nextImageRevision;
  snapshot.reachable = true;
  snapshot.ready = ready;
  if (snapshot.status != nextStatus ||
      snapshot.modelTag != nextModelTag ||
      snapshot.persona != nextPersona ||
      snapshot.userMessage != nextUserMessage ||
      snapshot.replyMessage != nextReplyMessage ||
      snapshot.imagePath != nextImagePath ||
      snapshot.imageRevision != nextImageRevision) {
    snapshot.changedAtMs = millis();
    Serial.printf("[Core1] Whisplay snapshot changed status=%s imageRev=%lu image=%s\n",
      nextStatus.c_str(),
      static_cast<unsigned long>(nextImageRevision),
      nextImagePath.c_str());
  }
  snapshot.status = nextStatus;
  snapshot.modelTag = nextModelTag;
  snapshot.persona = nextPersona;
  snapshot.userMessage = nextUserMessage;
  snapshot.replyMessage = nextReplyMessage;
  snapshot.imagePath = nextImagePath;
  snapshot.imageRevision = nextImageRevision;
  if (!snapshot.imagePath.length()) {
    resetSnapshotImageState(snapshot);
  } else if (imageMetadataChanged ||
             !snapshot.imageAvailable ||
             snapshot.imageCachePath.length() == 0 ||
             !SPIFFS.exists(snapshot.imageCachePath.c_str())) {
    refreshWhisplayImageCache(snapshot);
  }
  snapshot.lastSuccessMs = millis();
  snapshot.error = "";
  if (!snapshot.ready) {
    errorOut = "Whisplay not ready";
    snapshot.error = errorOut;
    return false;
  }
  return true;
}

static BackendSource chooseActiveBackendSource() {
  bool groqAvailable = groqputerSnapshot.configured && groqputerSnapshot.reachable && groqputerSnapshot.ready;
  bool whisplayAvailable = whisplaySnapshot.configured && whisplaySnapshot.reachable && whisplaySnapshot.ready;
  if (core1_backend_mode == BackendMode::Groqputer) {
    if (groqAvailable) return BackendSource::Groqputer;
    if (whisplayAvailable) return BackendSource::Whisplay;
    return groqputerSnapshot.configured ? BackendSource::Groqputer : BackendSource::Whisplay;
  }
  if (core1_backend_mode == BackendMode::Whisplay) {
    if (whisplayAvailable) return BackendSource::Whisplay;
    if (groqAvailable) return BackendSource::Groqputer;
    return whisplaySnapshot.configured ? BackendSource::Whisplay : BackendSource::Groqputer;
  }
  if (groqAvailable && whisplayAvailable) {
    return whisplaySnapshot.changedAtMs >= groqputerSnapshot.changedAtMs
      ? BackendSource::Whisplay
      : BackendSource::Groqputer;
  }
  if (whisplayAvailable) return BackendSource::Whisplay;
  if (groqAvailable) return BackendSource::Groqputer;
  if (snapshotHasVisibleContent(whisplaySnapshot) && whisplaySnapshot.changedAtMs >= groqputerSnapshot.changedAtMs) {
    return BackendSource::Whisplay;
  }
  if (snapshotHasVisibleContent(groqputerSnapshot)) {
    return BackendSource::Groqputer;
  }
  if (whisplaySnapshot.configured) return BackendSource::Whisplay;
  if (groqputerSnapshot.configured) return BackendSource::Groqputer;
  return BackendSource::None;
}

static const CompanionSnapshot *snapshotForSource(BackendSource source) {
  switch (source) {
    case BackendSource::Groqputer:
      return &groqputerSnapshot;
    case BackendSource::Whisplay:
      return &whisplaySnapshot;
    case BackendSource::None:
    default:
      return nullptr;
  }
}

static bool fetchCompanionChat(String &errorOut) {
  errorOut = "";
  String groqError;
  String whisplayError;
  if (groqputerSnapshot.configured) {
    fetchGroqputerSnapshot(groqputerSnapshot, groqError);
  }
  if (whisplaySnapshot.configured) {
    fetchWhisplaySnapshot(whisplaySnapshot, whisplayError);
  }

  activeBackendSource = chooseActiveBackendSource();
  const CompanionSnapshot *activeSnapshot = snapshotForSource(activeBackendSource);
  if (!activeSnapshot) {
    latestStatus = "offline";
    latestPollError = "No backend configured";
    errorOut = latestPollError;
    return false;
  }

  latestStatus = activeSnapshot->status;
  latestModelTag = activeSnapshot->modelTag;
  latestPersona = activeSnapshot->persona;
  latestUserMessage = normalizeMessage(
    activeSnapshot->userMessage,
    activeBackendSource == BackendSource::Whisplay
      ? "Whisplay live state"
      : "Waiting for the first prompt."
  );
  latestReplyMessage = normalizeMessage(
    activeSnapshot->replyMessage,
    activeBackendSource == BackendSource::Whisplay
      ? "Waiting for the next reply."
      : "Waiting for the first reply."
  );
  latestUpdatedAtMs = activeSnapshot->changedAtMs
    ? activeSnapshot->changedAtMs
    : (latestUpdatedAtMs ? latestUpdatedAtMs : millis());
  latestPollError = activeSnapshot->error;
  lastSuccessfulPollMs = activeSnapshot->lastSuccessMs;

  if (core1_backend_mode == BackendMode::Auto) {
    if (!groqError.length() || !whisplayError.length()) {
      latestPollError = "";
    } else {
      latestPollError = groqError + " | " + whisplayError;
    }
  }
  errorOut = latestPollError;
  return activeSnapshot->ready;
}

static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  String source = sourceText;
  source.replace("\r", "");
  String currentLine;
  for (size_t i = 0; i < source.length(); i++) {
    char c = source.charAt(i);
    if (c == '\n') {
      if (lineCount < maxLines) lines[lineCount++] = currentLine;
      currentLine = "";
      continue;
    }
    currentLine += c;
    if (currentLine.length() >= static_cast<size_t>(maxChars)) {
      int breakPos = currentLine.lastIndexOf(' ');
      if (breakPos > maxChars / 2) {
        if (lineCount < maxLines) lines[lineCount++] = currentLine.substring(0, breakPos);
        currentLine = currentLine.substring(breakPos + 1);
      } else {
        if (lineCount < maxLines) lines[lineCount++] = currentLine;
        currentLine = "";
      }
    }
  }
  if (currentLine.length() && lineCount < maxLines) {
    lines[lineCount++] = currentLine;
  }
}

static int scrollOffsetForPane(FocusPane pane) {
  return pane == FocusPane::Prompt ? promptScrollOffset : replyScrollOffset;
}

static void setScrollOffsetForPane(FocusPane pane, int value) {
  if (pane == FocusPane::Prompt) {
    promptScrollOffset = value;
  } else {
    replyScrollOffset = value;
  }
}

static int maxScrollForText(const String &text, int width, int height, int maxCharsPerLine, int lineHeight) {
  String lines[128];
  int lineCount = 0;
  fillWrappedLines(text, lines, lineCount, 128, maxCharsPerLine);
  int visibleLines = max(1, height / lineHeight);
  return max(0, lineCount - visibleLines);
}

static int paneTextHeight(FocusPane pane) {
  if (core1_display_mode == DisplayMode::FullReply) {
    return 182 - 38;
  }
  return pane == FocusPane::Prompt ? (72 - 38) : (102 - 38);
}

static void resetScrollLoopState() {
  autoScrollPauseStartedMs = 0;
  lastAutoScrollMs = 0;
}

static void touchInteraction() {
  lastInteractionMs = millis();
  screenBlanked = false;
  if (activeScreen == ScreenMode::Screensaver) {
    activeScreen = ScreenMode::Viewer;
    core1_ai_dirty = true;
    uiDirty = true;
  }
}

static void initScreensaverState() {
  for (int i = 0; i < CORE1_MATRIX_COLUMN_COUNT; i++) {
    matrixColumns[i].y = -static_cast<float>(random(0, 135));
    matrixColumns[i].speed = 1.0f + static_cast<float>(random(0, 22)) / 10.0f;
    matrixColumns[i].headChar = static_cast<uint8_t>(random(0, static_cast<int>(sizeof(CORE1_MATRIX_CHARS) - 1)));
  }
  for (int i = 0; i < CORE1_ENTROPY_PARTICLE_COUNT; i++) {
    entropyParticles[i].x = static_cast<float>(random(0, 320));
    entropyParticles[i].y = static_cast<float>(random(0, 240));
    entropyParticles[i].vx = (static_cast<float>(random(-14, 15)) / 10.0f);
    entropyParticles[i].vy = (static_cast<float>(random(-14, 15)) / 10.0f);
    if (fabs(entropyParticles[i].vx) < 0.2f) entropyParticles[i].vx = 0.6f;
    if (fabs(entropyParticles[i].vy) < 0.2f) entropyParticles[i].vy = -0.6f;
    switch (i % 4) {
      case 0: entropyParticles[i].color = TFT_GREEN; break;
      case 1: entropyParticles[i].color = TFT_CYAN; break;
      case 2: entropyParticles[i].color = TFT_MAGENTA; break;
      default: entropyParticles[i].color = TFT_YELLOW; break;
    }
  }
  core1_last_ai_slide_ms = 0;
  core1_ai_dirty = true;
}

static void drawMatrixScreensaver(unsigned long now) {
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(1);
  M5.Lcd.setTextSize(2);
  const int colSpacing = 18;
  for (int i = 0; i < CORE1_MATRIX_COLUMN_COUNT; i++) {
    int x = i * colSpacing;
    matrixColumns[i].y += matrixColumns[i].speed;
    if (matrixColumns[i].y > 260.0f) {
      matrixColumns[i].y = -static_cast<float>(random(20, 140));
      matrixColumns[i].speed = 1.0f + static_cast<float>(random(0, 22)) / 10.0f;
      matrixColumns[i].headChar = static_cast<uint8_t>(random(0, static_cast<int>(sizeof(CORE1_MATRIX_CHARS) - 1)));
    }
    for (int trail = 0; trail < 6; trail++) {
      int y = static_cast<int>(matrixColumns[i].y) - trail * 18;
      if (y < -16 || y > 240) continue;
      uint16_t color = trail == 0
        ? TFT_WHITE
        : (trail < 3 ? TFT_GREEN : static_cast<uint16_t>(0x0200 + trail * 0x0100));
      M5.Lcd.setTextColor(color, TFT_BLACK);
      M5.Lcd.setCursor(x + 4, y);
      char ch = CORE1_MATRIX_CHARS[(matrixColumns[i].headChar + trail + (now / 120) + i) % (sizeof(CORE1_MATRIX_CHARS) - 1)];
      M5.Lcd.print(ch);
    }
  }
  restoreBuiltinBodyFont();
}

static void drawRippleScreensaver(unsigned long now) {
  M5.Lcd.fillScreen(TFT_BLACK);
  const float phase = now / 240.0f;
  for (int y = 20; y < 220; y += 8) {
    int xOffset = static_cast<int>(sin((y * 0.12f) + phase) * 18.0f);
    uint16_t color = (y / 8) % 2 == 0 ? TFT_CYAN : TFT_BLUE;
    M5.Lcd.drawFastHLine(40 + xOffset, y, 240, color);
  }
  for (int radius = 20; radius <= 120; radius += 24) {
    int wobble = static_cast<int>(sin(phase + radius * 0.05f) * 10.0f);
    M5.Lcd.drawCircle(160, 120, radius + wobble, radius % 48 == 0 ? TFT_WHITE : TFT_CYAN);
  }
}

static void drawEntropyScreensaver(unsigned long now) {
  M5.Lcd.fillScreen(TFT_BLACK);
  for (int i = 0; i < CORE1_ENTROPY_PARTICLE_COUNT; i++) {
    EntropyParticle &particle = entropyParticles[i];
    particle.x += particle.vx;
    particle.y += particle.vy;
    if (particle.x < 0 || particle.x >= 320) {
      particle.vx *= -1.0f;
      particle.x = constrain(static_cast<int>(particle.x), 0, 319);
    }
    if (particle.y < 0 || particle.y >= 240) {
      particle.vy *= -1.0f;
      particle.y = constrain(static_cast<int>(particle.y), 0, 239);
    }
    int jitterX = static_cast<int>(sin((now / 180.0f) + i) * 4.0f);
    int jitterY = static_cast<int>(cos((now / 210.0f) + i) * 4.0f);
    M5.Lcd.fillCircle(static_cast<int>(particle.x) + jitterX, static_cast<int>(particle.y) + jitterY, 2, particle.color);
    if (i % 3 == 0) {
      M5.Lcd.drawLine(
        static_cast<int>(particle.x),
        static_cast<int>(particle.y),
        160,
        120,
        TFT_DARKGREY
      );
    }
  }
}

static void drawAiScreensaver() {
  unsigned long now = millis();
  if (!core1_ai_image_count) {
    static unsigned long lastLoggedNoCacheMs = 0;
    if (now - lastLoggedNoCacheMs > 3000UL) {
      Serial.printf("[Core1] AI screensaver empty error=%s\n", core1_ai_error.c_str());
      lastLoggedNoCacheMs = now;
    }
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setTextFont(2);
    M5.Lcd.setCursor(24, 96);
    M5.Lcd.print("No AI images cached");
    M5.Lcd.setCursor(24, 120);
    M5.Lcd.print(core1_ai_error.length() ? core1_ai_error : "Waiting for Whisplay AI images...");
    return;
  }

  if (core1_last_ai_slide_ms == 0) {
    core1_last_ai_slide_ms = now;
    core1_ai_dirty = true;
  } else if (now - core1_last_ai_slide_ms >= CORE1_AI_SLIDE_MS) {
    core1_last_ai_slide_ms = now;
    core1_ai_slide_index = (core1_ai_slide_index + 1) % core1_ai_image_count;
    core1_ai_dirty = true;
  }

  if (!core1_ai_dirty) {
    return;
  }

  Core1AiImage *image = currentAiImage();
  if (!image || !ensureCurrentAiImageCached()) {
    Serial.printf("[Core1] AI screensaver cache unavailable index=%u error=%s\n",
      static_cast<unsigned>(core1_ai_slide_index),
      core1_ai_error.c_str());
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setTextFont(2);
    M5.Lcd.setCursor(24, 96);
    M5.Lcd.print("AI cache unavailable");
    M5.Lcd.setCursor(24, 120);
    M5.Lcd.print(core1_ai_error.length() ? core1_ai_error : "Unable to load AI slide.");
    core1_ai_dirty = false;
    return;
  }

  fs::FS &storage = core1StorageFs(image->useSd);
  if (!storage.exists(image->cachePath.c_str())) {
    image->cached = false;
    core1_ai_error = image->useSd ? "SD cache missing" : "SPIFFS cache missing";
    core1_ai_dirty = true;
    return;
  }

  M5.Lcd.fillScreen(TFT_BLACK);
  Serial.printf("[Core1] AI screensaver draw index=%u cache=%s storage=%s\n",
    static_cast<unsigned>(core1_ai_slide_index),
    image->cachePath.c_str(),
    image->useSd ? "SD" : "SPIFFS");
  M5.Lcd.drawJpgFile(storage, image->cachePath.c_str(), 0, 0, 320, 240);
  core1_ai_dirty = false;
}

static void drawScreensaver() {
  unsigned long now = millis();
  if (screenBlanked) {
    return;
  }
  switch (core1_screensaver_mode) {
    case ScreensaverMode::Matrix:
      drawMatrixScreensaver(now);
      break;
    case ScreensaverMode::Ripple:
      drawRippleScreensaver(now);
      break;
    case ScreensaverMode::Entropy:
      drawEntropyScreensaver(now);
      break;
    case ScreensaverMode::AiSlideshow:
      drawAiScreensaver();
      break;
    case ScreensaverMode::Off:
    default:
      M5.Lcd.fillScreen(TFT_BLACK);
      break;
  }
  if (core1_screensaver_mode != ScreensaverMode::AiSlideshow) {
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setCursor(8, 222);
    M5.Lcd.print(screensaverModeLabel(core1_screensaver_mode));
  }
}

static void updateScreensaverState() {
  if (activeScreen == ScreenMode::Settings) {
    return;
  }
  unsigned long now = millis();
  unsigned long idleSince = max(lastInteractionMs, latestUpdatedAtMs);
  if (core1_screensaver_mode != ScreensaverMode::Off &&
      core1_screensaver_idle_ms > 0 &&
      activeScreen == ScreenMode::Viewer &&
      now - idleSince >= core1_screensaver_idle_ms) {
    activeScreen = ScreenMode::Screensaver;
    Serial.printf("[Core1] Enter screensaver mode=%d aiCount=%u status=%s imageVisible=%d\n",
      static_cast<int>(core1_screensaver_mode),
      static_cast<unsigned>(core1_ai_image_count),
      latestStatus.c_str(),
      activeWhisplayImageVisible());
    if (core1_screensaver_mode == ScreensaverMode::AiSlideshow && core1_ai_image_count == 0) {
      fetchCore1AiImages(true);
    }
    screensaverStartedMs = now;
    screenBlanked = false;
    core1_ai_dirty = true;
    uiDirty = true;
  }
  if (activeScreen == ScreenMode::Screensaver &&
      core1_screen_off_ms > 0 &&
      !screenBlanked &&
      now - screensaverStartedMs >= core1_screen_off_ms) {
    M5.Lcd.fillScreen(TFT_BLACK);
    screenBlanked = true;
  }
}

static void drawPane(int x, int y, int w, int h, const char *title, const String &text, bool active, FocusPane pane) {
  ColorTheme theme = currentTheme();
  uint16_t borderColor = active ? theme.accent : TFT_DARKGREY;
  M5.Lcd.drawRoundRect(x, y, w, h, 6, borderColor);
  M5.Lcd.setTextColor(active ? theme.accent : theme.title, TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(x + 8, y + 6);
  M5.Lcd.print(title);
  M5.Lcd.drawFastHLine(x + 6, y + 24, w - 12, borderColor);

  const int textX = x + 8;
  const int textY = y + 30;
  const int textW = w - 16;
  const int textH = h - 38;
  const int maxChars = max(8, textW / currentBodyCharWidth());
  const int lineHeight = currentBodyLineHeight();
  const int visibleLines = max(1, textH / lineHeight);
  String lines[128];
  int lineCount = 0;
  fillWrappedLines(text, lines, lineCount, 128, maxChars);
  int maxScroll = max(0, lineCount - visibleLines);
  int scrollOffset = constrain(scrollOffsetForPane(pane), 0, maxScroll);
  setScrollOffsetForPane(pane, scrollOffset);

  M5.Lcd.setTextColor(currentFontColor(), TFT_BLACK);
  applyBodyFont();
  int cursorY = textY;
  for (int i = scrollOffset; i < min(lineCount, scrollOffset + visibleLines); i++) {
    M5.Lcd.setCursor(textX, cursorY + currentBodyBaseline());
    M5.Lcd.print(lines[i]);
    cursorY += lineHeight;
  }
  restoreBuiltinBodyFont();

  if (maxScroll > 0) {
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setCursor(x + w - 40, y + h - 10);
    M5.Lcd.print(String(scrollOffset + 1) + "/" + String(maxScroll + 1));
  }
}

static void drawImagePane(int x, int y, int w, int h, const CompanionSnapshot &snapshot, bool active) {
  M5.Lcd.fillScreen(TFT_BLACK);
  if (snapshot.imageAvailable && snapshot.imageCachePath.length() > 0 && SPIFFS.exists(snapshot.imageCachePath.c_str())) {
    M5.Lcd.drawJpgFile(SPIFFS, snapshot.imageCachePath.c_str(), x, y, w, h);
  } else {
    ColorTheme theme = currentTheme();
    M5.Lcd.setTextColor(theme.title, TFT_BLACK);
    M5.Lcd.setTextFont(2);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(x + 24, y + 96);
    M5.Lcd.print("No photo yet");
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setCursor(x + 24, y + 120);
    M5.Lcd.print("Waiting for Whisplay image...");
  }
}

static String currentFooterInfoText() {
  String infoText = latestPollError.length()
    ? latestPollError
    : (lastSuccessfulPollMs ? String((millis() - lastSuccessfulPollMs) / 1000UL) + "s ago" : "never synced");
  if (infoText.length() > 28) {
    infoText = infoText.substring(0, 28);
  }
  return infoText;
}

static String currentBackendHostText() {
  if (activeBackendSource == BackendSource::Groqputer && core1_groqputer_url[0] != '\0') {
    return normalizeBaseUrl(String(core1_groqputer_url));
  }
  if (activeBackendSource == BackendSource::Whisplay && core1_whisplay_url[0] != '\0') {
    return normalizeBaseUrl(String(core1_whisplay_url));
  }
  if (core1_backend_mode == BackendMode::Whisplay && core1_whisplay_url[0] != '\0') {
    return normalizeBaseUrl(String(core1_whisplay_url));
  }
  if (core1_groqputer_url[0] != '\0') {
    return normalizeBaseUrl(String(core1_groqputer_url));
  }
  if (core1_whisplay_url[0] != '\0') {
    return normalizeBaseUrl(String(core1_whisplay_url));
  }
  return "";
}

static void drawFooterArea() {
  M5.Lcd.fillRect(0, 215, 320, 25, TFT_BLACK);
  M5.Lcd.drawFastHLine(0, 214, 320, TFT_DARKGREY);
  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Lcd.setTextFont(1);

  String hostText = String(backendModeLabel(core1_backend_mode)) + " " + currentBackendHostText();
  if (hostText.length() > 28) {
    hostText = hostText.substring(0, 28);
  }
  M5.Lcd.setCursor(8, 222);
  M5.Lcd.print(hostText.length() ? hostText : "No backend host set");

  M5.Lcd.setCursor(8, 232);
  M5.Lcd.print(currentFooterInfoText());

  M5.Lcd.setCursor(180, 222);
  M5.Lcd.print(backendSourceLabel(activeBackendSource));
  M5.Lcd.setCursor(174, 232);
  M5.Lcd.print("A/B scr C pane HB bot");
  footerDirty = false;
}

static String settingsFieldLabel(SettingsField field) {
  switch (field) {
    case SettingsField::BorderColor:
      return "Border Color";
    case SettingsField::FontColor:
      return "Font Color";
    case SettingsField::AutoScroll:
      return "Auto Scroll";
    case SettingsField::FontSize:
      return "Font Size";
    case SettingsField::FontFamily:
      return "Font";
    case SettingsField::ScreensaverMode:
      return "Saver";
    case SettingsField::ScreensaverOn:
      return "Saver On";
    case SettingsField::ScreenOff:
      return "Screen Off";
    case SettingsField::DisplayMode:
      return "Display Mode";
  }
  return "";
}

static String autoScrollLabel() {
  if (core1_auto_scroll_ms == 0) {
    return "Off";
  }
  return String(core1_auto_scroll_ms) + " ms";
}

static const char *screensaverModeLabel(ScreensaverMode mode) {
  switch (mode) {
    case ScreensaverMode::Matrix:
      return "Matrix";
    case ScreensaverMode::Ripple:
      return "Ripple";
    case ScreensaverMode::Entropy:
      return "Entropy";
    case ScreensaverMode::AiSlideshow:
      return "AI Show";
    case ScreensaverMode::Off:
    default:
      return "Off";
  }
}

static String durationLabel(uint32_t value) {
  if (value == 0) {
    return "Off";
  }
  if (value < 60000UL) {
    return String(value / 1000UL) + "s";
  }
  return String(value / 60000UL) + "m";
}

static String settingsFieldValue(SettingsField field) {
  switch (field) {
    case SettingsField::BorderColor:
      return CORE1_COLOR_THEMES[core1_color_theme_index].label;
    case SettingsField::FontColor:
      return CORE1_FONT_COLORS[core1_font_color_index].label;
    case SettingsField::AutoScroll:
      return autoScrollLabel();
    case SettingsField::FontSize:
      return String(core1_font_scale) + "x";
    case SettingsField::FontFamily:
      return currentBodyFont().label;
    case SettingsField::ScreensaverMode:
      return screensaverModeLabel(core1_screensaver_mode);
    case SettingsField::ScreensaverOn:
      return durationLabel(core1_screensaver_idle_ms);
    case SettingsField::ScreenOff:
      return durationLabel(core1_screen_off_ms);
    case SettingsField::DisplayMode:
      return core1_display_mode == DisplayMode::Split ? "Split" : "Full BOT";
  }
  return "";
}

static void drawSettingsUi() {
  ColorTheme theme = currentTheme();
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(theme.accent, TFT_BLACK);
  M5.Lcd.setCursor(10, 8);
  M5.Lcd.print("Core1 Settings");
  M5.Lcd.drawFastHLine(0, 26, 320, TFT_DARKGREY);

  const SettingsField fields[] = {
    SettingsField::BorderColor,
    SettingsField::FontColor,
    SettingsField::AutoScroll,
    SettingsField::FontSize,
    SettingsField::FontFamily,
    SettingsField::ScreensaverMode,
    SettingsField::ScreensaverOn,
    SettingsField::ScreenOff,
    SettingsField::DisplayMode,
  };
  const size_t fieldCount = sizeof(fields) / sizeof(fields[0]);
  int activeIndex = 0;
  for (size_t i = 0; i < fieldCount; i++) {
    if (fields[i] == activeSettingsField) {
      activeIndex = static_cast<int>(i);
      break;
    }
  }
  int startIndex = 0;
  if (activeIndex >= static_cast<int>(CORE1_SETTINGS_VISIBLE_ROWS)) {
    startIndex = activeIndex - static_cast<int>(CORE1_SETTINGS_VISIBLE_ROWS) + 1;
  }
  int endIndex = min(static_cast<int>(fieldCount), startIndex + static_cast<int>(CORE1_SETTINGS_VISIBLE_ROWS));
  int cursorY = 40;

  for (int i = startIndex; i < endIndex; i++) {
    SettingsField field = fields[i];
    bool active = field == activeSettingsField;
    M5.Lcd.setTextColor(active ? theme.accent : TFT_CYAN, TFT_BLACK);
    M5.Lcd.setCursor(12, cursorY);
    M5.Lcd.print(active ? "> " : "  ");
    M5.Lcd.print(settingsFieldLabel(field));
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(170, cursorY);
    M5.Lcd.print(settingsFieldValue(field));
    cursorY += 28;
  }

  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Lcd.drawFastHLine(0, 214, 320, TFT_DARKGREY);
  M5.Lcd.setTextFont(1);
  M5.Lcd.setCursor(8, 222);
  M5.Lcd.print("A next  B change  C close");
  M5.Lcd.setCursor(8, 232);
  M5.Lcd.print(String(activeIndex + 1) + "/" + String(fieldCount) + "  Hold C AP");
  uiDirty = false;
  footerDirty = false;
}

static void drawUi() {
  if (activeScreen == ScreenMode::Settings) {
    drawSettingsUi();
    return;
  }
  if (activeScreen == ScreenMode::Screensaver) {
    drawScreensaver();
    uiDirty = false;
    footerDirty = false;
    return;
  }
  if (activeWhisplayImageVisible()) {
    activePane = FocusPane::Reply;
    drawImagePane(0, 0, 320, 240, whisplaySnapshot, false);
    lastDrawnWifiStatus = WiFi.status();
    lastDrawnSyncAgeSeconds = lastSuccessfulPollMs ? static_cast<long>((millis() - lastSuccessfulPollMs) / 1000UL) : -1;
    uiDirty = false;
    footerDirty = false;
    return;
  }
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
  ColorTheme theme = currentTheme();
  M5.Lcd.setTextColor(theme.accent, TFT_BLACK);
  M5.Lcd.setCursor(10, 8);
  M5.Lcd.print("Core1 Chat");

  M5.Lcd.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_RED, TFT_BLACK);
  M5.Lcd.setCursor(116, 8);
  M5.Lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "NoWiFi");

  M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Lcd.setCursor(188, 8);
  M5.Lcd.print(latestModelTag);

  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Lcd.setCursor(232, 8);
  M5.Lcd.print(latestStatus);
  M5.Lcd.drawFastHLine(0, 26, 320, TFT_DARKGREY);

  if (core1_display_mode == DisplayMode::Split) {
    drawPane(8, 34, 304, 72, "YOU", latestUserMessage, activePane == FocusPane::Prompt, FocusPane::Prompt);
    drawPane(8, 114, 304, 102, "BOT", latestReplyMessage, activePane == FocusPane::Reply, FocusPane::Reply);
  } else {
    drawPane(8, 34, 304, 182, "BOT", latestReplyMessage, true, FocusPane::Reply);
  }
  drawFooterArea();
  lastDrawnWifiStatus = WiFi.status();
  lastDrawnSyncAgeSeconds = lastSuccessfulPollMs ? static_cast<long>((millis() - lastSuccessfulPollMs) / 1000UL) : -1;
  uiDirty = false;
}

static void scrollActivePane(int delta) {
  FocusPane targetPane = core1_display_mode == DisplayMode::FullReply ? FocusPane::Reply : activePane;
  const String &text = targetPane == FocusPane::Prompt ? latestUserMessage : latestReplyMessage;
  int maxScroll = maxScrollForText(text, 288, paneTextHeight(targetPane), max(8, 288 / currentBodyCharWidth()), currentBodyLineHeight());
  int current = scrollOffsetForPane(targetPane);
  int next = constrain(current + delta, 0, maxScroll);
  if (next != current) {
    setScrollOffsetForPane(targetPane, next);
    resetScrollLoopState();
    uiDirty = true;
  }
}

static void cycleSettingsField() {
  int index = static_cast<int>(activeSettingsField);
  index = (index + 1) % 9;
  activeSettingsField = static_cast<SettingsField>(index);
  uiDirty = true;
}

static void adjustSettingValue() {
  switch (activeSettingsField) {
    case SettingsField::BorderColor:
      core1_color_theme_index = (core1_color_theme_index + 1) % (sizeof(CORE1_COLOR_THEMES) / sizeof(CORE1_COLOR_THEMES[0]));
      break;
    case SettingsField::FontColor:
      core1_font_color_index = (core1_font_color_index + 1) % (sizeof(CORE1_FONT_COLORS) / sizeof(CORE1_FONT_COLORS[0]));
      break;
    case SettingsField::AutoScroll: {
      size_t count = sizeof(CORE1_AUTO_SCROLL_OPTIONS) / sizeof(CORE1_AUTO_SCROLL_OPTIONS[0]);
      size_t currentIndex = 0;
      for (size_t i = 0; i < count; i++) {
        if (CORE1_AUTO_SCROLL_OPTIONS[i] == core1_auto_scroll_ms) {
          currentIndex = i;
          break;
        }
      }
      currentIndex = (currentIndex + 1) % count;
      core1_auto_scroll_ms = CORE1_AUTO_SCROLL_OPTIONS[currentIndex];
      break;
    }
    case SettingsField::FontSize:
      core1_font_scale = core1_font_scale >= 3 ? 1 : core1_font_scale + 1;
      break;
    case SettingsField::FontFamily:
      core1_font_family_index =
        (core1_font_family_index + 1) % (sizeof(CORE1_BODY_FONTS) / sizeof(CORE1_BODY_FONTS[0]));
      break;
    case SettingsField::ScreensaverMode:
      core1_screensaver_mode = static_cast<ScreensaverMode>(
        (static_cast<int>(core1_screensaver_mode) + 1) % 5
      );
      if (core1_screensaver_mode == ScreensaverMode::AiSlideshow) {
        core1_last_ai_poll_ms = 0;
        fetchCore1AiImages(true);
      }
      break;
    case SettingsField::ScreensaverOn: {
      size_t count = sizeof(CORE1_SAVER_IDLE_OPTIONS) / sizeof(CORE1_SAVER_IDLE_OPTIONS[0]);
      size_t currentIndex = 0;
      for (size_t i = 0; i < count; i++) {
        if (CORE1_SAVER_IDLE_OPTIONS[i] == core1_screensaver_idle_ms) {
          currentIndex = i;
          break;
        }
      }
      core1_screensaver_idle_ms = CORE1_SAVER_IDLE_OPTIONS[(currentIndex + 1) % count];
      break;
    }
    case SettingsField::ScreenOff: {
      size_t count = sizeof(CORE1_SCREEN_OFF_OPTIONS) / sizeof(CORE1_SCREEN_OFF_OPTIONS[0]);
      size_t currentIndex = 0;
      for (size_t i = 0; i < count; i++) {
        if (CORE1_SCREEN_OFF_OPTIONS[i] == core1_screen_off_ms) {
          currentIndex = i;
          break;
        }
      }
      core1_screen_off_ms = CORE1_SCREEN_OFF_OPTIONS[(currentIndex + 1) % count];
      break;
    }
    case SettingsField::DisplayMode:
      core1_display_mode = core1_display_mode == DisplayMode::Split ? DisplayMode::FullReply : DisplayMode::Split;
      activePane = FocusPane::Reply;
      break;
  }
  persistViewerSettings();
  resetScrollLoopState();
  uiDirty = true;
  footerDirty = true;
}

static void pollAutoScroll() {
  if (activeScreen != ScreenMode::Viewer || core1_auto_scroll_ms == 0) {
    return;
  }
  unsigned long now = millis();
  if (lastAutoScrollMs != 0 && now - lastAutoScrollMs < core1_auto_scroll_ms) {
    return;
  }

  FocusPane targetPane = core1_display_mode == DisplayMode::FullReply ? FocusPane::Reply : activePane;
  const String &text = targetPane == FocusPane::Prompt ? latestUserMessage : latestReplyMessage;
  int maxScroll = maxScrollForText(text, 288, paneTextHeight(targetPane), max(8, 288 / currentBodyCharWidth()), currentBodyLineHeight());
  int current = scrollOffsetForPane(targetPane);
  if (maxScroll > 0 && current < maxScroll) {
    setScrollOffsetForPane(targetPane, current + 1);
    if (current + 1 >= maxScroll) {
      autoScrollPauseStartedMs = now;
    }
    uiDirty = true;
  } else if (maxScroll > 0 && current >= maxScroll) {
    if (autoScrollPauseStartedMs == 0) {
      autoScrollPauseStartedMs = now;
    } else if (now - autoScrollPauseStartedMs >= 1600) {
      setScrollOffsetForPane(targetPane, 0);
      autoScrollPauseStartedMs = 0;
      uiDirty = true;
    }
  }
  lastAutoScrollMs = now;
}

static void handleButtons() {
  static bool aLongHandled = false;
  static bool bLongHandled = false;
  static bool cLongHandled = false;

  if (M5.BtnA.wasPressed()) {
    aLongHandled = false;
    touchInteraction();
  }
  if (M5.BtnA.pressedFor(CORE1_LONG_PRESS_MS) && !aLongHandled) {
    aLongHandled = true;
    activeScreen = ScreenMode::Settings;
    resetScrollLoopState();
    uiDirty = true;
  }
  if (M5.BtnA.wasReleased() && !aLongHandled) {
    touchInteraction();
    if (activeScreen == ScreenMode::Settings) {
      cycleSettingsField();
    } else {
      scrollActivePane(-1);
    }
  }
  if (M5.BtnB.wasPressed()) {
    bLongHandled = false;
    touchInteraction();
  }
  if (M5.BtnB.pressedFor(CORE1_LONG_PRESS_MS) && !bLongHandled) {
    bLongHandled = true;
    if (activeScreen == ScreenMode::Viewer) {
      core1_backend_mode = cycleBackendMode(core1_backend_mode);
      persistViewerSettings();
      footerDirty = true;
      uiDirty = true;
    }
  }
  if (M5.BtnB.wasReleased() && !bLongHandled) {
    touchInteraction();
    if (activeScreen == ScreenMode::Settings) {
      adjustSettingValue();
    } else {
      scrollActivePane(1);
    }
  }

  if (M5.BtnC.wasPressed()) {
    cLongHandled = false;
    touchInteraction();
  }
  if (M5.BtnC.pressedFor(CORE1_LONG_PRESS_MS) && !cLongHandled) {
    cLongHandled = true;
    runSetupPortal();
  }
  if (M5.BtnC.wasReleased() && !cLongHandled) {
    if (activeScreen == ScreenMode::Settings) {
      activeScreen = ScreenMode::Viewer;
      resetScrollLoopState();
      uiDirty = true;
    } else if (core1_display_mode == DisplayMode::Split && !activeWhisplayImageVisible()) {
      activePane = activePane == FocusPane::Prompt ? FocusPane::Reply : FocusPane::Prompt;
      resetScrollLoopState();
      uiDirty = true;
    }
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  randomSeed(micros());
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setCursor(16, 20);
  M5.Lcd.print("Core1 Display");
  core1_image_cache_ready = SPIFFS.begin(true);
  ensureCore1SdReady();

  loadSettings();
  if (!core1_has_settings) {
    runSetupPortal();
  }

  latestUserMessage = "Waiting for the first prompt.";
  latestReplyMessage = "Waiting for the first reply.";
  initScreensaverState();
  lastInteractionMs = millis();
  startWifiStation();
  drawUi();
}

void loop() {
  M5.update();
  handleButtons();
  ensureWifiConnected();
  if (core1_screensaver_mode == ScreensaverMode::AiSlideshow) {
    fetchCore1AiImages();
    prefetchAiImages();
  }
  updateScreensaverState();
  pollAutoScroll();

  if (WiFi.status() != lastDrawnWifiStatus) {
    uiDirty = true;
  }

  if (millis() - lastPollAttemptMs >= core1_poll_ms) {
    lastPollAttemptMs = millis();
    String previousUser = latestUserMessage;
    String previousReply = latestReplyMessage;
    String previousModelTag = latestModelTag;
    String previousStatus = latestStatus;
    String previousPersona = latestPersona;
    String previousError = latestPollError;
    unsigned long previousLastSuccess = lastSuccessfulPollMs;
    BackendSource previousActiveSource = activeBackendSource;
    uint32_t previousWhisplayImageRevision = whisplaySnapshot.imageRevision;
    bool previousWhisplayImageAvailable = whisplaySnapshot.imageAvailable;
    String error;
    fetchCompanionChat(error);
    bool whisplayImageChanged =
      previousWhisplayImageRevision != whisplaySnapshot.imageRevision ||
      previousWhisplayImageAvailable != whisplaySnapshot.imageAvailable;
    bool freshText =
      latestUserMessage != previousUser ||
      latestReplyMessage != previousReply;
    bool backendStateChanged =
      latestModelTag != previousModelTag ||
      latestStatus != previousStatus ||
      latestPersona != previousPersona;
    bool whisplayVisualChanged =
      whisplayImageChanged &&
      (activeBackendSource == BackendSource::Whisplay ||
       previousActiveSource == BackendSource::Whisplay);
    bool shouldWakeFromAiScreensaver =
      activeScreen == ScreenMode::Screensaver &&
      core1_screensaver_mode == ScreensaverMode::AiSlideshow &&
      (freshText || isActiveChatStatus(latestStatus));
    if (freshText || backendStateChanged || whisplayVisualChanged) {
      if (latestUserMessage != previousUser) {
        promptScrollOffset = 0;
      }
      if (latestReplyMessage != previousReply) {
        replyScrollOffset = 0;
      }
      activePane = FocusPane::Reply;
      if (shouldWakeFromAiScreensaver) {
        activeScreen = ScreenMode::Viewer;
        screenBlanked = false;
        core1_ai_dirty = true;
      }
      resetScrollLoopState();
      uiDirty = true;
    }
    if (latestPollError != previousError || lastSuccessfulPollMs != previousLastSuccess) {
      footerDirty = true;
    }
  }

  long syncAgeSeconds = lastSuccessfulPollMs ? static_cast<long>((millis() - lastSuccessfulPollMs) / 1000UL) : -1;
  if (syncAgeSeconds != lastDrawnSyncAgeSeconds) {
    footerDirty = true;
    lastDrawnSyncAgeSeconds = syncAgeSeconds;
  }

  if (activeScreen == ScreenMode::Screensaver) {
    if (millis() - lastScreensaverFrameMs >= 60) {
      drawScreensaver();
      lastScreensaverFrameMs = millis();
    }
  } else if (uiDirty) {
    drawUi();
  } else if (footerDirty) {
    drawFooterArea();
  }
  delay(25);
}
