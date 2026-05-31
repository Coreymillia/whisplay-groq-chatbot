#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <JPEGDEC.h>
#include <XPT2046_Touchscreen.h>

#include "Portal.h"
#include "WhisplayApi.h"

#define GFX_BL 21
#define BOOT_BTN 0

Arduino_DataBus *bus = new Arduino_HWSPI(2, 15, 14, 13, 12);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 1);

#define XPT2046_IRQ 36
#define XPT2046_CS 33
#define XPT2046_CLK 25
#define XPT2046_MOSI 32
#define XPT2046_MISO 39

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
SPIClass sdSPI(HSPI);

#define SD_CS 5
#define SD_SCK 18
#define SD_MOSI 23
#define SD_MISO 19

static const uint16_t COLOR_BG = RGB565_BLACK;
static const uint16_t COLOR_PANEL = 0x0841;
static const uint16_t COLOR_HEADER = 0x0016;
static const uint16_t COLOR_HEADER_TEXT = 0x07FF;
static const uint16_t COLOR_TEXT = RGB565_WHITE;
static const uint16_t COLOR_DIM = 0x7BEF;
static const uint16_t COLOR_ACCENT = 0x07E0;
static const uint16_t COLOR_WARN = 0xFFE0;
static const uint16_t COLOR_ERROR = 0xF800;
static const uint16_t COLOR_ACTION = 0x01CF;
static const uint16_t COLOR_ACTION_ALT = 0x02A0;
static const uint16_t COLOR_CAPTURE = 0x4200;
static const uint16_t COLOR_SETTINGS = 0x780F;
static const uint16_t COLOR_MULTI[] = {
  RGB565_WHITE,
  RGB565_GREEN,
  RGB565_CYAN,
  RGB565_YELLOW,
  RGB565_MAGENTA,
  RGB565_BLUE,
};

static constexpr unsigned long TOUCH_DEBOUNCE_MS = 220;
static constexpr unsigned long STATE_POLL_MS = 900;
static constexpr unsigned long SETTINGS_POLL_MS = 6000;
static constexpr unsigned long PHOTOS_POLL_MS = 12000;
static constexpr unsigned long AI_PHOTOS_POLL_MS = 45000;
static constexpr unsigned long AI_SYNC_STEP_MS = 1200;
static constexpr unsigned long AI_SLIDE_ADVANCE_MS = 30000;
static constexpr unsigned long AI_CHAT_PEEK_MS = 15000;
static constexpr unsigned long PHOTO_REFRESH_BOOST_MS = 8000;
static constexpr unsigned long PHOTO_REFRESH_BOOST_POLL_MS = 1500;
static constexpr unsigned long BOOT_PORTAL_HOLD_MS = 3000;
static constexpr size_t MAX_LOG_ENTRIES = 32;
static constexpr size_t MAX_RENDER_LINES = 96;
static constexpr char PREVIEW_CACHE_PATH[] = "/preview.jpg";
static constexpr char AI_SLIDESHOW_DIR[] = "/whisplay-ai";
static constexpr char AI_SPIFFS_CACHE_PREFIX[] = "/ai-slide-";
static constexpr char AI_SLIDE_DRAW_CACHE_PATH[] = "/ai-slide-current-v2.jpg";
static constexpr char AI_MANIFEST_PATH[] = "/whisplay-ai-index-v2.txt";
static constexpr char AI_MANIFEST_TEMP_PATH[] = "/whisplay-ai-index-v2.tmp";
static constexpr char AI_CACHE_VERSION[] = "v2";
static constexpr int AI_SLIDE_RENDER_WIDTH = 320;
static constexpr int AI_SLIDE_RENDER_HEIGHT = 240;
static constexpr size_t MAX_AI_REMOTE_IMAGES = 48;
static constexpr size_t MAX_AI_FALLBACK_IMAGES = 24;
static constexpr size_t AI_SYNC_DOWNLOADS_PER_STEP = 2;
static constexpr uint32_t SD_SPI_FREQUENCY = 4000000;
static constexpr unsigned long AI_SLIDE_HUD_MS = 1400;
static constexpr char AI_PREFS_NAMESPACE[] = "compcyd";
static constexpr char AI_PREF_KEY_FILE[] = "aislide";
static constexpr char AI_PREF_KEY_INDEX[] = "aiidx";

enum class UiMode : uint8_t {
  AiSlideshow = 0,
  Chat,
  Capture,
  Gallery,
  Settings,
};

enum ChatColorMode : uint8_t {
  ChatColorWhite = 0,
  ChatColorGreen,
  ChatColorCyan,
  ChatColorAmber,
  ChatColorPink,
  ChatColorPurple,
  ChatColorBlue,
  ChatColorMulti,
};

enum class SettingsItemId : uint8_t {
  Personality = 0,
  VoiceMode,
  Volume,
  ScrollSpeed,
  RecordTime,
  UiTheme,
  HeaderMode,
  ScreensaverMode,
  IdleTimeout,
  ScreenBlankTimeout,
  RoomMonitorInterval,
  CameraSource,
  MusicShuffle,
  ChatTextSize,
  ChatTextColor,
};

struct TouchButton {
  const char *label;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  uint16_t bg;
};

static const TouchButton buttonModePrev = { "<", 4, 24, 24, 20, COLOR_SETTINGS };
static const TouchButton buttonAiShow = { "AI SHOW", 32, 24, 64, 20, COLOR_CAPTURE };
static const TouchButton buttonNewChat = { "NEW", 100, 24, 48, 20, COLOR_ACTION_ALT };
static const TouchButton buttonRepeat = { "REPEAT", 152, 24, 60, 20, COLOR_ACTION };
static const TouchButton buttonModeNext = { ">", 216, 24, 24, 20, COLOR_SETTINGS };
static const TouchButton buttonSetup = { "SETUP", 246, 24, 70, 20, 0x5008 };
static const TouchButton buttonCaptureAction = { "CAPTURE", 92, 188, 136, 28, COLOR_CAPTURE };
static const TouchButton buttonGalleryPrev = { "PREV", 18, 188, 84, 28, COLOR_ACTION };
static const TouchButton buttonGalleryNext = { "NEXT", 218, 188, 84, 28, COLOR_ACTION_ALT };
static const TouchButton buttonGalleryLatest = { "LATEST", 114, 188, 92, 28, COLOR_CAPTURE };
static const TouchButton buttonSettingsPrevItem = { "ITEM -", 12, 164, 142, 24, COLOR_ACTION };
static const TouchButton buttonSettingsNextItem = { "ITEM +", 166, 164, 142, 24, COLOR_ACTION_ALT };
static const TouchButton buttonSettingsPrevValue = { "VALUE -", 12, 194, 142, 24, COLOR_SETTINGS };
static const TouchButton buttonSettingsNextValue = { "VALUE +", 166, 194, 142, 24, COLOR_CAPTURE };

static const SettingsItemId SETTINGS_ITEMS[] = {
  SettingsItemId::Personality,
  SettingsItemId::VoiceMode,
  SettingsItemId::Volume,
  SettingsItemId::ScrollSpeed,
  SettingsItemId::RecordTime,
  SettingsItemId::UiTheme,
  SettingsItemId::HeaderMode,
  SettingsItemId::ScreensaverMode,
  SettingsItemId::IdleTimeout,
  SettingsItemId::ScreenBlankTimeout,
  SettingsItemId::RoomMonitorInterval,
  SettingsItemId::CameraSource,
  SettingsItemId::MusicShuffle,
  SettingsItemId::ChatTextSize,
  SettingsItemId::ChatTextColor,
};

static const char *VOICE_MODE_OPTIONS[] = {
  "text-only",
  "speak-on-demand",
  "voice-chat",
};
static const char *UI_THEME_OPTIONS[] = {
  "default",
  "matrix",
  "plasma",
  "amber-terminal",
};
static const char *HEADER_MODE_OPTIONS[] = {
  "emoji",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
  "vu-bars",
  "vu-scope",
  "vu-wave",
};
static const char *SCREENSAVER_MODE_OPTIONS[] = {
  "off",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
};
static const char *CAMERA_SOURCE_OPTIONS[] = {
  "pi-camera",
  "esp32-cam",
};
static const int RECORD_TIME_OPTIONS[] = {10, 15, 20, 30, 45, 60};
static const int IDLE_TIMEOUT_OPTIONS[] = {0, 60, 120, 180, 240, 300, 360, 420, 480, 540, 600};
static const int ROOM_MONITOR_INTERVAL_OPTIONS[] = {0, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600};
static const char *CHAT_COLOR_LABELS[] = {
  "White",
  "Green",
  "Cyan",
  "Amber",
  "Pink",
  "Purple",
  "Blue",
  "Multi",
};

static CompanionState companionState;
static CompanionSettings companionSettings;
static CompanionPhotoLibrary photoLibrary;
static CompanionPhotoLibrary aiPhotoLibrary;
static CompanionPhotoLibrary aiSyncPage;
static JPEGDEC previewJpeg;
static String toastMessage;
static String lastLoggedBotText;
static String previewFileName;
static String previewError;
static String aiSlideFileName;
static String aiSlideError;
static String conversationLog[MAX_LOG_ENTRIES];
static size_t conversationLogCount = 0;
static bool renderDirty = true;
static bool touchWasDown = false;
static bool previewReady = false;
static bool sdReady = false;
static String sdStatusMessage = "SD not initialized.";
static unsigned long lastTouchMs = 0;
static unsigned long lastStatePollMs = 0;
static unsigned long lastSettingsPollMs = 0;
static unsigned long lastPhotoPollMs = 0;
static unsigned long lastAiPhotoPollMs = 0;
static unsigned long lastAiSlideAdvanceMs = 0;
static unsigned long lastBootHoldStartMs = 0;
static unsigned long toastUntilMs = 0;
static unsigned long lastSuccessfulPollMs = 0;
static unsigned long photoRefreshBoostUntilMs = 0;
static unsigned long autoChatUntilMs = 0;
static bool autoChatActive = false;
static UiMode uiMode = UiMode::AiSlideshow;
static size_t galleryIndex = 0;
static size_t aiSlideIndex = 0;
static size_t selectedSettingsIndex = 0;
static String persistedAiSlideFileName;
static bool aiSlidePrefsLoaded = false;
static unsigned long aiSlideHudUntilMs = 0;
static size_t aiSdManifestCount = 0;
static size_t aiSdCacheFileCount = 0;
static size_t aiSyncOffset = 0;
static bool aiSyncHasMore = false;
static bool aiSyncInProgress = false;
static String aiSyncedRevision;

static void mapTouch(uint16_t rawX, uint16_t rawY, int &screenX, int &screenY) {
  screenX = map(rawX, 200, 3800, 0, 320);
  screenY = map(rawY, 200, 3800, 0, 240);
  screenX = constrain(screenX, 0, 319);
  screenY = constrain(screenY, 0, 239);
}

