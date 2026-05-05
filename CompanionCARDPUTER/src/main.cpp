#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <vector>

#include "Portal.h"
#include "WhisplayApi.h"

static const uint32_t COLOR_BG = BLACK;
static const uint32_t COLOR_PANEL = 0x18C3;
static const uint32_t COLOR_ACCENT = 0x07FF;
static const uint32_t COLOR_TEXT = WHITE;
static const uint32_t COLOR_DIM = 0x7BEF;
static const uint32_t COLOR_WARN = 0xFFE0;
static const uint32_t COLOR_OK = 0x07E0;
static const uint32_t COLOR_ERROR = 0xF800;

static CompanionState companionState;
static CompanionSettings companionSettings;
static String inputBuffer;
static String toastMessage;
static String lastLoggedBotText;
static std::vector<String> conversationLog;
static unsigned long toastUntilMs = 0;
static unsigned long lastStatePollMs = 0;
static unsigned long lastSettingsPollMs = 0;
static unsigned long lastRenderMs = 0;
static bool renderDirty = true;
static int logScrollOffset = 0;

enum class UiMode : uint8_t {
  Receive,
  Send,
};

static UiMode uiMode = UiMode::Receive;

static constexpr uint32_t STATE_POLL_MS = 900;
static constexpr uint32_t SETTINGS_POLL_MS = 6000;
static constexpr size_t RECORD_LENGTH = 240;
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr uint32_t RECORD_MS = 3000;
static constexpr size_t RECORD_CHUNKS = (SAMPLE_RATE * RECORD_MS / 1000) / RECORD_LENGTH;
static constexpr size_t TOTAL_SAMPLES = RECORD_CHUNKS * RECORD_LENGTH;
static constexpr size_t MAX_LOG_ENTRIES = 48;
static constexpr int LOG_SCROLL_STEP = 3;

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

static const char *voiceModeLabel(const String &mode) {
  if (mode == "voice-chat") return "Voice";
  if (mode == "speak-on-demand") return "OnDemand";
  return "Text";
}

static const char *uiModeLabel() {
  return uiMode == UiMode::Send ? "SEND" : "RECV";
}

static int chatTextScale() {
  return max(1, min(3, static_cast<int>(cp_text_scale)));
}

static int scaledCharWidth() {
  return 6 * chatTextScale();
}

static int scaledLineHeight() {
  return (8 * chatTextScale()) + 2;
}

static String clampLogText(const String &value, size_t maxChars = 700) {
  if (value.length() <= maxChars) return value;
  return value.substring(value.length() - maxChars);
}

static void appendLogEntry(const String &prefix, const String &text, bool replaceLast = false) {
  String normalized = text;
  normalized.trim();
  if (!normalized.length()) return;

  String entry = prefix + clampLogText(normalized);
  if (replaceLast && !conversationLog.empty()) {
    conversationLog.back() = entry;
  } else {
    conversationLog.push_back(entry);
    while (conversationLog.size() > MAX_LOG_ENTRIES) {
      conversationLog.erase(conversationLog.begin());
    }
  }
  logScrollOffset = 0;
  renderDirty = true;
}

static void syncIncomingLogText(const String &text) {
  String normalized = text;
  normalized.trim();
  if (!normalized.length()) return;

  if (
    lastLoggedBotText.length() &&
    normalized.startsWith(lastLoggedBotText) &&
    !conversationLog.empty() &&
    conversationLog.back().startsWith("BOT ")
  ) {
    conversationLog.back() = "BOT " + clampLogText(normalized);
  } else if (normalized != lastLoggedBotText) {
    appendLogEntry("BOT ", normalized);
  }
  lastLoggedBotText = normalized;
  logScrollOffset = 0;
  renderDirty = true;
}

static void setToast(const String &message, uint16_t durationMs = 2200) {
  toastMessage = message;
  toastUntilMs = millis() + durationMs;
  renderDirty = true;
}

static void setUiMode(UiMode nextMode) {
  if (uiMode == nextMode) return;
  uiMode = nextMode;
  renderDirty = true;
}

