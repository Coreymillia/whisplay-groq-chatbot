#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

#include "GroqApi.h"
#include "GroqLcd.h"
#include "GroqPortal.h"

static uint32_t COLOR_BG = BLACK;
static uint32_t COLOR_PANEL = 0x18C3;
static uint32_t COLOR_ACCENT = 0x07FF;
static const uint32_t COLOR_TEXT = WHITE;
static uint32_t COLOR_DIM = 0x7BEF;
static const uint32_t COLOR_WARN = 0xFFE0;
static const uint32_t COLOR_OK = 0x07E0;
static const uint32_t COLOR_ERROR = 0xF800;
static uint32_t CHAT_TEXT_COLOR = WHITE;
static bool CHAT_TEXT_MULTICOLOR = false;

static String inputBuffer;
static String toastMessage;
static uint8_t dirtyRegions = 0xFF;
static unsigned long toastUntilMs = 0;

struct ChatTurnPage {
  String userText;
  String replyText;
};

static std::vector<ChatTurnPage> chatTurnPages;
static std::vector<String> cameraPhotoPaths;
static int activeTurnIndex = -1;
static int activePhotoIndex = -1;
static uint8_t activePhotoRotationQuarterTurns = 0;
static int activeReplyScrollOffset = 0;
static unsigned long replyAutoScrollPauseUntilMs = 0;
static unsigned long lastReplyAutoScrollMs = 0;
static bool replyAutoScrollComplete = false;

static bool recordingActive = false;
static int16_t *recordingSamples = nullptr;
static size_t recordingCapacitySamples = 0;
static size_t recordingCapturedSamples = 0;
static unsigned long recordingStartedMs = 0;

static constexpr size_t SAMPLE_RATE = 16000;
static constexpr size_t RECORD_CHUNK_SAMPLES = 240;
static constexpr size_t MAX_DISPLAY_CHARS = 1400;
static constexpr int TURN_SCROLL_STEP = 2;
static constexpr unsigned long TURN_AUTO_SCROLL_INTERVAL_MS = 1200;
static constexpr unsigned long TURN_AUTO_SCROLL_PAUSE_MS = 2600;
static constexpr int HEADER_HEIGHT = 18;
static constexpr int FOOTER_HEIGHT = 14;
static constexpr int CONTENT_MARGIN = 6;
static constexpr int TOAST_HEIGHT = 20;
static constexpr uint8_t TFT_BRIGHTNESS_USB = 128;
static constexpr uint8_t TFT_BRIGHTNESS_BATTERY = 48;
static constexpr int SD_SPI_SCK_PIN = 40;
static constexpr int SD_SPI_MISO_PIN = 39;
static constexpr int SD_SPI_MOSI_PIN = 14;
static constexpr int SD_SPI_CS_PIN = 12;
static constexpr size_t CAMERA_DOWNLOAD_CHUNK = 1024;
static constexpr unsigned long SCREENSAVER_FRAME_MS = 45;
static constexpr unsigned long SCREENSAVER_WAKE_GUARD_MS = 180;
static constexpr uint8_t SCREENSAVER_BOOT_HINT_SECONDS = 8;
static constexpr unsigned long SCREENSAVER_RANDOM_SHIFT_MS = 120000;

enum class ChatPane : uint8_t {
  Incoming,
  Outgoing,
};

enum class ScreenMode : uint8_t {
  Chat,
  Screensaver,
  Settings,
  BotSettings,
  Hotkeys,
  CustomPersonality,
  PhotoViewer,
};

enum class BotSettingField : uint8_t {
  Model,
  Personality,
};

enum class SettingField : uint8_t {
  Wifi,
  Model,
  RecordScroll,
  TextLight,
  WeatherCamera,
  Screensaver,
  IdleDelay,
  Reader,
  ChatColor,
  Theme,
  Peer,
};

enum class CommandGuideSection : uint8_t {
  Hotkeys,
  Voice,
};

enum class CustomPersonalityStage : uint8_t {
  Prompt,
  Name,
  Confirm,
};

enum RenderRegion : uint8_t {
  DIRTY_NONE = 0,
  DIRTY_HEADER = 1 << 0,
  DIRTY_CONTENT = 1 << 1,
  DIRTY_FOOTER = 1 << 2,
  DIRTY_TOAST = 1 << 3,
  DIRTY_ALL = DIRTY_HEADER | DIRTY_CONTENT | DIRTY_FOOTER | DIRTY_TOAST,
};

static ChatPane activePane = ChatPane::Incoming;
static ScreenMode activeScreen = ScreenMode::Chat;
static BotSettingField activeBotSettingField = BotSettingField::Model;
static SettingField activeSettingField = SettingField::Screensaver;
static CommandGuideSection activeCommandGuideSection = CommandGuideSection::Hotkeys;
static int activeCommandGuideIndex = 0;
static CustomPersonalityStage activeCustomPersonalityStage = CustomPersonalityStage::Prompt;
static bool usingExternalPower = true;
static String customPersonalityPromptBuffer;
static String customPersonalityNameBuffer;
static bool sdCardInitAttempted = false;
static bool sdCardReady = false;
static WebServer *gp_companion_server = nullptr;
static unsigned long lastUserActivityMs = 0;
static unsigned long screensaverDismissUntilMs = 0;
static unsigned long screensaverStartedMs = 0;
static unsigned long lastScreensaverFrameMs = 0;
static bool bootScreensaverActive = false;
static int activeRandomScreensaverIndex = -1;
static unsigned long nextRandomScreensaverChangeMs = 0;

struct ScreensaverBall {
  float x;
  float y;
  float dx;
  float dy;
  int radius;
  uint16_t color;
};

struct ScreensaverStar {
  float x;
  float y;
  float z;
  uint16_t color;
};

struct TetrisRainPiece {
  int x;
  float y;
  float speed;
  uint8_t shape;
  uint8_t rotation;
  uint16_t color;
};

struct CommandGuideEntry {
  const char *label;
  const char *detail;
};

static constexpr uint32_t CHAT_RAINBOW_COLORS[] = {
  0x07FF,
  0x07E0,
  0xFFE0,
  0xF81F,
  0xFFFF,
};

static void markDirty(uint8_t regions) {
  dirtyRegions |= regions;
}

static String modelShortLabel();
static void resetReplyScroll(unsigned long pauseMs = TURN_AUTO_SCROLL_PAUSE_MS);
static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars);
static bool submitMessageForReply(const String &text, bool clearInputOnSuccess);
static void recordConversationTurn(const String &userText, const String &replyText);
static bool captureEsp32CamPhoto(String &savedPathOut, String &errorOut);
static bool openPhotoViewer(const String &preferredPath, String &errorOut);
static void ensureCompanionServer();
static void pollCompanionServer();
static void renderUi();
static String selectedScreensaverLabel();

static void noteUserActivity() {
  lastUserActivityMs = millis();
}

static int32_t currentBatteryLevel() {
  int32_t level = M5.Power.getBatteryLevel();
  if (level < 0 || level > 100) return -1;
  return level;
}

static int16_t currentBatteryMilliVolts() {
  int16_t mv = M5.Power.getBatteryVoltage();
  return mv > 0 ? mv : 0;
}

static String batteryStatusLabel() {
  int32_t level = currentBatteryLevel();
  if (level >= 0) {
    return String(level) + "%";
  }

  int16_t mv = currentBatteryMilliVolts();
  if (mv > 0) {
    return String(mv / 1000.0f, 1) + "V";
  }
  return "--";
}

static uint32_t batteryStatusColor() {
  int32_t level = currentBatteryLevel();
  if (level < 0) return COLOR_DIM;
  if (level <= 15) return COLOR_ERROR;
  if (level <= 35) return COLOR_WARN;
  return COLOR_OK;
}

static bool isExternalPowerPresent() {
  int16_t vbusVoltage = M5.Power.getVBUSVoltage();
  if (vbusVoltage > 4000) {
    return true;
  }
  return M5.Power.isCharging() != m5::Power_Class::is_charging_t::is_discharging;
}

static void applyDisplayBrightness(bool force = false) {
  bool externalPowerNow = isExternalPowerPresent();
  if (!force && externalPowerNow == usingExternalPower) {
    return;
  }
  usingExternalPower = externalPowerNow;
  M5.Display.setBrightness(usingExternalPower ? TFT_BRIGHTNESS_USB : TFT_BRIGHTNESS_BATTERY);
}

struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 1;
  uint32_t sampleRate = SAMPLE_RATE;
  uint32_t byteRate = SAMPLE_RATE * sizeof(int16_t);
  uint16_t blockAlign = sizeof(int16_t);
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize = 0;
};

static int chatTextScale() {
  return max(1, min(3, static_cast<int>(gp_text_scale)));
}

static int messageTextScale() {
  return max(2, min(4, static_cast<int>(gp_text_scale) + 1));
}

static int horizontalReaderTextScale() {
  return min(2, messageTextScale());
}

static int scaledCharWidth() {
  return 6 * messageTextScale();
}

static int scaledLineHeight() {
  return (8 * messageTextScale()) + 2;
}

static int horizontalReaderCharWidth() {
  return 6 * horizontalReaderTextScale();
}

static int horizontalReaderLineHeight() {
  return (8 * horizontalReaderTextScale()) + 2;
}

static unsigned long currentReaderAutoScrollIntervalMs() {
  return max<unsigned long>(200, gp_lcd_scroll_ms);
}

static void setToast(const String &message, uint16_t durationMs = 2200) {
  noteUserActivity();
  toastMessage = message;
  toastUntilMs = millis() + durationMs;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER | DIRTY_TOAST);
}

static int font0TextWidth(const String &text) {
  return static_cast<int>(text.length()) * 6;
}

static String truncateFont0Text(const String &text, int maxChars) {
  if (maxChars <= 0) return "";
  if (text.length() <= static_cast<size_t>(maxChars)) return text;
  if (maxChars <= 3) return text.substring(0, maxChars);
  return text.substring(0, maxChars - 3) + "...";
}

static String selectedTextThemeLabel() {
  return GP_TEXT_THEME_OPTIONS[gpCurrentTextThemeOptionIndex()].label;
}

static String selectedBackgroundThemeLabel() {
  return GP_BG_THEME_OPTIONS[gpCurrentBackgroundThemeOptionIndex()].label;
}

static uint32_t rainbowChatColorAt(int index) {
  const int count = static_cast<int>(sizeof(CHAT_RAINBOW_COLORS) / sizeof(CHAT_RAINBOW_COLORS[0]));
  index = ((index % count) + count) % count;
  return CHAT_RAINBOW_COLORS[index];
}

static uint32_t currentChatTextColor(int index = 0) {
  return CHAT_TEXT_MULTICOLOR ? rainbowChatColorAt(index) : CHAT_TEXT_COLOR;
}

static void applyThemeColors() {
  COLOR_BG = BLACK;
  COLOR_PANEL = 0x18C3;
  COLOR_ACCENT = 0x07FF;
  COLOR_DIM = 0x7BEF;

  if (strcmp(gp_bg_theme, "midnight") == 0) {
    COLOR_BG = 0x0843;
    COLOR_PANEL = 0x10A6;
    COLOR_ACCENT = 0x5DFF;
    COLOR_DIM = 0x6B6D;
  } else if (strcmp(gp_bg_theme, "forest") == 0) {
    COLOR_BG = 0x0200;
    COLOR_PANEL = 0x11C4;
    COLOR_ACCENT = 0xAFE5;
    COLOR_DIM = 0x7BEF;
  } else if (strcmp(gp_bg_theme, "plum") == 0) {
    COLOR_BG = 0x1002;
    COLOR_PANEL = 0x30A6;
    COLOR_ACCENT = 0xF81F;
    COLOR_DIM = 0xA514;
  } else if (strcmp(gp_bg_theme, "ember") == 0) {
    COLOR_BG = 0x2000;
    COLOR_PANEL = 0x5220;
    COLOR_ACCENT = 0xFD20;
    COLOR_DIM = 0xC618;
  }

  CHAT_TEXT_MULTICOLOR = false;
  CHAT_TEXT_COLOR = WHITE;
  if (strcmp(gp_text_theme, "mint") == 0) {
    CHAT_TEXT_COLOR = 0x97F0;
  } else if (strcmp(gp_text_theme, "cyan") == 0) {
    CHAT_TEXT_COLOR = 0x07FF;
  } else if (strcmp(gp_text_theme, "amber") == 0) {
    CHAT_TEXT_COLOR = 0xFD20;
  } else if (strcmp(gp_text_theme, "pink") == 0) {
    CHAT_TEXT_COLOR = 0xF81F;
  } else if (strcmp(gp_text_theme, "multicolor") == 0) {
    CHAT_TEXT_MULTICOLOR = true;
  }
}

static bool readerModeIsHorizontal(const char *mode) {
  return mode && strcmp(mode, "horizontal") == 0;
}

static bool currentReaderModeIsHorizontal() {
  return readerModeIsHorizontal(gp_reader_mode);
}

static bool shouldUseHorizontalReader() {
  return currentReaderModeIsHorizontal() && activePane == ChatPane::Incoming;
}

static String selectedReaderModeLabel() {
  return currentReaderModeIsHorizontal() ? "Horiz" : "Vert";
}

static String readerModeToastLabel() {
  return currentReaderModeIsHorizontal() ? "horizontal" : "vertical";
}

static String normalizeMarqueeText(const String &value) {
  String normalized;
  normalized.reserve(value.length());
  bool previousWasSpace = false;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '\r' || c == '\n' || c == '\t') {
      c = ' ';
    }
    if (c < 32) continue;
    if (c == ' ') {
      if (previousWasSpace) continue;
      previousWasSpace = true;
    } else {
      previousWasSpace = false;
    }
    normalized += c;
  }
  normalized.trim();
  return normalized.length() ? normalized : String("—");
}

static int marqueeOffsetLimit(const String &sourceText, int visibleChars) {
  if (visibleChars <= 0) return 0;
  String normalized = normalizeMarqueeText(sourceText);
  if (normalized.length() <= static_cast<size_t>(visibleChars)) return 0;
  return static_cast<int>(normalized.length()) + 2;
}

static String marqueeWindowText(const String &sourceText, int visibleChars, int offset) {
  if (visibleChars <= 0) return "";
  String normalized = normalizeMarqueeText(sourceText);
  if (normalized.length() <= static_cast<size_t>(visibleChars)) {
    return normalized;
  }

  String cycle = normalized + "   ";
  const int cycleLength = static_cast<int>(cycle.length());
  const int start = ((offset % cycleLength) + cycleLength) % cycleLength;
  String window;
  window.reserve(visibleChars);
  for (int i = 0; i < visibleChars; i++) {
    window += cycle.charAt((start + i) % cycleLength);
  }
  return window;
}

