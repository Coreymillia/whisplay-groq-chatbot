#include <Arduino.h>
#include <M5Cardputer.h>

static constexpr uint32_t COLOR_BG = BLACK;
static constexpr uint32_t COLOR_TEXT = WHITE;
static constexpr uint32_t COLOR_ACCENT = 0x07FF;
static constexpr uint32_t COLOR_OK = 0x07E0;
static constexpr uint32_t COLOR_WARN = 0xFFE0;
static constexpr uint32_t COLOR_ERROR = 0xF800;

static constexpr int STICKV_UART_BAUD = 115200;
static constexpr int CARDPUTER_GROVE_TX_PIN = 1;
static constexpr int CARDPUTER_GROVE_RX_PIN = 2;
static constexpr size_t LOG_LINE_COUNT = 7;
static constexpr unsigned long STATUS_STALE_MS = 5000;
static constexpr unsigned long PREVIEW_POLL_MS = 3000;
static constexpr unsigned long FRAME_RX_TIMEOUT_MS = 8000;
static constexpr int PREVIEW_FRAME_WIDTH = 80;
static constexpr int PREVIEW_FRAME_HEIGHT = 60;
static constexpr int PREVIEW_SCALE = 2;
static constexpr size_t PREVIEW_FRAME_BYTES =
    PREVIEW_FRAME_WIDTH * PREVIEW_FRAME_HEIGHT * 2;

static String logLines[LOG_LINE_COUNT];
static size_t logLineCount = 0;
static String lastStatus = "Waiting for StickV";
static uint16_t statusColor = COLOR_WARN;
static unsigned long lastInboundMs = 0;
static unsigned long lastHeartbeatMs = 0;
static unsigned long lastPreviewPollMs = 0;
static unsigned long frameRxStartedMs = 0;
static bool previewMode = false;
static bool previewPolling = false;
static bool receivingFrame = false;
static bool awaitingFrameTrailer = false;
static bool hasPreviewFrame = false;
static size_t previewFrameOffset = 0;
static uint8_t previewFrame[PREVIEW_FRAME_BYTES];
static char inboundLineBuffer[128];
static size_t inboundLineLength = 0;

static String summarizeInbound(const String &line) {
  if (line.startsWith("FACE:DETECTED")) {
    statusColor = COLOR_OK;
    return "Face detected";
  }
  if (line.startsWith("FACE:TRACKING")) {
    statusColor = COLOR_OK;
    return "Face tracking";
  }
  if (line.startsWith("FACE:NONE")) {
    statusColor = COLOR_WARN;
    return "No face in frame";
  }
  if (line.startsWith("MODEL:MISSING")) {
    statusColor = COLOR_ERROR;
    return "StickV model missing";
  }
  if (line.startsWith("STATUS:READY") || line.startsWith("STATUS:")) {
    statusColor = COLOR_OK;
    return "StickV ready";
  }
  if (line.startsWith("ERR:")) {
    statusColor = COLOR_ERROR;
    return "StickV error";
  }
  if (line.startsWith("BOOT:FACE_READY")) {
    statusColor = COLOR_OK;
    return "Face model ready";
  }
  if (line.startsWith("PONG")) {
    statusColor = COLOR_OK;
    return "PING ok";
  }
  statusColor = COLOR_ACCENT;
  return line;
}

static void pushLogLine(const String &line) {
  if (!line.length()) {
    return;
  }
  if (logLineCount < LOG_LINE_COUNT) {
    logLines[logLineCount++] = line;
    return;
  }
  for (size_t i = 1; i < LOG_LINE_COUNT; i++) {
    logLines[i - 1] = logLines[i];
  }
  logLines[LOG_LINE_COUNT - 1] = line;
}

static void drawPreviewImage() {
  const int frameWidth = PREVIEW_FRAME_WIDTH * PREVIEW_SCALE;
  const int frameHeight = PREVIEW_FRAME_HEIGHT * PREVIEW_SCALE;
  const int originX = (M5Cardputer.Display.width() - frameWidth) / 2;
  const int originY = 12;

  M5Cardputer.Display.fillRect(originX - 1, originY - 1, frameWidth + 2,
                               frameHeight + 2, COLOR_ACCENT);
  for (int y = 0; y < PREVIEW_FRAME_HEIGHT; y++) {
    for (int x = 0; x < PREVIEW_FRAME_WIDTH; x++) {
      const size_t pixelIndex = (y * PREVIEW_FRAME_WIDTH) + x;
      const size_t byteIndex = pixelIndex * 2;
      const uint16_t color =
          (static_cast<uint16_t>(previewFrame[byteIndex]) << 8) |
          previewFrame[byteIndex + 1];
      M5Cardputer.Display.fillRect(originX + (x * PREVIEW_SCALE),
                                   originY + (y * PREVIEW_SCALE),
                                   PREVIEW_SCALE, PREVIEW_SCALE, color);
    }
  }
}