static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars) {
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

static void drawWrappedTailText(int x, int y, int w, int h, const String &text, uint32_t fg, uint32_t bg) {
  const int maxChars = max(12, (w / 6) - 1);
  const int visibleLines = max(1, h / 10);
  String wrapped[80];
  int wrappedCount = 0;
  fillWrappedLines(text.length() ? text : "Waiting for Whisplay text...", wrapped, wrappedCount, 80, maxChars);
  int startLine = max(0, wrappedCount - visibleLines);
  M5Cardputer.Display.setTextColor(fg, bg);
  int cursorY = y;
  for (int i = startLine; i < wrappedCount; i++) {
    M5Cardputer.Display.setCursor(x, cursorY);
    M5Cardputer.Display.print(wrapped[i]);
    cursorY += 10;
  }
}

static String inputTail(const String &value, size_t maxChars) {
  if (value.length() <= maxChars) return value;
  return value.substring(value.length() - maxChars);
}

static void buildConversationLines(String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  if (conversationLog.empty()) {
    fillWrappedLines("Waiting for Whisplay text...", lines, lineCount, maxLines, maxChars);
    return;
  }

  for (size_t i = 0; i < conversationLog.size() && lineCount < maxLines; i++) {
    int wrappedCount = 0;
    fillWrappedLines(conversationLog[i], lines + lineCount, wrappedCount, maxLines - lineCount, maxChars);
    lineCount += wrappedCount;
    if (lineCount < maxLines && i + 1 < conversationLog.size()) {
      lines[lineCount++] = "";
    }
  }
}

static void drawConversationLog(int x, int y, int w, int h) {
  const int maxChars = max(8, (w / scaledCharWidth()) - 1);
  const int visibleLines = max(1, h / scaledLineHeight());
  String lines[180];
  int lineCount = 0;
  buildConversationLines(lines, lineCount, 180, maxChars);

  int maxOffset = max(0, lineCount - visibleLines);
  logScrollOffset = constrain(logScrollOffset, 0, maxOffset);
  int startLine = max(0, lineCount - visibleLines - logScrollOffset);
  int endLine = min(lineCount, startLine + visibleLines);

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  M5Cardputer.Display.setTextSize(chatTextScale());
  int cursorY = y;
  for (int i = startLine; i < endLine; i++) {
    M5Cardputer.Display.setCursor(x, cursorY);
    M5Cardputer.Display.print(lines[i]);
    cursorY += scaledLineHeight();
  }
  M5Cardputer.Display.setTextSize(1);

  if (maxOffset > 0) {
    if (logScrollOffset < maxOffset) {
      M5Cardputer.Display.fillTriangle(x + w - 10, y + 4, x + w - 4, y + 4, x + w - 7, y, COLOR_DIM);
    }
    if (logScrollOffset > 0) {
      M5Cardputer.Display.fillTriangle(x + w - 10, y + h - 5, x + w - 4, y + h - 5, x + w - 7, y + h - 1, COLOR_DIM);
    }
  }
}

static void drawInputArea(int x, int y, int w, int h) {
  M5Cardputer.Display.drawRoundRect(x, y, w, h, 4, COLOR_DIM);
  M5Cardputer.Display.fillRoundRect(x, y, w, h, 4, 0x0841);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, 0x0841);
  M5Cardputer.Display.setCursor(x + 6, y + 4);
  M5Cardputer.Display.print("YOU");

  String wrapped[60];
  int wrappedCount = 0;
  fillWrappedLines(
    inputBuffer.length() ? inputBuffer : "Type here...",
    wrapped,
    wrappedCount,
    60,
    max(8, ((w - 12) / scaledCharWidth()) - 1)
  );
  const int visibleLines = max(1, (h - 14) / scaledLineHeight());
  int startLine = max(0, wrappedCount - visibleLines);
  M5Cardputer.Display.setTextColor(inputBuffer.length() ? COLOR_TEXT : COLOR_DIM, 0x0841);
  M5Cardputer.Display.setTextSize(chatTextScale());
  int cursorY = y + 16;
  for (int i = startLine; i < wrappedCount; i++) {
    M5Cardputer.Display.setCursor(x + 6, cursorY);
    M5Cardputer.Display.print(wrapped[i]);
    cursorY += scaledLineHeight();
  }
  M5Cardputer.Display.setTextSize(1);
}