static int settingFieldCount() {
  return 11;
}

static int currentSettingFieldIndex() {
  return static_cast<int>(activeSettingField);
}

static SettingField settingFieldAt(int index) {
  return static_cast<SettingField>(constrain(index, 0, settingFieldCount() - 1));
}

static bool settingFieldEditable(SettingField field) {
  return field == SettingField::Screensaver ||
         field == SettingField::IdleDelay ||
         field == SettingField::Reader ||
         field == SettingField::ChatColor ||
         field == SettingField::Theme;
}

static String settingFieldLabel(SettingField field) {
  switch (field) {
    case SettingField::Wifi: return "WiFi";
    case SettingField::Model: return "Model";
    case SettingField::RecordScroll: return "Rec/Scr";
    case SettingField::TextLight: return "Txt/Lgt";
    case SettingField::WeatherCamera: return "Wx/Cam";
    case SettingField::Screensaver: return "Saver";
    case SettingField::IdleDelay: return "Idle";
    case SettingField::Reader: return "Reader";
    case SettingField::ChatColor: return "ChatClr";
    case SettingField::Theme: return "Theme";
    case SettingField::Peer: return "Peer";
  }
  return "";
}

static String settingFieldValue(SettingField field) {
  switch (field) {
    case SettingField::Wifi:
      return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("offline");
    case SettingField::Model:
      return modelShortLabel();
    case SettingField::RecordScroll:
      return String(gp_record_seconds) + "s / " + String(gp_lcd_scroll_ms);
    case SettingField::TextLight:
      return String(gp_text_scale) + " / " + (gp_lcd_backlight_enabled ? String("on") : String("off"));
    case SettingField::WeatherCamera:
      return String(gpWeatherCoordinatesReady() ? "ok" : "miss") + " / " +
             (String(gp_camera_base_url).length() ? "ok" : "miss");
    case SettingField::Screensaver:
      return selectedScreensaverLabel();
    case SettingField::IdleDelay:
      return String(gp_idle_saver_sec) + " sec";
    case SettingField::Reader:
      return selectedReaderModeLabel();
    case SettingField::ChatColor:
      return selectedTextThemeLabel();
    case SettingField::Theme:
      return selectedBackgroundThemeLabel();
    case SettingField::Peer:
      return String(gp_peer_mode_enabled ? "ON" : "OFF") + " / " +
             (gpPeerSettingsReady() ? "ready" : "missing");
  }
  return "";
}

static constexpr CommandGuideEntry GP_HOTKEY_GUIDE[] = {
  {"Fn+Space = Guide", "Open or close this cheat sheet."},
  {"Fn+H = Guide", "Backup shortcut for the same cheat sheet."},
  {"Fn+M / O = View", "Swap between bot reply view and your draft."},
  {"Fn+; / . = Read", "Move through the reply or seek the marquee."},
  {"Fn+, / / = Turn", "Move to the previous or next saved turn."},
  {"Fn+W = Reader", "Toggle vertical pages or horizontal marquee."},
  {"Fn+[ / ] = Speed", "Adjust reader scroll speed."},
  {"Fn+S = Settings", "Open device settings."},
  {"Fn+B = Bot", "Open model and persona settings."},
  {"Fn+P = Weather", "Ask for weather using saved coordinates."},
  {"Fn+G = Capture", "Take a photo from the ESP32-CAM."},
  {"Fn+I = Photos", "Open saved camera photos."},
  {"Fn+T = Rotate", "Rotate the current photo."},
  {"Fn+X = Saver", "Preview the selected screensaver now."},
  {"Fn+C = Linked", "Toggle connected-device mode."},
  {"Fn+V = Custom", "Open the custom bot editor."},
  {"Fn+A = Setup", "Open the setup AP at 192.168.4.1."},
  {"Fn+N / R = Chat", "New chat or clear history."},
  {"Hold BtnA = Talk", "Record and transcribe a voice prompt."},
};

static constexpr CommandGuideEntry GP_VOICE_GUIDE[] = {
  {"What's the weather?", "Full weather question using NOAA/NWS data."},
  {"Where's the weather?", "Alternate trigger for the same weather route."},
  {"Weather alerts", "Ask for current alerts at the saved location."},
  {"Weather forecast", "Ask for the local forecast summary."},
  {"Take photo", "Capture and save a fresh ESP32-CAM image."},
  {"Capture image", "Alternate phrase for the same camera capture."},
  {"Snapshot", "Short camera trigger that saves a photo."},
};

static size_t commandGuideEntryCount(CommandGuideSection section) {
  return section == CommandGuideSection::Hotkeys
    ? sizeof(GP_HOTKEY_GUIDE) / sizeof(GP_HOTKEY_GUIDE[0])
    : sizeof(GP_VOICE_GUIDE) / sizeof(GP_VOICE_GUIDE[0]);
}

static const CommandGuideEntry *commandGuideEntries(CommandGuideSection section) {
  return section == CommandGuideSection::Hotkeys ? GP_HOTKEY_GUIDE : GP_VOICE_GUIDE;
}

static const char *commandGuideSectionLabel(CommandGuideSection section) {
  return section == CommandGuideSection::Hotkeys ? "HOTKEYS" : "VOICE";
}

static void clampCommandGuideIndex() {
  int count = static_cast<int>(commandGuideEntryCount(activeCommandGuideSection));
  if (count <= 0) {
    activeCommandGuideIndex = 0;
    return;
  }
  activeCommandGuideIndex = constrain(activeCommandGuideIndex, 0, count - 1);
}

static void moveCommandGuideSelection(int delta) {
  int count = static_cast<int>(commandGuideEntryCount(activeCommandGuideSection));
  if (count <= 0 || delta == 0) return;
  activeCommandGuideIndex = (activeCommandGuideIndex + delta + count) % count;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void switchCommandGuideSection(int delta) {
  if (delta == 0) return;
  activeCommandGuideSection = delta > 0 ? CommandGuideSection::Voice : CommandGuideSection::Hotkeys;
  clampCommandGuideIndex();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void toggleCommandGuide() {
  activeScreen = activeScreen == ScreenMode::Hotkeys ? ScreenMode::Chat : ScreenMode::Hotkeys;
  clampCommandGuideIndex();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static bool screensaverModeIsRandom(const char *mode) {
  return mode && strcmp(mode, "random-shift") == 0;
}

static bool isConcreteScreensaverMode(const char *mode) {
  return mode && mode[0] != '\0' && !screensaverModeIsRandom(mode);
}

static void chooseRandomScreensaverMode() {
  int eligibleIndices[16];
  int eligibleCount = 0;
  for (size_t i = 0; i < gpScreensaverOptionCount(); i++) {
    if (!isConcreteScreensaverMode(GP_SCREENSAVER_OPTIONS[i].value)) continue;
    eligibleIndices[eligibleCount++] = static_cast<int>(i);
  }
  if (eligibleCount == 0) {
    activeRandomScreensaverIndex = 0;
    return;
  }

  int nextIndex = eligibleIndices[random(eligibleCount)];
  if (eligibleCount > 1 && nextIndex == activeRandomScreensaverIndex) {
    int currentPos = 0;
    for (int i = 0; i < eligibleCount; i++) {
      if (eligibleIndices[i] == activeRandomScreensaverIndex) {
        currentPos = i;
        break;
      }
    }
    nextIndex = eligibleIndices[(currentPos + 1 + random(eligibleCount - 1)) % eligibleCount];
  }
  activeRandomScreensaverIndex = nextIndex;
}

static const char *selectedScreensaverMode() {
  return bootScreensaverActive ? GP_DEFAULT_SAVER_MODE : gp_screensaver_mode;
}

static String selectedScreensaverLabel() {
  const char *mode = selectedScreensaverMode();
  for (size_t i = 0; i < gpScreensaverOptionCount(); i++) {
    if (strcmp(mode, GP_SCREENSAVER_OPTIONS[i].value) == 0) {
      return GP_SCREENSAVER_OPTIONS[i].label;
    }
  }
  return "Matrix";
}

static const char *currentScreensaverMode() {
  const char *mode = selectedScreensaverMode();
  if (!screensaverModeIsRandom(mode)) {
    return mode;
  }
  if (activeRandomScreensaverIndex < 0 ||
      activeRandomScreensaverIndex >= static_cast<int>(gpScreensaverOptionCount())) {
    chooseRandomScreensaverMode();
  }
  return GP_SCREENSAVER_OPTIONS[activeRandomScreensaverIndex].value;
}

static String currentScreensaverLabel() {
  if (screensaverModeIsRandom(selectedScreensaverMode()) && activeScreen != ScreenMode::Settings) {
    const char *activeMode = currentScreensaverMode();
    for (size_t i = 0; i < gpScreensaverOptionCount(); i++) {
      if (strcmp(activeMode, GP_SCREENSAVER_OPTIONS[i].value) == 0) {
        return GP_SCREENSAVER_OPTIONS[i].label;
      }
    }
  }
  return selectedScreensaverLabel();
}

static void enterScreensaver(bool bootEntry = false) {
  if (activeScreen == ScreenMode::Screensaver) {
    if (bootEntry) bootScreensaverActive = true;
    return;
  }
  activeScreen = ScreenMode::Screensaver;
  bootScreensaverActive = bootEntry;
  screensaverStartedMs = millis();
  lastScreensaverFrameMs = 0;
  if (screensaverModeIsRandom(selectedScreensaverMode())) {
    chooseRandomScreensaverMode();
    nextRandomScreensaverChangeMs = screensaverStartedMs + SCREENSAVER_RANDOM_SHIFT_MS;
  } else {
    activeRandomScreensaverIndex = -1;
    nextRandomScreensaverChangeMs = 0;
  }
  dirtyRegions = DIRTY_ALL;
}

static void exitScreensaver() {
  if (activeScreen != ScreenMode::Screensaver) return;
  bootScreensaverActive = false;
  activeScreen = ScreenMode::Chat;
  screensaverDismissUntilMs = millis() + SCREENSAVER_WAKE_GUARD_MS;
  noteUserActivity();
  markDirty(DIRTY_ALL);
}

static void drawMatrixScreensaver() {
  static struct {
    int x;
    int y;
    int speed;
    char chars[20];
  } streams[12];
  static bool initialized = false;

  if (!initialized) {
    for (int i = 0; i < 12; i++) {
      streams[i].x = random(M5Cardputer.Display.width());
      streams[i].y = random(-200, 0);
      streams[i].speed = random(2, 6);
      for (int j = 0; j < 20; j++) {
        streams[i].chars[j] = random(48, 90);
      }
    }
    initialized = true;
  }

  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setTextSize(1);
  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < 20; j++) {
      int drawY = streams[i].y + j * 8;
      if (drawY < 0 || drawY >= M5Cardputer.Display.height()) continue;
      uint16_t color = j < 2 ? 0xA7FF : (j < 7 ? 0x07E0 : 0x0320);
      M5Cardputer.Display.setCursor(streams[i].x, drawY);
      M5Cardputer.Display.setTextColor(color, BLACK);
      M5Cardputer.Display.print((char)streams[i].chars[j]);
    }
    streams[i].y += streams[i].speed;
    if (streams[i].y > M5Cardputer.Display.height() + 20) {
      streams[i].y = random(-180, -40);
      streams[i].x = random(M5Cardputer.Display.width());
      streams[i].speed = random(2, 6);
    }
    if (random(30) == 0) {
      streams[i].chars[random(20)] = random(48, 90);
    }
  }
}

static void drawBouncingBallsScreensaver() {
  static ScreensaverBall balls[8];
  static bool initialized = false;
  const int w = M5Cardputer.Display.width();
  const int h = M5Cardputer.Display.height();

  if (!initialized) {
    const uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, 0xFD20, 0xAFE5};
    for (int i = 0; i < 8; i++) {
      balls[i].radius = random(5, 11);
      balls[i].x = random(balls[i].radius, w - balls[i].radius);
      balls[i].y = random(balls[i].radius, h - balls[i].radius);
      balls[i].dx = random(8, 19) / 10.0f * (random(2) ? 1.0f : -1.0f);
      balls[i].dy = random(8, 19) / 10.0f * (random(2) ? 1.0f : -1.0f);
      balls[i].color = colors[i % 8];
    }
    initialized = true;
  }

  M5Cardputer.Display.fillScreen(BLACK);
  for (int i = 0; i < 8; i++) {
    balls[i].x += balls[i].dx;
    balls[i].y += balls[i].dy;
    if (balls[i].x <= balls[i].radius || balls[i].x >= w - balls[i].radius) {
      balls[i].dx = -balls[i].dx;
      balls[i].x = constrain(static_cast<int>(balls[i].x), balls[i].radius, w - balls[i].radius);
    }
    if (balls[i].y <= balls[i].radius || balls[i].y >= h - balls[i].radius) {
      balls[i].dy = -balls[i].dy;
      balls[i].y = constrain(static_cast<int>(balls[i].y), balls[i].radius, h - balls[i].radius);
    }
    M5Cardputer.Display.fillCircle(static_cast<int>(balls[i].x), static_cast<int>(balls[i].y), balls[i].radius, balls[i].color);
  }
}

static void drawKaleidoscopeScreensaver() {
  static float timePhase = 0.0f;
  timePhase += 0.075f;

  const int w = M5Cardputer.Display.width();
  const int h = M5Cardputer.Display.height();
  const int cx = w / 2;
  const int cy = h / 2;
  const int maxRadius = max(w, h);
  M5Cardputer.Display.fillScreen(BLACK);

  for (int i = 0; i < 34; i++) {
    float radius = (0.16f + 0.025f * i) * maxRadius;
    float baseAngle = timePhase * (0.45f + i * 0.015f) + i * 0.35f;
    int orbitRadius = 2 + (i % 4);
    uint8_t red = static_cast<uint8_t>(sin(baseAngle * 1.2f) * 100 + 140);
    uint8_t green = static_cast<uint8_t>(sin(baseAngle * 1.6f + 2.0f) * 100 + 140);
    uint8_t blue = static_cast<uint8_t>(sin(baseAngle * 1.1f + 4.0f) * 100 + 140);
    uint16_t color = M5Cardputer.Display.color565(red, green, blue);

    for (int mirror = 0; mirror < 8; mirror++) {
      float angle = baseAngle + mirror * (PI / 4.0f);
      int x = cx + static_cast<int>(cos(angle) * radius);
      int y = cy + static_cast<int>(sin(angle) * radius);
      int mx = cx - (x - cx);
      int my = cy - (y - cy);
      M5Cardputer.Display.fillCircle(x, y, orbitRadius, color);
      M5Cardputer.Display.fillCircle(mx, y, orbitRadius, color);
      M5Cardputer.Display.fillCircle(x, my, orbitRadius, color);
    }
  }
}