static void drawLogUi() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.println("StickVEYE Face Test");

  M5Cardputer.Display.setTextColor(statusColor, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 18);
  M5Cardputer.Display.println(lastStatus);

  M5Cardputer.Display.drawFastHLine(0, 30, M5Cardputer.Display.width(), COLOR_ACCENT);

  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  int y = 36;
  for (size_t i = 0; i < logLineCount; i++) {
    M5Cardputer.Display.setCursor(4, y);
    M5Cardputer.Display.println(logLines[i]);
    y += 13;
  }

  M5Cardputer.Display.drawFastHLine(0, 122, M5Cardputer.Display.width(), COLOR_ACCENT);
  M5Cardputer.Display.setTextColor(COLOR_OK, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 126);
  M5Cardputer.Display.print("P ping I info S snap F frame");
  M5Cardputer.Display.setTextColor(COLOR_WARN, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 134);
  M5Cardputer.Display.print("V auto L log E D C");
}

static void drawPreviewUi() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5Cardputer.Display.setCursor(4, 2);
  M5Cardputer.Display.print("StickVEYE Preview");
  M5Cardputer.Display.setTextColor(statusColor, COLOR_BG);
  M5Cardputer.Display.setCursor(130, 2);
  M5Cardputer.Display.print(lastStatus);

  if (hasPreviewFrame) {
    drawPreviewImage();
  } else {
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
    M5Cardputer.Display.setCursor(32, 56);
    M5Cardputer.Display.print(receivingFrame ? "Receiving preview..."
                                             : "Press F for a snapshot");
  }

  M5Cardputer.Display.drawFastHLine(0, 132, M5Cardputer.Display.width(),
                                    COLOR_ACCENT);
  M5Cardputer.Display.setTextColor(previewPolling ? COLOR_OK : COLOR_WARN,
                                   COLOR_BG);
  M5Cardputer.Display.setCursor(4, 134);
  M5Cardputer.Display.print(previewPolling ? "Auto preview ON" : "Auto preview OFF");
  M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5Cardputer.Display.setCursor(124, 134);
  M5Cardputer.Display.print("F frame V auto L log");
}

static void drawUi() {
  if (previewMode) {
    drawPreviewUi();
    return;
  }
  drawLogUi();
}

static void noteLocalStatus(const String &status, uint16_t color) {
  lastStatus = status;
  statusColor = color;
  drawUi();
}

static void requestPreviewFrame(bool recordLog);

static void finishPreviewFrame(bool ok, const String &detail) {
  receivingFrame = false;
  awaitingFrameTrailer = false;
  previewFrameOffset = 0;
  frameRxStartedMs = 0;

  if (ok) {
    hasPreviewFrame = true;
    lastInboundMs = millis();
    lastStatus = detail.length() ? detail : "Preview updated";
    statusColor = COLOR_OK;
  } else {
    lastStatus = detail.length() ? detail : "Preview failed";
    statusColor = COLOR_ERROR;
    pushLogLine("< ERR:FRAME");
  }
  drawUi();
}

static void beginPreviewFrame() {
  receivingFrame = true;
  awaitingFrameTrailer = false;
  previewFrameOffset = 0;
  frameRxStartedMs = millis();
  previewMode = true;
  noteLocalStatus("Receiving preview", COLOR_ACCENT);
}

static void handleInboundLine(const String &line) {
  if (!line.length()) {
    return;
  }

  if (awaitingFrameTrailer) {
    if (line == "FRAME:END") {
      finishPreviewFrame(true, "Preview updated");
      return;
    }
    finishPreviewFrame(false, "Bad frame trailer");
    pushLogLine("< " + line);
    return;
  }

  if (line == "FRAME:RGB565:80:60:9600") {
    beginPreviewFrame();
    return;
  }

  lastInboundMs = millis();
  lastStatus = summarizeInbound(line);
  pushLogLine("< " + line);
  Serial.println("[StickV RX] " + line);
  drawUi();
}

