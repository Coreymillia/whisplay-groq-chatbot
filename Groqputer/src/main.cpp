#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <vector>

#include "GroqApi.h"
#include "GroqLcd.h"
#include "GroqPortal.h"

static const uint32_t COLOR_BG = BLACK;
static const uint32_t COLOR_PANEL = 0x18C3;
static const uint32_t COLOR_ACCENT = 0x07FF;
static const uint32_t COLOR_TEXT = WHITE;
static const uint32_t COLOR_DIM = 0x7BEF;
static const uint32_t COLOR_WARN = 0xFFE0;
static const uint32_t COLOR_OK = 0x07E0;
static const uint32_t COLOR_ERROR = 0xF800;

static String inputBuffer;
static String toastMessage;
static uint8_t dirtyRegions = 0xFF;
static unsigned long toastUntilMs = 0;

struct ChatTurnPage {
  String userText;
  String replyText;
};

static std::vector<ChatTurnPage> chatTurnPages;
static int activeTurnIndex = -1;
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

enum class ChatPane : uint8_t {
  Incoming,
  Outgoing,
};

enum class ScreenMode : uint8_t {
  Chat,
  Settings,
  BotSettings,
  Hotkeys,
  CustomPersonality,
};

enum class BotSettingField : uint8_t {
  Model,
  Personality,
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
static CustomPersonalityStage activeCustomPersonalityStage = CustomPersonalityStage::Prompt;
static bool usingExternalPower = true;
static String customPersonalityPromptBuffer;
static String customPersonalityNameBuffer;

static void markDirty(uint8_t regions) {
  dirtyRegions |= regions;
}

static String modelShortLabel();
static void resetReplyScroll(unsigned long pauseMs = TURN_AUTO_SCROLL_PAUSE_MS);
static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars);

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

static int scaledCharWidth() {
  return 6 * messageTextScale();
}

static int scaledLineHeight() {
  return (8 * messageTextScale()) + 2;
}

static unsigned long currentReaderAutoScrollIntervalMs() {
  return max<unsigned long>(200, gp_lcd_scroll_ms);
}

static void setToast(const String &message, uint16_t durationMs = 2200) {
  toastMessage = message;
  toastUntilMs = millis() + durationMs;
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER | DIRTY_TOAST);
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
  int cursorY = y;
  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("SETTINGS");
  cursorY += lineHeight + 2;

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print(String("LCD ") + (gpIsLcdReady() ? "ready" : "missing"));
  cursorY += lineHeight;

  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("Scroll " + String(gp_lcd_scroll_ms) + "ms");
  cursorY += lineHeight;

  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print(String("Light ") + (gp_lcd_backlight_enabled ? "on" : "off"));
  cursorY += lineHeight;

  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("Text " + String(gp_text_scale) + "/3");
  cursorY += lineHeight;

  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("Record " + String(gp_record_seconds) + "s");
  cursorY += lineHeight;

  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print(String("Net ") + (gp_peer_mode_enabled ? "ON" : "OFF"));
  cursorY += lineHeight;

  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print(String("Peer ") + (gpPeerSettingsReady() ? "ready" : "missing"));
  cursorY += lineHeight;

  String model = modelShortLabel();
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("Model " + model);
  cursorY += lineHeight;

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, min(y + h - 22, cursorY + 2));
  M5Cardputer.Display.print("Fn+1 off Fn+2 on");
  M5Cardputer.Display.setCursor(x, y + h - 10);
  M5Cardputer.Display.print("Fn+S close Fn+C net");
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
  const int lineHeight = 11;
  int cursorY = y;

  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, cursorY);
  M5Cardputer.Display.print("HOTKEYS");
  cursorY += lineHeight + 1;

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  const char *lines[] = {
    "Fn+H close sheet",
    "Fn+M bot  Fn+O you",
    "Fn+,/ turns",
    "Fn+;. read up/down",
    "Fn+[/] scroll speed",
    "Fn+V custom bot",
    "Fn+S settings",
    "Fn+B bot settings",
    "Fn+C linked device",
    "Fn+N new  Fn+R clear",
    "Fn+A setup portal",
  };

  for (const char *line : lines) {
    if (cursorY > y + h - 22) break;
    M5Cardputer.Display.setCursor(x, cursorY);
    M5Cardputer.Display.print(line);
    cursorY += lineHeight;
  }

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(x, y + h - 10);
  M5Cardputer.Display.print("Enter sends only in draft");
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
  if (replyAutoScrollComplete) return;
  if (replyAutoScrollPauseUntilMs > millis()) return;
  const int contentW = M5Cardputer.Display.width() - (CONTENT_MARGIN * 2) - 12;
  const int contentH = M5Cardputer.Display.height() - HEADER_HEIGHT - FOOTER_HEIGHT - (CONTENT_MARGIN * 2) - 28;
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

static String currentReaderHint() {
  String hint = "Turn ";
  hint += currentTurnIndicator();
  if (activePane == ChatPane::Incoming) {
    hint += "  Fn+O YOU";
  } else {
    hint += "  Fn+M BOT";
  }
  return hint;
}