static const char *uiModeLabel(UiMode mode) {
  switch (mode) {
    case UiMode::AiSlideshow:
      return "AI SHOW";
    case UiMode::Chat:
      return "CHAT";
    case UiMode::Capture:
      return "CAPTURE";
    case UiMode::Gallery:
      return "GALLERY";
    case UiMode::Settings:
      return "SETTINGS";
    default:
      return "CHAT";
  }
}

static const char *voiceModeLabel(const String &mode) {
  if (mode == "voice-chat") {
    return "Voice";
  }
  if (mode == "speak-on-demand") {
    return "On demand";
  }
  return "Text";
}

static uint16_t voiceModeColor(const String &mode) {
  if (mode == "voice-chat") {
    return COLOR_ACCENT;
  }
  if (mode == "speak-on-demand") {
    return COLOR_WARN;
  }
  return COLOR_HEADER_TEXT;
}

static int chatTextScale() {
  return constrain(static_cast<int>(cc_chat_text_scale), 1, 3);
}

static int scaledCharWidth() {
  return 6 * chatTextScale();
}

static int scaledLineHeight() {
  return (8 * chatTextScale()) + 2;
}

static size_t arrayLength(const char *const *items, size_t sizeBytes) {
  return sizeBytes / sizeof(items[0]);
}

static String clampLogText(const String &value, size_t maxChars = 700) {
  if (value.length() <= maxChars) {
    return value;
  }
  return value.substring(value.length() - maxChars);
}

static void clearConversationLog() {
  for (size_t i = 0; i < conversationLogCount; i++) {
    conversationLog[i] = "";
  }
  conversationLogCount = 0;
  lastLoggedBotText = "";
}

static void appendLogEntry(const String &prefix, const String &text, bool replaceLast = false) {
  String normalized = text;
  normalized.trim();
  if (!normalized.length()) {
    return;
  }

  String entry = prefix + clampLogText(normalized);
  if (replaceLast && conversationLogCount > 0) {
    conversationLog[conversationLogCount - 1] = entry;
  } else {
    if (conversationLogCount < MAX_LOG_ENTRIES) {
      conversationLog[conversationLogCount++] = entry;
    } else {
      for (size_t i = 1; i < MAX_LOG_ENTRIES; i++) {
        conversationLog[i - 1] = conversationLog[i];
      }
      conversationLog[MAX_LOG_ENTRIES - 1] = entry;
    }
  }
  renderDirty = true;
}

static void syncIncomingLogText(const String &text) {
  String normalized = text;
  normalized.trim();
  if (!normalized.length()) {
    return;
  }

  if (
    lastLoggedBotText.length() &&
    normalized.startsWith(lastLoggedBotText) &&
    conversationLogCount > 0 &&
    conversationLog[conversationLogCount - 1].startsWith("BOT ")
  ) {
    conversationLog[conversationLogCount - 1] = "BOT " + clampLogText(normalized);
  } else if (normalized != lastLoggedBotText) {
    appendLogEntry("BOT ", normalized);
  }
  lastLoggedBotText = normalized;
  renderDirty = true;
}

static void setToast(const String &message, uint16_t durationMs = 2200) {
  toastMessage = message;
  toastUntilMs = millis() + durationMs;
  renderDirty = true;
}

static void drawButton(const TouchButton &button) {
  gfx->fillRoundRect(button.x, button.y, button.w, button.h, 6, button.bg);
  gfx->drawRoundRect(button.x, button.y, button.w, button.h, 6, COLOR_HEADER_TEXT);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_TEXT, button.bg);
  int textX = button.x + (button.w - (strlen(button.label) * 6)) / 2;
  int textY = button.y + (button.h - 8) / 2;
  gfx->setCursor(textX, textY);
  gfx->print(button.label);
}

static bool pointInButton(int x, int y, const TouchButton &button) {
  return x >= button.x && x <= (button.x + button.w) && y >= button.y && y <= (button.y + button.h);
}

static void fillWrappedLines(
  const String &sourceText,
  String *lines,
  int &lineCount,
  int maxLines,
  int maxChars
) {
  lineCount = 0;
  String source = sourceText;
  source.replace("\r", "");
  if (source.length() > 1200) {
    source = source.substring(source.length() - 1200);
  }

  String currentLine;
  for (size_t i = 0; i < source.length(); i++) {
    char c = source.charAt(i);
    if (c == '\n') {
      if (lineCount < maxLines) {
        lines[lineCount++] = currentLine;
      }
      currentLine = "";
      continue;
    }

    currentLine += c;
    if (currentLine.length() >= static_cast<size_t>(maxChars)) {
      int breakPos = currentLine.lastIndexOf(' ');
      if (breakPos > maxChars / 2) {
        if (lineCount < maxLines) {
          lines[lineCount++] = currentLine.substring(0, breakPos);
        }
        currentLine = currentLine.substring(breakPos + 1);
      } else {
        if (lineCount < maxLines) {
          lines[lineCount++] = currentLine;
        }
        currentLine = "";
      }
    }
  }

  if (currentLine.length() > 0 && lineCount < maxLines) {
    lines[lineCount++] = currentLine;
  }
}

static uint16_t currentChatColor() {
  switch (cc_chat_color_mode) {
    case ChatColorGreen:
      return RGB565_GREEN;
    case ChatColorCyan:
      return RGB565_CYAN;
    case ChatColorAmber:
      return RGB565_YELLOW;
    case ChatColorPink:
      return RGB565_MAGENTA;
    case ChatColorPurple:
      return RGB565_PURPLE;
    case ChatColorBlue:
      return RGB565_BLUE;
    case ChatColorMulti:
      return RGB565_WHITE;
    case ChatColorWhite:
    default:
      return RGB565_WHITE;
  }
}

static void buildConversationLines(String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  if (conversationLogCount == 0) {
    fillWrappedLines("Waiting for Whisplay text...", lines, lineCount, maxLines, maxChars);
    return;
  }

  for (size_t i = 0; i < conversationLogCount && lineCount < maxLines; i++) {
    int wrappedCount = 0;
    fillWrappedLines(conversationLog[i], lines + lineCount, wrappedCount, maxLines - lineCount, maxChars);
    lineCount += wrappedCount;
    if (lineCount < maxLines && i + 1 < conversationLogCount) {
      lines[lineCount++] = "";
    }
  }
}

static void drawConversationLog(int x, int y, int w, int h) {
  const int maxChars = max(8, (w / scaledCharWidth()) - 1);
  const int visibleLines = max(1, h / scaledLineHeight());
  String lines[MAX_RENDER_LINES];
  int lineCount = 0;
  buildConversationLines(lines, lineCount, MAX_RENDER_LINES, maxChars);
  int startLine = max(0, lineCount - visibleLines);

  gfx->setTextSize(chatTextScale());
  int cursorY = y;
  for (int i = startLine; i < lineCount; i++) {
    uint16_t lineColor = currentChatColor();
    if (cc_chat_color_mode == ChatColorMulti) {
      lineColor = COLOR_MULTI[i % (sizeof(COLOR_MULTI) / sizeof(COLOR_MULTI[0]))];
    }
    gfx->setTextColor(lineColor, COLOR_PANEL);
    gfx->setCursor(x, cursorY);
    gfx->print(lines[i]);
    cursorY += scaledLineHeight();
  }
  gfx->setTextSize(1);
}

static String trimTail(const String &value, size_t maxChars) {
  if (value.length() <= maxChars) {
    return value;
  }
  return value.substring(value.length() - maxChars);
}

static String compactDurationLabel(int seconds) {
  if (seconds <= 0) {
    return "Off";
  }
  if (seconds >= 3600) {
    return String(seconds / 3600) + "h";
  }
  if (seconds >= 60) {
    return String(seconds / 60) + "m";
  }
  return String(seconds) + "s";
}

static String compactThemeLabel(const String &value) {
  if (value == "amber-terminal") {
    return "Amber";
  }
  if (value == "matrix") {
    return "Matrix";
  }
  if (value == "plasma") {
    return "Plasma";
  }
  return "Default";
}

static String compactHeaderLabel(const String &value) {
  if (value == "matrix-binary") {
    return "Binary";
  }
  if (value == "matrix-blue") {
    return "Blue";
  }
  if (value == "retro-geometry") {
    return "Retro";
  }
  if (value == "plasma") {
    return "Plasma";
  }
  if (value == "neon-rain") {
    return "Neon";
  }
  if (value == "vu-bars") {
    return "VU Bars";
  }
  if (value == "vu-scope") {
    return "VU Scope";
  }
  if (value == "vu-wave") {
    return "VU Wave";
  }
  if (value == "matrix") {
    return "Matrix";
  }
  return "Emoji";
}

static String compactScreensaverLabel(const String &value) {
  if (value == "matrix-binary") {
    return "Binary";
  }
  if (value == "matrix-blue") {
    return "Blue";
  }
  if (value == "retro-geometry") {
    return "Retro";
  }
  if (value == "plasma") {
    return "Plasma";
  }
  if (value == "neon-rain") {
    return "Neon";
  }
  if (value == "matrix") {
    return "Matrix";
  }
  return "Off";
}

static String compactCameraSourceLabel(const String &value) {
  return value == "esp32-cam" ? "ESP32-CAM" : "Pi Camera";
}

