#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <vector>

#include "GroqApi.h"
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
static std::vector<String> conversationLog;
static bool renderDirty = true;
static unsigned long toastUntilMs = 0;
static unsigned long lastRenderMs = 0;
static int logScrollOffset = 0;

static bool recordingActive = false;
static int16_t *recordingSamples = nullptr;
static size_t recordingCapacitySamples = 0;
static size_t recordingCapturedSamples = 0;
static unsigned long recordingStartedMs = 0;

static constexpr size_t SAMPLE_RATE = 16000;
static constexpr size_t RECORD_CHUNK_SAMPLES = 240;
static constexpr size_t MAX_LOG_ENTRIES = 48;
static constexpr size_t MAX_DISPLAY_CHARS = 700;
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

static int chatTextScale() {
  return max(1, min(3, static_cast<int>(gp_text_scale)));
}

static int scaledCharWidth() {
  return 6 * chatTextScale();
}

static int scaledLineHeight() {
  return (8 * chatTextScale()) + 2;
}

static void setToast(const String &message, uint16_t durationMs = 2200) {
  toastMessage = message;
  toastUntilMs = millis() + durationMs;
  renderDirty = true;
}

static String clampLogText(const String &value) {
  if (value.length() <= MAX_DISPLAY_CHARS) return value;
  return value.substring(value.length() - MAX_DISPLAY_CHARS);
}

static void appendLogEntry(const String &prefix, const String &text) {
  String normalized = text;
  normalized.trim();
  if (!normalized.length()) return;
  conversationLog.push_back(prefix + clampLogText(normalized));
  while (conversationLog.size() > MAX_LOG_ENTRIES) {
    conversationLog.erase(conversationLog.begin());
  }
  logScrollOffset = 0;
  renderDirty = true;
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

static void buildConversationLines(String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  if (conversationLog.empty()) {
    fillWrappedLines("Groqputer ready. Type a message or hold BtnA to record.", lines, lineCount, maxLines, maxChars);
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
  int startLine = max(0, wrappedCount - max(1, (h - 18) / scaledLineHeight()));
  M5Cardputer.Display.setTextColor(COLOR_TEXT, 0x0841);
  M5Cardputer.Display.setTextSize(chatTextScale());
  int cursorY = y + 18;
  for (int i = startLine; i < wrappedCount; i++) {
    M5Cardputer.Display.setCursor(x + 6, cursorY);
    M5Cardputer.Display.print(wrapped[i]);
    cursorY += scaledLineHeight();
  }
  M5Cardputer.Display.setTextSize(1);
}

static String modelShortLabel() {
  String model = gp_model[0] ? gp_model : GP_DEFAULT_MODEL;
  if (model.length() <= 16) return model;
  return model.substring(0, 16);
}

static void renderUi() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextFont(&fonts::Font0);
  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.setCursor(8, 8);
  M5Cardputer.Display.print("Groqputer");

  M5Cardputer.Display.setTextColor(WiFi.status() == WL_CONNECTED ? COLOR_OK : COLOR_ERROR, COLOR_BG);
  M5Cardputer.Display.setCursor(94, 8);
  M5Cardputer.Display.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "NoWiFi");

  M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_BG);
  M5Cardputer.Display.setCursor(138, 8);
  M5Cardputer.Display.print(String(gp_record_seconds) + "s");

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(172, 8);
  M5Cardputer.Display.print(modelShortLabel());

  M5Cardputer.Display.drawFastHLine(0, 18, 240, COLOR_DIM);
  M5Cardputer.Display.fillRoundRect(6, 24, 228, 80, 6, COLOR_PANEL);
  drawConversationLog(12, 30, 214, 68);
  drawInputArea(6, 110, 228, 56);

  M5Cardputer.Display.setTextColor(COLOR_DIM, COLOR_BG);
  M5Cardputer.Display.setCursor(8, 172);
  if (recordingActive) {
    unsigned long elapsedMs = millis() - recordingStartedMs;
    M5Cardputer.Display.print("Recording ");
    M5Cardputer.Display.print(elapsedMs / 1000.0f, 1);
    M5Cardputer.Display.print(" / ");
    M5Cardputer.Display.print(gp_record_seconds);
    M5Cardputer.Display.print("s");
  } else {
    M5Cardputer.Display.print("Enter=send  Hold BtnA=record  Fn+A=setup");
  }

  if (toastMessage.length()) {
    M5Cardputer.Display.fillRoundRect(12, 190, 216, 24, 6, 0x18C3);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, 0x18C3);
    M5Cardputer.Display.setCursor(18, 198);
    M5Cardputer.Display.print(toastMessage);
  }
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

  appendLogEntry("YOU ", text);
  inputBuffer = "";
  renderDirty = true;

  String reply;
  String error;
  setToast("Sending to Groq...", 1200);
  renderUi();
  if (!gpSendChatMessage(text, reply, error)) {
    setToast(error.length() ? error : "Groq request failed.", 3000);
    return;
  }
  appendLogEntry("BOT ", reply);
  setToast("Reply received.");
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
  setToast("Recording...");
}

static void finishRecording() {
  if (!recordingActive) return;
  recordingActive = false;
  M5Cardputer.Mic.end();

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

  appendLogEntry("MIC ", transcript);
  setToast("Sending to Groq...", 1200);
  renderUi();

  String reply;
  if (!gpSendChatMessage(transcript, reply, error)) {
    setToast(error.length() ? error : "Groq request failed.", 3000);
    return;
  }
  appendLogEntry("BOT ", reply);
  setToast("Reply received.");
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
  renderDirty = true;
}

static void scrollLog(int deltaLines) {
  String lines[180];
  int lineCount = 0;
  buildConversationLines(lines, lineCount, 180, max(8, (224 / scaledCharWidth()) - 1));
  const int visibleLines = max(1, 80 / scaledLineHeight());
  int maxOffset = max(0, lineCount - visibleLines);
  logScrollOffset = constrain(logScrollOffset + deltaLines, 0, maxOffset);
  renderDirty = true;
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

static void handleKeyboard() {
  if (!M5Cardputer.Keyboard.isChange()) return;
  if (!M5Cardputer.Keyboard.isPressed()) return;

  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  if (status.fn) {
    bool handledFn = false;
    for (auto c : status.word) {
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
      } else if (c == 'r' || c == 'R') {
        gpResetChatHistory();
        conversationLog.clear();
        setToast("Chat history cleared.");
        handledFn = true;
      } else if (c == ';') {
        scrollLog(LOG_SCROLL_STEP);
        handledFn = true;
      } else if (c == '.') {
        scrollLog(-LOG_SCROLL_STEP);
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
      inputBuffer += c;
    }
  }
  if (status.del && inputBuffer.length() > 0) {
    inputBuffer.remove(inputBuffer.length() - 1);
  }
  if (status.enter) {
    submitCurrentInput();
  } else {
    renderDirty = true;
  }
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

  gpConnect(false);
  gpLoadChatHistory();
  if (gp_has_settings) {
    appendLogEntry("SYS ", "Groqputer ready.");
  }
  renderUi();
}

void loop() {
  M5Cardputer.update();
  handleKeyboard();

  if (!recordingActive && M5Cardputer.BtnA.wasPressed()) {
    startRecording();
  }
  pollRecording();

  gpEnsureWifiConnected();

  if (toastUntilMs > 0 && toastUntilMs <= millis()) {
    toastUntilMs = 0;
    toastMessage = "";
    renderDirty = true;
  }

  if (renderDirty || millis() - lastRenderMs >= 150) {
    renderUi();
    renderDirty = false;
    lastRenderMs = millis();
  }

  delay(10);
}
