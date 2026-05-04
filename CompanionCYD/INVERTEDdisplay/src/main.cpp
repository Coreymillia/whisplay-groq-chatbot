#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
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

static const uint16_t COLOR_BG = RGB565_BLACK;
static const uint16_t COLOR_PANEL = 0x0841;
static const uint16_t COLOR_HEADER = 0x0016;
static const uint16_t COLOR_HEADER_TEXT = 0x07FF;
static const uint16_t COLOR_TEXT = RGB565_WHITE;
static const uint16_t COLOR_DIM = 0x7BEF;
static const uint16_t COLOR_ACCENT = 0x07E0;
static const uint16_t COLOR_WARN = 0xFFE0;
static const uint16_t COLOR_ERROR = 0xF800;

struct TouchButton {
  const char *label;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  uint16_t bg;
};

static TouchButton buttons[] = {
  { "NEW CHAT", 10, 170, 145, 26, 0x02A0 },
  { "REPEAT", 165, 170, 145, 26, 0x01CF },
  { "CAPTURE", 10, 204, 145, 26, 0x4200 },
  { "VOICE", 165, 204, 145, 26, 0x780F },
};
static TouchButton setupButton = { "SETUP", 244, 3, 56, 16, 0x5008 };

static CompanionState companionState;
static CompanionSettings companionSettings;
static String toastMessage;
static bool renderDirty = true;
static bool touchWasDown = false;
static unsigned long lastTouchMs = 0;
static unsigned long lastStatePollMs = 0;
static unsigned long lastSettingsPollMs = 0;
static unsigned long lastBootHoldStartMs = 0;
static unsigned long toastUntilMs = 0;
static unsigned long lastSuccessfulPollMs = 0;

static const unsigned long TOUCH_DEBOUNCE_MS = 250;
static const unsigned long STATE_POLL_MS = 900;
static const unsigned long SETTINGS_POLL_MS = 6000;
static const unsigned long BOOT_PORTAL_HOLD_MS = 3000;

static void mapTouch(uint16_t rawX, uint16_t rawY, int &screenX, int &screenY) {
  screenX = map(rawX, 200, 3800, 0, 320);
  screenY = map(rawY, 200, 3800, 0, 240);
  screenX = constrain(screenX, 0, 319);
  screenY = constrain(screenY, 0, 239);
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
    return 0x07E0;
  }
  if (mode == "speak-on-demand") {
    return 0xFFE0;
  }
  return 0x07FF;
}

static void setToast(const String &message, uint16_t durationMs = 2000) {
  toastMessage = message;
  toastUntilMs = millis() + durationMs;
  renderDirty = true;
}