static void drawTetrisRainScreensaver() {
  static TetrisRainPiece pieces[7];
  static bool initialized = false;
  static const int shapes[7][4][2] = {
    {{0, 0}, {1, 0}, {-1, 0}, {2, 0}},
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
    {{0, 0}, {-1, 0}, {1, 0}, {0, 1}},
    {{0, 0}, {1, 0}, {0, 1}, {-1, 1}},
    {{0, 0}, {-1, 0}, {0, 1}, {1, 1}},
    {{0, 0}, {-1, 0}, {1, 0}, {1, 1}},
    {{0, 0}, {-1, 0}, {1, 0}, {-1, 1}},
  };
  static const uint16_t colors[] = {CYAN, YELLOW, MAGENTA, GREEN, RED, BLUE, 0xFD20};
  const int block = 6;
  const int w = M5Cardputer.Display.width();
  const int h = M5Cardputer.Display.height();

  if (!initialized) {
    for (int i = 0; i < 7; i++) {
      pieces[i].x = random(16, w - 16);
      pieces[i].y = random(-h, 0);
      pieces[i].speed = random(8, 16) / 10.0f;
      pieces[i].shape = i;
      pieces[i].rotation = random(4);
      pieces[i].color = colors[i];
    }
    initialized = true;
  }

  M5Cardputer.Display.fillScreen(BLACK);
  for (int i = 0; i < 7; i++) {
    pieces[i].y += pieces[i].speed;
    if (pieces[i].y > h + 18) {
      pieces[i].x = random(16, w - 16);
      pieces[i].y = random(-60, -12);
      pieces[i].speed = random(8, 16) / 10.0f;
      pieces[i].rotation = random(4);
      pieces[i].shape = random(7);
      pieces[i].color = colors[pieces[i].shape];
    }

    for (int blockIndex = 0; blockIndex < 4; blockIndex++) {
      int px = shapes[pieces[i].shape][blockIndex][0];
      int py = shapes[pieces[i].shape][blockIndex][1];
      for (int step = 0; step < pieces[i].rotation; step++) {
        int nextX = -py;
        int nextY = px;
        px = nextX;
        py = nextY;
      }
      int drawX = pieces[i].x + px * block;
      int drawY = static_cast<int>(pieces[i].y) + py * block;
      M5Cardputer.Display.fillRect(drawX, drawY, block, block, pieces[i].color);
      M5Cardputer.Display.drawRect(drawX, drawY, block, block, COLOR_DIM);
    }
  }
}

static void drawStarfieldScreensaver() {
  static ScreensaverStar stars[72];
  static bool initialized = false;
  const int w = M5Cardputer.Display.width();
  const int h = M5Cardputer.Display.height();
  const int cx = w / 2;
  const int cy = h / 2;

  if (!initialized) {
    const uint16_t palette[] = {WHITE, CYAN, YELLOW, MAGENTA};
    for (int i = 0; i < 72; i++) {
      stars[i].x = random(-w, w);
      stars[i].y = random(-h, h);
      stars[i].z = random(20, 120);
      stars[i].color = palette[random(4)];
    }
    initialized = true;
  }

  M5Cardputer.Display.fillScreen(BLACK);
  for (int i = 0; i < 72; i++) {
    int x = cx + static_cast<int>((stars[i].x * 96.0f) / stars[i].z);
    int y = cy + static_cast<int>((stars[i].y * 96.0f) / stars[i].z);
    int size = max(1, static_cast<int>(120.0f / stars[i].z));
    if (x >= 0 && x < w && y >= 0 && y < h) {
      M5Cardputer.Display.fillCircle(x, y, size, stars[i].color);
    }
    stars[i].z -= 2.2f;
    if (stars[i].z <= 2.0f) {
      stars[i].x = random(-w, w);
      stars[i].y = random(-h, h);
      stars[i].z = 120.0f;
    }
  }
}

static void drawCriticalScreensaver() {
  static float criticalTime = 0.0f;
  criticalTime += 0.1f;
  bool flash = (static_cast<int>(criticalTime * 8) % 2) == 0;
  M5Cardputer.Display.fillScreen(flash ? 0x1800 : BLACK);
  M5Cardputer.Display.setTextColor(flash ? WHITE : RED, flash ? 0x1800 : BLACK);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(20, 20);
  M5Cardputer.Display.print("CRITICAL");
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(12, 48);
  M5Cardputer.Display.print("SYSTEM FAILURE IMMINENT");
  M5Cardputer.Display.setCursor(12, 64);
  M5Cardputer.Display.print("Temp: " + String(150 + static_cast<int>(sin(criticalTime * 4) * 45)) + "C");
  M5Cardputer.Display.setCursor(12, 78);
  M5Cardputer.Display.print("Memory: " + String(95 + static_cast<int>(sin(criticalTime * 2) * 4)) + "%");
  M5Cardputer.Display.setCursor(12, 92);
  M5Cardputer.Display.print("Disk: " + String(98 + static_cast<int>(sin(criticalTime * 3) * 2)) + "%");
  M5Cardputer.Display.setTextColor(0x07E0, flash ? 0x1800 : BLACK);
  for (int i = 0; i < 3; i++) {
    M5Cardputer.Display.setCursor(12, 108 + i * 8);
    int errorCode = 0x8000 + (static_cast<int>(criticalTime * 100) + i * 137) % 0x0FFF;
    M5Cardputer.Display.printf("ERR 0x%04X", errorCode);
  }
}

static void drawPlasmaScreensaver() {
  static float timePhase = 0.0f;
  timePhase += 0.08f;
  const int w = M5Cardputer.Display.width();
  const int h = M5Cardputer.Display.height();
  M5Cardputer.Display.fillScreen(BLACK);
  for (int x = 0; x < w; x += 3) {
    for (int y = 0; y < h; y += 3) {
      float value = sin(x * 0.10f + timePhase) +
                    sin(y * 0.11f + timePhase * 1.3f) +
                    sin((x + y) * 0.05f + timePhase * 0.7f);
      uint8_t red = static_cast<uint8_t>(sin(value + timePhase) * 127 + 128);
      uint8_t green = static_cast<uint8_t>(sin(value + timePhase + 2) * 127 + 128);
      uint8_t blue = static_cast<uint8_t>(sin(value + timePhase + 4) * 127 + 128);
      M5Cardputer.Display.fillRect(x, y, 3, 3, M5Cardputer.Display.color565(red, green, blue));
    }
  }
}

static void renderScreensaverFrame(bool force = false) {
  if (activeScreen != ScreenMode::Screensaver) return;
  unsigned long now = millis();
  if (!force && now - lastScreensaverFrameMs < SCREENSAVER_FRAME_MS) return;
  lastScreensaverFrameMs = now;

  if (!bootScreensaverActive &&
      screensaverModeIsRandom(selectedScreensaverMode()) &&
      nextRandomScreensaverChangeMs > 0 &&
      now >= nextRandomScreensaverChangeMs) {
    chooseRandomScreensaverMode();
    nextRandomScreensaverChangeMs = now + SCREENSAVER_RANDOM_SHIFT_MS;
  }

  const char *mode = currentScreensaverMode();
  if (strcmp(mode, "bouncing-balls") == 0) {
    drawBouncingBallsScreensaver();
  } else if (strcmp(mode, "kaleidoscope") == 0) {
    drawKaleidoscopeScreensaver();
  } else if (strcmp(mode, "tetris-rain") == 0) {
    drawTetrisRainScreensaver();
  } else if (strcmp(mode, "starfield") == 0) {
    drawStarfieldScreensaver();
  } else if (strcmp(mode, "critical") == 0) {
    drawCriticalScreensaver();
  } else if (strcmp(mode, "plasma") == 0) {
    drawPlasmaScreensaver();
  } else {
    drawMatrixScreensaver();
  }

  if (bootScreensaverActive) {
    M5Cardputer.Display.fillRoundRect(10, 8, 128, 32, 6, 0x0000);
    M5Cardputer.Display.drawRoundRect(10, 8, 128, 32, 6, COLOR_DIM);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT, BLACK);
    M5Cardputer.Display.setCursor(18, 18);
    M5Cardputer.Display.print("Groqputer");
    M5Cardputer.Display.setTextColor(COLOR_DIM, BLACK);
    M5Cardputer.Display.setCursor(18, 30);
    M5Cardputer.Display.print("Press key to wake");
  } else if (now - screensaverStartedMs < 1400) {
    M5Cardputer.Display.fillRoundRect(8, 8, 110, 18, 6, 0x0000);
    M5Cardputer.Display.setTextColor(COLOR_DIM, BLACK);
    M5Cardputer.Display.setCursor(14, 20);
    M5Cardputer.Display.print(currentScreensaverLabel());
  }
}

static String normalizeBaseUrl(const String &value) {
  String normalized = value;
  normalized.trim();
  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }
  return normalized;
}

static bool ensureSdCardReady(String &errorOut) {
  errorOut = "";
  if (sdCardReady) {
    return true;
  }
  if (!sdCardInitAttempted) {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    sdCardReady = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
    sdCardInitAttempted = true;
  }
  if (!sdCardReady) {
    errorOut = "SD card is not ready.";
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    errorOut = "No SD card attached.";
    sdCardReady = false;
    return false;
  }
  if (!SD.exists("/camera")) {
    SD.mkdir("/camera");
  }
  return true;
}

static bool captureEsp32CamPhoto(String &savedPathOut, String &errorOut) {
  savedPathOut = "";
  errorOut = "";

  String cameraBaseUrl = normalizeBaseUrl(String(gp_camera_base_url));
  if (!cameraBaseUrl.length()) {
    errorOut = "Add ESP32-CAM URL in setup first.";
    return false;
  }
  if (!gpEnsureWifiConnected()) {
    errorOut = "WiFi is not connected.";
    return false;
  }

  HTTPClient statusHttp;
  if (!statusHttp.begin(cameraBaseUrl + "/status")) {
    errorOut = "Camera status request failed.";
    return false;
  }
  statusHttp.setTimeout(10000);
  int statusCode = statusHttp.GET();
  String statusBody = statusHttp.getString();
  statusHttp.end();
  if (statusCode < 200 || statusCode >= 300) {
    errorOut = statusBody.length() ? statusBody : String("Camera status HTTP ") + statusCode;
    return false;
  }

  String sdError;
  if (!ensureSdCardReady(sdError)) {
    errorOut = sdError;
    return false;
  }

  String filePath = "/camera/capture-" + String(millis()) + ".jpg";
  File file = SD.open(filePath, FILE_WRITE);
  if (!file) {
    errorOut = "Could not open SD file for photo.";
    return false;
  }

  HTTPClient imageHttp;
  if (!imageHttp.begin(cameraBaseUrl + "/latest.jpg")) {
    file.close();
    SD.remove(filePath);
    errorOut = "Camera image request failed.";
    return false;
  }
  imageHttp.setTimeout(15000);
  int imageCode = imageHttp.GET();
  if (imageCode < 200 || imageCode >= 300) {
    String body = imageHttp.getString();
    imageHttp.end();
    file.close();
    SD.remove(filePath);
    errorOut = body.length() ? body : String("Camera image HTTP ") + imageCode;
    return false;
  }

  WiFiClient *stream = imageHttp.getStreamPtr();
  int remaining = imageHttp.getSize();
  uint8_t buffer[CAMERA_DOWNLOAD_CHUNK];
  size_t totalWritten = 0;
  unsigned long idleDeadline = millis() + 15000;
  while (imageHttp.connected() && (remaining > 0 || remaining == -1)) {
    size_t available = stream->available();
    if (available == 0) {
      if (millis() > idleDeadline) {
        break;
      }
      delay(1);
      continue;
    }

    size_t toRead = min(available, sizeof(buffer));
    int bytesRead = stream->readBytes(buffer, toRead);
    if (bytesRead <= 0) {
      break;
    }
    size_t written = file.write(buffer, bytesRead);
    totalWritten += written;
    idleDeadline = millis() + 15000;
    if (remaining > 0) {
      remaining -= bytesRead;
    }
  }

  imageHttp.end();
  file.close();

  if (totalWritten < 512) {
    SD.remove(filePath);
    errorOut = "Camera photo download was too small.";
    return false;
  }

  savedPathOut = filePath;
  return true;
}

static bool isJpegPath(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".jpg") || lowered.endsWith(".jpeg");
}

static bool refreshCameraPhotoList(String &errorOut) {
  errorOut = "";
  cameraPhotoPaths.clear();
  activePhotoIndex = -1;

  String sdError;
  if (!ensureSdCardReady(sdError)) {
    errorOut = sdError;
    return false;
  }

  File directory = SD.open("/camera");
  if (!directory || !directory.isDirectory()) {
    errorOut = "Camera folder is missing.";
    return false;
  }

  while (true) {
    File entry = directory.openNextFile();
    if (!entry) {
      break;
    }
    if (!entry.isDirectory()) {
      String path = String(entry.name());
      if (!path.startsWith("/")) {
        path = String("/camera/") + path;
      }
      if (isJpegPath(path)) {
        cameraPhotoPaths.push_back(path);
      }
    }
    entry.close();
  }
  directory.close();

  if (cameraPhotoPaths.empty()) {
    errorOut = "No saved photos on SD.";
    return false;
  }

  std::sort(cameraPhotoPaths.begin(), cameraPhotoPaths.end(), [](const String &left, const String &right) {
    return left.compareTo(right) < 0;
  });
  return true;
}

static bool openPhotoViewer(const String &preferredPath, String &errorOut) {
  if (!refreshCameraPhotoList(errorOut)) {
    return false;
  }

  activePhotoIndex = static_cast<int>(cameraPhotoPaths.size()) - 1;
  activePhotoRotationQuarterTurns = 0;
  if (preferredPath.length()) {
    for (size_t i = 0; i < cameraPhotoPaths.size(); i++) {
      if (cameraPhotoPaths[i] == preferredPath) {
        activePhotoIndex = static_cast<int>(i);
        break;
      }
    }
  }

  activeScreen = ScreenMode::PhotoViewer;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  return true;
}

