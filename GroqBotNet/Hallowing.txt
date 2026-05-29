#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// HalloWing M0 Express built-in TFT pins
#define TFT_CS 39
#define TFT_DC 38
#define TFT_RST 37
#define TFT_LITE 7

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

static const uint32_t UART_BAUD = 115200;

// Supported UART lines from the Pi:
// HEADER=Groq / STATUS=idle / TIME=1:25 / RPD=42
// MODEL=gemini / METER=$1.23 / PERSONALITY=Spooky
// SOURCE=WHISPLAY or SOURCE=STANDALONE

String lineBuffer;

String headerLine = "Waiting for Pi";
String statusLine = "LINK";
String timeLine = "--:--";
String rpdLine = "--";
String modelLine = "--";
String moneyLine = "--";
String personalityLine = "--";
String sourceLine = "COMPANION";
bool hasPiLink = false;
bool dirty = true;

uint16_t colorOrange = ST77XX_YELLOW;
uint16_t colorPurple = ST77XX_MAGENTA;
uint16_t colorLime = ST77XX_GREEN;
uint16_t colorBlue = ST77XX_CYAN;
uint16_t colorText = ST77XX_WHITE;
uint16_t colorDim = 0x7BEF;
uint16_t colorCard = 0x1082;

String trimValue(String value) {
  value.trim();
  return value;
}

String upperKey(String key) {
  key.trim();
  key.toUpperCase();
  return key;
}

void resetDashboard() {
  headerLine = "Waiting for Pi";
  statusLine = "LINK";
  timeLine = "--:--";
  rpdLine = "--";
  modelLine = "--";
  moneyLine = "--";
  personalityLine = "--";
  sourceLine = "COMPANION";
  hasPiLink = false;
  dirty = true;
}

void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t outline) {
  tft.drawRoundRect(x, y, w, h, 6, outline);
  tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 6, colorCard);
}

void drawField(
  int16_t x,
  int16_t y,
  int16_t w,
  int16_t h,
  const String &label,
  const String &value,
  uint16_t accent
) {
  drawCard(x, y, w, h, accent);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setCursor(x + 4, y + 4);
  tft.setTextColor(colorDim);
  tft.print(label);
  tft.setCursor(x + 4, y + 15);
  tft.setTextColor(accent);
  tft.print(value.substring(0, 12));
}

void drawDashboard() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);

  tft.drawRect(0, 0, 128, 18, colorPurple);
  tft.setCursor(4, 5);
  tft.setTextColor(colorOrange);
  tft.print("HALLOWING");

  tft.setCursor(74, 5);
  tft.setTextColor(colorBlue);
  tft.print(timeLine.substring(0, 8));

  tft.setCursor(4, 22);
  tft.setTextColor(colorText);
  tft.print((hasPiLink ? headerLine : String("Waiting for Pi")).substring(0, 20));

  tft.setCursor(4, 32);
  tft.setTextColor(colorLime);
  tft.print((hasPiLink ? statusLine : String("Awaiting link")).substring(0, 20));

  drawField(4, 42, 58, 28, "RPD", rpdLine, colorOrange);
  drawField(66, 42, 58, 28, "MODEL", modelLine, colorBlue);
  drawField(4, 74, 58, 28, "METER", moneyLine, colorPurple);
  drawField(66, 74, 58, 28, "VIBE", personalityLine, colorLime);

  drawCard(4, 106, 120, 18, colorBlue);
  tft.setCursor(8, 112);
  tft.setTextColor(colorBlue);
  tft.print(sourceLine.substring(0, 18));
}

void handleCommand(String rawLine) {
  rawLine.trim();
  if (rawLine.length() == 0) {
    return;
  }

  int splitAt = rawLine.indexOf('=');
  if (splitAt < 0) {
    splitAt = rawLine.indexOf(':');
  }

  if (splitAt < 0) {
    return;
  }

  String key = upperKey(rawLine.substring(0, splitAt));
  String value = trimValue(rawLine.substring(splitAt + 1));

  hasPiLink = true;

  if (key == "HEADER" || key == "TITLE") {
    headerLine = value;
  } else if (key == "STATUS") {
    statusLine = value;
  } else if (key == "TIME" || key == "CLOCK") {
    timeLine = value;
  } else if (key == "RPD") {
    rpdLine = value;
  } else if (key == "MODEL") {
    modelLine = value;
  } else if (key == "METER" || key == "MONEY" || key == "BALANCE") {
    moneyLine = value;
  } else if (key == "PERSONALITY" || key == "VIBE") {
    personalityLine = value.length() > 0 ? value : "--";
  } else if (key == "SOURCE") {
    sourceLine = value.length() > 0 ? value : "COMPANION";
  } else if (key == "RESET") {
    resetDashboard();
    return;
  } else {
    return;
  }

  Serial.print("RX: ");
  Serial.println(rawLine);
  dirty = true;
}

void setup() {
  pinMode(TFT_LITE, OUTPUT);
  digitalWrite(TFT_LITE, HIGH);

  tft.initR(INITR_144GREENTAB);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);

  Serial.begin(115200);
  Serial1.begin(UART_BAUD);

  drawDashboard();
  Serial.println("HalloWing ready.");
}

void loop() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      handleCommand(lineBuffer);
      lineBuffer = "";
    } else if (lineBuffer.length() < 120) {
      lineBuffer += c;
    }
  }

  if (dirty) {
    drawDashboard();
    dirty = false;
  }
}