static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  String source = sourceText;
  source.replace("\r", "");
  if (source.length() > 700) {
    source = source.substring(source.length() - 700);
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

static void drawWrappedTailText(int16_t x, int16_t y, int16_t w, int16_t h, const String &text) {
  const int maxChars = max(10, (w / 6) - 1);
  const int visibleLines = max(1, h / 10);
  String wrapped[48];
  int wrappedCount = 0;
  fillWrappedLines(text.length() ? text : "Waiting for chatbot text...", wrapped, wrappedCount, 48, maxChars);

  int startLine = max(0, wrappedCount - visibleLines);
  gfx->setTextColor(COLOR_TEXT, COLOR_PANEL);
  gfx->setTextSize(1);
  int cursorY = y;
  for (int i = startLine; i < wrappedCount; i++) {
    gfx->setCursor(x, cursorY);
    gfx->print(wrapped[i]);
    cursorY += 10;
  }
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

static void openSetupPortal() {
  gfx->fillScreen(COLOR_BG);
  gfx->setTextColor(COLOR_WARN, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(22, 96);
  gfx->print("Opening setup...");
  delay(600);
  ccRunPortal();
}

static void renderUi() {
  gfx->fillScreen(COLOR_BG);

  gfx->fillRect(0, 0, 320, 22, COLOR_HEADER);
  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER);
  gfx->setTextSize(1);
  gfx->setCursor(8, 7);
  gfx->print("WHISPLAY CYD");
  drawButton(setupButton);

  uint16_t wifiColor = WiFi.status() == WL_CONNECTED ? COLOR_ACCENT : COLOR_ERROR;
  gfx->fillCircle(230, 11, 4, wifiColor);

  gfx->fillRect(0, 24, 320, 18, COLOR_BG);
  gfx->setTextColor(
    WiFi.status() == WL_CONNECTED ? COLOR_WARN : COLOR_ERROR,
    COLOR_BG
  );
  gfx->setCursor(8, 29);
  if (WiFi.status() != WL_CONNECTED) {
    gfx->print("Connecting WiFi...");
  } else {
    gfx->print(companionState.status.length() ? companionState.status : "Starting...");
  }

  gfx->setTextColor(voiceModeColor(companionSettings.voiceMode), COLOR_BG);
  const char *voiceLabel = voiceModeLabel(companionSettings.voiceMode);
  int voiceX = 314 - (strlen(voiceLabel) * 6);
  gfx->setCursor(voiceX, 29);
  gfx->print(voiceLabel);

  gfx->drawRoundRect(8, 48, 304, 112, 6, COLOR_DIM);
  gfx->fillRoundRect(8, 48, 304, 112, 6, COLOR_PANEL);

  gfx->setTextColor(COLOR_DIM, COLOR_PANEL);
  gfx->setCursor(16, 56);
  gfx->print("PRESET:");
  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_PANEL);
  gfx->setCursor(60, 56);
  gfx->print(companionSettings.personalityPresetId.length() ? companionSettings.personalityPresetId : "custom");

  gfx->setTextColor(COLOR_DIM, COLOR_PANEL);
  gfx->setCursor(236, 56);
  if (companionState.imageIconVisible) {
    gfx->print("IMG");
  }
  if (companionState.ragIconVisible) {
    gfx->setCursor(278, 56);
    gfx->print("RAG");
  }

  drawWrappedTailText(16, 72, 288, 78, companionState.text);

  for (const TouchButton &button : buttons) {
    drawButton(button);
  }

  gfx->fillRect(0, 234, 320, 6, COLOR_BG);
  gfx->setTextColor(COLOR_DIM, COLOR_BG);
  gfx->setCursor(8, 234);
  if (toastUntilMs > millis() && toastMessage.length()) {
    gfx->print(toastMessage);
  } else if (lastSuccessfulPollMs > 0) {
    gfx->print("Tap SETUP or BOOT 3s");
  } else {
    gfx->print("Connecting...");
  }
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
    nextState.imageIconVisible != companionState.imageIconVisible;

  companionState = nextState;
  lastSuccessfulPollMs = now;
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
    nextSettings.personalityPresetId != companionSettings.personalityPresetId;

  companionSettings = nextSettings;
  if (changed) {
    renderDirty = true;
  }
  return true;
}

static bool pointInButton(int x, int y, const TouchButton &button) {
  return x >= button.x && x <= (button.x + button.w) && y >= button.y && y <= (button.y + button.h);
}

static void handleButtonTap(const TouchButton &button) {
  bool ok = false;
  if (strcmp(button.label, "NEW CHAT") == 0) {
    ok = apiResetChat();
    if (ok) {
      companionState.text = "Started a new chat.";
    }
    setToast(ok ? "Started a new chat." : "New chat failed.");
  } else if (strcmp(button.label, "REPEAT") == 0) {
    ok = apiRepeatLastAnswer();
    setToast(ok ? "Replaying last answer." : "Replay failed.");
  } else if (strcmp(button.label, "CAPTURE") == 0) {
    ok = apiCaptureVision();
    setToast(ok ? "Capture requested." : "Capture failed.");
  } else if (strcmp(button.label, "VOICE") == 0) {
    ok = apiCycleVoiceMode(companionSettings);
    if (ok) {
      setToast(String("Voice: ") + voiceModeLabel(companionSettings.voiceMode));
    } else {
      setToast("Voice mode update failed.");
    }
  }
  renderDirty = true;
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

  if (pointInButton(x, y, setupButton)) {
    openSetupPortal();
    return;
  }

  for (const TouchButton &button : buttons) {
    if (pointInButton(x, y, button)) {
      handleButtonTap(button);
      return;
    }
  }
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
  gfx->invertDisplay(true);
  gfx->fillScreen(COLOR_BG);
  gfx->setTextColor(COLOR_HEADER_TEXT, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(28, 84);
  gfx->print("Whisplay CYD");
  gfx->setTextSize(1);
  gfx->setCursor(82, 108);
  gfx->print("Companion");

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  bool forcePortal = digitalRead(BOOT_BTN) == LOW;
  ccConnect(forcePortal);

  analogWrite(GFX_BL, cc_brightness);
  fetchStateIfDue();
  fetchSettingsIfDue();
  renderUi();
}

void loop() {
  ccEnsureWifiConnected();
  fetchStateIfDue();
  fetchSettingsIfDue();
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