static void drawHeader() {
  M5Cardputer.Display.fillRect(0, 0, 240, 16, COLOR_PANEL);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.print("Whisplay Cardputer");
  M5Cardputer.Display.setCursor(152, 4);
  M5Cardputer.Display.print(voiceModeLabel(companionSettings.voiceMode));
  M5Cardputer.Display.setCursor(198, 4);
  M5Cardputer.Display.print(uiModeLabel());
  M5Cardputer.Display.fillCircle(232, 8, 3, WiFi.status() == WL_CONNECTED ? COLOR_OK : COLOR_ERROR);

  M5Cardputer.Display.fillRect(0, 16, 240, 16, COLOR_BG);
  M5Cardputer.Display.setTextColor(WiFi.status() == WL_CONNECTED ? COLOR_WARN : COLOR_ERROR, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 20);
  M5Cardputer.Display.print(
    WiFi.status() != WL_CONNECTED
      ? "Connecting WiFi..."
      : (companionState.status.length() ? companionState.status : "Starting...")
  );
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(136, 20);
  M5Cardputer.Display.print("PRESET ");
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.print(inputTail(
    companionSettings.personalityPresetId.length() ? companionSettings.personalityPresetId : "custom",
    12
  ));
}

static String latestReplySummary() {
  if (companionState.text.length()) return companionState.text;
  if (companionState.status.length()) return companionState.status;
  return "Waiting for Whisplay reply...";
}

static void drawFooter(const String &message) {
  M5Cardputer.Display.fillRect(0, 126, 240, 14, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 130);
  M5Cardputer.Display.print(inputTail(message, 38));
}

static void renderReceiveUi() {
  M5Cardputer.Display.drawRoundRect(4, 34, 232, 90, 4, COLOR_DIM);
  M5Cardputer.Display.fillRoundRect(4, 34, 232, 90, 4, COLOR_PANEL);
  drawConversationLog(8, 40, 224, 78);

  if (toastUntilMs > millis() && toastMessage.length()) {
    drawFooter(toastMessage);
  } else if (logScrollOffset > 0) {
    drawFooter("Type=send  Fn+;/. scroll  Fn+/ end");
  } else {
    drawFooter("Type=send  Fn+S send  Fn+M msgs");
  }
}

static void renderSendUi() {
  drawInputArea(4, 34, 232, 70);

  M5Cardputer.Display.drawRoundRect(4, 108, 232, 16, 4, COLOR_DIM);
  M5Cardputer.Display.fillRoundRect(4, 108, 232, 16, 4, COLOR_PANEL);
  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_PANEL);
  M5Cardputer.Display.setCursor(8, 112);
  M5Cardputer.Display.print("LAST");
  drawWrappedTailText(40, 112, 192, 10, latestReplySummary(), COLOR_TEXT, COLOR_PANEL);

  if (toastUntilMs > millis() && toastMessage.length()) {
    drawFooter(toastMessage);
  } else {
    drawFooter("Enter=send  Fn+M recv  Fn+/- size");
  }
}

static void renderUi() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextFont(&fonts::Font0);

  drawHeader();

  if (uiMode == UiMode::Send) {
    renderSendUi();
  } else {
    renderReceiveUi();
  }
}

static bool fetchStateIfDue() {
  if (!cpEnsureWifiConnected()) return false;
  unsigned long now = millis();
  if (now - lastStatePollMs < STATE_POLL_MS) return false;
  lastStatePollMs = now;
  CompanionState nextState;
  if (!apiFetchState(nextState)) return false;

  bool changed =
    nextState.ready != companionState.ready ||
    nextState.status != companionState.status ||
    nextState.text != companionState.text ||
    nextState.textInputEnabled != companionState.textInputEnabled ||
    nextState.ragIconVisible != companionState.ragIconVisible ||
    nextState.imageIconVisible != companionState.imageIconVisible;
  if (nextState.text != companionState.text && nextState.text.length()) {
    syncIncomingLogText(nextState.text);
  }
  companionState = nextState;
  if (changed) renderDirty = true;
  return true;
}

