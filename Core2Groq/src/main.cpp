/**
 * Core2Groq — unified M5Core2 bot + OTR radio firmware
 *
 * Starts from the Core2 OTR radio firmware and adds a Groq chatbot mode.
 * Radio mode is preserved as much as possible while the first bot mode is
 * added as a separate screen with its own runtime.
 */

#include <Arduino.h>
#include <M5Core2.h>
#include <WiFiMulti.h>
#include <Audio.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>

#include "GroqApi.h"
#include "Core2Lcd.h"
#include "Portal.h"

#define SCREEN_W 320
#define SCREEN_H 240

// Radio palette
#define SW_AMBER 0xFAE0
#define SW_AMBER_D 0x7100
#define SW_HDR_BG 0x1082
#define SW_BTN_BG 0x2126
#define SW_SLOT_BG 0x0841
#define SW_GRN_DIM 0x0180
#define SW_DGREY 0x2945

// Radio layout
#define SW_LINE1 24
#define SW_DIAL_LN 44
#define SW_LINE2 66
#define SW_NAME_Y 72
#define SW_LINE3 108
#define SW_VU_BOT 162
#define SW_INFO_Y 166
#define SW_LINE4 178
#define SW_TCK_Y 182
#define SW_LINE5 201
#define SW_BTN_Y 206

#define DIAL_X1 20
#define DIAL_X2 300

#define TOUCH_FOOTER_Y 202
#define TOUCH_DEBOUNCE 250
#define ZONE_W 107

#define I2S_BCK 12
#define I2S_LRC 0
#define I2S_DOUT 2

static constexpr size_t BOT_MAX_VISIBLE_CHARS = 420;
static constexpr uint32_t BOT_SAMPLE_RATE = 44100;
static constexpr int BOT_REPLY_X = 10;
static constexpr int BOT_REPLY_Y = 30;
static constexpr int BOT_REPLY_W = 300;
static constexpr int BOT_REPLY_H = 144;
static constexpr int BOT_STATUS_Y = 178;
static constexpr int BOT_ACTION_Y = 192;
static constexpr int BOT_ACTION_H = 20;
static constexpr int BOT_ACTION_W = 96;
static constexpr int BOT_ACTION_GAP = 6;
static constexpr int BOT_ACTION_SETUP_X = 10;
static constexpr int BOT_ACTION_RADIO_X = BOT_ACTION_SETUP_X + BOT_ACTION_W + BOT_ACTION_GAP;
static constexpr int BOT_ACTION_NEW_X = BOT_ACTION_RADIO_X + BOT_ACTION_W + BOT_ACTION_GAP;
static constexpr int BOT_VIEW_TOGGLE_X = 194;
static constexpr int BOT_VIEW_TOGGLE_Y = 3;
static constexpr int BOT_VIEW_TOGGLE_W = 44;
static constexpr int BOT_VIEW_TOGGLE_H = 18;
static constexpr int BOT_FOOTER_LABEL_Y = 222;

enum class AppMode : uint8_t {
    Bot,
    Radio,
};

enum class BotRecordingMode : uint8_t {
    Timed,
    Hold,
};

enum class BotScreen : uint8_t {
    Chat,
    Settings,
};

enum class BotReplyView : uint8_t {
    Assistant,
    User,
};

enum BotDirtyRegion : uint8_t {
    BOT_DIRTY_NONE = 0,
    BOT_DIRTY_HEADER = 1 << 0,
    BOT_DIRTY_REPLY = 1 << 1,
    BOT_DIRTY_STATUS = 1 << 2,
    BOT_DIRTY_ACTIONS = 1 << 3,
    BOT_DIRTY_FOOTER = 1 << 4,
    BOT_DIRTY_ALL = BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_STATUS | BOT_DIRTY_ACTIONS |
                    BOT_DIRTY_FOOTER,
};

struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = BOT_SAMPLE_RATE;
    uint32_t byteRate = BOT_SAMPLE_RATE * sizeof(int16_t);
    uint16_t blockAlign = sizeof(int16_t);
    uint16_t bitsPerSample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;
};

static void haptic(int ms = 40) {
    M5.Axp.SetLDOEnable(3, true);
    delay(ms);
    M5.Axp.SetLDOEnable(3, false);
}

#define ns 12
String stations[ns] = {
    "http://149.255.60.195:8256/stream",
    "http://149.255.60.193:8162/stream",
    "http://149.255.60.194:8043/stream",
    "http://149.255.60.195:8027/stream",
    "http://149.255.60.195:8150/stream",
    "http://149.255.60.195:8168/stream",
    "http://149.255.60.193:8168/stream",
    "http://149.255.60.194:8039/stream",
    "http://149.255.60.195:8162/stream",
    "http://149.255.60.195:8174/stream",
    "http://149.255.60.195:8180/stream",
    "http://149.255.60.194:8110/stream",
};
String stationNames[ns] = {
    "1940s Radio",
    "American Comedy",
    "American Classics",
    "Jazz Central",
    "Comedy Gold",
    "Mystery Radio",
    "Crime & Suspense",
    "Crime Radio",
    "Adventure Stories",
    "Drama Radio",
    "Nostalgia Lane",
    "Science Fiction",
};

TFT_eSprite sprite2(&M5.Lcd);
Audio audio(false, 3, I2S_NUM_1);
Preferences prefs;
WiFiMulti wifiMulti;
TaskHandle_t audioTaskHandle = nullptr;

String curStation = "";
String songPlaying = "";
long bitrate = 0;
bool connected = false;
int songposition = -310;
float voltage = 4.20f;
int batLevel = 0;
bool canDraw = false;
int rssi = 0;
int chosen = 0;
int volume = 5;
int g[14] = {0};

bool inSettings = false;
int settingSel = 0;
int8_t settingBass = 0;
int8_t settingTreble = 0;
bool screenOn = true;
bool radioStreamingStarted = false;

unsigned short grays[18];