static void navigatePhotoViewer(int delta) {
  if (activeScreen != ScreenMode::PhotoViewer) {
    return;
  }
  if (cameraPhotoPaths.empty() || activePhotoIndex < 0) {
    setToast("No saved photos on SD.");
    return;
  }

  int next = constrain(activePhotoIndex + delta, 0, static_cast<int>(cameraPhotoPaths.size()) - 1);
  if (next == activePhotoIndex) {
    setToast(delta < 0 ? "Oldest photo." : "Newest photo.");
    return;
  }

  activePhotoIndex = next;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static String activePhotoFilename() {
  if (activePhotoIndex < 0 || activePhotoIndex >= static_cast<int>(cameraPhotoPaths.size())) {
    return "";
  }
  String path = cameraPhotoPaths[activePhotoIndex];
  int slashPos = path.lastIndexOf('/');
  return slashPos >= 0 ? path.substring(slashPos + 1) : path;
}

static bool readFileByte(File &file, uint8_t &value) {
  int next = file.read();
  if (next < 0) {
    return false;
  }
  value = static_cast<uint8_t>(next);
  return true;
}

static bool readFileBigEndian16(File &file, uint16_t &value) {
  uint8_t high = 0;
  uint8_t low = 0;
  if (!readFileByte(file, high) || !readFileByte(file, low)) {
    return false;
  }
  value = static_cast<uint16_t>((high << 8) | low);
  return true;
}

static bool readJpegDimensions(File &file, int &widthOut, int &heightOut) {
  widthOut = 0;
  heightOut = 0;
  if (!file.seek(0)) {
    return false;
  }

  uint8_t first = 0;
  uint8_t second = 0;
  if (!readFileByte(file, first) || !readFileByte(file, second) || first != 0xFF || second != 0xD8) {
    return false;
  }

  while (file.available()) {
    uint8_t markerPrefix = 0;
    if (!readFileByte(file, markerPrefix)) {
      return false;
    }
    if (markerPrefix != 0xFF) {
      continue;
    }

    uint8_t marker = 0;
    do {
      if (!readFileByte(file, marker)) {
        return false;
      }
    } while (marker == 0xFF);

    if (marker == 0xD8 || marker == 0xD9 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      continue;
    }

    uint16_t segmentLength = 0;
    if (!readFileBigEndian16(file, segmentLength) || segmentLength < 2) {
      return false;
    }

    const bool isStartOfFrame =
      marker == 0xC0 || marker == 0xC1 || marker == 0xC2 || marker == 0xC3 ||
      marker == 0xC5 || marker == 0xC6 || marker == 0xC7 ||
      marker == 0xC9 || marker == 0xCA || marker == 0xCB ||
      marker == 0xCD || marker == 0xCE || marker == 0xCF;
    if (isStartOfFrame) {
      uint8_t precision = 0;
      uint16_t height = 0;
      uint16_t width = 0;
      if (!readFileByte(file, precision) ||
          !readFileBigEndian16(file, height) ||
          !readFileBigEndian16(file, width)) {
        return false;
      }
      (void)precision;
      widthOut = static_cast<int>(width);
      heightOut = static_cast<int>(height);
      return widthOut > 0 && heightOut > 0;
    }

    size_t currentPos = file.position();
    if (!file.seek(currentPos + segmentLength - 2)) {
      return false;
    }
  }

  return false;
}

static void computePhotoFillParams(int sourceWidth, int sourceHeight, int targetWidth, int targetHeight,
                                   float &scaleOut, int &offsetXOut, int &offsetYOut) {
  float widthScale = static_cast<float>(targetWidth) / static_cast<float>(sourceWidth);
  float heightScale = static_cast<float>(targetHeight) / static_cast<float>(sourceHeight);
  scaleOut = max(widthScale, heightScale);
  int scaledWidth = static_cast<int>(ceilf(static_cast<float>(sourceWidth) * scaleOut));
  int scaledHeight = static_cast<int>(ceilf(static_cast<float>(sourceHeight) * scaleOut));
  offsetXOut = max(0, (scaledWidth - targetWidth) / 2);
  offsetYOut = max(0, (scaledHeight - targetHeight) / 2);
}

static void drawPhotoViewer(int x, int y, int w, int h) {
  if (activePhotoIndex < 0 || activePhotoIndex >= static_cast<int>(cameraPhotoPaths.size())) {
    M5Cardputer.Display.setTextColor(COLOR_ERROR, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, y + 18);
    M5Cardputer.Display.print("No photo selected.");
    return;
  }

  M5Cardputer.Display.fillRect(x, y, w, h, COLOR_BG);
  bool drawn = false;
  File photoFile = SD.open(cameraPhotoPaths[activePhotoIndex].c_str(), FILE_READ);
  if (photoFile) {
    int sourceWidth = 0;
    int sourceHeight = 0;
    if (readJpegDimensions(photoFile, sourceWidth, sourceHeight) && photoFile.seek(0)) {
      int spriteWidth = w;
      int spriteHeight = h;
      if (activePhotoRotationQuarterTurns % 2 == 1) {
        spriteWidth = h;
        spriteHeight = w;
      }

      M5Canvas photoCanvas(&M5Cardputer.Display);
      photoCanvas.setColorDepth(16);
      if (photoCanvas.createSprite(spriteWidth, spriteHeight)) {
        photoCanvas.fillSprite(COLOR_BG);
        float scale = 1.0f;
        int offsetX = 0;
        int offsetY = 0;
        computePhotoFillParams(sourceWidth, sourceHeight, spriteWidth, spriteHeight, scale, offsetX, offsetY);
        drawn = photoCanvas.drawJpg(
          &photoFile,
          0,
          0,
          spriteWidth,
          spriteHeight,
          offsetX,
          offsetY,
          scale,
          scale,
          top_left
        );

        if (drawn) {
          if (activePhotoRotationQuarterTurns == 0) {
            photoCanvas.pushSprite(x, y);
          } else {
            photoCanvas.setPivot((photoCanvas.width() / 2.0f) - 0.5f, (photoCanvas.height() / 2.0f) - 0.5f);
            photoCanvas.pushRotateZoom(
              &M5Cardputer.Display,
              x + (w / 2.0f),
              y + (h / 2.0f),
              activePhotoRotationQuarterTurns * 90.0f,
              1.0f,
              1.0f
            );
          }
        }
        photoCanvas.deleteSprite();
      }
    }
    photoFile.close();
  }

  if (!drawn) {
    M5Cardputer.Display.setTextColor(COLOR_ERROR, COLOR_BG);
    M5Cardputer.Display.setCursor(x + 8, y + 10);
    M5Cardputer.Display.print("Photo decode failed.");
    return;
  }

  M5Cardputer.Display.fillRect(x + 4, y + 4, 56, 10, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(x + 6, y + 12);
  M5Cardputer.Display.print(String(activePhotoIndex + 1) + "/" + String(cameraPhotoPaths.size()) +
                            " R" + String(activePhotoRotationQuarterTurns * 90));
}

static String clampLogText(const String &value) {
  if (value.length() <= MAX_DISPLAY_CHARS) return value;
  return value.substring(value.length() - MAX_DISPLAY_CHARS);
}

static const char *currentPaneLabel() {
  return activePane == ChatPane::Incoming ? "CHAT" : "DRAFT";
}

static String gpCurrentPersonalityLabel() {
  int personalityIndex = gpCurrentPersonalityPresetIndex();
  if (personalityIndex < 0) {
    return "Custom";
  }
  return gpPersonalityPresetLabelAt(static_cast<size_t>(personalityIndex));
}

static void drawSettingsView(int x, int y, int w, int h) {
  const int lineHeight = 11;
  const int titleY = y;
  const int bodyY = y + 14;
  const int hintY = y + h - 10;
  const int bodyH = max(1, hintY - bodyY - 2);
  const int totalRows = settingFieldCount();
  const int selectedIndex = currentSettingFieldIndex();
  const int visibleRows = max(1, min(totalRows, bodyH / lineHeight));
  const int startRow = constrain(selectedIndex - (visibleRows / 2), 0, max(0, totalRows - visibleRows));
  const int endRow = min(totalRows, startRow + visibleRows);
  const int labelWidth = 50;

  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, titleY);
  M5Cardputer.Display.print("SETTINGS");
  M5Cardputer.Display.drawFastHLine(x, y + 11, w, COLOR_DIM);

  for (int row = startRow; row < endRow; row++) {
    SettingField field = settingFieldAt(row);
    bool selected = row == selectedIndex;
    int rowY = bodyY + ((row - startRow) * lineHeight);

    M5Cardputer.Display.setTextColor(selected ? COLOR_WARN : COLOR_DIM, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, rowY);
    M5Cardputer.Display.print(selected ? "> " : "  ");
    M5Cardputer.Display.print(settingFieldLabel(field));

    String value = truncateFont0Text(settingFieldValue(field), max(4, (w - labelWidth) / 6));
    M5Cardputer.Display.setTextColor(settingFieldEditable(field) ? (selected ? COLOR_WARN : COLOR_TEXT) : COLOR_TEXT, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x + labelWidth, rowY);
    M5Cardputer.Display.print(value);
  }

  String hint = String(selectedIndex + 1) + "/" + String(totalRows);
  if (startRow > 0) hint += " ^";
  if (endRow < totalRows) hint += " v";
  if (!settingFieldEditable(settingFieldAt(selectedIndex))) {
    hint += "  info";
  }
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, hintY);
  M5Cardputer.Display.print(hint);
}

static void drawBotSettingsView(int x, int y, int w, int h) {
  const int lineHeight = 11;
  int cursorY = y;

  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("BOT SETTINGS");
  cursorY += lineHeight + 2;

  M5Cardputer.Display.setTextColor(activeBotSettingField == BotSettingField::Model ? COLOR_WARN : COLOR_TEXT, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print(activeBotSettingField == BotSettingField::Model ? "> " : "  ");
  M5Cardputer.Display.print("Model:");
  cursorY += lineHeight;
  M5Cardputer.Display.setCursor(x + 8, cursorY);
  M5Cardputer.Display.print(GP_MODEL_OPTIONS[gpCurrentModelOptionIndex()].label);
  cursorY += lineHeight + 1;

  M5Cardputer.Display.setTextColor(activeBotSettingField == BotSettingField::Personality ? COLOR_WARN : COLOR_TEXT, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print(activeBotSettingField == BotSettingField::Personality ? "> " : "  ");
  M5Cardputer.Display.print("Persona:");
  cursorY += lineHeight;
  M5Cardputer.Display.setCursor(x + 8, cursorY);
  M5Cardputer.Display.print(gpCurrentPersonalityLabel());
  cursorY += lineHeight + 1;

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  if (gpCurrentPersonalityPresetIndex() < 0) {
    M5Cardputer.Display.print("Current prompt is custom.");
    cursorY += lineHeight;
  }
  M5Cardputer.Display.setCursor(x, min(y + h - 22, cursorY + 1));
  M5Cardputer.Display.print("Fn+;/. row  Fn+,// set");
  M5Cardputer.Display.setCursor(x, y + h - 10);
  M5Cardputer.Display.print("Fn+B close  saves live");
}

static void drawHotkeysView(int x, int y, int w, int h) {
  const int lineHeight = 10;
  const int titleY = y;
  const int entriesY = y + 20;
  const int entriesVisible = 4;
  const int detailY = y + 64;
  const int hintY = y + h - 10;
  const CommandGuideEntry *entries = commandGuideEntries(activeCommandGuideSection);
  const int count = static_cast<int>(commandGuideEntryCount(activeCommandGuideSection));
  clampCommandGuideIndex();
  const int selected = constrain(activeCommandGuideIndex, 0, max(0, count - 1));
  const int start = constrain(selected - 1, 0, max(0, count - entriesVisible));

  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, titleY);
  M5Cardputer.Display.print("COMMANDS");

  String sectionLabel = String(commandGuideSectionLabel(activeCommandGuideSection)) +
                        " " + String(selected + 1) + "/" + String(max(1, count));
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(max(x + 58, x + w - font0TextWidth(sectionLabel)), titleY);
  M5Cardputer.Display.print(sectionLabel);
  M5Cardputer.Display.drawFastHLine(x, y + 11, w, COLOR_DIM);

  for (int row = 0; row < entriesVisible && start + row < count; row++) {
    const int entryIndex = start + row;
    const bool selectedRow = entryIndex == selected;
    const int rowY = entriesY + row * lineHeight;
    String label = truncateFont0Text(entries[entryIndex].label, max(8, (w - 12) / 6));
    M5Cardputer.Display.setTextColor(selectedRow ? COLOR_WARN : COLOR_TEXT, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, rowY);
    M5Cardputer.Display.print(selectedRow ? "> " : "  ");
    M5Cardputer.Display.print(label);
  }

  M5Cardputer.Display.drawFastHLine(x, detailY - 4, w, COLOR_DIM);
  if (count > 0) {
    String detailLines[5];
    int detailLineCount = 0;
    fillWrappedLines(entries[selected].detail, detailLines, detailLineCount, 5, max(8, w / 6));
    M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, detailY);
    M5Cardputer.Display.print(truncateFont0Text(entries[selected].label, max(8, w / 6)));
    M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
    for (int i = 0; i < detailLineCount && i < 3; i++) {
      M5Cardputer.Display.setCursor(x, detailY + 10 + i * 9);
      M5Cardputer.Display.print(detailLines[i]);
    }
  }

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, hintY);
  M5Cardputer.Display.print("Fn+;/. item Fn+,// tab");
}

static String customPersonalityStageLabel() {
  switch (activeCustomPersonalityStage) {
    case CustomPersonalityStage::Prompt:
      return "STEP 1/3 PROMPT";
    case CustomPersonalityStage::Name:
      return "STEP 2/3 NAME";
    case CustomPersonalityStage::Confirm:
      return "STEP 3/3 CONFIRM";
  }
  return "";
}