static bool fetchSettingsIfDue() {
  if (!cpEnsureWifiConnected()) return false;
  unsigned long now = millis();
  if (now - lastSettingsPollMs < SETTINGS_POLL_MS) return false;
  lastSettingsPollMs = now;
  CompanionSettings nextSettings;
  if (!apiFetchSettings(nextSettings)) return false;

  bool changed =
    !companionSettings.loaded ||
    nextSettings.voiceMode != companionSettings.voiceMode ||
    nextSettings.personalityPresetId != companionSettings.personalityPresetId;
  companionSettings = nextSettings;
  if (changed) renderDirty = true;
  return true;
}

static void submitCurrentInput() {
  String text = inputBuffer;
  text.trim();
  if (!text.length()) {
    setToast("Type a message first.");
    return;
  }
  if (!cpEnsureWifiConnected()) {
    setToast("WiFi is not connected.");
    return;
  }
  if (companionState.ready && !companionState.textInputEnabled) {
    setToast("Input is unavailable right now.");
    return;
  }
  bool ok = apiSendText(text.c_str());
  setToast(ok ? "Sent text." : "Text send failed.");
  if (ok) {
    appendLogEntry("YOU ", text);
    inputBuffer = "";
    lastLoggedBotText = "";
    setUiMode(UiMode::Receive);
    renderDirty = true;
  }
}