static int findStringOptionIndex(const String &current, const char *const *options, size_t optionCount) {
  for (size_t i = 0; i < optionCount; i++) {
    if (current == options[i]) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

static int findIntOptionIndex(int current, const int *options, size_t optionCount) {
  for (size_t i = 0; i < optionCount; i++) {
    if (current == options[i]) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

static size_t currentPresetIndex() {
  for (size_t i = 0; i < companionSettings.presetCount; i++) {
    if (companionSettings.presets[i].id == companionSettings.personalityPresetId) {
      return i;
    }
  }
  return 0;
}

static String currentPresetLabel() {
  for (size_t i = 0; i < companionSettings.presetCount; i++) {
    if (companionSettings.presets[i].id == companionSettings.personalityPresetId) {
      return companionSettings.presets[i].label.length()
        ? companionSettings.presets[i].label
        : companionSettings.presets[i].id;
    }
  }
  return companionSettings.personalityPresetId.length()
    ? companionSettings.personalityPresetId
    : "Custom";
}

static const CompanionPhoto *selectedPhoto() {
  if (photoLibrary.count == 0) {
    return nullptr;
  }
  if (uiMode == UiMode::Gallery) {
    if (galleryIndex >= photoLibrary.count) {
      galleryIndex = photoLibrary.count - 1;
    }
    return &photoLibrary.photos[galleryIndex];
  }
  return &photoLibrary.photos[0];
}

static const CompanionPhoto *selectedAiPhoto() {
  if (aiPhotoLibrary.count == 0) {
    return nullptr;
  }
  if (aiSlideIndex >= aiPhotoLibrary.count) {
    aiSlideIndex = 0;
  }
  return &aiPhotoLibrary.photos[aiSlideIndex];
}

static size_t aiSlideTotalCount() {
  if (sdReady) {
    return aiSdCacheFileCount;
  }
  return aiPhotoLibrary.count;
}

static size_t countAiManifestEntries() {
  if (!sdReady || !SD.exists(AI_MANIFEST_PATH)) {
    return 0;
  }
  File manifest = SD.open(AI_MANIFEST_PATH, "r");
  if (!manifest) {
    return 0;
  }
  size_t count = 0;
  while (manifest.available()) {
    String line = manifest.readStringUntil('\n');
    line.trim();
    if (line.length()) {
      count++;
    }
  }
  manifest.close();
  return count;
}

static size_t countAiCachedFilesOnSd() {
  if (!sdReady) {
    return 0;
  }
  File dir = SD.open(AI_SLIDESHOW_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return 0;
  }
  size_t count = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    if (!entry.isDirectory()) {
      String name = String(entry.name());
      name.toLowerCase();
      if (name.endsWith(".jpg") || name.endsWith(".jpeg")) {
        count++;
      }
    }
    entry.close();
  }
  dir.close();
  return count;
}

static String aiCachePathForFileName(const String &fileName, bool useSdCache);
static String aiCacheLeafName(const String &path);

static String aiSdCachedPathAt(size_t index) {
  if (!sdReady) {
    return "";
  }
  File dir = SD.open(AI_SLIDESHOW_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return "";
  }
  size_t current = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    if (!entry.isDirectory()) {
      const String name = String(entry.name());
      const String leaf = aiCacheLeafName(name);
      String normalized = leaf;
      normalized.toLowerCase();
      if (normalized.endsWith(".jpg") || normalized.endsWith(".jpeg")) {
        if (current == index) {
          entry.close();
          dir.close();
          return String(AI_SLIDESHOW_DIR) + "/" + leaf;
        }
        current++;
      }
    }
    entry.close();
  }
  dir.close();
  return "";
}

static bool aiManifestContainsFileName(const String &fileName) {
  if (!sdReady || !fileName.length() || !SD.exists(AI_MANIFEST_PATH)) {
    return false;
  }
  File manifest = SD.open(AI_MANIFEST_PATH, "r");
  if (!manifest) {
    return false;
  }
  while (manifest.available()) {
    String line = manifest.readStringUntil('\n');
    line.trim();
    if (line == fileName) {
      manifest.close();
      return true;
    }
  }
  manifest.close();
  return false;
}

static bool appendAiManifestFileName(const String &fileName) {
  if (!sdReady || !fileName.length()) {
    return false;
  }
  if (aiManifestContainsFileName(fileName)) {
    return true;
  }
  File manifest = SD.open(AI_MANIFEST_PATH, SD.exists(AI_MANIFEST_PATH) ? "a" : "w");
  if (!manifest) {
    aiSlideError = "SD manifest append";
    return false;
  }
  manifest.println(fileName);
  manifest.close();
  aiSdManifestCount++;
  aiSdCacheFileCount = max(aiSdCacheFileCount, aiSdManifestCount);
  return true;
}

static String aiManifestFileNameAt(size_t index) {
  if (!sdReady || !SD.exists(AI_MANIFEST_PATH)) {
    return "";
  }
  File manifest = SD.open(AI_MANIFEST_PATH, "r");
  if (!manifest) {
    return "";
  }
  size_t current = 0;
  String value;
  while (manifest.available()) {
    value = manifest.readStringUntil('\n');
    value.trim();
    if (!value.length()) {
      continue;
    }
    if (current == index) {
      manifest.close();
      return value;
    }
    current++;
  }
  manifest.close();
  return "";
}

static String selectedAiFileName() {
  if (sdReady) {
    const size_t total = aiSlideTotalCount();
    if (total == 0) {
      aiSlideIndex = 0;
      return "";
    }
    if (aiSlideIndex >= total) {
      aiSlideIndex = 0;
    }
    return aiCacheLeafName(aiSdCachedPathAt(aiSlideIndex));
  }
  const CompanionPhoto *photo = selectedAiPhoto();
  return photo ? photo->fileName : "";
}

static String selectedAiSdCachePath() {
  if (!sdReady) {
    return "";
  }
  const size_t total = aiSlideTotalCount();
  if (total == 0) {
    return "";
  }
  if (aiSlideIndex >= total) {
    aiSlideIndex = 0;
  }
  return aiSdCachedPathAt(aiSlideIndex);
}

static bool photoLibrariesEqual(const CompanionPhotoLibrary &a, const CompanionPhotoLibrary &b) {
  if (a.count != b.count) {
    return false;
  }
  for (size_t i = 0; i < a.count; i++) {
    if (
      a.photos[i].fileName != b.photos[i].fileName ||
      a.photos[i].imageUrl != b.photos[i].imageUrl ||
      a.photos[i].companionImageUrl != b.photos[i].companionImageUrl
    ) {
      return false;
    }
  }
  return true;
}

static bool isJpegFileName(const String &fileName) {
  String normalized = fileName;
  normalized.toLowerCase();
  return normalized.endsWith(".jpg") || normalized.endsWith(".jpeg");
}

static void clearPreviewCache() {
  previewReady = false;
  previewFileName = "";
  previewError = "";
  SPIFFS.remove(PREVIEW_CACHE_PATH);
}

static void clearAiSlideCacheState() {
  aiSlideFileName = "";
  aiSlideError = "";
}

static void loadAiSlidePrefs() {
  if (aiSlidePrefsLoaded) {
    return;
  }
  Preferences prefs;
  prefs.begin(AI_PREFS_NAMESPACE, true);
  persistedAiSlideFileName = prefs.getString(AI_PREF_KEY_FILE, "");
  aiSlideIndex = static_cast<size_t>(prefs.getUInt(AI_PREF_KEY_INDEX, 0));
  prefs.end();
  aiSlidePrefsLoaded = true;
}

static void saveAiSlidePrefs() {
  const String fileName = selectedAiFileName();
  if (!fileName.length()) {
    return;
  }
  if (persistedAiSlideFileName == fileName) {
    return;
  }
  persistedAiSlideFileName = fileName;
  Preferences prefs;
  prefs.begin(AI_PREFS_NAMESPACE, false);
  prefs.putString(AI_PREF_KEY_FILE, persistedAiSlideFileName);
  prefs.putUInt(AI_PREF_KEY_INDEX, static_cast<uint32_t>(aiSlideIndex));
  prefs.end();
}

static bool restoreAiSlideSelection(const String &preferredFileName) {
  if (sdReady) {
    const size_t total = aiSlideTotalCount();
    if (total == 0) {
      aiSlideIndex = 0;
      return false;
    }
    if (preferredFileName.length()) {
      for (size_t current = 0; current < total; current++) {
        if (aiCacheLeafName(aiSdCachedPathAt(current)) == preferredFileName) {
          aiSlideIndex = current;
          return true;
        }
      }
    }
    if (aiSlideIndex >= total) {
      aiSlideIndex = 0;
    }
    return false;
  }
  if (aiPhotoLibrary.count == 0) {
    aiSlideIndex = 0;
    return false;
  }
  if (preferredFileName.length()) {
    for (size_t i = 0; i < aiPhotoLibrary.count; i++) {
      if (aiPhotoLibrary.photos[i].fileName == preferredFileName) {
        aiSlideIndex = i;
        return true;
      }
    }
  }
  if (aiSlideIndex >= aiPhotoLibrary.count) {
    aiSlideIndex = 0;
  }
  return false;
}

static String aiSlideCountLabel() {
  const size_t total = aiSlideTotalCount();
  if (total == 0) {
    return "0/0";
  }
  return String(aiSlideIndex + 1) + "/" + String(total);
}

static String aiSyncStatusLabel() {
  if (!sdReady) {
    return sdStatusMessage.length() ? sdStatusMessage : "SD not ready.";
  }
  String label = String("SD files: ") + String(aiSdCacheFileCount);
  if (aiSyncInProgress) {
    label += " | syncing more...";
  } else {
    label += " | sync idle";
  }
  if (aiSlideError.length()) {
    label += " | ";
    label += trimTail(aiSlideError, 18);
  }
  return label;
}

static void showAiSlideHud() {
  aiSlideHudUntilMs = millis() + AI_SLIDE_HUD_MS;
}

static bool fetchAiPhotosIfDue(bool force = false);

static bool ensureSdCardReady() {
  if (sdReady) {
    return true;
  }
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI);
  if (!sdReady) {
    sdStatusMessage = "SD mount failed on HSPI.";
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    sdReady = false;
    sdStatusMessage = "SD card not detected.";
    return false;
  }
  sdStatusMessage = "SD ready.";
  SD.mkdir(AI_SLIDESHOW_DIR);
  aiSdCacheFileCount = countAiCachedFilesOnSd();
  return sdReady;
}

static String aiCacheKeyForFileName(const String &fileName) {
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
  safe += AI_CACHE_VERSION;
  return safe;
}

static String aiCachePathForFileName(const String &fileName, bool useSdCache) {
  const String safe = aiCacheKeyForFileName(fileName);
  if (useSdCache) {
    return String(AI_SLIDESHOW_DIR) + "/" + safe + ".jpg";
  }
  return String(AI_SPIFFS_CACHE_PREFIX) + safe + ".jpg";
}

static String aiCacheLeafName(const String &path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

static size_t aiFetchLimit() {
  return MAX_AI_FALLBACK_IMAGES;
}

static bool aiCacheExists(const String &localPath, bool useSdCache) {
  return useSdCache ? SD.exists(localPath) : SPIFFS.exists(localPath);
}

static bool aiDownloadToCache(const String &remotePath, const String &localPath, bool useSdCache) {
  if (!useSdCache && localPath != AI_SLIDE_DRAW_CACHE_PATH) {
    return false;
  }
  return useSdCache
    ? apiDownloadFile(remotePath, SD, localPath.c_str())
    : apiDownloadFile(remotePath, SPIFFS, localPath.c_str());
}

static bool copySdFileToSpiffs(const String &sourcePath, const char *targetPath) {
  File source = SD.open(sourcePath, "r");
  if (!source) {
    return false;
  }
  SPIFFS.remove(targetPath);
  File target = SPIFFS.open(targetPath, "w");
  if (!target) {
    source.close();
    return false;
  }

  uint8_t buffer[1024];
  while (true) {
    size_t readLen = source.read(buffer, sizeof(buffer));
    if (readLen == 0) {
      break;
    }
    if (target.write(buffer, readLen) != readLen) {
      target.close();
      source.close();
      SPIFFS.remove(targetPath);
      return false;
    }
  }

  target.close();
  source.close();
  return true;
}

static bool copySpiffsFileToSd(const char *sourcePath, const String &targetPath) {
  File source = SPIFFS.open(sourcePath, "r");
  if (!source) {
    return false;
  }
  SD.remove(targetPath);
  File target = SD.open(targetPath, "w");
  if (!target) {
    source.close();
    return false;
  }

  uint8_t buffer[1024];
  while (true) {
    size_t readLen = source.read(buffer, sizeof(buffer));
    if (readLen == 0) {
      break;
    }
    if (target.write(buffer, readLen) != readLen) {
      target.close();
      source.close();
      SD.remove(targetPath);
      return false;
    }
  }

  target.close();
  source.close();
  return true;
}

static String aiRemotePathForPhoto(const CompanionPhoto &photo) {
  if (photo.companionImageUrl.length()) {
    return photo.companionImageUrl +
      "?width=" + String(AI_SLIDE_RENDER_WIDTH) +
      "&height=" + String(AI_SLIDE_RENDER_HEIGHT);
  }
  return photo.imageUrl;
}

static bool fileLooksLikeJpeg(fs::FS &fileSystem, const char *path) {
  File file = fileSystem.open(path, "r");
  if (!file) {
    return false;
  }
  size_t size = file.size();
  if (size < 4) {
    file.close();
    return false;
  }

  uint8_t start[2] = {0, 0};
  uint8_t end[2] = {0, 0};
  bool ok = file.read(start, sizeof(start)) == static_cast<int>(sizeof(start));
  if (ok) {
    ok = file.seek(size - 2);
  }
  if (ok) {
    ok = file.read(end, sizeof(end)) == static_cast<int>(sizeof(end));
  }
  file.close();
  return ok && start[0] == 0xFF && start[1] == 0xD8 && end[0] == 0xFF && end[1] == 0xD9;
}

static String fileSizeLabel(fs::FS &fileSystem, const char *path) {
  File file = fileSystem.open(path, "r");
  if (!file) {
    return "0b";
  }
  const size_t size = file.size();
  file.close();
  return String(size) + "b";
}

static bool aiLibraryContainsFile(const String &fileName) {
  for (size_t i = 0; i < aiPhotoLibrary.count; i++) {
    if (aiPhotoLibrary.photos[i].fileName == fileName) {
      return true;
    }
  }
  return false;
}

static bool ensureCurrentAiSlideCachedFromSd(const String &fileName) {
  const String archivePath = selectedAiSdCachePath();
  if (!archivePath.length()) {
    const String fallbackPath = aiCachePathForFileName(fileName, true);
    if (fallbackPath.length() && SD.exists(fallbackPath)) {
      if (copySdFileToSpiffs(fallbackPath, AI_SLIDE_DRAW_CACHE_PATH) && fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)) {
        return true;
      }
    }
    aiSlideError = "SD slide path";
    return false;
  }
  if (SD.exists(archivePath) && fileLooksLikeJpeg(SD, archivePath.c_str())) {
    if (copySdFileToSpiffs(archivePath, AI_SLIDE_DRAW_CACHE_PATH) && fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)) {
      return true;
    }
    aiSlideError = "SD copy fail";
    SPIFFS.remove(AI_SLIDE_DRAW_CACHE_PATH);
    return false;
  }
  if (SD.exists(archivePath)) {
    aiSlideError = "SD bad jpg " + fileSizeLabel(SD, archivePath.c_str());
    SD.remove(archivePath);
  } else {
    aiSlideError = "SD slide missing";
  }
  return false;
}