static void drawCustomPersonalityView(int x, int y, int w, int h) {
  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, y);
  M5Cardputer.Display.print("CUSTOM BOT");
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x + 62, y);
  M5Cardputer.Display.print(customPersonalityStageLabel());
  M5Cardputer.Display.drawFastHLine(x, y + 10, w, COLOR_DIM);

  const int bodyY = y + 16;
  const int bodyH = max(1, h - 18);
  if (activeCustomPersonalityStage == CustomPersonalityStage::Confirm) {
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, bodyY);
    M5Cardputer.Display.print("Name:");
    M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x + 32, bodyY);
    M5Cardputer.Display.print(customPersonalityNameBuffer.length() ? customPersonalityNameBuffer : "(none)");

    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, bodyY + 14);
    M5Cardputer.Display.print("Prompt:");

    const int maxChars = max(8, (w / 6) - 1);
    String lines[10];
    int lineCount = 0;
    fillWrappedLines(customPersonalityPromptBuffer.length() ? customPersonalityPromptBuffer : "—", lines, lineCount, 10, maxChars);
    int cursorY = bodyY + 26;
    M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
    for (int i = 0; i < min(lineCount, 5); i++) {
      M5Cardputer.Display.setCursor(x, cursorY);
      M5Cardputer.Display.print(lines[i]);
      cursorY += 10;
    }

    M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, y + h - 30);
    M5Cardputer.Display.print("Y save   T test");
    M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, y + h - 18);
    M5Cardputer.Display.print("N cancel");
    return;
  }

  String editorText = activeCustomPersonalityStage == CustomPersonalityStage::Prompt
    ? customPersonalityPromptBuffer
    : customPersonalityNameBuffer;
  String helperText = activeCustomPersonalityStage == CustomPersonalityStage::Prompt
    ? "Type what the bot should be, then Enter."
    : "Type the bot name, then Enter.";

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, bodyY);
  M5Cardputer.Display.print(helperText);

  const int textY = bodyY + 12;
  const int textH = max(1, bodyH - 14);
  const int maxChars = max(8, (w / scaledCharWidth()) - 1);
  const int visibleLines = max(1, textH / scaledLineHeight());
  String lines[160];
  int lineCount = 0;
  fillWrappedLines(editorText.length() ? editorText : "_", lines, lineCount, 160, maxChars);
  int startLine = max(0, lineCount - visibleLines);

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  M5Cardputer.Display.setTextSize(messageTextScale());
  int cursorY = textY;
  for (int i = startLine; i < min(lineCount, startLine + visibleLines); i++) {
    M5Cardputer.Display.setCursor(x, cursorY);
    M5Cardputer.Display.print(lines[i]);
    cursorY += scaledLineHeight();
  }
  M5Cardputer.Display.setTextSize(1);
}

static void rebuildTurnPagesFromPersistedChat() {
  int previousTurnIndex = activeTurnIndex;
  chatTurnPages.clear();
  activeTurnIndex = -1;
  if (!gp_chat_history.length()) {
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, gp_chat_history)) {
    return;
  }
  JsonArray history = doc.as<JsonArray>();
  if (history.isNull()) {
    return;
  }

  String pendingUserText;
  for (JsonVariant value : history) {
    String role = String(value["role"] | "");
    String content = clampLogText(String(value["content"] | ""));
    content.trim();
    if (!content.length()) continue;
    if (role == "user") {
      pendingUserText = content;
    } else if (role == "assistant") {
      ChatTurnPage turnPage;
      turnPage.userText = pendingUserText.length() ? pendingUserText : "No user message recorded.";
      turnPage.replyText = content;
      chatTurnPages.push_back(turnPage);
      pendingUserText = "";
    }
  }

  if (chatTurnPages.empty()) {
    activeTurnIndex = -1;
    return;
  }

  if (previousTurnIndex < 0 || previousTurnIndex >= static_cast<int>(chatTurnPages.size())) {
    activeTurnIndex = static_cast<int>(chatTurnPages.size()) - 1;
  } else {
    activeTurnIndex = previousTurnIndex;
  }
  resetReplyScroll();
}

static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  String source = sourceText;
  source.replace("\r", "");
  if (source.length() > 1400) {
    source = source.substring(source.length() - 1400);
  }
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
  if (currentLine.length() > 0 && lineCount < maxLines) {
    lines[lineCount++] = currentLine;
  }
}

static String currentTurnIndicator() {
  if (chatTurnPages.empty()) return "0/0";
  int current = constrain(activeTurnIndex, 0, static_cast<int>(chatTurnPages.size()) - 1);
  return String(current + 1) + "/" + String(chatTurnPages.size());
}

static void resetReplyScroll(unsigned long pauseMs) {
  activeReplyScrollOffset = 0;
  lastReplyAutoScrollMs = 0;
  replyAutoScrollComplete = false;
  replyAutoScrollPauseUntilMs = millis() + pauseMs;
}

static int currentReplyMaxScrollOffset(int width, int height) {
  if (chatTurnPages.empty() || activeTurnIndex < 0 || activeTurnIndex >= static_cast<int>(chatTurnPages.size())) {
    return 0;
  }
  const ChatTurnPage &turnPage = chatTurnPages[activeTurnIndex];
  if (shouldUseHorizontalReader()) {
    const int visibleChars = max(4, width / horizontalReaderCharWidth());
    return marqueeOffsetLimit(turnPage.replyText.length() ? turnPage.replyText : "—", visibleChars);
  }
  const int bodyHeight = max(1, height - 18);
  const int maxChars = max(8, (width / scaledCharWidth()) - 1);
  const int visibleReplyLines = max(1, bodyHeight / scaledLineHeight());
  String replyLines[160];
  int replyLineCount = 0;
  fillWrappedLines(turnPage.replyText.length() ? turnPage.replyText : "—", replyLines, replyLineCount, 160, maxChars);
  return max(0, replyLineCount - visibleReplyLines);
}

static void scrollCurrentReply(int delta, int width, int height) {
  int maxOffset = currentReplyMaxScrollOffset(width, height);
  if (maxOffset <= 0) {
    setToast("Reply fits on one page.");
    return;
  }
  if (shouldUseHorizontalReader()) {
    int step = delta < 0 ? -1 : 1;
    int next = constrain(activeReplyScrollOffset + step, 0, maxOffset);
    if (next == activeReplyScrollOffset) {
      setToast(delta < 0 ? "Start of marquee." : "End of marquee.");
      return;
    }
    activeReplyScrollOffset = next;
    replyAutoScrollComplete = false;
    replyAutoScrollPauseUntilMs = millis() + TURN_AUTO_SCROLL_PAUSE_MS;
    lastReplyAutoScrollMs = millis();
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
    return;
  }
  int next = constrain(activeReplyScrollOffset + delta, 0, maxOffset);
  if (next == activeReplyScrollOffset) {
    setToast(delta < 0 ? "Top of reply." : "End of reply.");
    return;
  }
  activeReplyScrollOffset = next;
  replyAutoScrollComplete = next >= maxOffset;
  replyAutoScrollPauseUntilMs = millis() + TURN_AUTO_SCROLL_PAUSE_MS;
  lastReplyAutoScrollMs = millis();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void pollReplyAutoScroll() {
  if (activePane != ChatPane::Incoming || activeScreen != ScreenMode::Chat) return;
  if (replyAutoScrollPauseUntilMs > millis()) return;
  const int contentW = M5Cardputer.Display.width() - (CONTENT_MARGIN * 2) - 12;
  const int contentH = M5Cardputer.Display.height() - HEADER_HEIGHT - FOOTER_HEIGHT - (CONTENT_MARGIN * 2) - 28;
  if (shouldUseHorizontalReader()) {
    int maxOffset = currentReplyMaxScrollOffset(contentW, contentH);
    if (maxOffset <= 0) return;
    if (lastReplyAutoScrollMs != 0 && millis() - lastReplyAutoScrollMs < currentReaderAutoScrollIntervalMs()) {
      return;
    }
    activeReplyScrollOffset = (activeReplyScrollOffset + 1) % (maxOffset + 1);
    replyAutoScrollComplete = false;
    lastReplyAutoScrollMs = millis();
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
    return;
  }
  if (replyAutoScrollComplete) return;
  int maxOffset = currentReplyMaxScrollOffset(contentW, contentH);
  if (maxOffset <= 0) return;
  if (lastReplyAutoScrollMs != 0 && millis() - lastReplyAutoScrollMs < currentReaderAutoScrollIntervalMs()) {
    return;
  }

  if (activeReplyScrollOffset >= maxOffset) {
    replyAutoScrollComplete = true;
    lastReplyAutoScrollMs = 0;
    return;
  }
  activeReplyScrollOffset += 1;
  if (activeReplyScrollOffset >= maxOffset) {
    activeReplyScrollOffset = maxOffset;
    replyAutoScrollComplete = true;
  }
  lastReplyAutoScrollMs = millis();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static const ChatTurnPage *activeTurnPage() {
  if (chatTurnPages.empty() || activeTurnIndex < 0 || activeTurnIndex >= static_cast<int>(chatTurnPages.size())) {
    return nullptr;
  }
  return &chatTurnPages[activeTurnIndex];
}

static String currentReaderTitle() {
  if (activePane == ChatPane::Incoming) {
    return "BOT";
  }
  return inputBuffer.length() ? "DRAFT" : "YOU";
}

static String currentReaderText() {
  const ChatTurnPage *turnPage = activeTurnPage();
  if (activePane == ChatPane::Incoming) {
    if (turnPage) {
      return turnPage->replyText.length() ? turnPage->replyText : String("Reply was empty.");
    }
    return "Send or record a message to create the first bot reply.";
  }

  if (inputBuffer.length()) {
    return inputBuffer;
  }
  if (turnPage) {
    return turnPage->userText.length() ? turnPage->userText : String("No user message recorded.");
  }
  return "Type a message or hold BtnA to record.";
}

static String currentReaderPromptHeaderText() {
  const ChatTurnPage *turnPage = activeTurnPage();
  if (turnPage && turnPage->userText.length()) {
    return turnPage->userText;
  }
  if (inputBuffer.length()) {
    return inputBuffer;
  }
  return "Type or speak a prompt.";
}

static String currentReaderHint() {
  String hint = currentTurnIndicator();
  hint += shouldUseHorizontalReader() ? " H" : " V";
  if (activePane == ChatPane::Incoming) {
    hint += "  Fn+O";
  } else {
    hint += "  Fn+M";
  }
  return hint;
}

static void drawHorizontalReaderView(int x, int y, int w, int h) {
  String title = currentReaderTitle();
  String text = currentReaderText();
  String promptText = currentReaderPromptHeaderText();
  const int headerY = y;
  const int promptStripY = y + 14;
  const int promptStripH = 14;
  const int replyY = promptStripY + promptStripH + 4;
  const int promptLabelWidth = 22;
  const int promptChars = max(6, (w - promptLabelWidth - 4) / 6);
  const int replyChars = max(4, w / horizontalReaderCharWidth());

  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, headerY + 2);
  M5Cardputer.Display.print(title);
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  String readerHint = currentReaderHint();
  M5Cardputer.Display.setCursor(max(x + 22, x + w - font0TextWidth(readerHint) - 2), headerY + 2);
  M5Cardputer.Display.print(readerHint);
  M5Cardputer.Display.drawFastHLine(x, y + 12, w, COLOR_DIM);

  M5Cardputer.Display.fillRoundRect(x, promptStripY, w, promptStripH, 4, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.setCursor(x + 4, promptStripY + 10);
  M5Cardputer.Display.print("YOU");
  M5Cardputer.Display.setTextColor(currentChatTextColor(activeReplyScrollOffset + 1), COLOR_BG);
  M5Cardputer.Display.setCursor(x + promptLabelWidth, promptStripY + 10);
  M5Cardputer.Display.print(truncateFont0Text(normalizeMarqueeText(promptText), promptChars));
  M5Cardputer.Display.drawFastHLine(x, promptStripY + promptStripH + 2, w, COLOR_DIM);

  String replyWindow = marqueeWindowText(text, replyChars, activeReplyScrollOffset);
  M5Cardputer.Display.setTextColor(currentChatTextColor(activeReplyScrollOffset), COLOR_PANEL);
  M5Cardputer.Display.setTextSize(horizontalReaderTextScale());
  int textX = x;
  if (normalizeMarqueeText(text).length() <= static_cast<size_t>(replyChars)) {
    textX = x + max(0, (w - (static_cast<int>(replyWindow.length()) * horizontalReaderCharWidth())) / 2);
  }
  int textY = min(y + h - 4, replyY + horizontalReaderLineHeight());
  M5Cardputer.Display.setCursor(textX, textY);
  M5Cardputer.Display.print(replyWindow);
  M5Cardputer.Display.setTextSize(1);
}

static void drawReaderView(int x, int y, int w, int h) {
  if (shouldUseHorizontalReader()) {
    drawHorizontalReaderView(x, y, w, h);
    return;
  }
  String title = currentReaderTitle();
  String text = currentReaderText();
  const bool incomingView = activePane == ChatPane::Incoming;
  const bool composingDraft = !incomingView && inputBuffer.length() > 0;
  const uint32_t titleColor = incomingView ? COLOR_OK : COLOR_ACCENT;
  const int headerY = y;
  const int bodyY = y + 14;
  const int bodyH = max(1, h - 14);
  const int maxChars = max(8, (w / scaledCharWidth()) - 1);
  const int visibleLines = max(1, bodyH / scaledLineHeight());
  String lines[160];
  int lineCount = 0;
  fillWrappedLines(text.length() ? text : "—", lines, lineCount, 160, maxChars);

  int startLine = 0;
  int maxOffset = 0;
  if (incomingView) {
    maxOffset = max(0, lineCount - visibleLines);
    activeReplyScrollOffset = constrain(activeReplyScrollOffset, 0, maxOffset);
    startLine = activeReplyScrollOffset;
  } else if (composingDraft) {
    startLine = max(0, lineCount - visibleLines);
  }
  int endLine = min(lineCount, startLine + visibleLines);

  M5Cardputer.Display.setTextColor(titleColor, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, headerY + 2);
  M5Cardputer.Display.print(title);
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  String readerHint = currentReaderHint();
  M5Cardputer.Display.setCursor(max(x + 22, x + w - font0TextWidth(readerHint) - 2), headerY + 2);
  M5Cardputer.Display.print(readerHint);
  M5Cardputer.Display.drawFastHLine(x, y + 12, w, COLOR_DIM);

  M5Cardputer.Display.setTextSize(messageTextScale());
  int cursorY = bodyY;
  for (int i = startLine; i < endLine; i++) {
    M5Cardputer.Display.setTextColor(currentChatTextColor(i - startLine), COLOR_PANEL);
    M5Cardputer.Display.setCursor(x, cursorY);
    M5Cardputer.Display.print(lines[i]);
    cursorY += scaledLineHeight();
  }
  M5Cardputer.Display.setTextSize(1);

  if (incomingView && maxOffset > 0) {
    M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
    String pageIndicator = String(activeReplyScrollOffset + 1) + "/" + String(maxOffset + 1);
    M5Cardputer.Display.setCursor(max(x + 4, x + w - font0TextWidth(pageIndicator) - 2), y + h - 10);
    M5Cardputer.Display.print(pageIndicator);
  } else if (composingDraft && startLine > 0) {
    M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x + w - 16, y + h - 10);
    M5Cardputer.Display.print("v");
  } else if (!incomingView && lineCount > visibleLines) {
    M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x + w - 16, y + h - 10);
    M5Cardputer.Display.print("...");
  }
}