static bool buildWavPayload(const int16_t *samples, size_t sampleCount, uint8_t **bufferOut, size_t *lengthOut) {
  if (!samples || !bufferOut || !lengthOut) return false;
  size_t pcmBytes = sampleCount * sizeof(int16_t);
  size_t totalBytes = sizeof(WavHeader) + pcmBytes;
  uint8_t *payload = static_cast<uint8_t *>(heap_caps_malloc(totalBytes, MALLOC_CAP_8BIT));
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

static void recordAndSendAudio() {
  if (!cpEnsureWifiConnected()) {
    setToast("WiFi is not connected.");
    return;
  }

  int16_t *samples = static_cast<int16_t *>(heap_caps_malloc(TOTAL_SAMPLES * sizeof(int16_t), MALLOC_CAP_8BIT));
  if (!samples) {
    setToast("Mic buffer alloc failed.");
    return;
  }
  memset(samples, 0, TOTAL_SAMPLES * sizeof(int16_t));

  if (M5Cardputer.Speaker.isEnabled()) {
    M5Cardputer.Speaker.end();
  }
  if (!M5Cardputer.Mic.isEnabled()) {
    M5Cardputer.Mic.begin();
  }

  setToast("Recording 3s...");
  renderUi();

  for (size_t i = 0; i < RECORD_CHUNKS; i++) {
    M5Cardputer.update();
    if (!M5Cardputer.Mic.record(&samples[i * RECORD_LENGTH], RECORD_LENGTH, SAMPLE_RATE)) {
      delay(2);
    }
  }

  uint8_t *wavPayload = nullptr;
  size_t wavLength = 0;
  if (!buildWavPayload(samples, TOTAL_SAMPLES, &wavPayload, &wavLength)) {
    free(samples);
    setToast("WAV build failed.");
    return;
  }

  String transcript;
  bool ok = apiSendAudioWav(wavPayload, wavLength, transcript);
  free(wavPayload);
  free(samples);

  if (!ok) {
    transcript.trim();
    setToast(transcript.length() ? transcript : "Audio send failed.");
    return;
  }

  if (transcript.length()) {
    appendLogEntry("MIC ", transcript);
    lastLoggedBotText = "";
  }
  setToast(transcript.length() ? "Heard: " + transcript : "Audio sent.");
}

static void scrollLog(int deltaLines) {
  String lines[180];
  int lineCount = 0;
  buildConversationLines(lines, lineCount, 180, max(8, (224 / scaledCharWidth()) - 1));
  const int visibleLines = max(1, 52 / scaledLineHeight());
  int maxOffset = max(0, lineCount - visibleLines);
  logScrollOffset = constrain(logScrollOffset + deltaLines, 0, maxOffset);
  renderDirty = true;
}

static void adjustTextScale(int delta) {
  uint8_t previous = cp_text_scale;
  int next = static_cast<int>(cp_text_scale) + delta;
  if (next < 1) next = 1;
  if (next > 3) next = 3;
  cpSetTextScale(static_cast<uint8_t>(next));
  if (cp_text_scale == previous) {
    setToast(cp_text_scale == 1 ? "Already smallest text." : "Already largest text.");
    return;
  }
  setToast("Text size " + String(cp_text_scale) + "/3");
  renderDirty = true;
}

static void handleKeyboard() {
  if (!M5Cardputer.Keyboard.isChange()) return;
  if (!M5Cardputer.Keyboard.isPressed()) return;

  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  if (status.fn) {
    bool handledFn = false;
    for (auto c : status.word) {
      if (c == 's' || c == 'S') {
        setUiMode(UiMode::Send);
        handledFn = true;
      } else if (c == 'm' || c == 'M' || c == 'r' || c == 'R') {
        setUiMode(UiMode::Receive);
        handledFn = true;
      } else if (c == ';' && uiMode == UiMode::Receive) {
        scrollLog(LOG_SCROLL_STEP);
        handledFn = true;
      } else if (c == '.' && uiMode == UiMode::Receive) {
        scrollLog(-LOG_SCROLL_STEP);
        handledFn = true;
      } else if (c == '/' && uiMode == UiMode::Receive) {
        logScrollOffset = 0;
        renderDirty = true;
        handledFn = true;
      } else if (c == '+' || c == '=') {
        adjustTextScale(1);
        handledFn = true;
      } else if (c == '-' || c == '_') {
        adjustTextScale(-1);
        handledFn = true;
      }
    }
    if (handledFn) return;
  }

  for (auto c : status.word) {
    if (c >= 32 && c <= 126) {
      if (uiMode != UiMode::Send) {
        setUiMode(UiMode::Send);
      }
      inputBuffer += c;
    }
  }
  if (status.del && inputBuffer.length() > 0) {
    if (uiMode != UiMode::Send) {
      setUiMode(UiMode::Send);
    }
    inputBuffer.remove(inputBuffer.length() - 1);
  }
  if (status.enter) {
    setUiMode(UiMode::Send);
    submitCurrentInput();
  } else {
    renderDirty = true;
  }
}

static void openSetupPortal() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextFont(&fonts::Font0);
  M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_BG);
  M5Cardputer.Display.setCursor(10, 16);
  M5Cardputer.Display.print("Opening setup AP...");
  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5Cardputer.Display.setCursor(10, 32);
  M5Cardputer.Display.print("SSID: WhisplayCardputer-Setup");
  M5Cardputer.Display.setCursor(10, 44);
  M5Cardputer.Display.print("Open http://192.168.4.1");
  delay(700);
  cpRunPortal();
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(&fonts::Font0);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillScreen(COLOR_BG);

  M5Cardputer.Speaker.end();
  M5Cardputer.Mic.begin();

  cpConnect(false);
  renderUi();
}

void loop() {
  M5Cardputer.update();
  handleKeyboard();

  if (M5Cardputer.BtnA.wasHold()) {
    openSetupPortal();
  } else if (M5Cardputer.BtnA.wasClicked()) {
    recordAndSendAudio();
  }

  cpEnsureWifiConnected();
  fetchStateIfDue();
  fetchSettingsIfDue();

  if (toastUntilMs > 0 && toastUntilMs <= millis()) {
    toastUntilMs = 0;
    toastMessage = "";
    renderDirty = true;
  }

  if (renderDirty || millis() - lastRenderMs >= 1000) {
    renderUi();
    renderDirty = false;
    lastRenderMs = millis();
  }

  delay(20);
}
