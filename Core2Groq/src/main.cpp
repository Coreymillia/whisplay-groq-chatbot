/**
 * Core2Groq — unified M5Core2 bot + OTR radio firmware
 *
 * Starts from the Core2 OTR radio firmware and adds a Groq chatbot mode.
 * Radio mode is preserved as much as possible while the first bot mode is
 * added as a separate screen with its own runtime.
 */

#include <Arduino.h>
#include <M5Core2.h>
#include <SD.h>
#include <WiFiMulti.h>
#include <Audio.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <vector>

#include "GroqApi.h"
#include "Core2Lcd.h"
#include "Portal.h"
#include "CosmicPortal.h"

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
static constexpr int BOT_REPLY_H = 132;
static constexpr int BOT_STATUS_Y = 166;
static constexpr int BOT_ACTION_Y = 178;
static constexpr int BOT_ACTION_H = 30;
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
static constexpr int BOT_SETTINGS_TILE_COLS = 3;
static constexpr int BOT_SETTINGS_TILE_ROWS = 4;
static constexpr int BOT_SETTINGS_TILE_W = 96;
static constexpr int BOT_SETTINGS_TILE_H = 58;
static constexpr int BOT_SETTINGS_TILE_GAP = 8;
static constexpr int BOT_SETTINGS_GRID_X = 8;
static constexpr int BOT_SETTINGS_GRID_Y = 34;
static constexpr int AI_IMAGE_Y = 0;
static constexpr int AI_IMAGE_H = SCREEN_H;
static constexpr unsigned long AI_LIST_REFRESH_MS = 30000;
static constexpr unsigned long AI_SLIDE_INTERVAL_MS = 18000;
static constexpr size_t AI_MAX_IMAGE_BYTES = 512 * 1024;
static constexpr char AI_CACHE_DIR[] = "/ai-cache";

enum class AppMode : uint8_t {
    Bot,
    Radio,
    AiScreensaver,
    CosmicPortal,
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
AppMode cpPrevMode = AppMode::Bot;
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
bool aiDrawNeeded = true;
bool aiImageReady = false;
bool aiSdChecked = false;
bool aiSdReady = false;
int aiSlideIndex = -1;
unsigned long aiLastListRefreshMs = 0;
unsigned long aiLastSlideMs = 0;
String aiStatus = "Waiting for AI images...";
String aiCurrentFileName;
String aiCurrentSlidePath;
String aiLastError;
std::vector<String> aiRemotePhotoFiles;
uint8_t *aiImageBuffer = nullptr;
size_t aiImageBufferLength = 0;

static void markBotDirty(uint8_t regions) {
    botDirtyRegions |= regions;
    botDrawNeeded = true;
}

static String clampText(const String &value, size_t maxChars = BOT_MAX_VISIBLE_CHARS) {
    if (value.length() <= maxChars) return value;
    return value.substring(value.length() - maxChars);
}

static String urlEncode(const String &value) {
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); i++) {
        const unsigned char c = static_cast<unsigned char>(value.charAt(i));
        const bool safe =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~';
        if (safe) {
            encoded += static_cast<char>(c);
        } else {
            char chunk[4];
            snprintf(chunk, sizeof(chunk), "%%%02X", c);
            encoded += chunk;
        }
    }
    return encoded;
}

static void aiClearImageBuffer() {
    if (aiImageBuffer) {
        free(aiImageBuffer);
        aiImageBuffer = nullptr;
    }
    aiImageBufferLength = 0;
}

static void aiClearCurrentSlide() {
    aiClearImageBuffer();
    aiCurrentSlidePath = "";
    aiImageReady = false;
}

static String aiSanitizeCacheName(const String &fileName) {
    String sanitized;
    sanitized.reserve(fileName.length() + 8);
    for (size_t i = 0; i < fileName.length(); i++) {
        const char c = fileName.charAt(i);
        const bool safe =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.';
        sanitized += safe ? c : '_';
    }
    if (!sanitized.length()) {
        sanitized = "slide.jpg";
    } else if (!sanitized.endsWith(".jpg") && !sanitized.endsWith(".jpeg")) {
        sanitized += ".jpg";
    }
    if (sanitized.length() > 60) {
        sanitized = sanitized.substring(0, 60);
    }
    return sanitized;
}

static String aiCachePathForFile(const String &fileName) {
    return String(AI_CACHE_DIR) + "/" + aiSanitizeCacheName(fileName);
}