static void jumpToLatestTurn() {
  activeTurnIndex = chatTurnPages.empty() ? -1 : static_cast<int>(chatTurnPages.size()) - 1;
  activePane = ChatPane::Incoming;
  resetReplyScroll();
}

static void navigateTurnPages(int delta) {
  if (chatTurnPages.empty()) {
    setToast("No reply pages yet.");
    return;
  }
  if (delta == 0) return;

  int current = activeTurnIndex;
  if (current < 0 || current >= static_cast<int>(chatTurnPages.size())) {
    current = static_cast<int>(chatTurnPages.size()) - 1;
  }
  int next = constrain(current + delta, 0, static_cast<int>(chatTurnPages.size()) - 1);
  if (next == current) {
    setToast(delta < 0 ? "Oldest page." : "Newest page.");
    return;
  }

  activeTurnIndex = next;
  activePane = ChatPane::Incoming;
  activeScreen = ScreenMode::Chat;
  resetReplyScroll();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void drawHeader(int screenW) {
  M5Cardputer.Display.fillRect(0, 0, screenW, HEADER_HEIGHT + 1, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.setCursor(8, 8);
  M5Cardputer.Display.print("Groqputer");

  M5Cardputer.Display.setTextColor(WiFi.status() == WL_CONNECTED ? COLOR_OK : COLOR_ERROR, COLOR_BG);
  M5Cardputer.Display.setCursor(72, 8);
  M5Cardputer.Display.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "NoWiFi");

  M5Cardputer.Display.setTextColor(batteryStatusColor(), COLOR_BG);
  M5Cardputer.Display.setCursor(112, 8);
  M5Cardputer.Display.print(batteryStatusLabel());

  M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_BG);
  M5Cardputer.Display.setCursor(152, 8);
  M5Cardputer.Display.print(String(gp_record_seconds) + "s");

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(176, 8);
  M5Cardputer.Display.print(modelShortLabel());
  M5Cardputer.Display.drawFastHLine(0, HEADER_HEIGHT, screenW, COLOR_DIM);
}

static void drawContentPanel(int screenW, int screenH) {
  const int contentX = CONTENT_MARGIN;
  const int contentY = HEADER_HEIGHT + CONTENT_MARGIN;
  const int contentW = screenW - (CONTENT_MARGIN * 2);
  const int contentH = screenH - HEADER_HEIGHT - FOOTER_HEIGHT - (CONTENT_MARGIN * 2);

  M5Cardputer.Display.fillRect(contentX, contentY, contentW, contentH, COLOR_BG);
  M5Cardputer.Display.fillRoundRect(contentX, contentY, contentW, contentH, 6, COLOR_PANEL);
  if (activeScreen == ScreenMode::Settings) {
    drawSettingsView(contentX + 6, contentY + 8, contentW - 12, contentH - 12);
    return;
  } else if (activeScreen == ScreenMode::BotSettings) {
    drawBotSettingsView(contentX + 6, contentY + 8, contentW - 12, contentH - 12);
    return;
  } else if (activeScreen == ScreenMode::Hotkeys) {
    drawHotkeysView(contentX + 6, contentY + 8, contentW - 12, contentH - 12);
    return;
  } else if (activeScreen == ScreenMode::CustomPersonality) {
    drawCustomPersonalityView(contentX + 6, contentY + 8, contentW - 12, contentH - 12);
    return;
  } else if (activeScreen == ScreenMode::PhotoViewer) {
    drawPhotoViewer(contentX + 6, contentY + 8, contentW - 12, contentH - 12);
    return;
  }
  drawReaderView(contentX + 6, contentY + 8, contentW - 12, contentH - 12);
}

static void drawFooter(int screenW, int screenH) {
  M5Cardputer.Display.fillRect(0, screenH - FOOTER_HEIGHT, screenW, FOOTER_HEIGHT, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(8, screenH - 10);
  if (recordingActive) {
    unsigned long elapsedMs = millis() - recordingStartedMs;
    M5Cardputer.Display.print("Recording ");
    M5Cardputer.Display.print(elapsedMs / 1000.0f, 1);
    M5Cardputer.Display.print(" / ");
    M5Cardputer.Display.print(gp_record_seconds);
    M5Cardputer.Display.print("s");
  } else if (activeScreen == ScreenMode::Settings) {
    M5Cardputer.Display.print("Fn+;/. ROW Fn+,// SET");
  } else if (activeScreen == ScreenMode::BotSettings) {
    M5Cardputer.Display.print("Fn+B BOT  Fn+;/. NAV");
  } else if (activeScreen == ScreenMode::Hotkeys) {
    M5Cardputer.Display.print("Fn+Space CLOSE Fn+,/ TAB");
  } else if (activeScreen == ScreenMode::CustomPersonality) {
    if (activeCustomPersonalityStage == CustomPersonalityStage::Confirm) {
      M5Cardputer.Display.print("Y SAVE  T TEST  N CANCEL");
    } else {
      M5Cardputer.Display.print("Enter next  Del erase  Fn+V close");
    }
  } else if (activeScreen == ScreenMode::PhotoViewer) {
    M5Cardputer.Display.print("Fn+I CLOSE Fn+,/ PHOTOS Fn+T ROT");
  } else if (activeScreen == ScreenMode::Screensaver) {
    M5Cardputer.Display.print("Any key wakes Groqputer");
  } else {
    if (activePane == ChatPane::Incoming) {
      M5Cardputer.Display.print(shouldUseHorizontalReader() ? "Fn+;. seek Fn+W mode" : "Fn+;. read Fn+W mode");
    } else {
      M5Cardputer.Display.print("Enter send  Fn+M BOT");
    }
  }
}

static void drawToast(int screenW, int screenH) {
  const int toastY = max(HEADER_HEIGHT + 4, screenH - 30);
  M5Cardputer.Display.fillRect(12, toastY, screenW - 24, TOAST_HEIGHT, COLOR_BG);
  if (!toastMessage.length()) {
    return;
  }

  M5Cardputer.Display.fillRoundRect(12, toastY, screenW - 24, TOAST_HEIGHT, 6, 0x18C3);
  M5Cardputer.Display.setTextColor(COLOR_TEXT, 0x18C3);
  M5Cardputer.Display.setCursor(18, toastY + 6);
  M5Cardputer.Display.print(toastMessage);
}

static String modelShortLabel() {
  String model = gp_model[0] ? gp_model : GP_DEFAULT_MODEL;
  if (model.length() <= 8) return model;
  return model.substring(0, 8);
}

static void renderUi() {
  if (activeScreen == ScreenMode::Screensaver) {
    return;
  }
  const int screenW = M5Cardputer.Display.width();
  const int screenH = M5Cardputer.Display.height();

  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setTextSize(1);
  if (dirtyRegions & DIRTY_HEADER) {
    drawHeader(screenW);
  }
  if (dirtyRegions & DIRTY_CONTENT) {
    drawContentPanel(screenW, screenH);
  }
  if (dirtyRegions & DIRTY_FOOTER) {
    drawFooter(screenW, screenH);
  }
  if (dirtyRegions & DIRTY_TOAST) {
    drawToast(screenW, screenH);
  }
  dirtyRegions = DIRTY_NONE;
}

static void recordConversationTurn(const String &userText, const String &replyText) {
  String normalizedUser = clampLogText(userText);
  String normalizedReply = clampLogText(replyText);
  normalizedUser.trim();
  normalizedReply.trim();
  if (!normalizedReply.length()) {
    return;
  }

  gpAppendChatHistoryPair(normalizedUser, normalizedReply);

  ChatTurnPage turnPage;
  turnPage.userText = normalizedUser.length() ? normalizedUser : "No user message recorded.";
  turnPage.replyText = normalizedReply;
  chatTurnPages.push_back(turnPage);
  while (chatTurnPages.size() > GP_MAX_HISTORY_PAIRS) {
    chatTurnPages.erase(chatTurnPages.begin());
  }
}

static bool buildWavPayload(int16_t **samplesInOut, size_t sampleCount, uint8_t **bufferOut, size_t *lengthOut) {
  if (!samplesInOut || !*samplesInOut || !bufferOut || !lengthOut || sampleCount == 0) return false;
  size_t pcmBytes = sampleCount * sizeof(int16_t);
  size_t totalBytes = sizeof(WavHeader) + pcmBytes;
  uint8_t *payload = static_cast<uint8_t *>(realloc(*samplesInOut, totalBytes));
  if (!payload) return false;

  memmove(payload + sizeof(WavHeader), payload, pcmBytes);

  WavHeader header;
  header.fileSize = 36 + pcmBytes;
  header.dataSize = pcmBytes;
  memcpy(payload, &header, sizeof(WavHeader));
  *samplesInOut = nullptr;
  *bufferOut = payload;
  *lengthOut = totalBytes;
  return true;
}

static String normalizeIntentText(const String &value) {
  String normalized;
  normalized.reserve(value.length());
  bool previousWasSpace = false;
  for (size_t i = 0; i < value.length(); i++) {
    char c = static_cast<char>(tolower(static_cast<unsigned char>(value.charAt(i))));
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      normalized += c;
      previousWasSpace = false;
      continue;
    }
    if (!previousWasSpace && normalized.length()) {
      normalized += ' ';
      previousWasSpace = true;
    }
  }
  normalized.trim();
  return normalized;
}

static bool shouldRouteToWeather(const String &value) {
  String normalized = normalizeIntentText(value);
  if (!normalized.length()) {
    return false;
  }
  return
    normalized == "weather" ||
    normalized == "forecast" ||
    normalized == "alerts" ||
    normalized == "weather forecast" ||
    normalized == "weather alerts" ||
    normalized == "any alerts" ||
    normalized == "are there any alerts" ||
    normalized == "wheres the weather" ||
    normalized == "where s the weather" ||
    normalized == "hows the weather" ||
    normalized == "how s the weather" ||
    normalized == "is it going to snow" ||
    normalized == "is snow coming" ||
    normalized == "snow forecast" ||
    normalized.startsWith("what s the weather") ||
    normalized.startsWith("what is the weather") ||
    normalized.startsWith("what is the forecast") ||
    normalized.startsWith("what s the forecast");
}

static bool shouldCapturePhoto(const String &value) {
  String normalized = normalizeIntentText(value);
  if (!normalized.length()) {
    return false;
  }
  return
    normalized == "take photo" ||
    normalized == "take a photo" ||
    normalized == "take picture" ||
    normalized == "take a picture" ||
    normalized == "capture photo" ||
    normalized == "capture image" ||
    normalized == "capture picture" ||
    normalized == "snapshot";
}

static bool submitMessageForReply(const String &text, bool clearInputOnSuccess) {
  noteUserActivity();
  String message = text;
  message.trim();
  if (!message.length()) {
    setToast("Type a message first.");
    return false;
  }
  if (!gpEnsureWifiConnected()) {
    setToast("WiFi is not connected.");
    return false;
  }

  gpSetCompanionPendingPrompt(message);

  String reply;
  String error;
  bool usedWeatherRoute = false;

  if (shouldCapturePhoto(message)) {
    String savedPath;
    setToast("Checking camera...", 1200);
    renderUi();
    if (!captureEsp32CamPhoto(savedPath, error)) {
      gpSetCompanionErrorState();
      setToast(error.length() ? error : "Camera capture failed.", 3000);
      return false;
    }
    if (clearInputOnSuccess) {
      inputBuffer = "";
    }
    if (!openPhotoViewer(savedPath, error)) {
      activePane = ChatPane::Incoming;
      gpSetLcdIncomingMessage("Photo saved to SD");
      markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
      setToast(error.length() ? error : "Photo saved to SD.", 3200);
      return true;
    }
    setToast("Photo saved: " + savedPath, 3200);
    return true;
  } else if (shouldRouteToWeather(message)) {
    gpSetLcdLastPrompt(message);
    usedWeatherRoute = true;
    if (!gpWeatherCoordinatesReady()) {
      gpSetCompanionErrorState();
      setToast("Add weather lat/lon in setup.", 3000);
      return false;
    }
    setToast("Checking NWS...", 1500);
    renderUi();

    String weatherSummary;
    if (!gpFetchWeatherSummary(weatherSummary, error)) {
      gpSetCompanionErrorState();
      setToast(error.length() ? error : "Weather fetch failed.", 3000);
      return false;
    }

    setToast("Asking Groq...", 1200);
    renderUi();
    if (!gpSendWeatherChatMessage(message, weatherSummary, reply, error)) {
      gpSetCompanionErrorState();
      setToast(error.length() ? error : "Groq weather request failed.", 3000);
      return false;
    }
  } else {
    gpSetLcdLastPrompt(message);
    setToast(gp_peer_mode_enabled ? "Sending to peer..." : "Sending to Groq...", 1200);
    renderUi();
    bool ok = gp_peer_mode_enabled
      ? gpSendPeerChatMessage(message, reply, error)
      : gpSendChatMessage(message, reply, error);
    if (!ok) {
      gpSetCompanionErrorState();
      setToast(error.length() ? error : (gp_peer_mode_enabled ? "Peer request failed." : "Groq request failed."), 3000);
      return false;
    }
  }

  if (clearInputOnSuccess) {
    inputBuffer = "";
  }
  gpSetLcdIncomingMessage(reply);
  recordConversationTurn(message, reply);
  jumpToLatestTurn();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast(usedWeatherRoute ? "Weather reply received." : (gp_peer_mode_enabled ? "Peer reply received." : "Reply received."));
  return true;
}

static void submitCurrentInput() {
  submitMessageForReply(inputBuffer, true);
}