AppMode activeMode = AppMode::Bot;
BotScreen botScreen = BotScreen::Chat;
String botStatus = "Ready.";
String botPendingUserMessage;
String botLastUserMessage;
bool botRecording = false;
bool botDrawNeeded = true;
uint8_t botDirtyRegions = BOT_DIRTY_ALL;
uint8_t *botRecordingBuffer = nullptr;
size_t botRecordingCapacity = 0;
size_t botRecordingBytes = 0;
unsigned long botRecordingStartedMs = 0;
BotRecordingMode botRecordingMode = BotRecordingMode::Timed;
String botUserPreviewMessage;
unsigned long botUserPreviewUntilMs = 0;
int botReplyScrollOffset = 0;
unsigned long botAutoScrollPauseUntilMs = 0;
unsigned long botLastAutoScrollMs = 0;
BotReplyView botReplyView = BotReplyView::Assistant;

static void markBotDirty(uint8_t regions) {
    botDirtyRegions |= regions;
    botDrawNeeded = true;
}

static String clampText(const String &value, size_t maxChars = BOT_MAX_VISIBLE_CHARS) {
    if (value.length() <= maxChars) return value;
    return value.substring(value.length() - maxChars);
}

static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount,
                             int maxLines, int maxChars) {
    lineCount = 0;
    String source = sourceText;
    source.replace("\r", "");
    source = clampText(source, 900);
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

static void drawWrappedBlock(const String &text, int x, int y, int w, int h,
                             uint16_t fg, uint16_t bg, uint8_t font = 2) {
    String lines[24];
    int lineCount = 0;
    int maxChars = max(10, (w / 8) - 1);
    fillWrappedLines(text.length() ? text : "—", lines, lineCount, 24, maxChars);

    M5.Lcd.fillRoundRect(x, y, w, h, 6, bg);
    M5.Lcd.drawRoundRect(x, y, w, h, 6, fg);
    M5.Lcd.setTextFont(font);
    M5.Lcd.setTextColor(fg, bg);

    int cursorY = y + 8;
    for (int i = 0; i < lineCount && cursorY < y + h - 10; i++) {
        M5.Lcd.drawString(lines[i], x + 8, cursorY, font);
        cursorY += (font == 4) ? 26 : 16;
    }
}

static void measureBatt() {
    float v = M5.Axp.GetBatVoltage();
    voltage = v;
    float pct = constrain((v - 3.0f) / 1.2f * 100.0f, 0.0f, 100.0f);
    batLevel = static_cast<int>(pct / 100.0f * 13.0f);
}

static bool buildWavPayload(const uint8_t *pcmData, size_t pcmBytes, uint8_t **bufferOut,
                            size_t *lengthOut) {
    if (!pcmData || !bufferOut || !lengthOut || pcmBytes == 0) return false;

    size_t totalBytes = sizeof(WavHeader) + pcmBytes;
    uint8_t *payload = static_cast<uint8_t *>(malloc(totalBytes));
    if (!payload) return false;

    WavHeader header;
    header.fileSize = 36 + pcmBytes;
    header.dataSize = pcmBytes;
    memcpy(payload, &header, sizeof(WavHeader));
    memcpy(payload + sizeof(WavHeader), pcmData, pcmBytes);
    *bufferOut = payload;
    *lengthOut = totalBytes;
    return true;
}

static void audioTask(void *param) {
    while (true) {
        audio.loop();
        vTaskDelay(1);
    }
}

static void stopRadioPlayback() {
    audio.stopSong();
    radioStreamingStarted = false;
}

static void startRadioPlayback() {
    stopRadioPlayback();
    delay(150);
    audio.setVolume(volume * 2);
    audio.connecttohost(stations[chosen].c_str());
    radioStreamingStarted = true;
}

static void setupRadioUi();
static void drawRadioDynamic();
static void drawSettings();
static void drawBotUi();

static void resetBotAutoScroll(unsigned long pauseMs = 1600);

static unsigned long lastStationChange = 0;
static void changeStation(int newChosen) {
    if (millis() - lastStationChange < 1500) return;
    lastStationChange = millis();
    chosen = newChosen;
    if (activeMode == AppMode::Radio) {
        startRadioPlayback();
    }
    canDraw = true;
}

static void settingsIncrement() {
    if (settingSel == 0) {
        volume++;
        if (volume > 10) volume = 0;
        audio.setVolume(volume * 2);
    } else if (settingSel == 1) {
        settingBass++;
        if (settingBass > 6) settingBass = -6;
        audio.setTone(settingBass, 0, settingTreble);
    } else if (settingSel == 2) {
        settingTreble++;
        if (settingTreble > 6) settingTreble = -6;
        audio.setTone(settingBass, 0, settingTreble);
    }
    drawSettings();
}

static int footerZoneTapped(unsigned long &lastTouch, bool &prevTouch) {
    bool touchNow = M5.Touch.ispressed();
    bool edge = touchNow && !prevTouch && (millis() - lastTouch > TOUCH_DEBOUNCE);
    prevTouch = touchNow;
    if (!edge) return -1;

    TouchPoint_t p = M5.Touch.getPressPoint();
    if (p.y < TOUCH_FOOTER_Y) return -1;
    lastTouch = millis();
    haptic();
    if (p.x < ZONE_W) return 0;
    if (p.x < ZONE_W * 2) return 1;
    return 2;
}

static bool botTouchInRect(const TouchPoint_t &p, int x, int y, int w, int h) {
    return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
}

static bool radioBotButtonTouched(unsigned long &lastTouch, bool &prevTouch) {
    bool touchNow = M5.Touch.ispressed();
    bool edge = touchNow && !prevTouch && (millis() - lastTouch > TOUCH_DEBOUNCE);
    prevTouch = touchNow;
    if (!edge) return false;
    TouchPoint_t p = M5.Touch.getPressPoint();
    if (p.y > 22 || p.x < 266) return false;
    lastTouch = millis();
    haptic();
    return true;
}

static void setBotStatus(const String &message) {
    botStatus = message;
    markBotDirty(BOT_DIRTY_STATUS);
}

static void openSetupPortal() {
    stopRadioPlayback();
    if (botRecording) {
        botRecording = false;
        M5.Spk.InitI2SSpeakOrMic(MODE_SPK);
    }
    if (botRecordingBuffer) {
        free(botRecordingBuffer);
        botRecordingBuffer = nullptr;
        botRecordingCapacity = 0;
        botRecordingBytes = 0;
    }
    rdInitPortal();
    while (!portalDone) {
        rdRunPortal();
        delay(1);
    }
    rdClosePortal();
    delay(300);
    ESP.restart();
}

static void enterBotMode(bool redraw = true) {
    stopRadioPlayback();
    inSettings = false;
    activeMode = AppMode::Bot;
    botScreen = BotScreen::Chat;
    botReplyView = BotReplyView::Assistant;
    resetBotAutoScroll();
    setBotStatus(rdHasBotSettingsReady() ? "Use REC / STOP / HOLD." : "Open setup and add Groq key.");
    if (redraw) {
        botDirtyRegions = BOT_DIRTY_ALL;
    }
}

static void enterRadioMode(bool redraw = true) {
    activeMode = AppMode::Radio;
    inSettings = false;
    startRadioPlayback();
    setupRadioUi();
    canDraw = true;
    if (redraw) drawRadioDynamic();
}

static void clearBotChat() {
    rdResetChatHistory();
    botPendingUserMessage = "";
    botLastUserMessage = "";
    botUserPreviewMessage = "";
    botUserPreviewUntilMs = 0;
    botReplyView = BotReplyView::Assistant;
    resetBotAutoScroll();
    setBotStatus("New chat started.");
    markBotDirty(BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void drawFooterButtons(const char *left, const char *middle, const char *right,
                              uint16_t textColor = SW_AMBER) {
    const int centers[] = {53, 160, 267};
    const char *labels[] = {left, middle, right};
    M5.Lcd.setTextFont(2);
    for (int i = 0; i < 3; i++) {
        int bx = centers[i] - 30;
        M5.Lcd.fillRoundRect(bx, SW_BTN_Y, 60, 24, 5, SW_BTN_BG);
        M5.Lcd.drawRoundRect(bx, SW_BTN_Y, 60, 24, 5, SW_AMBER_D);
        M5.Lcd.setTextColor(textColor, SW_BTN_BG);
        M5.Lcd.drawCentreString(labels[i], centers[i], SW_BTN_Y + 4, 2);
    }
}

static String botCurrentReplyText() {
    if (rd_last_reply_message.length()) return rd_last_reply_message;
    return "No reply yet. Open setup, add WiFi + Groq key, then use REC or HOLD.";
}

static String botCurrentPanelTitle() {
    return botReplyView == BotReplyView::User ? "YOU" : "BOT";
}

static String botCurrentPanelText() {
    if (botReplyView == BotReplyView::User) {
        if (botLastUserMessage.length()) return botLastUserMessage;
        if (botPendingUserMessage.length()) return botPendingUserMessage;
        return "No user message yet. Record a question to see your transcript here.";
    }
    return botCurrentReplyText();
}

static int botReplyVisibleLines() {
    return max(1, (BOT_REPLY_H - 16) / 16);
}

static int botReplyMaxCharsPerLine() {
    return max(12, (BOT_REPLY_W - 18) / 8);
}

static int botReplyMaxScrollOffset() {
    String lines[80];
    int lineCount = 0;
    fillWrappedLines(clampText(botCurrentPanelText(), 1400), lines, lineCount, 80,
                     botReplyMaxCharsPerLine());
    return max(0, lineCount - botReplyVisibleLines());
}

static void resetBotAutoScroll(unsigned long pauseMs) {
    botReplyScrollOffset = 0;
    botLastAutoScrollMs = 0;
    botAutoScrollPauseUntilMs = millis() + pauseMs;
}

static void cycleBotModel(int delta) {
    int count = static_cast<int>(rdModelOptionCount());
    int index = rdCurrentModelIndex();
    index = (index + delta + count) % count;
    rdSetActiveModel(RD_MODEL_OPTIONS[index].value);
    setBotStatus(String("Model: ") + RD_MODEL_OPTIONS[index].label);
    markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void cycleBotPersonality(int delta) {
    int count = static_cast<int>(rdPersonalityPresetCount());
    int index = rdCurrentPersonalityPresetIndex();
    if (index < 0) {
        index = delta >= 0 ? 0 : count - 1;
    } else {
        index = (index + delta + count) % count;
    }
    rdSetActivePersonalityPrompt(RD_PERSONALITY_PRESETS[index].prompt);
    setBotStatus(String("Persona: ") + RD_PERSONALITY_PRESETS[index].label);
    markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void toggleBotSettingsMenu() {
    botScreen = (botScreen == BotScreen::Settings) ? BotScreen::Chat : BotScreen::Settings;
    resetBotAutoScroll(botScreen == BotScreen::Settings ? 0 : 1600);
    markBotDirty(BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_ACTIONS | BOT_DIRTY_STATUS);
}

static void scrollBotReply(int delta) {
    int maxOffset = botReplyMaxScrollOffset();
    int next = constrain(botReplyScrollOffset + delta, 0, maxOffset);
    if (next == botReplyScrollOffset) return;
    botReplyScrollOffset = next;
    botAutoScrollPauseUntilMs = millis() + (rd_scroll_ms * 2UL);
    botLastAutoScrollMs = millis();
    markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void cycleBotScrollSpeed() {
    int count = static_cast<int>(rdScrollSpeedOptionCount());
    int index = rdCurrentScrollSpeedIndex();
    index = (index + 1) % count;
    rdSetScrollSpeedMs(RD_SCROLL_SPEED_OPTIONS[index].ms);
    setBotStatus(String("Auto scroll: ") + RD_SCROLL_SPEED_OPTIONS[index].label);
    resetBotAutoScroll();
    markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void toggleBotReplyView() {
    if (botScreen != BotScreen::Chat) return;
    if (botReplyView == BotReplyView::Assistant) {
        if (!botLastUserMessage.length() && !botPendingUserMessage.length()) {
            setBotStatus("No user message yet.");
            markBotDirty(BOT_DIRTY_STATUS);
            return;
        }
        botReplyView = BotReplyView::User;
    } else {
        botReplyView = BotReplyView::Assistant;
    }
    resetBotAutoScroll();
    markBotDirty(BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void pollBotReplyAutoScroll() {
    if (botScreen != BotScreen::Chat) return;

    int maxOffset = botReplyMaxScrollOffset();
    if (maxOffset <= 0) {
        if (botReplyScrollOffset != 0) {
            botReplyScrollOffset = 0;
            markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
        }
        return;
    }
    if (millis() < botAutoScrollPauseUntilMs) return;
    if (botLastAutoScrollMs != 0 && millis() - botLastAutoScrollMs < rd_scroll_ms) return;

    botLastAutoScrollMs = millis();
    int next = botReplyScrollOffset + 1;
    if (next > maxOffset) next = 0;
    if (next != botReplyScrollOffset) {
        botReplyScrollOffset = next;
        markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
    }
}

static void drawBotHeader() {
    M5.Lcd.fillRect(0, 0, SCREEN_W, 24, 0x18C3);
    M5.Lcd.setTextColor(TFT_CYAN, 0x18C3);
    M5.Lcd.setTextFont(2);
    M5.Lcd.drawString("CORE2GROQ", 6, 4, 2);
    M5.Lcd.setTextColor(connected ? TFT_GREEN : TFT_RED, 0x18C3);
    M5.Lcd.drawString(connected ? "WiFi" : "NoWiFi", 132, 4, 2);
    M5.Lcd.fillRoundRect(BOT_VIEW_TOGGLE_X, BOT_VIEW_TOGGLE_Y, BOT_VIEW_TOGGLE_W,
                         BOT_VIEW_TOGGLE_H, 5, 0x1082);
    M5.Lcd.drawRoundRect(BOT_VIEW_TOGGLE_X, BOT_VIEW_TOGGLE_Y, BOT_VIEW_TOGGLE_W,
                         BOT_VIEW_TOGGLE_H, 5, TFT_CYAN);
    M5.Lcd.setTextColor(SW_AMBER, 0x1082);
    M5.Lcd.drawCentreString(botScreen == BotScreen::Settings ? "SET" : botCurrentPanelTitle(),
                            BOT_VIEW_TOGGLE_X + (BOT_VIEW_TOGGLE_W / 2), 6, 2);
    M5.Lcd.setTextColor(TFT_WHITE, 0x18C3);
    M5.Lcd.drawRightString(String(voltage, 2) + "V", 314, 4, 2);
}

static void drawBotReplyPanel() {
    M5.Lcd.fillRoundRect(BOT_REPLY_X, BOT_REPLY_Y, BOT_REPLY_W, BOT_REPLY_H, 6, 0x0841);
    M5.Lcd.drawRoundRect(BOT_REPLY_X, BOT_REPLY_Y, BOT_REPLY_W, BOT_REPLY_H, 6, TFT_GREEN);

    if (botScreen == BotScreen::Settings) {
        M5.Lcd.setTextFont(2);
        M5.Lcd.setTextColor(TFT_CYAN, 0x0841);
        M5.Lcd.drawString("SETTINGS", BOT_REPLY_X + 8, BOT_REPLY_Y + 8, 2);

        const int rowY[] = {50, 76, 102, 128, 154};
        const char *labels[] = {"Setup", "Personality", "Model", "Scroll", "Back"};
        String values[] = {
            "Open AP setup",
            rdCurrentPersonalityLabel(),
            String(rd_groq_model),
            rdCurrentScrollSpeedLabel(),
            "Return to chat",
        };
        for (int i = 0; i < 5; i++) {
            uint16_t bg = (i == 0) ? 0x2104 : 0x1082;
            M5.Lcd.fillRoundRect(BOT_REPLY_X + 8, rowY[i], BOT_REPLY_W - 16, 22, 4, bg);
            M5.Lcd.setTextColor(TFT_WHITE, bg);
            M5.Lcd.setTextFont(2);
            M5.Lcd.drawString(labels[i], BOT_REPLY_X + 14, rowY[i] + 4, 2);
            M5.Lcd.setTextFont(1);
            M5.Lcd.drawRightString(clampText(values[i], 20), BOT_REPLY_X + BOT_REPLY_W - 16,
                                   rowY[i] + 8, 1);
        }
        return;
    }

    String lines[80];
    int lineCount = 0;
    fillWrappedLines(clampText(botCurrentPanelText(), 1400), lines, lineCount, 80,
                     botReplyMaxCharsPerLine());
    int visibleLines = botReplyVisibleLines();
    int maxOffset = max(0, lineCount - visibleLines);
    botReplyScrollOffset = constrain(botReplyScrollOffset, 0, maxOffset);
    int start = botReplyScrollOffset;
    int end = min(lineCount, start + visibleLines);

    M5.Lcd.setTextFont(2);
    M5.Lcd.setTextColor(TFT_GREEN, 0x0841);
    M5.Lcd.drawString(botCurrentPanelTitle(), BOT_REPLY_X + 8, BOT_REPLY_Y + 8, 2);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(SW_AMBER, 0x0841);
    M5.Lcd.drawRightString(rdCurrentScrollSpeedLabel(), BOT_REPLY_X + BOT_REPLY_W - 8,
                           BOT_REPLY_Y + 10, 1);

    int cursorY = BOT_REPLY_Y + 28;
    for (int i = start; i < end; i++) {
        if (!lines[i].length()) continue;
        M5.Lcd.drawString(lines[i], BOT_REPLY_X + 8, cursorY, 2);
        cursorY += 16;
    }

    if (maxOffset > 0) {
        String scrollInfo = String(botReplyScrollOffset + 1) + "/" + String(maxOffset + 1);
        M5.Lcd.drawRightString(scrollInfo, BOT_REPLY_X + BOT_REPLY_W - 8,
                               BOT_REPLY_Y + BOT_REPLY_H - 12, 1);
    }
}

static void drawBotStatusLine() {
    M5.Lcd.fillRect(0, BOT_STATUS_Y, SCREEN_W, 8, TFT_BLACK);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(SW_AMBER, TFT_BLACK);
    String statusLine = clampText(botStatus, 64);
    if (botUserPreviewUntilMs > millis() && botUserPreviewMessage.length()) {
        statusLine = "YOU: " + clampText(botUserPreviewMessage, 56);
    }
    M5.Lcd.drawString(statusLine, 12, BOT_STATUS_Y + 1, 1);
    M5.Lcd.drawRightString(String(rd_record_seconds) + "s / " + rdCurrentScrollSpeedLabel(),
                           306, BOT_STATUS_Y + 1, 1);
}

static void drawBotActionButtons() {
    M5.Lcd.fillRect(0, BOT_ACTION_Y, SCREEN_W, BOT_ACTION_H + 6, TFT_BLACK);
    const int actionXs[] = {BOT_ACTION_SETUP_X, BOT_ACTION_RADIO_X, BOT_ACTION_NEW_X};
    const char *actionLabels[] = {"SET", "RADIO", "NEW"};
    for (int i = 0; i < 3; i++) {
        M5.Lcd.fillRoundRect(actionXs[i], BOT_ACTION_Y, BOT_ACTION_W, BOT_ACTION_H, 4, 0x1082);
        M5.Lcd.drawRoundRect(actionXs[i], BOT_ACTION_Y, BOT_ACTION_W, BOT_ACTION_H, 4, TFT_CYAN);
        M5.Lcd.setTextColor(TFT_CYAN, 0x1082);
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawCentreString(actionLabels[i], actionXs[i] + BOT_ACTION_W / 2,
                                BOT_ACTION_Y + 4, 2);
    }
}

static void drawBotFooterHints() {
    M5.Lcd.fillRect(0, BOT_FOOTER_LABEL_Y - 2, SCREEN_W, 12, TFT_BLACK);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(botRecording ? TFT_RED : TFT_CYAN, TFT_BLACK);
    M5.Lcd.drawCentreString("REC", 53, BOT_FOOTER_LABEL_Y, 1);
    M5.Lcd.drawCentreString("STOP", 160, BOT_FOOTER_LABEL_Y, 1);
    M5.Lcd.drawCentreString("HOLD", 267, BOT_FOOTER_LABEL_Y, 1);
}

static void drawBotUi() {
    if (botDirtyRegions == BOT_DIRTY_ALL) {
        M5.Lcd.fillScreen(TFT_BLACK);
    }
    if (botDirtyRegions & BOT_DIRTY_HEADER) {
        drawBotHeader();
    }
    if (botDirtyRegions & BOT_DIRTY_REPLY) {
        drawBotReplyPanel();
    }
    if (botDirtyRegions & BOT_DIRTY_STATUS) {
        drawBotStatusLine();
    }
    if (botDirtyRegions & BOT_DIRTY_ACTIONS) {
        drawBotActionButtons();
    }
    if (botDirtyRegions & BOT_DIRTY_FOOTER) {
        drawBotFooterHints();
    }
    botDirtyRegions = BOT_DIRTY_NONE;
    botDrawNeeded = false;
}

static void setupRadioUi() {
    M5.Lcd.startWrite();
    M5.Lcd.fillScreen(TFT_BLACK);

    M5.Lcd.fillRect(0, 0, SCREEN_W, SW_LINE1, SW_HDR_BG);
    M5.Lcd.setTextFont(2);
    M5.Lcd.setTextColor(SW_AMBER, SW_HDR_BG);
    M5.Lcd.drawString("M5 SHORTWAVE", 6, 4);

    const int divs[] = {SW_LINE1, SW_LINE2, SW_LINE3, SW_LINE4, SW_LINE5};
    for (int i = 0; i < 5; i++) {
        M5.Lcd.drawFastHLine(0, divs[i], SCREEN_W, SW_AMBER);
    }

    M5.Lcd.drawFastHLine(DIAL_X1, SW_DIAL_LN, DIAL_X2 - DIAL_X1 + 1, SW_AMBER_D);
    int dialSpacing = (DIAL_X2 - DIAL_X1) / (ns - 1);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(SW_AMBER_D, TFT_BLACK);
    for (int i = 0; i < ns; i++) {
        int tx = DIAL_X1 + i * dialSpacing;
        M5.Lcd.drawFastVLine(tx, 32, SW_DIAL_LN - 32, SW_AMBER_D);
        char lbl[3];
        snprintf(lbl, sizeof(lbl), "%d", i + 1);
        M5.Lcd.drawString(lbl, tx - 3, 25);
    }

    for (int i = 0; i < 14; i++) {
        int bx = 10 + i * 21;
        for (int j = 0; j < 4; j++) {
            M5.Lcd.fillRect(bx, SW_VU_BOT - 10 - j * 13, 19, 10, SW_SLOT_BG);
        }
    }

    drawFooterButtons("SET", "STA", "VOL");
    M5.Lcd.endWrite();
}

static void drawRadioDynamic() {
    M5.Lcd.startWrite();

    M5.Lcd.fillRect(120, 2, 198, 20, SW_HDR_BG);
    M5.Lcd.fillCircle(127, 12, 4, connected ? TFT_GREEN : TFT_RED);

    int sigBars = 0;
    if (connected) {
        if (rssi > -55) sigBars = 4;
        else if (rssi > -65) sigBars = 3;
        else if (rssi > -75) sigBars = 2;
        else sigBars = 1;
    }
    for (int i = 0; i < 4; i++) {
        int bh = 4 + i * 3;
        int bx = 135 + i * 8;
        M5.Lcd.fillRect(bx, 22 - bh, 5, bh, (i < sigBars) ? TFT_GREEN : SW_GRN_DIM);
    }

    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(SW_AMBER_D, SW_HDR_BG);
    char staCur[8];
    snprintf(staCur, sizeof(staCur), "STA%d/%d", chosen + 1, ns);
    M5.Lcd.drawString(staCur, 166, 8);

    M5.Lcd.setTextColor(SW_AMBER, SW_HDR_BG);
    M5.Lcd.drawString(String(voltage, 2) + "V", 218, 8);

    M5.Lcd.drawRect(253, 6, 20, 12, SW_AMBER);
    M5.Lcd.fillRect(255, 8, 16, 8, TFT_BLACK);
    M5.Lcd.fillRect(255, 8, constrain(batLevel, 0, 13), 8,
                    batLevel > 4 ? TFT_GREEN : TFT_RED);
    M5.Lcd.fillRect(273, 9, 2, 4, SW_AMBER);

    M5.Lcd.fillRoundRect(274, 3, 42, 18, 6, 0x18C3);
    M5.Lcd.drawRoundRect(274, 3, 42, 18, 6, TFT_CYAN);
    M5.Lcd.setTextColor(TFT_CYAN, 0x18C3);
    M5.Lcd.drawCentreString("BOT", 295, 7, 2);

    M5.Lcd.fillRect(0, SW_DIAL_LN + 1, SCREEN_W, 14, TFT_BLACK);
    int dialSpacing = (DIAL_X2 - DIAL_X1) / (ns - 1);
    int nx = DIAL_X1 + chosen * dialSpacing;
    M5.Lcd.fillTriangle(nx, SW_DIAL_LN + 1, nx - 6, SW_DIAL_LN + 13, nx + 6,
                        SW_DIAL_LN + 13, TFT_GREEN);

    M5.Lcd.fillRect(0, SW_LINE2 + 1, SCREEN_W, SW_LINE3 - SW_LINE2 - 1, TFT_BLACK);
    M5.Lcd.setTextFont(4);
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.drawCentreString(stationNames[chosen], 160, SW_NAME_Y, 4);

    static const uint16_t vuOn[4] = {0x07E0, 0x47E0, SW_AMBER, TFT_ORANGE};
    for (int i = 0; i < 14; i++) {
        g[i] = connected ? random(1, 5) : 0;
        int bx = 10 + i * 21;
        for (int j = 0; j < 4; j++) {
            int by = SW_VU_BOT - 10 - j * 13;
            M5.Lcd.fillRect(bx, by, 19, 10, j < g[i] ? vuOn[j] : SW_SLOT_BG);
        }
    }

    M5.Lcd.fillRect(0, SW_INFO_Y - 1, SCREEN_W, 12, TFT_BLACK);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(SW_AMBER, TFT_BLACK);
    M5.Lcd.drawString("BR:" + String(bitrate) + "k", 6, SW_INFO_Y);
    M5.Lcd.drawString("VOL:", 200, SW_INFO_Y);
    for (int i = 0; i < 10; i++) {
        M5.Lcd.fillRect(224 + i * 7, SW_INFO_Y, 5, 7, i < volume ? SW_AMBER : SW_DGREY);
    }

    M5.Lcd.endWrite();
    canDraw = false;
}

static void drawTicker() {
    songposition--;
    if (songposition < -310) songposition = 310;
    sprite2.fillSprite(TFT_BLACK);
    sprite2.drawString(">> " + songPlaying, songposition, 0);
    sprite2.pushSprite(5, SW_TCK_Y);
}

static void drawSettings() {
    int numRows = 3;
    int rowH = 55;
    int rowY0 = 48;

    const char *labels[] = {"Volume", "Bass  ", "Treble"};
    int mins[] = {0, -6, -6};
    int maxs[] = {20, 6, 6};

    M5.Lcd.startWrite();
    M5.Lcd.fillRect(0, 0, SCREEN_W, SCREEN_H, TFT_BLACK);

    M5.Lcd.setTextFont(2);
    M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
    M5.Lcd.drawString("== SOUND SETTINGS ==", 55, 10);
    M5.Lcd.drawFastHLine(0, 30, SCREEN_W, TFT_ORANGE);

    for (int i = 0; i < numRows; i++) {
        bool sel = (i == settingSel);
        int y = rowY0 + i * rowH;
        uint16_t bg = sel ? 0x1082 : TFT_BLACK;

        String valStr;
        int valInt;
        if (i == 0) {
            valInt = volume * 2;
            valStr = String(valInt);
        } else if (i == 1) {
            valInt = settingBass;
            valStr = String(valInt);
        } else if (i == 2) {
            valInt = settingTreble;
            valStr = String(valInt);
        }

        M5.Lcd.fillRect(20, y - 4, 280, rowH - 6, bg);
        M5.Lcd.setTextColor(sel ? TFT_GREEN : TFT_WHITE, bg);
        M5.Lcd.drawString(labels[i], 30, y);
        M5.Lcd.drawString(valStr, 230, y);

        int pos = map(valInt, mins[i], maxs[i], 0, 200);
        int by = y + 20;
        int bh = 10;
        M5.Lcd.drawRect(30, by, 202, bh, sel ? TFT_GREEN : grays[12]);
        M5.Lcd.fillRect(31, by + 1, 200, bh - 2, TFT_BLACK);
        M5.Lcd.fillRect(31, by + 1, pos, bh - 2, sel ? TFT_GREEN : grays[8]);
    }

    drawFooterButtons("BACK", "SEL", "+");
    M5.Lcd.drawFastHLine(0, SCREEN_H - 34, SCREEN_W, TFT_ORANGE);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(grays[6], TFT_BLACK);
    M5.Lcd.drawString("BACK: exit    SEL: param    +: value", 26, SCREEN_H - 22);
    M5.Lcd.endWrite();
}

static void startBotRecording(BotRecordingMode mode) {
    if (botRecording) return;
    if (WiFi.status() != WL_CONNECTED) {
        setBotStatus("WiFi is not connected.");
        return;
    }
    if (!rdHasBotSettingsReady()) {
        setBotStatus("Open setup and add Groq key.");
        return;
    }

    stopRadioPlayback();
    M5.Spk.InitI2SSpeakOrMic(MODE_MIC);

    botRecordingCapacity = BOT_SAMPLE_RATE * rd_record_seconds * sizeof(int16_t);
    botRecordingBuffer = static_cast<uint8_t *>(malloc(botRecordingCapacity));
    if (!botRecordingBuffer) {
        M5.Spk.InitI2SSpeakOrMic(MODE_SPK);
        setBotStatus("Mic buffer alloc failed.");
        return;
    }

    memset(botRecordingBuffer, 0, botRecordingCapacity);
    botRecordingBytes = 0;
    botRecordingStartedMs = millis();
    botRecording = true;
    botRecordingMode = mode;
    setBotStatus(mode == BotRecordingMode::Hold ? "Hold recording..." : "Timed recording...");
    markBotDirty(BOT_DIRTY_FOOTER | BOT_DIRTY_STATUS);
}

static void finishBotRecording() {
    if (!botRecording) return;
    botRecording = false;
    M5.Spk.InitI2SSpeakOrMic(MODE_SPK);
    markBotDirty(BOT_DIRTY_FOOTER | BOT_DIRTY_STATUS);

    if (botRecordingBytes < 2048) {
        free(botRecordingBuffer);
        botRecordingBuffer = nullptr;
        botRecordingCapacity = 0;
        botRecordingBytes = 0;
        setBotStatus("Message not heard.");
        return;
    }

    uint8_t *wavPayload = nullptr;
    size_t wavLength = 0;
    if (!buildWavPayload(botRecordingBuffer, botRecordingBytes, &wavPayload, &wavLength)) {
        free(botRecordingBuffer);
        botRecordingBuffer = nullptr;
        botRecordingCapacity = 0;
        botRecordingBytes = 0;
        setBotStatus("WAV build failed.");
        return;
    }

    free(botRecordingBuffer);
    botRecordingBuffer = nullptr;
    botRecordingCapacity = 0;
    botRecordingBytes = 0;

    setBotStatus("Transcribing...");
    drawBotUi();

    String transcript;
    String error;
    if (!rdTranscribeWav(wavPayload, wavLength, transcript, error)) {
        free(wavPayload);
        setBotStatus(error.length() ? error : "Transcription failed.");
        return;
    }
    free(wavPayload);

    botPendingUserMessage = transcript;
    botLastUserMessage = transcript;
    botUserPreviewMessage = transcript;
    botUserPreviewUntilMs = millis() + 3500;
    botReplyView = BotReplyView::User;
    resetBotAutoScroll(2500);
    setBotStatus("Sending to Groq...");
    markBotDirty(BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
    drawBotUi();

    String reply;
    if (!rdSendChatMessage(transcript, reply, error)) {
        setBotStatus(error.length() ? error : "Groq request failed.");
        markBotDirty(BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
        return;
    }

    botPendingUserMessage = "";
    botReplyView = BotReplyView::Assistant;
    resetBotAutoScroll();
    setBotStatus("Reply received.");
    markBotDirty(BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void pollBotRecording() {
    if (!botRecording) return;

    size_t remaining = botRecordingCapacity - botRecordingBytes;
    if (remaining > 0) {
        size_t chunk = min(static_cast<size_t>(DATA_SIZE), remaining);
        size_t bytesRead = 0;
        i2s_read(Speak_I2S_NUMBER, botRecordingBuffer + botRecordingBytes, chunk, &bytesRead,
                 10 / portTICK_RATE_MS);
        botRecordingBytes += bytesRead;
    }

    bool timedOut =
        millis() - botRecordingStartedMs >= static_cast<unsigned long>(rd_record_seconds) * 1000UL;
    bool full = botRecordingBytes >= botRecordingCapacity;
    if (timedOut || full) {
        finishBotRecording();
    }
}

void setup() {
    M5.begin(true, true, true, false, kMBusModeOutput, true);
    audio.setPinout(I2S_BCK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume * 2);

    int co = 214;
    for (int i = 0; i < 18; i++) {
        grays[i] = M5.Lcd.color565(co, co, co + 40);
        co -= 13;
    }

    sprite2.setColorDepth(16);
    sprite2.createSprite(310, 16);
    sprite2.setTextFont(2);
    sprite2.setTextColor(SW_AMBER, TFT_BLACK);

    rdLoadSettings();
    rdLoadChatHistory();

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Core2Groq");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(4, 40);
    M5.Lcd.print("Hold BtnA for setup...");

    bool enterPortal = !rd_has_settings || (!rdBootsToRadio() && !rdHasBotSettingsReady());
    unsigned long bootStart = millis();
    while (millis() - bootStart < 3000) {
        M5.update();
        if (M5.BtnA.isPressed()) {
            enterPortal = true;
            break;
        }
        delay(50);
    }

    if (enterPortal) {
        rdInitPortal();
        while (!portalDone) {
            rdRunPortal();
            delay(1);
        }
        rdClosePortal();
        rdLoadSettings();
    }

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setCursor(2, 20);
    M5.Lcd.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(rd_wifi_ssid, rd_wifi_pass);
    wifiMulti.run();

    M5.Lcd.setCursor(2, 50);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.println("Radio player ready.");
    delay(400);

    xTaskCreatePinnedToCore(audioTask, "audioT", 8192, nullptr, 2, &audioTaskHandle, 0);

    activeMode = (rdBootsToRadio() || !rdHasBotSettingsReady()) ? AppMode::Radio : AppMode::Bot;
    if (activeMode == AppMode::Radio) {
        setupRadioUi();
        startRadioPlayback();
        canDraw = true;
        drawRadioDynamic();
    } else {
        botDirtyRegions = BOT_DIRTY_ALL;
        drawBotUi();
    }

    c2ScheduleLcdInit(4000);
}

void loop() {
    M5.update();

    static unsigned long lastRSSI = 0;
    static unsigned long lastSlide = 0;
    static unsigned long lastDraw = 0;
    static unsigned long lastTouch = 0;
    static bool prevTouch = false;
    static bool botPrevTouch = false;

    if (activeMode == AppMode::Bot) {
        if (millis() - lastRSSI > 500) {
            lastRSSI = millis();
            measureBatt();
            connected = (WiFi.status() == WL_CONNECTED);
            rssi = connected ? WiFi.RSSI() : -99;
            markBotDirty(BOT_DIRTY_HEADER);
        }

        if (botUserPreviewUntilMs && millis() > botUserPreviewUntilMs) {
            botUserPreviewUntilMs = 0;
            botUserPreviewMessage = "";
            markBotDirty(BOT_DIRTY_STATUS);
        }

        if (M5.BtnA.wasPressed()) {
            haptic();
            if (!botRecording) {
                startBotRecording(BotRecordingMode::Timed);
            }
        }
        if (M5.BtnB.wasPressed()) {
            haptic();
            if (botRecording) {
                finishBotRecording();
            } else {
                setBotStatus("Not recording.");
            }
        }
        if (M5.BtnC.pressedFor(250) && !botRecording) {
            haptic();
            startBotRecording(BotRecordingMode::Hold);
        }
        if (M5.BtnC.wasReleased() && botRecording &&
            botRecordingMode == BotRecordingMode::Hold) {
            finishBotRecording();
        }

        bool touchNow = M5.Touch.ispressed();
        TouchPoint_t p = touchNow ? M5.Touch.getPressPoint() : TouchPoint_t();
        bool edgeTouch = touchNow && !botPrevTouch && (millis() - lastTouch > TOUCH_DEBOUNCE);

        if (edgeTouch && p.y >= TOUCH_FOOTER_Y) {
            lastTouch = millis();
            haptic();
            if (p.x < ZONE_W) {
                if (!botRecording) {
                    startBotRecording(BotRecordingMode::Timed);
                }
            } else if (p.x < ZONE_W * 2) {
                if (botRecording) {
                    finishBotRecording();
                } else {
                    setBotStatus("Not recording.");
                }
            } else {
                if (!botRecording) {
                    startBotRecording(BotRecordingMode::Hold);
                }
            }
        } else if (edgeTouch) {
            lastTouch = millis();
            haptic();
            if (botTouchInRect(p, BOT_VIEW_TOGGLE_X, BOT_VIEW_TOGGLE_Y, BOT_VIEW_TOGGLE_W,
                               BOT_VIEW_TOGGLE_H)) {
                toggleBotReplyView();
            } else if (botTouchInRect(p, BOT_ACTION_SETUP_X, BOT_ACTION_Y, BOT_ACTION_W,
                                      BOT_ACTION_H)) {
                toggleBotSettingsMenu();
            } else if (botTouchInRect(p, BOT_ACTION_RADIO_X, BOT_ACTION_Y, BOT_ACTION_W,
                                      BOT_ACTION_H)) {
                enterRadioMode();
                botPrevTouch = touchNow;
                return;
            } else if (botTouchInRect(p, BOT_ACTION_NEW_X, BOT_ACTION_Y, BOT_ACTION_W,
                                      BOT_ACTION_H)) {
                clearBotChat();
            } else if (botTouchInRect(p, BOT_REPLY_X, BOT_REPLY_Y, BOT_REPLY_W, BOT_REPLY_H)) {
                if (botScreen == BotScreen::Settings) {
                    if (botTouchInRect(p, BOT_REPLY_X + 8, 50, BOT_REPLY_W - 16, 22)) {
                        openSetupPortal();
                    } else if (botTouchInRect(p, BOT_REPLY_X + 8, 76, BOT_REPLY_W - 16, 22)) {
                        cycleBotPersonality(1);
                    } else if (botTouchInRect(p, BOT_REPLY_X + 8, 102, BOT_REPLY_W - 16, 22)) {
                        cycleBotModel(1);
                    } else if (botTouchInRect(p, BOT_REPLY_X + 8, 128, BOT_REPLY_W - 16, 22)) {
                        cycleBotScrollSpeed();
                    } else if (botTouchInRect(p, BOT_REPLY_X + 8, 154, BOT_REPLY_W - 16, 22)) {
                        toggleBotSettingsMenu();
                    }
                } else {
                    if (p.x < BOT_REPLY_X + (BOT_REPLY_W / 2)) {
                        scrollBotReply(-1);
                    } else {
                        scrollBotReply(1);
                    }
                }
            }
        }

        if (!touchNow && botPrevTouch && botRecording &&
            botRecordingMode == BotRecordingMode::Hold) {
            finishBotRecording();
        }
        botPrevTouch = touchNow;

        pollBotRecording();
        pollBotReplyAutoScroll();
        wifiMulti.run();
        c2EnsureLcdInit();
        c2SetLcdMessage(botCurrentPanelText());
        c2UpdateLcd(false, botRecording, botRecordingStartedMs, rd_record_seconds);
        if (botDrawNeeded) {
            lastDraw = millis();
            drawBotUi();
        }

        vTaskDelay(5);
        return;
    }

    if (inSettings) {
        if (M5.BtnA.wasPressed()) {
            haptic();
            inSettings = false;
            setupRadioUi();
            canDraw = true;
        }
        if (M5.BtnB.wasPressed()) {
            haptic();
            settingSel = (settingSel + 1) % 3;
            drawSettings();
        }
        if (M5.BtnC.wasPressed()) {
            haptic();
            settingsIncrement();
        }

        int z = footerZoneTapped(lastTouch, prevTouch);
        if (z == 0) {
            inSettings = false;
            setupRadioUi();
            canDraw = true;
        } else if (z == 1) {
            settingSel = (settingSel + 1) % 3;
            drawSettings();
        } else if (z == 2) {
            settingsIncrement();
        }

        vTaskDelay(5);
        return;
    }

    if (millis() - lastRSSI > 240) {
        lastRSSI = millis();
        rssi = WiFi.RSSI();
        connected = (WiFi.status() == WL_CONNECTED);
        measureBatt();
        canDraw = true;
        if (!connected) songPlaying = "WIFI NOT CONNECTED";
    }

    if (millis() - lastSlide > 30) {
        lastSlide = millis();
        drawTicker();
    }

    c2EnsureLcdInit();
    c2SetLcdMessage(songPlaying.length() ? songPlaying : stationNames[chosen]);
    c2UpdateLcd(true, false, 0, rd_record_seconds);

    if (M5.BtnA.wasReleased()) {
        haptic();
        inSettings = true;
        drawSettings();
    }

    if (M5.BtnB.wasPressed()) {
        haptic();
        changeStation((chosen + 1) % ns);
    }

    static bool screenToggleHandled = false;
    if (M5.BtnC.pressedFor(1000) && !screenToggleHandled) {
        screenToggleHandled = true;
        screenOn = !screenOn;
        M5.Axp.SetDCVoltage(2, screenOn ? 3300 : 2500);
        if (screenOn) canDraw = true;
    }
    if (M5.BtnC.wasReleased()) {
        if (!screenToggleHandled) {
            haptic();
            volume = (volume >= 10) ? 0 : volume + 1;
            audio.setVolume(volume * 2);
            canDraw = true;
        }
        screenToggleHandled = false;
    }

    if (radioBotButtonTouched(lastTouch, prevTouch)) {
        enterBotMode();
        return;
    }

    int z = footerZoneTapped(lastTouch, prevTouch);
    if (z == 0) {
        inSettings = true;
        drawSettings();
    } else if (z == 1) {
        changeStation((chosen + 1) % ns);
    } else if (z == 2) {
        volume = (volume >= 10) ? 0 : volume + 1;
        audio.setVolume(volume * 2);
        canDraw = true;
    }

    if (canDraw && millis() - lastDraw > 800) {
        lastDraw = millis();
        drawRadioDynamic();
    }

    vTaskDelay(5);
}

void audio_info(const char *info) {
    Serial.print("info        ");
    Serial.println(info);
}

void audio_id3data(const char *info) {
    Serial.print("id3data     ");
    Serial.println(info);
}

void audio_showstation(const char *info) {
    curStation = info;
    canDraw = true;
}

void audio_showstreamtitle(const char *info) {
    songPlaying = info;
    canDraw = true;
}

void audio_bitrate(const char *info) {
    bitrate = String(info).toInt() / 1000;
}