static bool aiEnsureSdCache() {
    if (aiSdChecked) {
        return aiSdReady;
    }
    aiSdChecked = true;
    Serial.println("[AI] checking SD cache...");
    aiSdReady = SD.begin();
    if (!aiSdReady || SD.cardType() == CARD_NONE) {
        aiSdReady = false;
        Serial.println("[AI] SD cache unavailable");
        return false;
    }
    if (!SD.exists(AI_CACHE_DIR)) {
        SD.mkdir(AI_CACHE_DIR);
    }
    aiSdReady = SD.exists(AI_CACHE_DIR);
    Serial.println(aiSdReady ? "[AI] SD cache ready" : "[AI] SD cache directory unavailable");
    return aiSdReady;
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
static void drawAiUi();
static void enterAiMode(bool redraw = true);
static void enterBotMode(bool redraw = true);
static void enterRadioMode(bool redraw = true);

static void resetBotAutoScroll(unsigned long pauseMs = 1600);

static AppMode appModeForBootMode(RdBootMode bootMode) {
    switch (bootMode) {
        case RdBootMode::Radio:
            return AppMode::Radio;
        case RdBootMode::AiScreensaver:
            return AppMode::AiScreensaver;
        case RdBootMode::CosmicPortal:
            return AppMode::CosmicPortal;
        case RdBootMode::Bot:
        default:
            return AppMode::Bot;
    }
}

static bool bootModeIsConfigured(RdBootMode bootMode) {
    switch (bootMode) {
        case RdBootMode::AiScreensaver:
            return rdHasAiScreensaverReady();
        case RdBootMode::Radio:
            return true;
        case RdBootMode::CosmicPortal:
            return true;
        case RdBootMode::Bot:
        default:
            return rdHasBotSettingsReady();
    }
}

static String bootModeMissingMessage(RdBootMode bootMode) {
    switch (bootMode) {
        case RdBootMode::AiScreensaver:
            return "Add Whisplay URL in setup.";
        case RdBootMode::Bot:
            return "Add Groq key in setup.";
        case RdBootMode::Radio:
        case RdBootMode::CosmicPortal:
        default:
            return "Finish setup first.";
    }
}

static bool aiFetchRemotePhotoList(String &errorOut) {
    errorOut = "";
    aiRemotePhotoFiles.clear();
    if (!rdHasAiScreensaverReady()) {
        errorOut = "Add Whisplay URL in setup.";
        Serial.println("[AI] no Whisplay URL configured");
        return false;
    }

    HTTPClient http;
    const String requestUrl = rdNormalizeBaseUrl(String(rd_whisplay_url)) + "/api/generated-images";
    Serial.println(String("[AI] fetching list: ") + requestUrl);
    http.begin(requestUrl);
    http.setTimeout(7000);
    const int code = http.GET();
    if (code != 200) {
        errorOut = code > 0 ? "HTTP " + String(code) : "List request failed.";
        Serial.println(String("[AI] list fetch failed: ") + errorOut);
        http.end();
        return false;
    }

    const String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        errorOut = "Image list parse failed.";
        return false;
    }

    JsonArray photos = doc["photos"].as<JsonArray>();
    if (photos.isNull()) {
        errorOut = "No image list found.";
        return false;
    }

    aiRemotePhotoFiles.reserve(photos.size());
    for (JsonVariant photoVariant : photos) {
        JsonObject photoObject = photoVariant.as<JsonObject>();
        const String fileName = String(photoObject["fileName"] | "");
        if (!fileName.length()) {
            continue;
        }
        aiRemotePhotoFiles.push_back(fileName);
    }

    aiLastListRefreshMs = millis();
    if (aiRemotePhotoFiles.empty()) {
        errorOut = "No AI images yet.";
        Serial.println("[AI] remote list empty");
        return false;
    }
    Serial.println(String("[AI] loaded remote photo count: ") + aiRemotePhotoFiles.size());
    return true;
}

static bool aiSetCurrentSlideFromMemory(const String &fileName, uint8_t *buffer, size_t length) {
    aiClearCurrentSlide();
    aiImageBuffer = buffer;
    aiImageBufferLength = length;
    aiCurrentFileName = fileName;
    aiLastSlideMs = millis();
    aiStatus = "Showing AI gallery";
    aiLastError = "";
    aiImageReady = true;
    aiDrawNeeded = true;
    Serial.println(String("[AI] slide ready from RAM: ") + aiCurrentFileName);
    return true;
}