static File previewJpegFile;

static int32_t previewFileRead(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
  (void)handle;
  return static_cast<int32_t>(previewJpegFile.read(buffer, length));
}

static int32_t previewFileSeek(JPEGFILE *handle, int32_t position) {
  (void)handle;
  return previewJpegFile.seek(position) ? position : 0;
}

static void previewFileClose(void *handle) {
  (void)handle;
  previewJpegFile.close();
}

static int jpegDrawCallback(JPEGDRAW *draw) {
  gfx->draw16bitBeRGBBitmap(draw->x, draw->y, draw->pPixels, draw->iWidth, draw->iHeight);
  return 1;
}

static int jpegScaleOptionForBounds(int width, int height, int maxW, int maxH, int &scaledW, int &scaledH) {
  int option = 0;
  scaledW = width;
  scaledH = height;

  if (scaledW > maxW || scaledH > maxH) {
    option = JPEG_SCALE_HALF;
    scaledW = max(1, width / 2);
    scaledH = max(1, height / 2);
  }
  if (scaledW > maxW || scaledH > maxH) {
    option = JPEG_SCALE_QUARTER;
    scaledW = max(1, width / 4);
    scaledH = max(1, height / 4);
  }
  if (scaledW > maxW || scaledH > maxH) {
    option = JPEG_SCALE_EIGHTH;
    scaledW = max(1, width / 8);
    scaledH = max(1, height / 8);
  }
  return option;
}

static bool drawJpegFromFs(
  fs::FS &fileSystem,
  const char *localPath,
  int x,
  int y,
  int w,
  int h,
  String *errorOut = nullptr
) {
  previewJpegFile = fileSystem.open(localPath, "r");
  if (!previewJpegFile) {
    if (errorOut) {
      *errorOut = "Open fail";
    }
    return false;
  }

  int openResult = previewJpeg.open(
    static_cast<void *>(&previewJpegFile),
    previewJpegFile.size(),
    previewFileClose,
    previewFileRead,
    previewFileSeek,
    jpegDrawCallback
  );
  if (!openResult) {
    const int jpegError = previewJpeg.getLastError();
    previewJpegFile.close();
    if (errorOut) {
      *errorOut = "JPEG open " + String(jpegError);
    }
    return false;
  }

  previewJpeg.setPixelType(RGB565_BIG_ENDIAN);
  int scaledW = previewJpeg.getWidth();
  int scaledH = previewJpeg.getHeight();
  int decodeOption = jpegScaleOptionForBounds(
    previewJpeg.getWidth(),
    previewJpeg.getHeight(),
    w,
    h,
    scaledW,
    scaledH
  );

  int drawX = x + max(0, (w - scaledW) / 2);
  int drawY = y + max(0, (h - scaledH) / 2);
  int decodeResult = previewJpeg.decode(drawX, drawY, decodeOption);
  bool ok = decodeResult != 0;
  previewJpeg.close();
  if (!ok && errorOut) {
    *errorOut = "JPEG decode " + String(previewJpeg.getLastError());
  }
  return ok;
}

static bool drawPreviewImage(int x, int y, int w, int h) {
  bool ok = drawJpegFromFs(SPIFFS, PREVIEW_CACHE_PATH, x, y, w, h);
  if (!ok && !previewError.length()) {
    previewError = "Preview open failed.";
  }
  return ok;
}

static bool ensurePreviewForSelection() {
  const CompanionPhoto *photo = selectedPhoto();
  if (!photo) {
    clearPreviewCache();
    previewError = "No captures yet.";
    return false;
  }
  if (previewReady && previewFileName == photo->fileName) {
    return true;
  }
  if (!isJpegFileName(photo->fileName)) {
    previewReady = false;
    previewFileName = photo->fileName;
    previewError = "JPEG preview only.";
    return false;
  }
  if (!ccEnsureWifiConnected()) {
    previewReady = false;
    previewError = "WiFi offline.";
    return false;
  }
  bool ok = apiDownloadFile(photo->imageUrl, SPIFFS, PREVIEW_CACHE_PATH);
  previewReady = ok;
  previewFileName = photo->fileName;
  previewError = ok ? "" : "Preview download failed.";
  return ok;
}

static void pruneAiSlideshowCache(bool useSdCache) {
  if (useSdCache) {
    return;
  }
  File dir = useSdCache ? SD.open(AI_SLIDESHOW_DIR) : SPIFFS.open("/");
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return;
  }
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    String entryName = String(entry.name());
    entry.close();
    int slash = entryName.lastIndexOf('/');
    if (slash >= 0) {
      entryName = entryName.substring(slash + 1);
    }
    if (!useSdCache && entryName == aiCacheLeafName(AI_SLIDE_DRAW_CACHE_PATH)) {
      continue;
    }
    if (!useSdCache && !entryName.startsWith(aiCacheLeafName(AI_SPIFFS_CACHE_PREFIX))) {
      continue;
    }
    bool keep = false;
    for (size_t i = 0; i < aiPhotoLibrary.count; i++) {
      String expected = aiCacheLeafName(aiCachePathForFileName(aiPhotoLibrary.photos[i].fileName, useSdCache));
      if (entryName == expected) {
        keep = true;
        break;
      }
    }
    if (!keep) {
      const String stalePath = useSdCache
        ? String(AI_SLIDESHOW_DIR) + "/" + entryName
        : String("/") + entryName;
      if (useSdCache) {
        SD.remove(stalePath);
      } else {
        SPIFFS.remove(stalePath);
      }
    }
  }
  dir.close();
}

static bool syncAiSlideshowCacheStep(bool restartSync) {
  const bool useSdCache = ensureSdCardReady();
  bool anyReady = aiSdCacheFileCount > 0;
  if (!useSdCache) {
    return SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH) && fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH);
  }
  if (restartSync) {
    aiSyncOffset = 0;
    aiSyncHasMore = true;
    aiSyncInProgress = true;
  }
  if (!aiSyncInProgress) {
    return anyReady;
  }
  CompanionPhotoLibrary &page = aiSyncPage;
  page.loaded = false;
  page.count = 0;
  page.lastUpdateMs = 0;
  bool hasMore = false;
  if (!apiFetchGeneratedImagesPage(page, aiSyncOffset, COMPANION_GENERATED_IMAGES_PAGE_SIZE, &hasMore, nullptr)) {
    return false;
  }
  if (page.count == 0) {
    aiSyncOffset = 0;
    aiSyncHasMore = false;
    aiSyncInProgress = false;
    if (companionState.generatedImagesRevision.length()) {
      aiSyncedRevision = companionState.generatedImagesRevision;
    }
    return anyReady;
  }

  size_t downloadBudget = AI_SYNC_DOWNLOADS_PER_STEP;
  bool pageComplete = true;
  for (size_t i = 0; i < page.count; i++) {
    const CompanionPhoto &photo = page.photos[i];
    if (!photo.fileName.length()) {
      continue;
    }
    const String localPath = aiCachePathForFileName(photo.fileName, true);
    bool valid = SD.exists(localPath) && fileLooksLikeJpeg(SD, localPath.c_str());
    if (SD.exists(localPath) && !valid) {
      SD.remove(localPath);
    }
    if (!valid && downloadBudget > 0) {
      const String remotePath = aiRemotePathForPhoto(photo);
      valid = remotePath.length() &&
        aiDownloadToCache(remotePath, localPath, true) &&
        fileLooksLikeJpeg(SD, localPath.c_str());
      if (valid) {
        downloadBudget--;
      }
    }
    if (valid) {
      anyReady = true;
    }
    if (!valid) {
      pageComplete = false;
    }
    yield();
  }
  aiSdCacheFileCount = countAiCachedFilesOnSd();

  if (pageComplete) {
    aiSyncOffset += page.count;
    aiSyncHasMore = hasMore;
  } else {
    aiSyncHasMore = true;
  }
  if (pageComplete && !aiSyncHasMore) {
    aiSyncOffset = 0;
    aiSyncInProgress = false;
    if (companionState.generatedImagesRevision.length()) {
      aiSyncedRevision = companionState.generatedImagesRevision;
    }
  }
  return anyReady;
}