static void startRecording() {
  if (recordingActive) return;
  noteUserActivity();
  if (!gpEnsureWifiConnected()) {
    setToast("WiFi is not connected.");
    return;
  }
  if (gp_groq_api_key[0] == '\0') {
    setToast("Add Groq key in setup.");
    return;
  }

  recordingCapacitySamples = SAMPLE_RATE * gp_record_seconds;
  recordingSamples = static_cast<int16_t *>(malloc(recordingCapacitySamples * sizeof(int16_t)));
  if (!recordingSamples) {
    setToast("Mic buffer alloc failed.");
    return;
  }
  memset(recordingSamples, 0, recordingCapacitySamples * sizeof(int16_t));
  recordingCapturedSamples = 0;
  recordingStartedMs = millis();
  recordingActive = true;

  if (M5Cardputer.Speaker.isEnabled()) {
    M5Cardputer.Speaker.end();
  }
  if (!M5Cardputer.Mic.isEnabled()) {
    M5Cardputer.Mic.begin();
  }
  markDirty(DIRTY_FOOTER);
  setToast("Recording...");
}

static void finishRecording() {
  if (!recordingActive) return;
  noteUserActivity();
  recordingActive = false;
  M5Cardputer.Mic.end();
  markDirty(DIRTY_FOOTER);

  if (recordingCapturedSamples < 512) {
    free(recordingSamples);
    recordingSamples = nullptr;
    setToast("Message not heard.");
    return;
  }

  uint8_t *wavPayload = nullptr;
  size_t wavLength = 0;
  if (!buildWavPayload(&recordingSamples, recordingCapturedSamples, &wavPayload, &wavLength)) {
    free(recordingSamples);
    recordingSamples = nullptr;
    setToast("WAV build failed.");
    return;
  }
  free(recordingSamples);
  recordingSamples = nullptr;

  String transcript;
  String error;
  setToast("Transcribing...", 1200);
  renderUi();
  if (!gpTranscribeWav(wavPayload, wavLength, transcript, error)) {
    free(wavPayload);
    setToast(error.length() ? error : "Transcription failed.", 3000);
    return;
  }
  free(wavPayload);

  submitMessageForReply(transcript, false);
}

static void pollRecording() {
  if (!recordingActive) return;

  size_t remaining = recordingCapacitySamples - recordingCapturedSamples;
  if (remaining > 0) {
    size_t chunkSamples = min(RECORD_CHUNK_SAMPLES, remaining);
    if (M5Cardputer.Mic.record(&recordingSamples[recordingCapturedSamples], chunkSamples, SAMPLE_RATE)) {
      recordingCapturedSamples += chunkSamples;
    }
  }

  bool released = !M5Cardputer.BtnA.isPressed();
  bool timedOut = millis() - recordingStartedMs >= static_cast<unsigned long>(gp_record_seconds) * 1000UL;
  bool full = recordingCapturedSamples >= recordingCapacitySamples;
  if (released || timedOut || full) {
    finishRecording();
  }
}

static void adjustTextScale(int delta) {
  uint8_t previous = gp_text_scale;
  int next = static_cast<int>(gp_text_scale) + delta;
  if (next < 1) next = 1;
  if (next > 3) next = 3;
  gpSetTextScale(static_cast<uint8_t>(next));
  if (gp_text_scale == previous) {
    setToast(gp_text_scale == 1 ? "Already smallest text." : "Already largest text.");
    return;
  }
  setToast("Text size " + String(gp_text_scale) + "/3");
}

static void adjustLcdScrollSpeed(int deltaMs) {
  uint16_t previous = gp_lcd_scroll_ms;
  int next = static_cast<int>(gp_lcd_scroll_ms) + deltaMs;
  gpSetLcdScrollMs(static_cast<uint16_t>(next));
  if (gp_lcd_scroll_ms == previous) {
    setToast(deltaMs > 0 ? "Scroll already slowest." : "Scroll already fastest.");
    return;
  }
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast("Scroll " + String(gp_lcd_scroll_ms) + "ms");
}

static void setLcdBacklightEnabled(bool enabled) {
  if (gp_lcd_backlight_enabled == enabled) {
    setToast(enabled ? "LCD light already on." : "LCD light already off.");
    return;
  }
  gpSetLcdBacklightEnabled(enabled);
  markDirty(DIRTY_CONTENT);
  setToast(enabled ? "LCD light on." : "LCD light off.");
}

static void cycleBotSettingField(int delta) {
  if (delta == 0) return;
  activeBotSettingField = (delta > 0) ? BotSettingField::Personality : BotSettingField::Model;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void cycleSettingField(int delta) {
  if (delta == 0) return;
  int count = settingFieldCount();
  int index = currentSettingFieldIndex();
  index = (index + delta + count) % count;
  activeSettingField = settingFieldAt(index);
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void cycleSettingsValue(int delta) {
  if (delta == 0) return;

  if (activeSettingField == SettingField::Screensaver) {
    int count = static_cast<int>(gpScreensaverOptionCount());
    int index = gpCurrentScreensaverOptionIndex();
    index = (index + delta + count) % count;
    gpSetActiveScreensaverMode(GP_SCREENSAVER_OPTIONS[index].value);
    activeRandomScreensaverIndex = -1;
    nextRandomScreensaverChangeMs = 0;
    markDirty(DIRTY_CONTENT);
    setToast(String("Saver: ") + GP_SCREENSAVER_OPTIONS[index].label);
    return;
  }

  if (activeSettingField == SettingField::IdleDelay) {
    static const uint16_t idleOptions[] = {0, 15, 30, 60, 120, 300, 600};
    const int optionCount = static_cast<int>(sizeof(idleOptions) / sizeof(idleOptions[0]));
    int optionIndex = 0;
    for (int i = 0; i < optionCount; i++) {
      if (gp_idle_saver_sec <= idleOptions[i]) {
        optionIndex = i;
        break;
      }
      optionIndex = i;
    }
    optionIndex = (optionIndex + delta + optionCount) % optionCount;
    gpSetIdleSaverSec(idleOptions[optionIndex]);
    markDirty(DIRTY_CONTENT);
    setToast(String("Idle saver: ") + String(gp_idle_saver_sec) + "s");
    return;
  }

  if (activeSettingField == SettingField::Reader) {
    gpSetReaderMode(currentReaderModeIsHorizontal() ? "vertical" : "horizontal");
    resetReplyScroll(500);
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
    setToast(String("Reader: ") + readerModeToastLabel());
    return;
  }

  if (activeSettingField == SettingField::ChatColor) {
    int count = static_cast<int>(gpTextThemeOptionCount());
    int index = gpCurrentTextThemeOptionIndex();
    index = (index + delta + count) % count;
    gpSetTextTheme(GP_TEXT_THEME_OPTIONS[index].value);
    applyThemeColors();
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER | DIRTY_TOAST);
    setToast(String("Chat color: ") + GP_TEXT_THEME_OPTIONS[index].label);
    return;
  }

  if (activeSettingField == SettingField::Theme) {
    int count = static_cast<int>(gpBackgroundThemeOptionCount());
    int index = gpCurrentBackgroundThemeOptionIndex();
    index = (index + delta + count) % count;
    gpSetBackgroundTheme(GP_BG_THEME_OPTIONS[index].value);
    applyThemeColors();
    markDirty(DIRTY_ALL);
    setToast(String("Theme: ") + GP_BG_THEME_OPTIONS[index].label);
    return;
  }

  if (activeSettingField == SettingField::Model) {
    setToast("Use Fn+B for model.");
  } else if (activeSettingField == SettingField::Wifi ||
             activeSettingField == SettingField::WeatherCamera ||
             activeSettingField == SettingField::Peer) {
    setToast("Edit in setup AP.");
  } else {
    setToast("Info row only.");
  }
}

static void cycleBotSettingValue(int delta) {
  if (delta == 0) return;

  if (activeBotSettingField == BotSettingField::Model) {
    int count = static_cast<int>(gpModelOptionCount());
    int index = gpCurrentModelOptionIndex();
    index = (index + delta + count) % count;
    gpSetActiveModel(GP_MODEL_OPTIONS[index].value);
    markDirty(DIRTY_HEADER | DIRTY_CONTENT);
    setToast(String("Model: ") + GP_MODEL_OPTIONS[index].label);
    return;
  }

  int count = static_cast<int>(gpPersonalityPresetCount());
  int index = gpCurrentPersonalityPresetIndex();
  if (index < 0) {
    index = delta > 0 ? 0 : count - 1;
  } else {
    index = (index + delta + count) % count;
  }
  gpSetActivePersonalityPrompt(gpPersonalityPresetPromptAt(static_cast<size_t>(index)));
  markDirty(DIRTY_CONTENT);
  setToast(String("Persona: ") + gpPersonalityPresetLabelAt(static_cast<size_t>(index)));
}

static void togglePeerMode() {
  if (gp_peer_mode_enabled) {
    gpSetPeerModeEnabled(false);
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
    setToast("Connected device off.");
    return;
  }

  if (!gpPeerSettingsReady()) {
    setToast("Set bot URLs in AP first.");
    return;
  }

  gpSetPeerModeEnabled(true);
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast("Connected device on.");
}

static void toggleReaderMode() {
  gpSetReaderMode(currentReaderModeIsHorizontal() ? "vertical" : "horizontal");
  resetReplyScroll(500);
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast(String("Reader: ") + readerModeToastLabel());
}

static bool isRepeatableFnKey(char c) {
  return c == ';' || c == '.' || c == ',' || c == '/' || c == '[' || c == '{' || c == ']' || c == '}';
}

static void openCustomPersonalityEditor() {
  activeScreen = ScreenMode::CustomPersonality;
  activeCustomPersonalityStage = CustomPersonalityStage::Prompt;
  customPersonalityPromptBuffer = "";
  customPersonalityNameBuffer = "";
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void closeCustomPersonalityEditor() {
  activeScreen = ScreenMode::Chat;
  activeCustomPersonalityStage = CustomPersonalityStage::Prompt;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
}

static void commitCustomPersonalitySave() {
  String error;
  if (!gpSaveCustomPersonalityPreset(customPersonalityNameBuffer, customPersonalityPromptBuffer, error)) {
    setToast(error.length() ? error : "Custom bot save failed.", 3000);
    return;
  }
  gpSetActivePersonalityPrompt(customPersonalityPromptBuffer);
  closeCustomPersonalityEditor();
  setToast(String("Saved bot: ") + customPersonalityNameBuffer, 2800);
}

static void testCustomPersonalityPrompt() {
  String prompt = customPersonalityPromptBuffer;
  prompt.trim();
  if (!prompt.length()) {
    setToast("Enter a custom prompt first.");
    return;
  }
  gpSetRuntimePersonalityPrompt(prompt);
  closeCustomPersonalityEditor();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast("Testing custom bot.");
}

static void handleCustomPersonalityInput(const Keyboard_Class::KeysState &status) {
  if (activeCustomPersonalityStage == CustomPersonalityStage::Confirm) {
    for (auto c : status.word) {
      if (c == 'y' || c == 'Y') {
        commitCustomPersonalitySave();
        return;
      }
      if (c == 't' || c == 'T') {
        testCustomPersonalityPrompt();
        return;
      }
      if (c == 'n' || c == 'N') {
        closeCustomPersonalityEditor();
        setToast("Custom bot canceled.");
        return;
      }
    }
    return;
  }

  String *buffer = activeCustomPersonalityStage == CustomPersonalityStage::Prompt
    ? &customPersonalityPromptBuffer
    : &customPersonalityNameBuffer;

  bool changedBuffer = false;
  for (auto c : status.word) {
    if (c >= 32 && c <= 126) {
      *buffer += c;
      changedBuffer = true;
    }
  }
  if (status.del && buffer->length() > 0) {
    buffer->remove(buffer->length() - 1);
    changedBuffer = true;
  }
  if (status.enter) {
    String value = *buffer;
    value.trim();
    if (!value.length()) {
      setToast(activeCustomPersonalityStage == CustomPersonalityStage::Prompt ? "Enter a prompt first." : "Enter a bot name first.");
      return;
    }
    if (activeCustomPersonalityStage == CustomPersonalityStage::Prompt) {
      activeCustomPersonalityStage = CustomPersonalityStage::Name;
    } else {
      activeCustomPersonalityStage = CustomPersonalityStage::Confirm;
    }
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
    return;
  }
  if (changedBuffer) {
    markDirty(DIRTY_CONTENT);
  }
}

static bool handleRepeatableFnAction(char c) {
  if (c == ';') {
    if (activeScreen == ScreenMode::BotSettings) {
      cycleBotSettingField(-1);
    } else if (activeScreen == ScreenMode::Settings) {
      cycleSettingField(-1);
    } else if (activeScreen == ScreenMode::Hotkeys) {
      moveCommandGuideSelection(-1);
    } else if (activeScreen == ScreenMode::Chat) {
      scrollCurrentReply(-TURN_SCROLL_STEP,
                         M5Cardputer.Display.width() - (CONTENT_MARGIN * 2) - 12,
                         M5Cardputer.Display.height() - HEADER_HEIGHT - FOOTER_HEIGHT - (CONTENT_MARGIN * 2) - 28);
    } else {
      return false;
    }
    return true;
  }

  if (c == '.') {
    if (activeScreen == ScreenMode::BotSettings) {
      cycleBotSettingField(1);
    } else if (activeScreen == ScreenMode::Settings) {
      cycleSettingField(1);
    } else if (activeScreen == ScreenMode::Hotkeys) {
      moveCommandGuideSelection(1);
    } else if (activeScreen == ScreenMode::Chat) {
      scrollCurrentReply(TURN_SCROLL_STEP,
                         M5Cardputer.Display.width() - (CONTENT_MARGIN * 2) - 12,
                         M5Cardputer.Display.height() - HEADER_HEIGHT - FOOTER_HEIGHT - (CONTENT_MARGIN * 2) - 28);
    } else {
      return false;
    }
    return true;
  }

  if (c == ',') {
    if (activeScreen == ScreenMode::BotSettings) {
      cycleBotSettingValue(-1);
    } else if (activeScreen == ScreenMode::Settings) {
      cycleSettingsValue(-1);
    } else if (activeScreen == ScreenMode::Hotkeys) {
      switchCommandGuideSection(-1);
    } else if (activeScreen == ScreenMode::PhotoViewer) {
      navigatePhotoViewer(-1);
    } else if (activeScreen == ScreenMode::Chat) {
      navigateTurnPages(-1);
    } else {
      return false;
    }
    return true;
  }

  if (c == '/') {
    if (activeScreen == ScreenMode::BotSettings) {
      cycleBotSettingValue(1);
    } else if (activeScreen == ScreenMode::Settings) {
      cycleSettingsValue(1);
    } else if (activeScreen == ScreenMode::Hotkeys) {
      switchCommandGuideSection(1);
    } else if (activeScreen == ScreenMode::PhotoViewer) {
      navigatePhotoViewer(1);
    } else if (activeScreen == ScreenMode::Chat) {
      navigateTurnPages(1);
    } else {
      return false;
    }
    return true;
  }

  if (c == '[' || c == '{') {
    adjustLcdScrollSpeed(50);
    return true;
  }

  if (c == ']' || c == '}') {
    adjustLcdScrollSpeed(-50);
    return true;
  }

  return false;
}

static void handleKeyboard() {
  static bool fnComboConsumed = false;
  static char heldFnRepeatKey = 0;
  static unsigned long heldFnRepeatAfterMs = 0;
  bool changed = M5Cardputer.Keyboard.isChange();
  bool pressed = M5Cardputer.Keyboard.isPressed();

  if (!pressed) {
    fnComboConsumed = false;
    heldFnRepeatKey = 0;
    heldFnRepeatAfterMs = 0;
    if (!changed) return;
    return;
  }

  if (millis() < screensaverDismissUntilMs) {
    return;
  }

  if (activeScreen == ScreenMode::Screensaver) {
    exitScreensaver();
    fnComboConsumed = false;
    heldFnRepeatKey = 0;
    heldFnRepeatAfterMs = 0;
    return;
  }

  noteUserActivity();

  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  if (status.fn) {
    char repeatableKey = 0;
    for (auto c : status.word) {
      if (isRepeatableFnKey(c)) {
        repeatableKey = c;
        break;
      }
    }

    bool fnOnly = status.word.empty() && !status.enter && !status.del && !status.space && !status.tab;
    if (fnOnly) {
      fnComboConsumed = false;
      heldFnRepeatKey = 0;
      heldFnRepeatAfterMs = 0;
      return;
    }

    if (repeatableKey != 0) {
      bool shouldRepeat = false;
      if (changed || heldFnRepeatKey != repeatableKey) {
        shouldRepeat = true;
        heldFnRepeatAfterMs = millis() + 260;
      } else if (millis() >= heldFnRepeatAfterMs) {
        shouldRepeat = true;
        heldFnRepeatAfterMs = millis() + 95;
      }

      heldFnRepeatKey = repeatableKey;
      if (shouldRepeat && handleRepeatableFnAction(repeatableKey)) {
        return;
      }
    } else {
      heldFnRepeatKey = 0;
      heldFnRepeatAfterMs = 0;
    }

    if (!changed) return;
    if (fnComboConsumed) return;
    bool handledFn = false;
    if (status.space) {
      toggleCommandGuide();
      handledFn = true;
    }
    for (auto c : status.word) {
      if (isRepeatableFnKey(c)) continue;
      if (c == 'a' || c == 'A') {
        handledFn = true;
        M5Cardputer.Display.fillScreen(COLOR_BG);
        M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_BG);
        M5Cardputer.Display.setCursor(10, 20);
        M5Cardputer.Display.print("Opening setup AP...");
        M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        M5Cardputer.Display.setCursor(10, 36);
        M5Cardputer.Display.print(String("SSID: ") + GP_AP_SSID);
        M5Cardputer.Display.setCursor(10, 52);
        M5Cardputer.Display.print("Open http://192.168.4.1");
        delay(700);
        gpRunPortal();
        markDirty(DIRTY_ALL);
      } else if (c == 'm' || c == 'M') {
        activeScreen = ScreenMode::Chat;
        jumpToLatestTurn();
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        handledFn = true;
      } else if (c == 'o' || c == 'O') {
        activeScreen = ScreenMode::Chat;
        activePane = ChatPane::Outgoing;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        handledFn = true;
      } else if (c == 's' || c == 'S') {
        activeScreen = activeScreen == ScreenMode::Settings ? ScreenMode::Chat : ScreenMode::Settings;
        activeSettingField = SettingField::Screensaver;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        handledFn = true;
      } else if (c == 'b' || c == 'B') {
        activeScreen = activeScreen == ScreenMode::BotSettings ? ScreenMode::Chat : ScreenMode::BotSettings;
        activeBotSettingField = BotSettingField::Model;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        handledFn = true;
      } else if (c == 'h' || c == 'H') {
        toggleCommandGuide();
        handledFn = true;
      } else if (c == 'v' || c == 'V') {
        if (activeScreen == ScreenMode::CustomPersonality) {
          closeCustomPersonalityEditor();
          setToast("Custom bot canceled.");
        } else {
          openCustomPersonalityEditor();
        }
        handledFn = true;
      } else if (c == 'c' || c == 'C') {
        togglePeerMode();
        handledFn = true;
      } else if (c == 'g' || c == 'G') {
        String savedPath;
        String error;
        setToast("Checking camera...", 1200);
        renderUi();
        if (!captureEsp32CamPhoto(savedPath, error)) {
          setToast(error.length() ? error : "Camera capture failed.", 3000);
        } else {
          if (!openPhotoViewer(savedPath, error)) {
            gpSetLcdIncomingMessage("Photo saved to SD");
            markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
          }
          setToast("Photo saved: " + savedPath, 3200);
        }
        handledFn = true;
      } else if (c == 'i' || c == 'I') {
        String error;
        if (activeScreen == ScreenMode::PhotoViewer) {
          activeScreen = ScreenMode::Chat;
          markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        } else if (!openPhotoViewer("", error)) {
          setToast(error.length() ? error : "No saved photos on SD.", 2800);
        }
        handledFn = true;
      } else if (c == 't' || c == 'T') {
        if (activeScreen == ScreenMode::PhotoViewer) {
          activePhotoRotationQuarterTurns = (activePhotoRotationQuarterTurns + 1) % 4;
          markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
          setToast("Photo rotate " + String(activePhotoRotationQuarterTurns * 90) + " deg", 2200);
        }
        handledFn = true;
      } else if (c == 'p' || c == 'P') {
        submitMessageForReply("What's the weather?", false);
        handledFn = true;
      } else if (c == 'w' || c == 'W') {
        toggleReaderMode();
        handledFn = true;
      } else if (c == 'x' || c == 'X') {
        if (!recordingActive) {
          enterScreensaver(false);
        }
        handledFn = true;
      } else if (c == '1') {
        setLcdBacklightEnabled(false);
        handledFn = true;
      } else if (c == '2') {
        setLcdBacklightEnabled(true);
        handledFn = true;
      } else if (c == 'n' || c == 'N') {
        gpResetChatHistory();
        chatTurnPages.clear();
        activeTurnIndex = -1;
        inputBuffer = "";
        gpSetLcdIncomingMessage("New chat started.");
        jumpToLatestTurn();
        activeScreen = ScreenMode::Chat;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        setToast("New chat started.");
        handledFn = true;
      } else if (c == 'r' || c == 'R') {
        gpResetChatHistory();
        chatTurnPages.clear();
        activeTurnIndex = -1;
        inputBuffer = "";
        jumpToLatestTurn();
        activeScreen = ScreenMode::Chat;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        setToast("Chat history cleared.");
        handledFn = true;
      } else if (c == '+' || c == '=') {
        adjustTextScale(1);
        handledFn = true;
      } else if (c == '-' || c == '_') {
        adjustTextScale(-1);
        handledFn = true;
      }
    }
    if (handledFn) {
      fnComboConsumed = true;
      return;
    }
  } else {
    fnComboConsumed = false;
    heldFnRepeatKey = 0;
    heldFnRepeatAfterMs = 0;
  }

  if (!changed) return;

  if (activeScreen == ScreenMode::Settings ||
      activeScreen == ScreenMode::BotSettings ||
      activeScreen == ScreenMode::Hotkeys ||
      activeScreen == ScreenMode::PhotoViewer) {
    return;
  }

  if (activeScreen == ScreenMode::CustomPersonality) {
    handleCustomPersonalityInput(status);
    return;
  }

  for (auto c : status.word) {
    if (c >= 32 && c <= 126) {
      activePane = ChatPane::Outgoing;
      inputBuffer += c;
    }
  }
  if (status.del && inputBuffer.length() > 0) {
    activePane = ChatPane::Outgoing;
    inputBuffer.remove(inputBuffer.length() - 1);
  }
  if (status.enter) {
    activeScreen = ScreenMode::Chat;
    activePane = ChatPane::Outgoing;
    submitCurrentInput();
  } else {
    markDirty(DIRTY_CONTENT);
  }
}

static void showSetupPortalInstructions() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.setCursor(8, 10);
  M5Cardputer.Display.print("Groqputer Setup");

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5Cardputer.Display.setCursor(8, 28);
  M5Cardputer.Display.print("Connect to AP:");
  M5Cardputer.Display.setCursor(8, 40);
  M5Cardputer.Display.print(GP_AP_SSID);

  M5Cardputer.Display.setCursor(8, 58);
  M5Cardputer.Display.print("Open in browser:");
  M5Cardputer.Display.setCursor(8, 70);
  M5Cardputer.Display.print("http://192.168.4.1");

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(8, 94);
  M5Cardputer.Display.print("Add WiFi + Groq key");
  M5Cardputer.Display.setCursor(8, 106);
  M5Cardputer.Display.print("then save & reboot.");
}