static bool aiSetCurrentSlideFromSd(const String &fileName, const String &cachePath) {
    aiClearCurrentSlide();
    aiCurrentSlidePath = cachePath;
    aiCurrentFileName = fileName;
    aiLastSlideMs = millis();
    aiStatus = "Showing AI gallery";
    aiLastError = "";
    aiImageReady = true;
    aiDrawNeeded = true;
    Serial.println(String("[AI] slide ready from SD: ") + aiCurrentFileName);
    return true;
}

static bool aiDownloadSlideToMemory(const String &fileName, const String &requestUrl,
                                    String &errorOut) {
    HTTPClient http;
    http.begin(requestUrl);
    http.setTimeout(15000);
    const int code = http.GET();
    if (code != 200) {
        errorOut = code > 0 ? "HTTP " + String(code) : "Image download failed.";
        Serial.println(String("[AI] RAM slide download failed: ") + errorOut);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    const int contentLength = http.getSize();
    size_t capacity = contentLength > 0 ? static_cast<size_t>(contentLength) : 32768;
    if (capacity > AI_MAX_IMAGE_BYTES) {
        http.end();
        errorOut = "Image too large.";
        Serial.println("[AI] image too large for RAM buffer");
        return false;
    }

    uint8_t *buffer = static_cast<uint8_t *>(malloc(capacity));
    if (!buffer) {
        http.end();
        errorOut = "Image alloc failed.";
        Serial.println("[AI] image alloc failed");
        return false;
    }

    size_t totalRead = 0;
    unsigned long lastDataMs = millis();
    while (http.connected() &&
           (contentLength < 0 || static_cast<int>(totalRead) < contentLength || stream->available())) {
        size_t available = stream->available();
        if (available == 0) {
            if (millis() - lastDataMs > 2000) {
                break;
            }
            delay(1);
            continue;
        }

        lastDataMs = millis();
        if (totalRead + available > capacity) {
            size_t nextCapacity = capacity;
            while (totalRead + available > nextCapacity) {
                nextCapacity *= 2;
            }
            if (nextCapacity > AI_MAX_IMAGE_BYTES) {
                free(buffer);
                http.end();
                errorOut = "Image exceeded buffer cap.";
                Serial.println("[AI] image exceeded RAM buffer cap");
                return false;
            }
            uint8_t *grown = static_cast<uint8_t *>(realloc(buffer, nextCapacity));
            if (!grown) {
                free(buffer);
                http.end();
                errorOut = "Image realloc failed.";
                Serial.println("[AI] image realloc failed");
                return false;
            }
            buffer = grown;
            capacity = nextCapacity;
        }

        const size_t remainingHint =
            contentLength > 0 ? static_cast<size_t>(contentLength - static_cast<int>(totalRead))
                              : available;
        const size_t chunkSize = min(available, remainingHint);
        const int bytesRead = stream->readBytes(buffer + totalRead, chunkSize);
        if (bytesRead <= 0) {
            break;
        }
        totalRead += static_cast<size_t>(bytesRead);
    }

    http.end();
    const bool ok = totalRead > 0 &&
                    (contentLength < 0 || totalRead == static_cast<size_t>(contentLength));
    if (!ok) {
        free(buffer);
        errorOut = "Image buffer read failed.";
        Serial.println(String("[AI] image buffer read failed, bytes=") + totalRead +
                       " expected=" + contentLength);
        return false;
    }

    return aiSetCurrentSlideFromMemory(fileName, buffer, totalRead);
}

static bool aiDownloadSlide(size_t index, String &errorOut) {
    errorOut = "";
    if (index >= aiRemotePhotoFiles.size()) {
        errorOut = "Slide index out of range.";
        return false;
    }

    const String fileName = aiRemotePhotoFiles[index];
    const String requestUrl =
        rdNormalizeBaseUrl(String(rd_whisplay_url)) + "/api/generated-images/companion/" +
        urlEncode(fileName) + "?width=" + String(SCREEN_W) + "&height=" + String(AI_IMAGE_H);
    Serial.println(String("[AI] downloading slide ") + fileName + " from " + requestUrl);
    if (aiEnsureSdCache()) {
        const String cachePath = aiCachePathForFile(fileName);
        if (SD.exists(cachePath)) {
            aiSlideIndex = static_cast<int>(index);
            return aiSetCurrentSlideFromSd(fileName, cachePath);
        }

        HTTPClient http;
        http.begin(requestUrl);
        http.setTimeout(15000);
        const int code = http.GET();
        if (code == 200) {
            SD.remove(cachePath);
            File file = SD.open(cachePath, FILE_WRITE);
            if (file) {
                const int expectedSize = http.getSize();
                const int written = http.writeToStream(&file);
                file.close();
                http.end();

                size_t finalSize = 0;
                File verify = SD.open(cachePath, FILE_READ);
                if (verify) {
                    finalSize = verify.size();
                    verify.close();
                }
                const bool ok = written >= 0 && finalSize > 0 &&
                                (expectedSize < 0 ||
                                 (written == expectedSize &&
                                  static_cast<int>(finalSize) == expectedSize));
                if (ok) {
                    aiSlideIndex = static_cast<int>(index);
                    return aiSetCurrentSlideFromSd(fileName, cachePath);
                }
                SD.remove(cachePath);
                Serial.println(String("[AI] SD cache write failed for ") + fileName);
            } else {
                http.end();
                Serial.println(String("[AI] SD open failed for ") + cachePath);
            }
        } else {
            errorOut = code > 0 ? "HTTP " + String(code) : "Image download failed.";
            Serial.println(String("[AI] SD slide download failed: ") + errorOut);
            http.end();
        }
        Serial.println("[AI] falling back to RAM for this slide");
    }

    const bool loaded = aiDownloadSlideToMemory(fileName, requestUrl, errorOut);
    if (loaded) {
        aiSlideIndex = static_cast<int>(index);
    }
    return loaded;
}

static bool aiShowNextSlide(bool forceListRefresh = false) {
    Serial.println(String("[AI] show next slide, force refresh=") + (forceListRefresh ? "yes" : "no"));
    if (WiFi.status() != WL_CONNECTED) {
        aiImageReady = false;
        aiStatus = "Waiting for WiFi...";
        aiLastError = "WiFi is not connected.";
        aiDrawNeeded = true;
        Serial.println("[AI] WiFi not connected");
        return false;
    }

    String error;
    if (forceListRefresh || aiRemotePhotoFiles.empty() ||
        millis() - aiLastListRefreshMs >= AI_LIST_REFRESH_MS) {
        if (!aiFetchRemotePhotoList(error)) {
            aiImageReady = false;
            aiStatus = error.length() ? error : "No AI images yet.";
            aiLastError = aiStatus;
            aiDrawNeeded = true;
            Serial.println(String("[AI] refresh failed: ") + aiStatus);
            return false;
        }
    }

    if (aiRemotePhotoFiles.empty()) {
        aiImageReady = false;
        aiStatus = "No AI images yet.";
        aiLastError = aiStatus;
        aiDrawNeeded = true;
        return false;
    }

    size_t nextIndex = !aiRemotePhotoFiles.empty()
                           ? static_cast<size_t>((aiSlideIndex + 1) % aiRemotePhotoFiles.size())
                           : 0;
    for (size_t attempt = 0; attempt < aiRemotePhotoFiles.size(); attempt++) {
        if (aiDownloadSlide(nextIndex, error)) {
            return true;
        }
        nextIndex = (nextIndex + 1) % aiRemotePhotoFiles.size();
    }

    aiImageReady = false;
    aiStatus = error.length() ? error : "Slide download failed.";
    aiLastError = aiStatus;
    aiDrawNeeded = true;
    Serial.println(String("[AI] failed to load any slide: ") + aiStatus);
    return false;
}

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

static void enterBotMode(bool redraw) {
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

static void enterRadioMode(bool redraw) {
    activeMode = AppMode::Radio;
    inSettings = false;
    startRadioPlayback();
    setupRadioUi();
    canDraw = true;
    if (redraw) drawRadioDynamic();
}

static void enterAiMode(bool redraw) {
    stopRadioPlayback();
    inSettings = false;
    activeMode = AppMode::AiScreensaver;
    aiDrawNeeded = true;
    aiStatus = rdHasAiScreensaverReady() ? "Loading AI gallery..." : "Add Whisplay URL in setup.";
    aiLastError = rdHasAiScreensaverReady() ? "" : "Whisplay URL is not configured.";
    Serial.println(String("[AI] enter mode, url=") + rd_whisplay_url);
    if (redraw) {
        aiShowNextSlide(true);
        drawAiUi();
    }
}

static void enterCosmicPortalMode() {
    stopRadioPlayback();
    inSettings = false;
    activeMode = AppMode::CosmicPortal;
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(40, 60);
    M5.Lcd.print("COSMIC PORTAL");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(60, 100);
    M5.Lcd.print("Starting AP...");
    // Use the saved cosmic portal name if set, otherwise default
    if (rd_cosmic_portal_name[0] != '\0') {
        cpPrefs.begin("cosmic", false);
        cpPrefs.putString("cpApName", String(rd_cosmic_portal_name));
        cpPrefs.end();
    }
    cpInitPortal();
}

static void drawCosmicPortalClock() {
    // Minimal clock display — only time on screen
    unsigned long now = millis();
    unsigned long totalSecs = now / 1000;
    int hours = (totalSecs / 3600) % 24;
    int mins = (totalSecs / 60) % 60;
    int secs = totalSecs % 60;

    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", hours, mins, secs);

    // Only redraw if seconds changed
    static int lastSecs = -1;
    if (secs == lastSecs) return;
    lastSecs = secs;

    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(56, 80);
    M5.Lcd.print(timeBuf);

    // WiFi AP status line
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.setCursor(44, 140);
    const char* apLabel = cpApName.length() > 0 ? cpApName.c_str() : CP_AP_SSID;
    M5.Lcd.print(apLabel);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(72, 160);
    M5.Lcd.print(CP_PORTAL_IP);

    int clients = WiFi.softAPgetStationNum();
    M5.Lcd.setCursor(80, 180);
    M5.Lcd.print("Visitors: ");
    M5.Lcd.print(clients);
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

static void drawBotSettingsTile(int index, const String &label, const String &value,
                                uint16_t accent = TFT_CYAN) {
    const int col = index % BOT_SETTINGS_TILE_COLS;
    const int row = index / BOT_SETTINGS_TILE_COLS;
    const int x = BOT_SETTINGS_GRID_X + col * (BOT_SETTINGS_TILE_W + BOT_SETTINGS_TILE_GAP);
    const int y = BOT_SETTINGS_GRID_Y + row * (BOT_SETTINGS_TILE_H + BOT_SETTINGS_TILE_GAP);
    M5.Lcd.fillRoundRect(x, y, BOT_SETTINGS_TILE_W, BOT_SETTINGS_TILE_H, 8, 0x1082);
    M5.Lcd.drawRoundRect(x, y, BOT_SETTINGS_TILE_W, BOT_SETTINGS_TILE_H, 8, accent);
    M5.Lcd.setTextColor(accent, 0x1082);
    M5.Lcd.setTextFont(2);
    M5.Lcd.drawCentreString(label, x + BOT_SETTINGS_TILE_W / 2, y + 10, 2);
    M5.Lcd.setTextColor(TFT_WHITE, 0x1082);
    M5.Lcd.setTextFont(1);
    M5.Lcd.drawCentreString(clampText(value, 18), x + BOT_SETTINGS_TILE_W / 2, y + 34, 1);
}

static void drawBotSettingsScreen() {
    M5.Lcd.fillRect(0, 24, SCREEN_W, SCREEN_H - 24, TFT_BLACK);
    drawBotSettingsTile(0, "SETUP", "AP portal");
    drawBotSettingsTile(1, "PERSONA", rdCurrentPersonalityLabel(), TFT_GREEN);
    drawBotSettingsTile(2, "MODEL", String(rd_groq_model), SW_AMBER);
    drawBotSettingsTile(3, "SCROLL", rdCurrentScrollSpeedLabel(), TFT_CYAN);
    drawBotSettingsTile(4, "BOOT", rdCurrentBootModeLabel(), TFT_ORANGE);
    drawBotSettingsTile(5, "LAUNCH", rdCurrentBootModeLabel(), TFT_GREEN);
    drawBotSettingsTile(6, "AI SHOW", rdHasAiScreensaverReady() ? "Launch now" : "Needs URL",
                        rdHasAiScreensaverReady() ? TFT_CYAN : TFT_RED);
    drawBotSettingsTile(7, "RADIO", "Launch now", TFT_ORANGE);
    drawBotSettingsTile(8, "BACK", "Chat", TFT_WHITE);
    drawBotSettingsTile(10, "PORTAL", "Cosmic art AP", TFT_MAGENTA);
}

static int botSettingsTileAtPoint(const TouchPoint_t &p) {
    for (int index = 0; index < BOT_SETTINGS_TILE_COLS * BOT_SETTINGS_TILE_ROWS; index++) {
        const int col = index % BOT_SETTINGS_TILE_COLS;
        const int row = index / BOT_SETTINGS_TILE_COLS;
        const int x = BOT_SETTINGS_GRID_X + col * (BOT_SETTINGS_TILE_W + BOT_SETTINGS_TILE_GAP);
        const int y = BOT_SETTINGS_GRID_Y + row * (BOT_SETTINGS_TILE_H + BOT_SETTINGS_TILE_GAP);
        if (botTouchInRect(p, x, y, BOT_SETTINGS_TILE_W, BOT_SETTINGS_TILE_H)) {
            return index;
        }
    }
    return -1;
}

static void cycleBotBootMode() {
    RdBootMode nextMode = RdBootMode::Bot;
    switch (rdGetBootMode()) {
        case RdBootMode::Bot:
            nextMode = RdBootMode::AiScreensaver;
            break;
        case RdBootMode::AiScreensaver:
            nextMode = RdBootMode::CosmicPortal;
            break;
        case RdBootMode::CosmicPortal:
            nextMode = RdBootMode::Radio;
            break;
        case RdBootMode::Radio:
        default:
            nextMode = RdBootMode::Bot;
            break;
    }
    rdSetBootMode(nextMode);
    setBotStatus(String("Boot mode: ") + rdCurrentBootModeLabel());
    markBotDirty(BOT_DIRTY_HEADER | BOT_DIRTY_REPLY | BOT_DIRTY_STATUS);
}

static void launchBotSelectedBootMode() {
    const RdBootMode bootMode = rdGetBootMode();
    if (!bootModeIsConfigured(bootMode)) {
        setBotStatus(bootModeMissingMessage(bootMode));
        markBotDirty(BOT_DIRTY_STATUS | BOT_DIRTY_REPLY);
        return;
    }
    switch (appModeForBootMode(bootMode)) {
        case AppMode::AiScreensaver:
            enterAiMode();
            break;
        case AppMode::Radio:
            enterRadioMode();
            break;
        case AppMode::CosmicPortal:
            enterCosmicPortalMode();
            break;
        case AppMode::Bot:
        default:
            enterBotMode();
            break;
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
        drawBotSettingsScreen();
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
    if (botScreen == BotScreen::Settings) {
        return;
    }
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
    if (botScreen == BotScreen::Settings) {
        return;
    }
    M5.Lcd.fillRect(0, BOT_ACTION_Y, SCREEN_W, BOT_ACTION_H + 6, TFT_BLACK);
    const int actionXs[] = {BOT_ACTION_SETUP_X, BOT_ACTION_RADIO_X, BOT_ACTION_NEW_X};
    const char *actionLabels[] = {"SET", "RADIO", "NEW"};
    for (int i = 0; i < 3; i++) {
        M5.Lcd.fillRoundRect(actionXs[i], BOT_ACTION_Y, BOT_ACTION_W, BOT_ACTION_H, 4, 0x1082);
        M5.Lcd.drawRoundRect(actionXs[i], BOT_ACTION_Y, BOT_ACTION_W, BOT_ACTION_H, 4, TFT_CYAN);
        M5.Lcd.setTextColor(TFT_CYAN, 0x1082);
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawCentreString(actionLabels[i], actionXs[i] + BOT_ACTION_W / 2,
                                BOT_ACTION_Y + 9, 2);
    }
}

static void drawBotFooterHints() {
    if (botScreen == BotScreen::Settings) {
        return;
    }
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

static void drawAiUi() {
    M5.Lcd.fillScreen(TFT_BLACK);
    if (aiImageReady && aiCurrentSlidePath.length() && SD.exists(aiCurrentSlidePath)) {
        M5.Lcd.drawJpgFile(SD, aiCurrentSlidePath.c_str(), 0, AI_IMAGE_Y, SCREEN_W, AI_IMAGE_H);
    } else if (aiImageReady && aiImageBuffer && aiImageBufferLength > 0) {
        M5.Lcd.drawJpg(aiImageBuffer, aiImageBufferLength, 0, AI_IMAGE_Y, SCREEN_W, AI_IMAGE_H);
    } else {
        M5.Lcd.fillRect(0, AI_IMAGE_Y, SCREEN_W, AI_IMAGE_H, TFT_BLACK);
        M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawCentreString("AI SCREENSAVER", SCREEN_W / 2, 86, 2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.drawCentreString(clampText(aiStatus, 40), SCREEN_W / 2, 112, 2);
        if (aiLastError.length()) {
            M5.Lcd.setTextFont(1);
            M5.Lcd.drawCentreString(clampText(aiLastError, 48), SCREEN_W / 2, 136, 1);
        }
    }
    aiDrawNeeded = false;
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
    Serial.begin(115200);
    delay(50);
    Serial.println("[BOOT] Core2Groq starting");
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
    Serial.println(String("[BOOT] settings loaded: hasWifi=") + (rd_has_settings ? "yes" : "no") +
                   " bootMode=" + rdCurrentBootModeLabel() +
                   " hasGroqKey=" + (rdHasBotSettingsReady() ? "yes" : "no") +
                   " hasUrl=" + (rdHasAiScreensaverReady() ? "yes" : "no"));
    if (rdHasAiScreensaverReady()) {
        Serial.println(String("[BOOT] Whisplay URL: ") + rd_whisplay_url);
    }

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Core2Groq");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(4, 40);
    M5.Lcd.print("Hold BtnA for setup...");

    bool enterPortal = !rd_has_settings || !bootModeIsConfigured(rdGetBootMode());
    Serial.println(String("[BOOT] enterPortal=") + (enterPortal ? "yes" : "no"));
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
        Serial.println("[BOOT] opening setup portal");
        rdInitPortal();
        while (!portalDone) {
            rdRunPortal();
            delay(1);
        }
        rdClosePortal();
        rdLoadSettings();
        Serial.println(String("[BOOT] portal done, bootMode=") + rdCurrentBootModeLabel() +
                       " hasUrl=" + (rdHasAiScreensaverReady() ? "yes" : "no"));
    }

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);

    const AppMode bootMode = appModeForBootMode(rdGetBootMode());

    if (bootMode != AppMode::CosmicPortal) {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.setCursor(2, 20);
        M5.Lcd.println("Connecting to WiFi...");

        WiFi.mode(WIFI_STA);
        wifiMulti.addAP(rd_wifi_ssid, rd_wifi_pass);
        wifiMulti.run();
        Serial.println(String("[BOOT] WiFi status after first run: ") + WiFi.status());

        M5.Lcd.setCursor(2, 50);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.println("Core2Groq runtime ready.");
        delay(400);

        xTaskCreatePinnedToCore(audioTask, "audioT", 8192, nullptr, 2, &audioTaskHandle, 0);
    }

    Serial.println(String("[BOOT] launching mode: ") +
                   (bootMode == AppMode::Bot ? "bot" :
                    bootMode == AppMode::Radio ? "radio" :
                    bootMode == AppMode::AiScreensaver ? "ai" : "portal"));
    
    if (bootMode == AppMode::CosmicPortal) {
        // Skip WiFi STA — go straight to AP portal mode
        cpPrevMode = AppMode::Bot;
        enterCosmicPortalMode();
    } else if (bootMode == AppMode::Radio) {
        enterRadioMode();
    } else if (bootMode == AppMode::AiScreensaver) {
        enterAiMode();
    } else {
        enterBotMode();
        drawBotUi();
    }

    c2ScheduleLcdInit(4000);
}

void loop() {
    M5.update();

    // ── WiFi watchdog: auto-enter/exit Cosmic Portal ────────────────────────
    static unsigned long lastWifiCheck = 0;
    static unsigned long wifiLostSince = 0;
    static unsigned long lastPortalWifiScan = 0;

    if (millis() - lastWifiCheck > 3000) {
        lastWifiCheck = millis();
        if (activeMode != AppMode::CosmicPortal && activeMode != AppMode::Radio) {
            // In Bot/Ai modes, watch for WiFi loss
            if (WiFi.status() != WL_CONNECTED) {
                if (wifiLostSince == 0) wifiLostSince = millis();
                else if (millis() - wifiLostSince > 15000 && rd_has_settings) {
                    Serial.println("[WiFi] Lost connection, auto-entering Cosmic Portal");
                    cpPrevMode = activeMode;
                    stopRadioPlayback();
                    enterCosmicPortalMode();
                    wifiLostSince = 0;
                    return;
                }
            } else {
                wifiLostSince = 0;
            }
        } else if (activeMode == AppMode::CosmicPortal && rd_has_settings) {
            // In portal mode, periodically scan for saved WiFi
            if (millis() - lastPortalWifiScan > 60000) {
                lastPortalWifiScan = millis();
                int n = WiFi.scanNetworks();
                for (int i = 0; i < n; i++) {
                    if (WiFi.SSID(i) == String(rd_wifi_ssid)) {
                        Serial.println("[WiFi] Found saved network, exiting portal");
                        cpClosePortal();
                        WiFi.mode(WIFI_STA);
                        wifiMulti.addAP(rd_wifi_ssid, rd_wifi_pass);
                        wifiMulti.run();
                        activeMode = cpPrevMode;
                        if (cpPrevMode == AppMode::AiScreensaver) enterAiMode();
                        else enterBotMode();
                        WiFi.scanDelete();
                        return;
                    }
                }
                WiFi.scanDelete();
            }
        }
    }

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
            } else if (botScreen == BotScreen::Settings) {
                const int tileIndex = botSettingsTileAtPoint(p);
                if (tileIndex == 0) {
                    openSetupPortal();
                } else if (tileIndex == 1) {
                    cycleBotPersonality(1);
                } else if (tileIndex == 2) {
                    cycleBotModel(1);
                } else if (tileIndex == 3) {
                    cycleBotScrollSpeed();
                } else if (tileIndex == 4) {
                    cycleBotBootMode();
                } else if (tileIndex == 5) {
                    launchBotSelectedBootMode();
                    botPrevTouch = touchNow;
                    return;
                } else if (tileIndex == 6) {
                    if (!rdHasAiScreensaverReady()) {
                        setBotStatus("Add Whisplay URL in setup.");
                        markBotDirty(BOT_DIRTY_STATUS | BOT_DIRTY_REPLY);
                    } else {
                        enterAiMode();
                        botPrevTouch = touchNow;
                        return;
                    }
                } else if (tileIndex == 7) {
                    enterRadioMode();
                    botPrevTouch = touchNow;
                    return;
                } else if (tileIndex == 8) {
                    toggleBotSettingsMenu();
                } else if (tileIndex == 10) {
                    enterCosmicPortalMode();
                    botPrevTouch = touchNow;
                    return;
                }
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
                if (p.x < BOT_REPLY_X + (BOT_REPLY_W / 2)) {
                    scrollBotReply(-1);
                } else {
                    scrollBotReply(1);
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

    if (activeMode == AppMode::AiScreensaver) {
        if (millis() - lastRSSI > 800) {
            lastRSSI = millis();
            connected = (WiFi.status() == WL_CONNECTED);
            rssi = connected ? WiFi.RSSI() : -99;
            measureBatt();
        }

        if (M5.BtnA.wasPressed()) {
            haptic();
            openSetupPortal();
        }
        if (M5.BtnB.wasPressed()) {
            haptic();
            aiShowNextSlide(true);
        }
        if (M5.BtnC.wasPressed()) {
            haptic();
        }

        const bool touchNow = M5.Touch.ispressed();
        const TouchPoint_t p = touchNow ? M5.Touch.getPressPoint() : TouchPoint_t();
        const bool edgeTouch = touchNow && !botPrevTouch && (millis() - lastTouch > TOUCH_DEBOUNCE);
        if (edgeTouch) {
            lastTouch = millis();
            haptic();
            if (p.y >= TOUCH_FOOTER_Y && p.x < ZONE_W) {
                openSetupPortal();
            } else if (p.y >= TOUCH_FOOTER_Y && p.x < ZONE_W * 2) {
                aiShowNextSlide(true);
            }
        }
        botPrevTouch = touchNow;

        if (millis() - aiLastSlideMs >= AI_SLIDE_INTERVAL_MS) {
            aiShowNextSlide();
        }

        wifiMulti.run();
        c2EnsureLcdInit();
        c2SetLcdMessage(aiCurrentFileName.length() ? aiCurrentFileName : aiStatus);
        c2UpdateLcd(false, false, 0, rd_record_seconds);
        if (aiDrawNeeded) {
            lastDraw = millis();
            drawAiUi();
        }

        vTaskDelay(5);
        return;
    }

    if (activeMode == AppMode::CosmicPortal) {
        cpRunPortal();
        drawCosmicPortalClock();

        // Button A: exit portal back to previous mode
        if (M5.BtnA.wasPressed()) {
            haptic();
            cpClosePortal();
            activeMode = cpPrevMode;
            if (cpPrevMode == AppMode::Radio) {
                WiFi.mode(WIFI_STA);
                wifiMulti.addAP(rd_wifi_ssid, rd_wifi_pass);
                enterRadioMode();
            } else if (cpPrevMode == AppMode::AiScreensaver) {
                WiFi.mode(WIFI_STA);
                wifiMulti.addAP(rd_wifi_ssid, rd_wifi_pass);
                enterAiMode();
            } else {
                WiFi.mode(WIFI_STA);
                wifiMulti.addAP(rd_wifi_ssid, rd_wifi_pass);
                enterBotMode();
            }
            return;
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
