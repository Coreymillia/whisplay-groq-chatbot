#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>

#define GFX_BL 21

Arduino_DataBus *bus = new Arduino_HWSPI(2, 15, 14, 13, 12);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 1);

static constexpr uint16_t SCREEN_BACKGROUND = RGB565_BLACK;
static constexpr uint16_t SCREEN_TEXT = RGB565_GREEN;
static constexpr uint16_t SCREEN_ACCENT = RGB565_CYAN;
static constexpr uint16_t SCREEN_FRAME = RGB565_WHITE;

static void initializeDisplay() {
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  gfx->begin();
  gfx->setRotation(1);
  gfx->invertDisplay(true);
}

static void drawStarterScreen() {
  gfx->fillScreen(SCREEN_BACKGROUND);

  gfx->drawRect(12, 12, 296, 216, SCREEN_FRAME);
  gfx->drawLine(24, 56, 296, 56, SCREEN_ACCENT);
  gfx->fillRect(24, 160, 40, 40, SCREEN_ACCENT);

  gfx->setTextColor(SCREEN_TEXT);
  gfx->setTextSize(3);
  gfx->setCursor(34, 82);
  gfx->print("CYD STARTER");

  gfx->setTextColor(SCREEN_FRAME);
  gfx->setTextSize(1);
  gfx->setCursor(24, 132);
  gfx->print("Edit src/main.cpp for text, color, lines, and shapes.");
}

void setup() {
  initializeDisplay();
  drawStarterScreen();
}

void loop() {
  delay(100);
}