static void sendCommand(const char *command, bool recordLog = true) {
  Serial1.print(command);
  Serial1.print('\n');
  if (recordLog) {
    String summary = "> ";
    summary += command;
    pushLogLine(summary);
  }
  lastStatus = String("TX ") + command;
  statusColor = COLOR_ACCENT;
  Serial.println(String("[StickV TX] ") + command);
  drawUi();
}

static void requestPreviewFrame(bool recordLog) {
  if (receivingFrame || awaitingFrameTrailer) {
    return;
  }
  previewMode = true;
  lastPreviewPollMs = millis();
  sendCommand("FRAME", recordLog);
}

static void clearLog() {
  logLineCount = 0;
  lastStatus = "Log cleared";
  statusColor = COLOR_WARN;
  drawUi();
}

static void pollKeyboard() {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
  for (char key : state.word) {
    switch (key) {
      case 'p':
      case 'P':
        sendCommand("PING");
        break;
      case 'i':
      case 'I':
        sendCommand("STATUS");
        break;
      case 's':
      case 'S':
        sendCommand("SNAP");
        break;
      case 'f':
      case 'F':
        previewPolling = false;
        previewMode = true;
        requestPreviewFrame(true);
        break;
      case 'v':
      case 'V':
        previewMode = true;
        previewPolling = !previewPolling;
        if (previewPolling) {
          requestPreviewFrame(true);
        } else {
          noteLocalStatus("Auto preview paused", COLOR_WARN);
        }
        break;
      case 'l':
      case 'L':
        previewPolling = false;
        previewMode = false;
        drawUi();
        break;
      case 'e':
      case 'E':
        sendCommand("EVENT");
        break;
      case 'd':
      case 'D':
        sendCommand("DETECT:TOGGLE");
        break;
      case 'c':
      case 'C':
        clearLog();
        break;
      default:
        break;
    }
  }
}

static void pollStickVSerial() {
  while (Serial1.available() > 0) {
    if (receivingFrame) {
      const size_t remaining = PREVIEW_FRAME_BYTES - previewFrameOffset;
      const size_t available = Serial1.available();
      const size_t toRead = available < remaining ? available : remaining;
      if (toRead == 0) {
        break;
      }
      previewFrameOffset += Serial1.readBytes(
          previewFrame + previewFrameOffset, toRead);
      if (previewFrameOffset >= PREVIEW_FRAME_BYTES) {
        receivingFrame = false;
        awaitingFrameTrailer = true;
      }
      continue;
    }

    const char c = static_cast<char>(Serial1.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      inboundLineBuffer[inboundLineLength] = '\0';
      handleInboundLine(String(inboundLineBuffer));
      inboundLineLength = 0;
      continue;
    }
    if (inboundLineLength < sizeof(inboundLineBuffer) - 1) {
      inboundLineBuffer[inboundLineLength++] = c;
    }
  }

  const unsigned long now = millis();
  if ((receivingFrame || awaitingFrameTrailer) && frameRxStartedMs != 0 &&
      now - frameRxStartedMs > FRAME_RX_TIMEOUT_MS) {
    finishPreviewFrame(false, "Preview timeout");
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);
  Serial.begin(115200);
  Serial1.setRxBufferSize(PREVIEW_FRAME_BYTES + 512);
  Serial1.begin(STICKV_UART_BAUD, SERIAL_8N1, CARDPUTER_GROVE_RX_PIN, CARDPUTER_GROVE_TX_PIN);
  pushLogLine("Grove UART RX=2 TX=1 @115200");
  pushLogLine("StickV 35->G1, 34->G2");
  pushLogLine("Use Cardputer 5VIN path only");
  drawUi();
  sendCommand("PING");
}

void loop() {
  M5Cardputer.update();
  pollKeyboard();
  pollStickVSerial();

  unsigned long now = millis();
  if (previewPolling && !receivingFrame && !awaitingFrameTrailer &&
      now - lastPreviewPollMs >= PREVIEW_POLL_MS) {
    requestPreviewFrame(false);
  }
  if (!previewMode && now - lastHeartbeatMs >= 4000) {
    lastHeartbeatMs = now;
    sendCommand("STATUS");
  }
  if (lastInboundMs != 0 && now - lastInboundMs > STATUS_STALE_MS && statusColor != COLOR_WARN) {
    lastStatus = "No fresh StickV reply";
    statusColor = COLOR_WARN;
    drawUi();
  }
  delay(20);
}