static String companionModelTag() {
  return gpCurrentModelTag();
}

static String companionPersonaLabel() {
  int personalityIndex = gpCurrentPersonalityPresetIndex();
  if (personalityIndex < 0) {
    return "Custom";
  }
  return gpPersonalityPresetLabelAt(static_cast<size_t>(personalityIndex));
}

static String buildCompanionChatJson() {
  JsonDocument doc;
  doc["ready"] = WiFi.status() == WL_CONNECTED;
  doc["status"] = gp_companion_status;
  doc["device"] = "groqputer";
  doc["model"] = gp_model[0] ? gp_model : GP_DEFAULT_MODEL;
  doc["modelTag"] = companionModelTag();
  doc["persona"] = companionPersonaLabel();
  doc["latestUser"] = gp_last_user_message;
  doc["latestReply"] = gp_last_reply_message;
  doc["messagePairs"] = gp_message_pairs;
  doc["updatedAtMs"] = gp_last_chat_update_ms;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";

  String payload;
  serializeJson(doc, payload);
  return payload;
}

static void handleCompanionChatApi() {
  if (!gp_companion_server) {
    return;
  }
  gp_companion_server->send(200, "application/json", buildCompanionChatJson());
}

static void ensureCompanionServer() {
  if (WiFi.status() != WL_CONNECTED || gp_companion_server) {
    return;
  }

  gp_companion_server = new WebServer(80);
  gp_companion_server->on("/api/companion/chat", HTTP_GET, handleCompanionChatApi);
  gp_companion_server->on("/api/companion/status", HTTP_GET, handleCompanionChatApi);
  gp_companion_server->begin();
}

static void pollCompanionServer() {
  if (gp_companion_server) {
    gp_companion_server->handleClient();
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillScreen(COLOR_BG);
  applyDisplayBrightness(true);

  M5Cardputer.Speaker.end();
  M5Cardputer.Mic.begin();
  gpInitLcd();

  gpLoadSettings();
  applyThemeColors();
  if (!gp_has_settings) {
    showSetupPortalInstructions();
    gpSetLcdIncomingMessage("Join Groqputer-Setup at 192.168.4.1");
    gpRunPortal();
  }

  gpConnect(false);
  gpLoadChatHistory();
  rebuildTurnPagesFromPersistedChat();
  noteUserActivity();
  if (gp_has_settings) {
    if (chatTurnPages.empty()) {
      setToast("Groqputer ready.");
    } else {
      jumpToLatestTurn();
    }
    enterScreensaver(true);
  } else {
    gpSetLcdIncomingMessage("Run setup AP to add WiFi and Groq key.");
  }
  markDirty(DIRTY_ALL);
  if (activeScreen == ScreenMode::Screensaver) {
    renderScreensaverFrame(true);
  } else {
    renderUi();
  }
}

void loop() {
  static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
  static int lastRecordingTenths = -1;
  static unsigned long lastPowerCheckMs = 0;
  static int32_t lastBatteryLevel = -999;
  static int16_t lastBatteryMilliVolts = -999;

  M5Cardputer.update();
  handleKeyboard();

  if (activeScreen == ScreenMode::Screensaver && M5Cardputer.BtnA.wasPressed()) {
    exitScreensaver();
  } else if (!recordingActive && millis() >= screensaverDismissUntilMs && M5Cardputer.BtnA.wasPressed()) {
    startRecording();
  }
  pollRecording();
  pollReplyAutoScroll();

  gpEnsureWifiConnected();
  ensureCompanionServer();
  pollCompanionServer();

  if (millis() - lastPowerCheckMs >= 1500) {
    lastPowerCheckMs = millis();
    applyDisplayBrightness();

    int32_t batteryLevel = currentBatteryLevel();
    int16_t batteryMilliVolts = currentBatteryMilliVolts();
    if (batteryLevel != lastBatteryLevel || batteryMilliVolts != lastBatteryMilliVolts) {
      lastBatteryLevel = batteryLevel;
      lastBatteryMilliVolts = batteryMilliVolts;
      markDirty(DIRTY_HEADER);
    }
  }

  wl_status_t wifiStatus = WiFi.status();
  if (wifiStatus != lastWifiStatus) {
    lastWifiStatus = wifiStatus;
    markDirty(DIRTY_HEADER);
  }

  if (recordingActive) {
    int recordingTenths = static_cast<int>((millis() - recordingStartedMs) / 100UL);
    if (recordingTenths != lastRecordingTenths) {
      lastRecordingTenths = recordingTenths;
      markDirty(DIRTY_FOOTER);
    }
  } else if (lastRecordingTenths != -1) {
    lastRecordingTenths = -1;
    markDirty(DIRTY_FOOTER);
  }

  if (toastUntilMs > 0 && toastUntilMs <= millis()) {
    toastUntilMs = 0;
    toastMessage = "";
    markDirty(DIRTY_CONTENT | DIRTY_FOOTER | DIRTY_TOAST);
  }

  if (
    gp_has_settings &&
    activeScreen != ScreenMode::Screensaver &&
    !recordingActive &&
    gp_idle_saver_sec > 0 &&
    millis() >= screensaverDismissUntilMs &&
    millis() - lastUserActivityMs >= static_cast<unsigned long>(gp_idle_saver_sec) * 1000UL
  ) {
    enterScreensaver(false);
  }

  if (activeScreen == ScreenMode::Screensaver) {
    renderScreensaverFrame();
  } else if (dirtyRegions != DIRTY_NONE) {
    renderUi();
  }

  gpUpdateLcd(recordingActive, recordingStartedMs, gp_record_seconds);

  delay(10);
}