static bool ensureCurrentAiSlideCached(const CompanionPhoto &photo) {
  const bool useSdCache = ensureSdCardReady();
  const String remotePath = aiRemotePathForPhoto(photo);
  if (!remotePath.length()) {
    aiSlideError = "AI image URL missing.";
    return false;
  }

  if (useSdCache) {
    const String archivePath = aiCachePathForFileName(photo.fileName, true);
    if (SD.exists(archivePath) && fileLooksLikeJpeg(SD, archivePath.c_str())) {
      if (copySdFileToSpiffs(archivePath, AI_SLIDE_DRAW_CACHE_PATH) && fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)) {
        return true;
      }
      aiSlideError = "SD copy fail";
      SPIFFS.remove(AI_SLIDE_DRAW_CACHE_PATH);
    } else if (SD.exists(archivePath)) {
      aiSlideError = "SD bad jpg " + fileSizeLabel(SD, archivePath.c_str());
      SD.remove(archivePath);
    }

    String downloadError;
    if (
      apiDownloadFile(remotePath, SPIFFS, AI_SLIDE_DRAW_CACHE_PATH, &downloadError) &&
      fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)
    ) {
      if (copySpiffsFileToSd(AI_SLIDE_DRAW_CACHE_PATH, archivePath) && fileLooksLikeJpeg(SD, archivePath.c_str())) {
        return true;
      }
      return true;
    }
    if (SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH) && !fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)) {
      aiSlideError = "Tmp bad dl " + fileSizeLabel(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH);
    } else if (SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH)) {
      aiSlideError = "Tmp dl bad " + fileSizeLabel(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH);
    } else {
      aiSlideError = downloadError.length() ? "Tmp " + downloadError : "Tmp dl fail";
    }
    SPIFFS.remove(AI_SLIDE_DRAW_CACHE_PATH);
    return false;
  }

  if (aiSlideFileName == photo.fileName && SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH) && fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)) {
    return true;
  } else if (SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH)) {
    aiSlideError = "Local bad jpg " + fileSizeLabel(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH);
    SPIFFS.remove(AI_SLIDE_DRAW_CACHE_PATH);
  }
  String downloadError;
  if (
    apiDownloadFile(remotePath, SPIFFS, AI_SLIDE_DRAW_CACHE_PATH, &downloadError) &&
    fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)
  ) {
    return true;
  }
  if (SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH) && !fileLooksLikeJpeg(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH)) {
    aiSlideError = "Local bad dl " + fileSizeLabel(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH);
  } else if (SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH)) {
    aiSlideError = "Local dl bad " + fileSizeLabel(SPIFFS, AI_SLIDE_DRAW_CACHE_PATH);
  } else {
    aiSlideError = downloadError.length() ? "Local " + downloadError : "Local dl fail";
  }
  SPIFFS.remove(AI_SLIDE_DRAW_CACHE_PATH);
  return false;
}

static bool ensureAiSlideReady() {
  const String fileName = selectedAiFileName();
  if (!fileName.length()) {
    clearAiSlideCacheState();
    aiSlideError = "No AI images yet.";
    return false;
  }
  const bool useSdCache = ensureSdCardReady();
  if (aiSlideFileName == fileName) {
    if (
      (useSdCache && SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH)) ||
      (!useSdCache && SPIFFS.exists(AI_SLIDE_DRAW_CACHE_PATH))
    ) {
      aiSlideError = "";
      return true;
    }
  }
  const CompanionPhoto *photo = useSdCache ? nullptr : selectedAiPhoto();
  const bool cached = useSdCache
    ? ensureCurrentAiSlideCachedFromSd(fileName)
    : (photo ? ensureCurrentAiSlideCached(*photo) : false);
  if (!cached) {
    aiSlideFileName = fileName;
    return false;
  }
  aiSlideFileName = fileName;
  aiSlideError = "";
  saveAiSlidePrefs();
  return true;
}

static void moveAiSlide(int delta, bool showHud = true) {
  const size_t total = aiSlideTotalCount();
  if (total == 0) {
    return;
  }
  int next = static_cast<int>(aiSlideIndex) + delta;
  while (next < 0) {
    next += static_cast<int>(total);
  }
  aiSlideIndex = static_cast<size_t>(next % static_cast<int>(total));
  lastAiSlideAdvanceMs = millis();
  clearAiSlideCacheState();
  saveAiSlidePrefs();
  if (showHud) {
    showAiSlideHud();
  }
  renderDirty = true;
}

static void advanceAiSlide(bool force = false) {
  if (aiSlideTotalCount() == 0) {
    return;
  }
  unsigned long now = millis();
  if (!force && now - lastAiSlideAdvanceMs < AI_SLIDE_ADVANCE_MS) {
    return;
  }
  moveAiSlide(1, true);
}