static void drawReaderView(int x, int y, int w, int h) {
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
  M5Cardputer.Display.setCursor(x + 34, headerY + 2);
  M5Cardputer.Display.print(currentReaderHint());
  M5Cardputer.Display.drawFastHLine(x, y + 12, w, COLOR_DIM);

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  M5Cardputer.Display.setTextSize(messageTextScale());
  int cursorY = bodyY;
  for (int i = startLine; i < endLine; i++) {
    M5Cardputer.Display.setCursor(x, cursorY);
    M5Cardputer.Display.print(lines[i]);
    cursorY += scaledLineHeight();
  }
  M5Cardputer.Display.setTextSize(1);

  if (incomingView && maxOffset > 0) {
    M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
    M5Cardputer.Display.setCursor(x + w - 42, y + h - 10);
    M5Cardputer.Display.print(String(activeReplyScrollOffset + 1) + "/" + String(maxOffset + 1));
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
    M5Cardputer.Display.print("Fn+S CLOSE  Fn+C NET");
  } else if (activeScreen == ScreenMode::BotSettings) {
    M5Cardputer.Display.print("Fn+B BOT  Fn+;/. NAV");
  } else if (activeScreen == ScreenMode::Hotkeys) {
    M5Cardputer.Display.print("Fn+H CLOSE Fn+M BOT");
  } else if (activeScreen == ScreenMode::CustomPersonality) {
    if (activeCustomPersonalityStage == CustomPersonalityStage::Confirm) {
      M5Cardputer.Display.print("Y SAVE  T TEST  N CANCEL");
    } else {
      M5Cardputer.Display.print("Enter next  Del erase  Fn+V close");
    }
  } else {
    if (activePane == ChatPane::Incoming) {
      M5Cardputer.Display.print("Fn+;. read  Fn+,/ turn");
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

static bool buildWavPayload(const int16_t *samples, size_t sampleCount, uint8_t **bufferOut, size_t *lengthOut) {
  if (!samples || !bufferOut || !lengthOut || sampleCount == 0) return false;
  size_t pcmBytes = sampleCount * sizeof(int16_t);
  size_t totalBytes = sizeof(WavHeader) + pcmBytes;
  uint8_t *payload = static_cast<uint8_t *>(malloc(totalBytes));
  if (!payload) return false;

  WavHeader header;
  header.fileSize = 36 + pcmBytes;
  header.dataSize = pcmBytes;
  memcpy(payload, &header, sizeof(WavHeader));
  memcpy(payload + sizeof(WavHeader), samples, pcmBytes);
  *bufferOut = payload;
  *lengthOut = totalBytes;
  return true;
}

static void submitCurrentInput() {
  String text = inputBuffer;
  text.trim();
  if (!text.length()) {
    setToast("Type a message first.");
    return;
  }
  if (!gpEnsureWifiConnected()) {
    setToast("WiFi is not connected.");
    return;
  }

  String reply;
  String error;
  setToast(gp_peer_mode_enabled ? "Sending to peer..." : "Sending to Groq...", 1200);
  renderUi();
  bool ok = gp_peer_mode_enabled
    ? gpSendPeerChatMessage(text, reply, error)
    : gpSendChatMessage(text, reply, error);
  if (!ok) {
    setToast(error.length() ? error : (gp_peer_mode_enabled ? "Peer request failed." : "Groq request failed."), 3000);
    return;
  }
  inputBuffer = "";
  gpSetLcdIncomingMessage(reply);
  rebuildTurnPagesFromPersistedChat();
  jumpToLatestTurn();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast(gp_peer_mode_enabled ? "Peer reply received." : "Reply received.");
}

static void startRecording() {
  if (recordingActive) return;
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
  if (!buildWavPayload(recordingSamples, recordingCapturedSamples, &wavPayload, &wavLength)) {
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

  setToast(gp_peer_mode_enabled ? "Sending to peer..." : "Sending to Groq...", 1200);
  renderUi();

  String reply;
  bool ok = gp_peer_mode_enabled
    ? gpSendPeerChatMessage(transcript, reply, error)
    : gpSendChatMessage(transcript, reply, error);
  if (!ok) {
    setToast(error.length() ? error : (gp_peer_mode_enabled ? "Peer request failed." : "Groq request failed."), 3000);
    return;
  }
  gpSetLcdIncomingMessage(reply);
  rebuildTurnPagesFromPersistedChat();
  jumpToLatestTurn();
  markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
  setToast(gp_peer_mode_enabled ? "Peer reply received." : "Reply received.");
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
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        handledFn = true;
      } else if (c == 'b' || c == 'B') {
        activeScreen = activeScreen == ScreenMode::BotSettings ? ScreenMode::Chat : ScreenMode::BotSettings;
        activeBotSettingField = BotSettingField::Model;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
        handledFn = true;
      } else if (c == 'h' || c == 'H') {
        activeScreen = activeScreen == ScreenMode::Hotkeys ? ScreenMode::Chat : ScreenMode::Hotkeys;
        markDirty(DIRTY_CONTENT | DIRTY_FOOTER);
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
      activeScreen == ScreenMode::Hotkeys) {
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
  if (!gp_has_settings) {
    showSetupPortalInstructions();
    gpSetLcdIncomingMessage("Join Groqputer-Setup at 192.168.4.1");
    gpRunPortal();
  }

  gpConnect(false);
  gpLoadChatHistory();
  rebuildTurnPagesFromPersistedChat();
  if (gp_has_settings) {
    if (chatTurnPages.empty()) {
      setToast("Groqputer ready.");
    } else {
      jumpToLatestTurn();
    }
  } else {
    gpSetLcdIncomingMessage("Run setup AP to add WiFi and Groq key.");
  }
  markDirty(DIRTY_ALL);
  renderUi();
}

void loop() {
  static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
  static int lastRecordingTenths = -1;
  static unsigned long lastPowerCheckMs = 0;
  static int32_t lastBatteryLevel = -999;
  static int16_t lastBatteryMilliVolts = -999;

  M5Cardputer.update();
  handleKeyboard();

  if (!recordingActive && M5Cardputer.BtnA.wasPressed()) {
    startRecording();
  }
  pollRecording();
  pollReplyAutoScroll();

  gpEnsureWifiConnected();

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

  if (dirtyRegions != DIRTY_NONE) {
    renderUi();
  }

  gpUpdateLcd(recordingActive, recordingStartedMs, gp_record_seconds);

  delay(10);
}