static void openSetupPortal() {
  gfx->fillScreen(COLOR_BG);
  gfx->setTextColor(COLOR_WARN, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(22, 96);
  gfx->print("Opening setup...");
  delay(600);
  ccRunPortal();
}

static void setUiMode(UiMode nextMode) {
  if (uiMode == nextMode) {
    return;
  }
  uiMode = nextMode;
  autoChatActive = false;
  autoChatUntilMs = 0;
  if (uiMode == UiMode::Capture) {
    galleryIndex = 0;
  }
  renderDirty = true;
}

static void stepUiMode(int delta) {
  int next = static_cast<int>(uiMode) + delta;
  if (next < 0) {
    next = static_cast<int>(UiMode::Settings);
  } else if (next > static_cast<int>(UiMode::Settings)) {
    next = static_cast<int>(UiMode::Chat);
  }
  setUiMode(static_cast<UiMode>(next));
}

static bool fetchStateIfDue() {
  if (!ccEnsureWifiConnected()) {
    return false;
  }
  unsigned long now = millis();
  if (now - lastStatePollMs < STATE_POLL_MS) {
    return false;
  }
  lastStatePollMs = now;

  CompanionState nextState;
  if (!apiFetchState(nextState)) {
    return false;
  }

  bool changed =
    nextState.ready != companionState.ready ||
    nextState.status != companionState.status ||
    nextState.text != companionState.text ||
    nextState.textInputEnabled != companionState.textInputEnabled ||
    nextState.ragIconVisible != companionState.ragIconVisible ||
    nextState.imageIconVisible != companionState.imageIconVisible ||
    nextState.generatedImagesRevision != companionState.generatedImagesRevision;

  bool freshText = nextState.text.length() && nextState.text != companionState.text;
  String previousGeneratedImagesRevision = companionState.generatedImagesRevision;
  if (freshText) {
    syncIncomingLogText(nextState.text);
  }

  companionState = nextState;
  lastSuccessfulPollMs = now;
  if (
    previousGeneratedImagesRevision.length() &&
    companionState.generatedImagesRevision.length() &&
    companionState.generatedImagesRevision != previousGeneratedImagesRevision
  ) {
    fetchAiPhotosIfDue(true);
  }
  bool activeChat = companionState.status.length() &&
    companionState.status != "idle" &&
    companionState.status != "last reply";
  if ((activeChat || freshText) && uiMode == UiMode::AiSlideshow) {
    uiMode = UiMode::Chat;
    autoChatActive = true;
    autoChatUntilMs = now + AI_CHAT_PEEK_MS;
    renderDirty = true;
  }
  if (changed) {
    renderDirty = true;
  }
  return true;
}

static bool fetchSettingsIfDue() {
  if (!ccEnsureWifiConnected()) {
    return false;
  }
  unsigned long now = millis();
  if (now - lastSettingsPollMs < SETTINGS_POLL_MS) {
    return false;
  }
  lastSettingsPollMs = now;

  CompanionSettings nextSettings;
  if (!apiFetchSettings(nextSettings)) {
    return false;
  }

  bool changed =
    !companionSettings.loaded ||
    nextSettings.voiceMode != companionSettings.voiceMode ||
    nextSettings.personalityPresetId != companionSettings.personalityPresetId ||
    nextSettings.musicShuffle != companionSettings.musicShuffle ||
    nextSettings.volumeLevel != companionSettings.volumeLevel ||
    nextSettings.scrollSpeedLevel != companionSettings.scrollSpeedLevel ||
    nextSettings.manualRecordMaxSec != companionSettings.manualRecordMaxSec ||
    nextSettings.uiTheme != companionSettings.uiTheme ||
    nextSettings.headerMode != companionSettings.headerMode ||
    nextSettings.screensaverMode != companionSettings.screensaverMode ||
    nextSettings.idleTimeoutSec != companionSettings.idleTimeoutSec ||
    nextSettings.screenBlankTimeoutSec != companionSettings.screenBlankTimeoutSec ||
    nextSettings.roomMonitorIntervalSec != companionSettings.roomMonitorIntervalSec ||
    nextSettings.cameraSource != companionSettings.cameraSource;

  companionSettings = nextSettings;
  if (changed) {
    renderDirty = true;
  }
  return true;
}

static bool fetchPhotosIfDue(bool force = false) {
  if (!ccEnsureWifiConnected()) {
    return false;
  }

  unsigned long now = millis();
  unsigned long interval = now < photoRefreshBoostUntilMs
    ? PHOTO_REFRESH_BOOST_POLL_MS
    : PHOTOS_POLL_MS;
  if (!force && now - lastPhotoPollMs < interval) {
    return false;
  }
  lastPhotoPollMs = now;

  String currentSelectionFile = selectedPhoto() ? selectedPhoto()->fileName : "";
  CompanionPhotoLibrary *nextPhotos = new CompanionPhotoLibrary();
  if (!nextPhotos) {
    return false;
  }
  if (!apiFetchPhotos(*nextPhotos)) {
    delete nextPhotos;
    return false;
  }

  bool changed = !photoLibrariesEqual(photoLibrary, *nextPhotos);
  photoLibrary = *nextPhotos;
  delete nextPhotos;
  if (photoLibrary.count == 0) {
    galleryIndex = 0;
    clearPreviewCache();
  } else if (uiMode == UiMode::Gallery) {
    size_t matchedIndex = 0;
    bool found = false;
    for (size_t i = 0; i < photoLibrary.count; i++) {
      if (photoLibrary.photos[i].fileName == currentSelectionFile) {
        matchedIndex = i;
        found = true;
        break;
      }
    }
    galleryIndex = found ? matchedIndex : 0;
  }

  if (!selectedPhoto() || previewFileName != selectedPhoto()->fileName) {
    previewReady = false;
  }

  if (changed) {
    renderDirty = true;
  }
  return true;
}

static bool fetchAiPhotosIfDue(bool force) {
  if (!ccEnsureWifiConnected()) {
    return false;
  }
  if (!ensureSdCardReady()) {
    aiPhotoLibrary.count = 0;
    aiPhotoLibrary.loaded = false;
    return false;
  }
  unsigned long now = millis();
  const unsigned long pollInterval = (sdReady && aiSyncInProgress) ? AI_SYNC_STEP_MS : AI_PHOTOS_POLL_MS;
  if (!force && now - lastAiPhotoPollMs < pollInterval) {
    return false;
  }
  lastAiPhotoPollMs = now;

  aiPhotoLibrary.count = 0;
  aiPhotoLibrary.loaded = false;
  const String currentSelectionFile = selectedAiFileName().length()
    ? selectedAiFileName()
    : persistedAiSlideFileName;
  const size_t previousCount = aiSdCacheFileCount;
  const bool revisionChanged =
    companionState.generatedImagesRevision.length() &&
    companionState.generatedImagesRevision != aiSyncedRevision;
  const bool needsInitialSync =
    !aiSyncInProgress &&
    aiSyncOffset == 0 &&
    !aiSyncedRevision.length();
  const bool restartSync = force || revisionChanged || needsInitialSync;
  if (!syncAiSlideshowCacheStep(restartSync)) {
    return false;
  }
  aiSdCacheFileCount = countAiCachedFilesOnSd();
  restoreAiSlideSelection(currentSelectionFile);
  saveAiSlidePrefs();
  clearAiSlideCacheState();
  if (aiSdCacheFileCount != previousCount || force) {
    showAiSlideHud();
    renderDirty = true;
  }
  return true;
}

static String settingValueLabel(SettingsItemId item) {
  switch (item) {
    case SettingsItemId::Personality:
      return currentPresetLabel();
    case SettingsItemId::VoiceMode:
      return voiceModeLabel(companionSettings.voiceMode);
    case SettingsItemId::Volume:
      return String(companionSettings.volumeLevel);
    case SettingsItemId::ScrollSpeed:
      return String(companionSettings.scrollSpeedLevel);
    case SettingsItemId::RecordTime:
      return String(companionSettings.manualRecordMaxSec) + "s";
    case SettingsItemId::UiTheme:
      return compactThemeLabel(companionSettings.uiTheme);
    case SettingsItemId::HeaderMode:
      return compactHeaderLabel(companionSettings.headerMode);
    case SettingsItemId::ScreensaverMode:
      return compactScreensaverLabel(companionSettings.screensaverMode);
    case SettingsItemId::IdleTimeout:
      return compactDurationLabel(companionSettings.idleTimeoutSec);
    case SettingsItemId::ScreenBlankTimeout:
      return compactDurationLabel(companionSettings.screenBlankTimeoutSec);
    case SettingsItemId::RoomMonitorInterval:
      return compactDurationLabel(companionSettings.roomMonitorIntervalSec);
    case SettingsItemId::CameraSource:
      return compactCameraSourceLabel(companionSettings.cameraSource);
    case SettingsItemId::MusicShuffle:
      return companionSettings.musicShuffle ? "On" : "Off";
    case SettingsItemId::ChatTextSize:
      switch (chatTextScale()) {
        case 3:
          return "Large";
        case 2:
          return "Medium";
        case 1:
        default:
          return "Small";
      }
    case SettingsItemId::ChatTextColor:
      return CHAT_COLOR_LABELS[min(static_cast<int>(cc_chat_color_mode), 7)];
    default:
      return "";
  }
}

static const char *settingLabel(SettingsItemId item) {
  switch (item) {
    case SettingsItemId::Personality:
      return "Personality";
    case SettingsItemId::VoiceMode:
      return "Voice Mode";
    case SettingsItemId::Volume:
      return "Volume";
    case SettingsItemId::ScrollSpeed:
      return "Scroll Speed";
    case SettingsItemId::RecordTime:
      return "Record Time";
    case SettingsItemId::UiTheme:
      return "UI Theme";
    case SettingsItemId::HeaderMode:
      return "Header Mode";
    case SettingsItemId::ScreensaverMode:
      return "Saver Mode";
    case SettingsItemId::IdleTimeout:
      return "Idle Timeout";
    case SettingsItemId::ScreenBlankTimeout:
      return "Blank Timeout";
    case SettingsItemId::RoomMonitorInterval:
      return "Room Monitor";
    case SettingsItemId::CameraSource:
      return "Camera Source";
    case SettingsItemId::MusicShuffle:
      return "Music Shuffle";
    case SettingsItemId::ChatTextSize:
      return "Chat Text Size";
    case SettingsItemId::ChatTextColor:
      return "Chat Color";
    default:
      return "";
  }
}

static bool cycleSettingsValue(int delta) {
  SettingsItemId item = SETTINGS_ITEMS[selectedSettingsIndex];
  bool ok = false;
  String toast;

  switch (item) {
    case SettingsItemId::Personality: {
      if (companionSettings.presetCount == 0) {
        setToast("No presets loaded.");
        return false;
      }
      int next = static_cast<int>(currentPresetIndex()) + delta;
      if (next < 0) {
        next = static_cast<int>(companionSettings.presetCount) - 1;
      } else if (next >= static_cast<int>(companionSettings.presetCount)) {
        next = 0;
      }
      const CompanionPreset &preset = companionSettings.presets[next];
      ok = apiSetPersonalityPreset(preset);
      if (ok) {
        companionSettings.personalityPresetId = preset.id;
        toast = "Preset: " + (preset.label.length() ? preset.label : preset.id);
      }
      break;
    }
    case SettingsItemId::VoiceMode: {
      int next = findStringOptionIndex(
        companionSettings.voiceMode,
        VOICE_MODE_OPTIONS,
        sizeof(VOICE_MODE_OPTIONS) / sizeof(VOICE_MODE_OPTIONS[0])
      ) + delta;
      int count = sizeof(VOICE_MODE_OPTIONS) / sizeof(VOICE_MODE_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetStringSetting("voiceMode", VOICE_MODE_OPTIONS[next]);
      if (ok) {
        companionSettings.voiceMode = VOICE_MODE_OPTIONS[next];
        toast = "Voice: " + String(voiceModeLabel(companionSettings.voiceMode));
      }
      break;
    }
    case SettingsItemId::Volume: {
      int next = companionSettings.volumeLevel + delta;
      if (next < 1) {
        next = 10;
      } else if (next > 10) {
        next = 1;
      }
      ok = apiSetIntSetting("volumeLevel", next);
      if (ok) {
        companionSettings.volumeLevel = next;
        toast = "Volume: " + String(next);
      }
      break;
    }
    case SettingsItemId::ScrollSpeed: {
      int next = companionSettings.scrollSpeedLevel + delta;
      if (next < 1) {
        next = 10;
      } else if (next > 10) {
        next = 1;
      }
      ok = apiSetIntSetting("scrollSpeedLevel", next);
      if (ok) {
        companionSettings.scrollSpeedLevel = next;
        toast = "Scroll: " + String(next);
      }
      break;
    }
    case SettingsItemId::RecordTime: {
      int next = findIntOptionIndex(
        companionSettings.manualRecordMaxSec,
        RECORD_TIME_OPTIONS,
        sizeof(RECORD_TIME_OPTIONS) / sizeof(RECORD_TIME_OPTIONS[0])
      ) + delta;
      int count = sizeof(RECORD_TIME_OPTIONS) / sizeof(RECORD_TIME_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetIntSetting("manualRecordMaxSec", RECORD_TIME_OPTIONS[next]);
      if (ok) {
        companionSettings.manualRecordMaxSec = RECORD_TIME_OPTIONS[next];
        toast = "Record: " + String(companionSettings.manualRecordMaxSec) + "s";
      }
      break;
    }
    case SettingsItemId::UiTheme: {
      int next = findStringOptionIndex(
        companionSettings.uiTheme,
        UI_THEME_OPTIONS,
        sizeof(UI_THEME_OPTIONS) / sizeof(UI_THEME_OPTIONS[0])
      ) + delta;
      int count = sizeof(UI_THEME_OPTIONS) / sizeof(UI_THEME_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetStringSetting("uiTheme", UI_THEME_OPTIONS[next]);
      if (ok) {
        companionSettings.uiTheme = UI_THEME_OPTIONS[next];
        toast = "Theme: " + compactThemeLabel(companionSettings.uiTheme);
      }
      break;
    }
    case SettingsItemId::HeaderMode: {
      int next = findStringOptionIndex(
        companionSettings.headerMode,
        HEADER_MODE_OPTIONS,
        sizeof(HEADER_MODE_OPTIONS) / sizeof(HEADER_MODE_OPTIONS[0])
      ) + delta;
      int count = sizeof(HEADER_MODE_OPTIONS) / sizeof(HEADER_MODE_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetStringSetting("headerMode", HEADER_MODE_OPTIONS[next]);
      if (ok) {
        companionSettings.headerMode = HEADER_MODE_OPTIONS[next];
        toast = "Header: " + compactHeaderLabel(companionSettings.headerMode);
      }
      break;
    }
    case SettingsItemId::ScreensaverMode: {
      int next = findStringOptionIndex(
        companionSettings.screensaverMode,
        SCREENSAVER_MODE_OPTIONS,
        sizeof(SCREENSAVER_MODE_OPTIONS) / sizeof(SCREENSAVER_MODE_OPTIONS[0])
      ) + delta;
      int count = sizeof(SCREENSAVER_MODE_OPTIONS) / sizeof(SCREENSAVER_MODE_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetStringSetting("screensaverMode", SCREENSAVER_MODE_OPTIONS[next]);
      if (ok) {
        companionSettings.screensaverMode = SCREENSAVER_MODE_OPTIONS[next];
        toast = "Saver: " + compactScreensaverLabel(companionSettings.screensaverMode);
      }
      break;
    }
    case SettingsItemId::IdleTimeout: {
      int next = findIntOptionIndex(
        companionSettings.idleTimeoutSec,
        IDLE_TIMEOUT_OPTIONS,
        sizeof(IDLE_TIMEOUT_OPTIONS) / sizeof(IDLE_TIMEOUT_OPTIONS[0])
      ) + delta;
      int count = sizeof(IDLE_TIMEOUT_OPTIONS) / sizeof(IDLE_TIMEOUT_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetIntSetting("idleTimeoutSec", IDLE_TIMEOUT_OPTIONS[next]);
      if (ok) {
        companionSettings.idleTimeoutSec = IDLE_TIMEOUT_OPTIONS[next];
        toast = "Idle: " + compactDurationLabel(companionSettings.idleTimeoutSec);
      }
      break;
    }
    case SettingsItemId::ScreenBlankTimeout: {
      int next = findIntOptionIndex(
        companionSettings.screenBlankTimeoutSec,
        IDLE_TIMEOUT_OPTIONS,
        sizeof(IDLE_TIMEOUT_OPTIONS) / sizeof(IDLE_TIMEOUT_OPTIONS[0])
      ) + delta;
      int count = sizeof(IDLE_TIMEOUT_OPTIONS) / sizeof(IDLE_TIMEOUT_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetIntSetting("screenBlankTimeoutSec", IDLE_TIMEOUT_OPTIONS[next]);
      if (ok) {
        companionSettings.screenBlankTimeoutSec = IDLE_TIMEOUT_OPTIONS[next];
        toast = "Blank: " + compactDurationLabel(companionSettings.screenBlankTimeoutSec);
      }
      break;
    }
    case SettingsItemId::RoomMonitorInterval: {
      int next = findIntOptionIndex(
        companionSettings.roomMonitorIntervalSec,
        ROOM_MONITOR_INTERVAL_OPTIONS,
        sizeof(ROOM_MONITOR_INTERVAL_OPTIONS) / sizeof(ROOM_MONITOR_INTERVAL_OPTIONS[0])
      ) + delta;
      int count = sizeof(ROOM_MONITOR_INTERVAL_OPTIONS) / sizeof(ROOM_MONITOR_INTERVAL_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetIntSetting("roomMonitorIntervalSec", ROOM_MONITOR_INTERVAL_OPTIONS[next]);
      if (ok) {
        companionSettings.roomMonitorIntervalSec = ROOM_MONITOR_INTERVAL_OPTIONS[next];
        toast = "Monitor: " + compactDurationLabel(companionSettings.roomMonitorIntervalSec);
      }
      break;
    }
    case SettingsItemId::CameraSource: {
      int next = findStringOptionIndex(
        companionSettings.cameraSource,
        CAMERA_SOURCE_OPTIONS,
        sizeof(CAMERA_SOURCE_OPTIONS) / sizeof(CAMERA_SOURCE_OPTIONS[0])
      ) + delta;
      int count = sizeof(CAMERA_SOURCE_OPTIONS) / sizeof(CAMERA_SOURCE_OPTIONS[0]);
      if (next < 0) {
        next = count - 1;
      } else if (next >= count) {
        next = 0;
      }
      ok = apiSetStringSetting("cameraSource", CAMERA_SOURCE_OPTIONS[next]);
      if (ok) {
        companionSettings.cameraSource = CAMERA_SOURCE_OPTIONS[next];
        toast = "Camera: " + compactCameraSourceLabel(companionSettings.cameraSource);
      }
      break;
    }
    case SettingsItemId::MusicShuffle: {
      bool next = !companionSettings.musicShuffle;
      ok = apiSetBoolSetting("musicShuffle", next);
      if (ok) {
        companionSettings.musicShuffle = next;
        toast = String("Shuffle: ") + (next ? "On" : "Off");
      }
      break;
    }
    case SettingsItemId::ChatTextSize: {
      int next = chatTextScale() + delta;
      if (next < 1) {
        next = 3;
      } else if (next > 3) {
        next = 1;
      }
      ccSaveLocalUiSettings(static_cast<uint8_t>(next), cc_chat_color_mode);
      ok = true;
      toast = "Chat size: " + settingValueLabel(item);
      break;
    }
    case SettingsItemId::ChatTextColor: {
      int next = static_cast<int>(cc_chat_color_mode) + delta;
      if (next < 0) {
        next = 7;
      } else if (next > 7) {
        next = 0;
      }
      ccSaveLocalUiSettings(cc_chat_text_scale, static_cast<uint8_t>(next));
      ok = true;
      toast = "Chat color: " + settingValueLabel(item);
      break;
    }
    default:
      break;
  }

  setToast(ok ? toast : "Setting update failed.");
  renderDirty = true;
  return ok;
}

static void stepSettingsItem(int delta) {
  int next = static_cast<int>(selectedSettingsIndex) + delta;
  int count = sizeof(SETTINGS_ITEMS) / sizeof(SETTINGS_ITEMS[0]);
  if (next < 0) {
    next = count - 1;
  } else if (next >= count) {
    next = 0;
  }
  selectedSettingsIndex = static_cast<size_t>(next);
  renderDirty = true;
}

static void handleNewChat() {
  bool ok = apiResetChat();
  if (ok) {
    clearConversationLog();
    appendLogEntry("SYS ", "Started a new chat.");
    companionState.text = "Started a new chat.";
  }
  setToast(ok ? "Started a new chat." : "New chat failed.");
}

static void handleRepeat() {
  bool ok = apiRepeatLastAnswer();
  setToast(ok ? "Replaying last answer." : "Replay failed.");
}

static void handleCapture() {
  bool ok = apiCaptureVision();
  if (ok) {
    photoRefreshBoostUntilMs = millis() + PHOTO_REFRESH_BOOST_MS;
    lastPhotoPollMs = 0;
    setUiMode(UiMode::Capture);
  }
  setToast(ok ? "Capture requested." : "Capture failed.");
}

static void drawHeader() {
  gfx->fillRect(0, 0, 320, 20, COLOR_HEADER);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
  gfx->setCursor(8, 6);
  gfx->print("WHISPLAY CYD");

  const char *modeLabel = uiModeLabel(uiMode);
  int modeX = 160 - ((strlen(modeLabel) * 6) / 2);
  gfx->setCursor(modeX, 6);
  gfx->print(modeLabel);

  gfx->fillCircle(234, 10, 4, WiFi.status() == WL_CONNECTED ? COLOR_ACCENT : COLOR_ERROR);
  drawButton(buttonModePrev);
  drawButton(buttonAiShow);
  drawButton(buttonNewChat);
  drawButton(buttonRepeat);
  drawButton(buttonModeNext);
  drawButton(buttonSetup);
}

static void drawFooter(const String &message) {
  gfx->fillRect(0, 222, 320, 18, COLOR_BG);
  gfx->setTextColor(COLOR_DIM, COLOR_BG);
  gfx->setCursor(8, 228);
  gfx->print(trimTail(message, 50));
}

static void drawPhotoPanel(int x, int y, int w, int h, const String &title, const String &subtitle) {
  gfx->drawRoundRect(x, y, w, h, 6, COLOR_DIM);
  gfx->fillRoundRect(x, y, w, h, 6, COLOR_PANEL);
  gfx->setTextColor(COLOR_DIM, COLOR_PANEL);
  gfx->setCursor(x + 8, y + 8);
  gfx->print(title);
  if (subtitle.length()) {
    gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_PANEL);
    gfx->setCursor(x + w - (subtitle.length() * 6) - 8, y + 8);
    gfx->print(subtitle);
  }
}

static void drawPreviewStatusText(int x, int y, int w, int h, const String &line1, const String &line2) {
  gfx->setTextColor(COLOR_TEXT, COLOR_PANEL);
  gfx->setTextSize(1);
  int textX = x + 12;
  int textY = y + (h / 2) - 8;
  gfx->setCursor(textX, textY);
  gfx->print(trimTail(line1, 36));
  if (line2.length()) {
    gfx->setCursor(textX, textY + 14);
    gfx->print(trimTail(line2, 36));
  }
}

static void renderChatMode() {
  gfx->drawRoundRect(8, 50, 304, 164, 6, COLOR_DIM);
  gfx->fillRoundRect(8, 50, 304, 164, 6, COLOR_PANEL);

  gfx->setTextColor(COLOR_DIM, COLOR_PANEL);
  gfx->setCursor(16, 58);
  gfx->print("PRESET");
  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_PANEL);
  gfx->setCursor(58, 58);
  gfx->print(trimTail(currentPresetLabel(), 20));

  gfx->setTextColor(voiceModeColor(companionSettings.voiceMode), COLOR_PANEL);
  const char *voiceLabel = voiceModeLabel(companionSettings.voiceMode);
  gfx->setCursor(258 - (strlen(voiceLabel) * 6), 58);
  gfx->print(voiceLabel);

  gfx->setTextColor(COLOR_DIM, COLOR_PANEL);
  gfx->setCursor(264, 58);
  if (companionState.imageIconVisible) {
    gfx->print("IMG");
  }
  if (companionState.ragIconVisible) {
    gfx->setCursor(286, 58);
    gfx->print("RAG");
  }

  drawConversationLog(16, 74, 288, 130);

  if (toastUntilMs > millis() && toastMessage.length()) {
    drawFooter(toastMessage);
  } else if (WiFi.status() != WL_CONNECTED) {
    drawFooter("WiFi reconnecting...");
  } else {
    drawFooter(String("Status: ") + (companionState.status.length() ? companionState.status : "Waiting"));
  }
}

static void renderAiSlideshowMode() {
  const String fileName = selectedAiFileName();
  const bool useSdCache = ensureSdCardReady();
  const CompanionPhoto *photo = useSdCache ? nullptr : selectedAiPhoto();
  if (fileName.length() && ensureAiSlideReady()) {
    String cachePath = String(AI_SLIDE_DRAW_CACHE_PATH);
    String drawError;
    bool drawOk = drawJpegFromFs(SPIFFS, cachePath.c_str(), 0, 0, 320, 240, &drawError);
    if (!drawOk) {
      SPIFFS.remove(cachePath);
      const bool recached = useSdCache
        ? ensureCurrentAiSlideCachedFromSd(fileName)
        : (photo ? ensureCurrentAiSlideCached(*photo) : false);
      if (recached) {
        drawError = "";
        drawOk = drawJpegFromFs(SPIFFS, cachePath.c_str(), 0, 0, 320, 240, &drawError);
      }
    }
    if (!drawOk) {
      gfx->fillScreen(COLOR_BG);
      aiSlideError = (useSdCache ? "Tmp " : "Local ") + (drawError.length() ? drawError : String("draw fail"));
      drawPreviewStatusText(0, 0, 320, 240, trimTail(fileName, 32), aiSlideError);
    } else if (aiSlideHudUntilMs > millis()) {
      gfx->fillRect(0, 220, 320, 20, COLOR_BG);
      gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_BG);
      gfx->setCursor(8, 226);
      gfx->print(aiSlideCountLabel());
      gfx->setCursor(312 - min(static_cast<int>(trimTail(fileName, 26).length()), 26) * 6, 226);
      gfx->print(trimTail(fileName, 26));
    }
  } else if (fileName.length()) {
    gfx->fillScreen(COLOR_BG);
    drawPreviewStatusText(0, 0, 320, 240, trimTail(fileName, 32), aiSlideError.length() ? aiSlideError : "Waiting for AI cache.");
  } else {
    gfx->fillScreen(COLOR_BG);
    drawPreviewStatusText(
      0,
      0,
      320,
      240,
      useSdCache ? "No AI images on SD yet." : "No SD card detected.",
      aiSyncStatusLabel()
    );
  }
}

static void renderCaptureMode() {
  const CompanionPhoto *photo = photoLibrary.count > 0 ? &photoLibrary.photos[0] : nullptr;
  drawPhotoPanel(8, 50, 304, 128, "LATEST CAPTURE", photo ? "1/" + String(photoLibrary.count) : "0/24");

  if (photo && ensurePreviewForSelection() && drawPreviewImage(16, 70, 288, 92)) {
    // Preview drawn.
  } else if (photo) {
    drawPreviewStatusText(8, 50, 304, 128, trimTail(photo->fileName, 30), previewError.length() ? previewError : "Tap capture to refresh.");
  } else {
    drawPreviewStatusText(8, 50, 304, 128, "No captured images yet.", "Tap CAPTURE to add one.");
  }

  drawButton(buttonCaptureAction);

  if (toastUntilMs > millis() && toastMessage.length()) {
    drawFooter(toastMessage);
  } else {
    drawFooter("Latest photo preview from the Pi capture list.");
  }
}

static void renderGalleryMode() {
  String indexLabel = photoLibrary.count > 0
    ? String(galleryIndex + 1) + "/" + String(photoLibrary.count)
    : "0/24";
  drawPhotoPanel(8, 50, 304, 128, "GALLERY", indexLabel);

  const CompanionPhoto *photo = selectedPhoto();
  if (photo && ensurePreviewForSelection() && drawPreviewImage(16, 70, 288, 92)) {
    // Preview drawn.
  } else if (photo) {
    drawPreviewStatusText(8, 50, 304, 128, trimTail(photo->fileName, 30), previewError.length() ? previewError : "Tap NEXT/PREV to browse.");
  } else {
    drawPreviewStatusText(8, 50, 304, 128, "No captures available.", "Use Capture mode first.");
  }

  drawButton(buttonGalleryPrev);
  drawButton(buttonGalleryLatest);
  drawButton(buttonGalleryNext);

  if (toastUntilMs > millis() && toastMessage.length()) {
    drawFooter(toastMessage);
  } else {
    drawFooter("Browse the newest 24 photos from the Pi.");
  }
}

static void renderSettingsMode() {
  SettingsItemId item = SETTINGS_ITEMS[selectedSettingsIndex];
  gfx->drawRoundRect(8, 50, 304, 102, 6, COLOR_DIM);
  gfx->fillRoundRect(8, 50, 304, 102, 6, COLOR_PANEL);

  gfx->setTextColor(COLOR_DIM, COLOR_PANEL);
  gfx->setCursor(16, 60);
  gfx->print("SETTING");
  String indexLabel = String(selectedSettingsIndex + 1) + "/" + String(sizeof(SETTINGS_ITEMS) / sizeof(SETTINGS_ITEMS[0]));
  gfx->setCursor(312 - (indexLabel.length() * 6) - 10, 60);
  gfx->print(indexLabel);

  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_PANEL);
  gfx->setTextSize(2);
  gfx->setCursor(16, 82);
  gfx->print(trimTail(settingLabel(item), 24));

  gfx->setTextColor(item == SettingsItemId::ChatTextColor ? currentChatColor() : COLOR_WARN, COLOR_PANEL);
  gfx->setCursor(16, 114);
  gfx->print(trimTail(settingValueLabel(item), 18));
  gfx->setTextSize(1);

  drawButton(buttonSettingsPrevItem);
  drawButton(buttonSettingsNextItem);
  drawButton(buttonSettingsPrevValue);
  drawButton(buttonSettingsNextValue);

  if (toastUntilMs > millis() && toastMessage.length()) {
    drawFooter(toastMessage);
  } else {
    drawFooter("ITEM changes rows, VALUE changes the selected setting.");
  }
}

static void renderUi() {
  gfx->fillScreen(COLOR_BG);
  if (uiMode != UiMode::AiSlideshow) {
    drawHeader();
  }

  switch (uiMode) {
    case UiMode::AiSlideshow:
      renderAiSlideshowMode();
      break;
    case UiMode::Chat:
      renderChatMode();
      break;
    case UiMode::Capture:
      renderCaptureMode();
      break;
    case UiMode::Gallery:
      renderGalleryMode();
      break;
    case UiMode::Settings:
      renderSettingsMode();
      break;
    default:
      renderChatMode();
      break;
  }
}

static void handleModeTouch(int x, int y) {
  if (uiMode == UiMode::AiSlideshow) {
    if (x < 80) {
      moveAiSlide(-1);
      return;
    }
    if (x > 240) {
      moveAiSlide(1);
      return;
    }
  }

  if (pointInButton(x, y, buttonCaptureAction)) {
    handleCapture();
    return;
  }

  if (uiMode == UiMode::Gallery) {
    if (pointInButton(x, y, buttonGalleryPrev)) {
      if (photoLibrary.count > 0) {
        galleryIndex = galleryIndex == 0 ? photoLibrary.count - 1 : galleryIndex - 1;
        previewReady = false;
        renderDirty = true;
      }
      return;
    }
    if (pointInButton(x, y, buttonGalleryLatest)) {
      galleryIndex = 0;
      previewReady = false;
      renderDirty = true;
      return;
    }
    if (pointInButton(x, y, buttonGalleryNext)) {
      if (photoLibrary.count > 0) {
        galleryIndex = (galleryIndex + 1) % photoLibrary.count;
        previewReady = false;
        renderDirty = true;
      }
      return;
    }
  }

  if (uiMode == UiMode::Settings) {
    if (pointInButton(x, y, buttonSettingsPrevItem)) {
      stepSettingsItem(-1);
      return;
    }
    if (pointInButton(x, y, buttonSettingsNextItem)) {
      stepSettingsItem(1);
      return;
    }
    if (pointInButton(x, y, buttonSettingsPrevValue)) {
      cycleSettingsValue(-1);
      return;
    }
    if (pointInButton(x, y, buttonSettingsNextValue)) {
      cycleSettingsValue(1);
      return;
    }
  }
}

static void handleTouch() {
  bool touched = ts.tirqTouched() && ts.touched();
  if (!touched) {
    touchWasDown = false;
    return;
  }
  if (touchWasDown || millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) {
    return;
  }

  TS_Point point = ts.getPoint();
  int x = 0;
  int y = 0;
  mapTouch(point.x, point.y, x, y);
  touchWasDown = true;
  lastTouchMs = millis();

  if (pointInButton(x, y, buttonSetup)) {
    openSetupPortal();
    return;
  }
  if (pointInButton(x, y, buttonAiShow)) {
    if (uiMode == UiMode::AiSlideshow) {
      lastAiPhotoPollMs = 0;
      clearAiSlideCacheState();
      bool ok = fetchAiPhotosIfDue(true);
      setToast(ok ? "AI slideshow refreshed." : "AI refresh failed.");
      renderDirty = true;
    } else {
      setUiMode(UiMode::AiSlideshow);
    }
    return;
  }
  if (pointInButton(x, y, buttonModePrev)) {
    stepUiMode(-1);
    return;
  }
  if (pointInButton(x, y, buttonModeNext)) {
    stepUiMode(1);
    return;
  }
  if (pointInButton(x, y, buttonNewChat)) {
    handleNewChat();
    return;
  }
  if (pointInButton(x, y, buttonRepeat)) {
    handleRepeat();
    return;
  }

  handleModeTouch(x, y);
}

static void handleBootPortalHold() {
  if (digitalRead(BOOT_BTN) == LOW) {
    if (lastBootHoldStartMs == 0) {
      lastBootHoldStartMs = millis();
    } else if (millis() - lastBootHoldStartMs >= BOOT_PORTAL_HOLD_MS) {
      openSetupPortal();
    }
  } else {
    lastBootHoldStartMs = 0;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(GFX_BL, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  digitalWrite(GFX_BL, HIGH);
  gfx->begin();
  gfx->fillScreen(COLOR_BG);
  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(28, 84);
  gfx->print("Whisplay CYD");
  gfx->setTextSize(1);
  gfx->setCursor(74, 108);
  gfx->print("Companion Modes");

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  SPIFFS.begin(true);
  loadAiSlidePrefs();
  pruneAiSlideshowCache(false);
  ensureSdCardReady();
  aiSdManifestCount = countAiManifestEntries();
  aiSdCacheFileCount = countAiCachedFilesOnSd();
  if (sdReady) {
    aiPhotoLibrary.count = 0;
    aiPhotoLibrary.loaded = false;
  }

  bool forcePortal = digitalRead(BOOT_BTN) == LOW;
  ccConnect(forcePortal);

  analogWrite(GFX_BL, cc_brightness);
  fetchStateIfDue();
  fetchSettingsIfDue();
  fetchPhotosIfDue(true);
  showAiSlideHud();
  renderUi();
}

void loop() {
  ccEnsureWifiConnected();
  fetchStateIfDue();
  fetchSettingsIfDue();
  fetchPhotosIfDue();
  fetchAiPhotosIfDue();

  if (uiMode == UiMode::Capture || uiMode == UiMode::Gallery) {
    ensurePreviewForSelection();
  }
  if (uiMode == UiMode::AiSlideshow) {
    ensureAiSlideReady();
    advanceAiSlide();
  } else if (
    autoChatActive &&
    uiMode == UiMode::Chat &&
    millis() > autoChatUntilMs &&
    (companionState.status == "idle" || companionState.status == "last reply")
  ) {
    setUiMode(UiMode::AiSlideshow);
  }

  handleTouch();
  handleBootPortalHold();

  if (toastUntilMs > 0 && millis() > toastUntilMs) {
    toastUntilMs = 0;
    toastMessage = "";
    renderDirty = true;
  }

  if (renderDirty) {
    renderUi();
    renderDirty = false;
  }

  delay(10);
}
