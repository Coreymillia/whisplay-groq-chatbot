#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_log.h>
#include <time.h>
#include <vector>

#include "pin_config.h"

#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <SensorPCF85063.hpp>
#include <XPowersLib.h>

#include "AiScreensaver.h"
#include "AppModes.h"
#include "AppSettings.h"
#include "CosmicPortal.h"
#include "GroqApi.h"
#include "GroqWatchLog.h"
#include "mic_audio.h"
#include "PixelCanvas.h"
#include "SettingsMenu.h"
#include "SetupPortal.h"

using namespace GroqWatch;

void manualScreenSleep();

// ── Hardware globals ─────────────────────────────────────────────────────────
namespace {

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_GFX *gfx = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT,
                                      22, 0, 0, 0);
std::shared_ptr<Arduino_IIC_DriveBus> i2cBus =
    std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
void onTouchInterrupt();
std::unique_ptr<Arduino_IIC> touchDev(
    new Arduino_FT3x68(i2cBus, FT3168_DEVICE_ADDRESS, TP_RESET, TP_INT, onTouchInterrupt));
SensorPCF85063 rtc;
XPowersPMU power;
PixelCanvas canvas;
AppSettings settings;
SettingsMenu settingsMenu;

bool displayReady = false;
bool touchReady = false;
bool rtcReady = false;
bool pmuReady = false;
bool rtcHasValidTime = false;
bool wifiConnected = false;
bool touchPollingSuspended = false;

AppMode currentMode = AppMode::Watch;
const char *timeStatus = "INIT";
unsigned long lastFrameMs = 0;
unsigned long lastRtcPollMs = 0;
unsigned long lastTouchLogMs = 0;
unsigned long lastWifiCheckMs = 0;
unsigned long aiFrameLastMs = 0;
bool aiModeActive = false;
bool aiBootDown = false;
bool aiBootLongHandled = false;
unsigned long aiBootDownMs = 0;
unsigned long lastActivityMs = 0;
bool screenBlanked = false;
unsigned long wakeInputIgnoreUntilMs = 0;
bool wakeBootWaitRelease = false;
static constexpr unsigned long AI_BOOT_LONG_MS = 900;

// ── Clean watch face state ────────────────────────────────────────────
bool cleanWatchDirty = true;
int cleanWatchLastMinute = -1;
String cleanWatchForecast = "";
unsigned long cleanWatchForecastMs = 0;
unsigned long cleanWatchForecastNextAttemptMs = 0;
static constexpr unsigned long WEATHER_REFRESH_MS = 30 * 60 * 1000;  // 30 min
static constexpr unsigned long WEATHER_RETRY_MS = 60 * 1000;         // 1 min

struct CachedClock {
    int year = 2026, month = 1, day = 1, hour = 12, minute = 0, second = 0;
    bool valid = false;
} clockNow;
time_t fallbackEpoch = 0;
unsigned long fallbackEpochMs = 0;

struct TouchState {
    bool pressed = false, justPressed = false, justReleased = false;
    uint16_t x = 0, y = 0;
};

bool touchPrevPressed = false;
uint16_t touchLastX = 0, touchLastY = 0;
unsigned long lastTouchPollMs = 0;
unsigned long touchErrorBackoffUntilMs = 0;
static constexpr unsigned long TOUCH_IDLE_POLL_MS = 30;
static constexpr unsigned long TOUCH_ACTIVE_POLL_MS = 16;
static constexpr unsigned long TOUCH_ERROR_BACKOFF_MS = 80;

struct Particle {
    int16_t x = 0, y = 0;
    int8_t vx = 0, vy = 0;
    uint8_t size = 1;
};
static constexpr uint8_t kParticleCount = 18;
Particle particles[kParticleCount];
const char *kStyleNames[] = {"STARS", "BUBBLES", "GRID"};
uint8_t activeStyle = 0;

void onTouchInterrupt() { touchDev->IIC_Interrupt_Flag = true; }
uint8_t clampStyle(uint8_t s) { return s > 2 ? 0 : s; }

int monthFromBuildName(const char *m) {
    static const char *mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    for (int i = 0; i < 12; i++) if (strncmp(m, mo[i], 3) == 0) return i;
    return 0;
}

time_t compileTimeEpoch() {
    char mn[4]; int d=1,y=2026,h=0,min=0,s=0;
    sscanf(__DATE__, "%3s %d %d", mn, &d, &y); sscanf(__TIME__, "%d:%d:%d", &h, &min, &s);
    tm bt={}; bt.tm_year=y-1900; bt.tm_mon=monthFromBuildName(mn); bt.tm_mday=d;
    bt.tm_hour=h; bt.tm_min=min; bt.tm_sec=s; bt.tm_isdst=-1;
    return mktime(&bt);
}

bool rtcDateTimeLooksValid(const RTC_DateTime &dt) {
    return dt.getYear()>=2024 && dt.getMonth()>=1 && dt.getMonth()<=12 && dt.getDay()>=1 && dt.getDay()<=31;
}

void setFallbackFromRtc(const RTC_DateTime &dt) {
    tm lt={}; lt.tm_year=dt.getYear()-1900; lt.tm_mon=dt.getMonth()-1; lt.tm_mday=dt.getDay();
    lt.tm_hour=dt.getHour(); lt.tm_min=dt.getMinute(); lt.tm_sec=dt.getSecond(); lt.tm_isdst=-1;
    fallbackEpoch=mktime(&lt); fallbackEpochMs=millis();
}

void updateClockFromFallback() {
    time_t now=fallbackEpoch+static_cast<time_t>((millis()-fallbackEpochMs)/1000UL);
    tm lt={}; localtime_r(&now,&lt);
    clockNow.year=lt.tm_year+1900; clockNow.month=lt.tm_mon+1; clockNow.day=lt.tm_mday;
    clockNow.hour=lt.tm_hour; clockNow.minute=lt.tm_min; clockNow.second=lt.tm_sec;
    clockNow.valid=true;
}

void pollClock() {
    if (rtcReady && millis()-lastRtcPollMs>=500) {
        lastRtcPollMs=millis();
        RTC_DateTime dt=rtc.getDateTime();
        if (rtcDateTimeLooksValid(dt)) {
            rtcHasValidTime=true;
            clockNow.year=dt.getYear(); clockNow.month=dt.getMonth(); clockNow.day=dt.getDay();
            clockNow.hour=dt.getHour(); clockNow.minute=dt.getMinute(); clockNow.second=dt.getSecond();
            clockNow.valid=true; setFallbackFromRtc(dt); return;
        }
    }
    updateClockFromFallback();
}

// ── Status screen────────────────────────────────────────────────────────
void drawStatusScreen(const char *title, const char *l1=nullptr, const char *l2=nullptr,
                      const char *l3=nullptr, uint16_t acc=RGB565_CYAN) {
    if (!displayReady) return;
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(acc, RGB565_BLACK); gfx->setTextSize(2);
    gfx->setCursor(22, 30); gfx->print(title);
    gfx->setTextColor(RGB565_WHITE, RGB565_BLACK); gfx->setTextSize(1);
    int y=95;
    if(l1){gfx->setCursor(18,y);gfx->print(l1);y+=26;}
    if(l2){gfx->setCursor(18,y);gfx->print(l2);y+=26;}
    if(l3){gfx->setCursor(18,y);gfx->print(l3);}
}

// ── WiFi / NTP ─────────────────────────────────────────────────────────
bool connectWiFi() {
    if (!hasWiFi(settings)) { timeStatus="NO WIFI"; return false; }
    drawStatusScreen("GroqWatch","Connecting...",settings.wifiSsid);
    WiFi.mode(WIFI_STA); WiFi.setSleep(false);
    WiFi.begin(settings.wifiSsid, settings.wifiPass);
    unsigned long s=millis();
    while (WiFi.status()!=WL_CONNECTED && millis()-s<15000UL) delay(100);
    if (WiFi.status()!=WL_CONNECTED) {
        timeStatus="WIFI FAIL"; WiFi.disconnect(true,true); WiFi.mode(WIFI_OFF);
        return false;
    }
    wifiConnected=true;
    configTzTime(settings.timezone,"pool.ntp.org","time.nist.gov","time.google.com");
    tm lt={}; bool ok=false;
    unsigned long ns=millis();
    while (millis()-ns<15000UL) {
        if (getLocalTime(&lt,500)) { ok=true; break; }
        delay(100);
    }
    if (ok) {
        fallbackEpoch=mktime(&lt); fallbackEpochMs=millis();
        if (rtcReady) {
            rtc.setDateTime(lt.tm_year+1900,lt.tm_mon+1,lt.tm_mday,lt.tm_hour,lt.tm_min,lt.tm_sec);
            rtcHasValidTime=rtcDateTimeLooksValid(rtc.getDateTime());
        }
        timeStatus=rtcReady?"NTP->RTC":"NTP";
    } else timeStatus=rtcHasValidTime?"RTC":"TIME FAIL";
    return ok;
}

void disconnectWiFi() {
    WiFi.disconnect(true,true); WiFi.mode(WIFI_OFF);
    wifiConnected=false;
}

bool ensureWifiConnected() {
    if (wifiConnected && WiFi.status()==WL_CONNECTED) return true;
    return connectWiFi();
}

void renderCurrentMode();
void resetTouchState();
bool botTouchReadSuspended();
void dirtyAllModes();
void applyWatchFaceConnectivity();
static bool botFetchRemoteState(bool force);
void renderCleanWatchFace();
void renderStubWatchFace();
void cycleWatchFace();
void fetchNwsForecast();
enum class BotState : uint8_t { Idle, Syncing, ShowingReply };
extern BotState botState;
extern String botStatus;
extern bool botRemoteReady;
extern unsigned long botLastPollMs;
extern unsigned long botLastActionMs;
extern bool botBtnPrev;
extern bool botTouchHoldActive;
extern unsigned long botTimedStopMs;
extern unsigned long botTouchResumeMs;
extern int botLastRenderMinuteKey;
extern unsigned long botLastRenderTick;

// ── Mode management ────────────────────────────────────────────────────
void setMode(AppMode newMode) {
    AppMode oldMode = currentMode;

    if (oldMode == AppMode::Bot && newMode != AppMode::Bot) {
        botTouchHoldActive = false;
        botTimedStopMs = 0;
        botTouchResumeMs = 0;
        botBtnPrev = false;
        resetTouchState();
    }
    if (oldMode == AppMode::Watch && newMode != AppMode::Watch && cpIsRunning()) {
        cpClosePortal();
    }

    currentMode = newMode;
    lastActivityMs = millis();
    screenBlanked = false;
    GW_LOGF("mode %s -> %s\n", modeLabel(oldMode), modeLabel(currentMode));

    if (currentMode == AppMode::AiScreensaver) {
        aiModeActive = false;
        aiEnsureSdCache();
        if (!wifiConnected) {
            ensureWifiConnected();
        }
        pinMode(WATCH_BOOT_BUTTON_PIN, INPUT_PULLUP);
        aiBootDown = false;
        aiBootLongHandled = false;
        aiBootDownMs = 0;
        wakeBootWaitRelease = digitalRead(WATCH_BOOT_BUTTON_PIN) == LOW;
        wakeInputIgnoreUntilMs = millis() + 250;
        aiModeActive = aiShowNextSlide(settings.whisplayUrl, true);
        aiFrameLastMs = millis();
    }
    if (currentMode == AppMode::Bot) {
        ensureWifiConnected();
        pinMode(WATCH_BOOT_BUTTON_PIN, INPUT_PULLUP);
        botBtnPrev = false;
        botTouchHoldActive = false;
        botTimedStopMs = 0;
        botTouchResumeMs = 0;
        botLastPollMs = 0;
        botLastActionMs = 0;
        botLastRenderMinuteKey = -1;
        botLastRenderTick = 0xFFFFFFFFUL;
        botStatus = hasWhisplayUrl(settings) ? "Connecting to Whisplay..." : "Set Whisplay URL in Setup";
        botState = BotState::Syncing;
        resetTouchState();
        botFetchRemoteState(true);
    }
    if (currentMode == AppMode::Watch) {
        aiClearCurrentSlide();
        aiModeActive = false;
        applyWatchFaceConnectivity();
    }
    if (currentMode == AppMode::Settings) {
        settingsMenu.begin(settings, currentMode);
    }

    renderCurrentMode();
}

void checkWifiAndFallback() {
    if (currentMode == AppMode::Watch || currentMode == AppMode::Settings) return;
    if (millis() - lastWifiCheckMs < 10000) return;
    lastWifiCheckMs = millis();

    if (currentMode == AppMode::AiScreensaver) {
        if (wifiConnected && WiFi.status() != WL_CONNECTED) {
            GW_LOGLN("WiFi lost -> AI continuing from SD cache if available");
            disconnectWiFi();
        }
        return;
    }

    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
        GW_LOGLN("WiFi lost -> fallback to Watch");
        disconnectWiFi();
        setMode(AppMode::Watch);
    }
}

// ── Touch ──────────────────────────────────────────────────────────────
bool touchShouldPollNow() {
    const unsigned long now = millis();
    if (!touchReady) return false;
    if (now < touchErrorBackoffUntilMs) return false;

    const unsigned long pollMs = touchPrevPressed ? TOUCH_ACTIVE_POLL_MS : TOUCH_IDLE_POLL_MS;
    if (lastTouchPollMs != 0 && now - lastTouchPollMs < pollMs) return false;
    return true;
}

bool pollTouch(TouchState &ts) {
    if (!touchShouldPollNow()) return false;

    lastTouchPollMs = millis();
    const int fc = (int)touchDev->IIC_Read_Device_Value(
        Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER);
    if (fc < 0) {
        touchErrorBackoffUntilMs = millis() + TOUCH_ERROR_BACKOFF_MS;
        return false;
    }

    ts.pressed = fc > 0;
    if (ts.pressed) {
        const int rx = (int)touchDev->IIC_Read_Device_Value(
            Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
        const int ry = (int)touchDev->IIC_Read_Device_Value(
            Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
        if (rx < 0 || ry < 0) {
            touchErrorBackoffUntilMs = millis() + TOUCH_ERROR_BACKOFF_MS;
            return false;
        }
        touchLastX = (uint16_t)rx;
        touchLastY = (uint16_t)ry;
    }

    ts.x = touchLastX;
    ts.y = touchLastY;
    ts.justPressed = ts.pressed && !touchPrevPressed;
    ts.justReleased = !ts.pressed && touchPrevPressed;
    touchPrevPressed = ts.pressed;

    if (touchDev->IIC_Interrupt_Flag == true) {
        touchDev->IIC_Interrupt_Flag = false;
    }

    return ts.pressed || ts.justPressed || ts.justReleased;
}

void resetTouchState() {
    touchPrevPressed = false;
    touchLastX = 0;
    touchLastY = 0;
    lastTouchPollMs = 0;
    touchErrorBackoffUntilMs = 0;
    touchDev->IIC_Interrupt_Flag = false;
}

bool botTouchReadSuspended() {
    return currentMode == AppMode::Bot &&
           (MicAudio::recording || millis() < botTouchResumeMs);
}

// ── Presets ────────────────────────────────────────────────────────────
struct PersonaPreset { const char *label; const char *prompt; };
static const PersonaPreset kPersonas[] = {
    {"Neutral",  "You are a concise and practical assistant. Keep answers clear and useful."},
    {"Friendly", "You are a warm and encouraging assistant. Keep replies upbeat and helpful."},
    {"Cranky",   "You are a helpful chatbot that answers in a cranky, mildly annoyed tone."},
    {"Roast",    "You are a witty wristwatch chatbot with a playful roast-comedy personality."},
    {"Tutor",    "You are a patient, clear, step-by-step tutor. Teach without talking down."},
    {"Zen",      "You are a calm, steady, minimal assistant. Keep replies grounded."},
};
static constexpr int kPersonaCount = 6;

void cyclePersona() {
    static int idx = 0;
    // Find current in presets or use idx
    String cur = settings.personaPrompt; cur.trim();
    for (int i = 0; i < kPersonaCount; i++) {
        if (cur == kPersonas[i].prompt) { idx = i; break; }
    }
    idx = (idx + 1) % kPersonaCount;
    strlcpy(settings.personaPrompt, kPersonas[idx].prompt, sizeof(settings.personaPrompt));
    saveSettings(settings);
}

void cycleModel() {
    static constexpr const char *models[] = {
        "llama-3.1-8b-instant", "llama-3.3-70b-versatile",
        "qwen/qwen3-32b", "groq/compound-mini", "openai/gpt-oss-20b"
    };
    static constexpr int mc = 5;
    int idx = 0;
    for (int i = 0; i < mc; i++) { if (String(settings.model) == models[i]) { idx = i; break; } }
    idx = (idx + 1) % mc;
    strlcpy(settings.model, models[idx], sizeof(settings.model));
    saveSettings(settings);
}

void cycleBootMode() {
    if (strcmp(settings.bootMode, "watch") == 0) strlcpy(settings.bootMode, "ai", sizeof(settings.bootMode));
    else if (strcmp(settings.bootMode, "ai") == 0) strlcpy(settings.bootMode, "bot", sizeof(settings.bootMode));
    else strlcpy(settings.bootMode, "watch", sizeof(settings.bootMode));
    saveSettings(settings);
}

bool watchFaceUsesPortal(uint8_t face) {
    return face != 1;
}

void applyWatchFaceConnectivity() {
    if (currentMode != AppMode::Watch) return;

    if (watchFaceUsesPortal(settings.watchFace)) {
        if (!cpIsRunning()) {
            disconnectWiFi();
            cpInitPortal();
        }
    } else {
        if (cpIsRunning()) {
            cpClosePortal();
        }
        if (hasWiFi(settings)) {
            ensureWifiConnected();
        } else {
            wifiConnected = false;
        }
        if (!cleanWatchForecast.length()) {
            cleanWatchForecastNextAttemptMs = 0;
        }
    }

    cleanWatchDirty = true;
    cleanWatchLastMinute = -1;
}

// ── Watch rendering ────────────────────────────────────────────────────
void initParticlesForStyle(uint8_t s) {
    activeStyle=clampStyle(s); randomSeed(esp_random());
    for(uint8_t i=0;i<kParticleCount;i++){
        particles[i].size=1+(random(0,100)>72?1:0);
        if(activeStyle==0){particles[i].x=random(0,PixelCanvas::kLogicalWidth);particles[i].y=random(0,PixelCanvas::kLogicalHeight);particles[i].vx=0;particles[i].vy=1+(i%2);}
        else if(activeStyle==1){particles[i].x=random(4,PixelCanvas::kLogicalWidth-4);particles[i].y=random(0,PixelCanvas::kLogicalHeight);particles[i].vx=(i%3)-1;particles[i].vy=-1-(i%2);}
        else{particles[i].x=random(0,PixelCanvas::kLogicalWidth);particles[i].y=random(0,PixelCanvas::kLogicalHeight);particles[i].vx=(i%2==0)?1:-1;particles[i].vy=(i%3==0)?1:0;}
    }
}

void updateParticles() {
    for(uint8_t i=0;i<kParticleCount;i++){
        Particle &p=particles[i];
        if(activeStyle==0){p.y+=p.vy;if(p.y>=PixelCanvas::kLogicalHeight){p.y=0;p.x=random(0,PixelCanvas::kLogicalWidth);}}
        else if(activeStyle==1){p.y+=p.vy;p.x+=p.vx;if(p.y<-2){p.y=PixelCanvas::kLogicalHeight+random(0,8);p.x=random(6,PixelCanvas::kLogicalWidth-6);}if(p.x<2)p.x=2;if(p.x>PixelCanvas::kLogicalWidth-2)p.x=PixelCanvas::kLogicalWidth-2;}
        else{p.x+=p.vx;p.y+=p.vy;if(p.x<0)p.x=PixelCanvas::kLogicalWidth-1;if(p.x>=PixelCanvas::kLogicalWidth)p.x=0;if(p.y<0)p.y=PixelCanvas::kLogicalHeight-1;if(p.y>=PixelCanvas::kLogicalHeight)p.y=0;}
    }
}

void renderStyleBackground() {
    if(activeStyle==0){
        canvas.clear(RGB565_BLACK); uint16_t sA=gfx->color565(120,220,255),sB=gfx->color565(255,255,255);
        for(uint8_t i=0;i<kParticleCount;i++) canvas.fillRect(particles[i].x,particles[i].y,particles[i].size,particles[i].size,(i%3==0)?sB:sA);
    }else if(activeStyle==1){
        uint16_t bg=gfx->color565(4,10,18),bub=gfx->color565(90,210,255),wd=gfx->color565(0,180,90);
        canvas.clear(bg);
        for(uint8_t x=0;x<PixelCanvas::kLogicalWidth;x+=7){int16_t h=10+((x*3+millis()/120)%16);canvas.fillRect(x,PixelCanvas::kLogicalHeight-h,2,h,wd);}
        for(uint8_t i=0;i<kParticleCount;i++) canvas.drawRect(particles[i].x,particles[i].y,2+particles[i].size,2+particles[i].size,bub);
    }else{
        uint16_t bg=gfx->color565(8,4,18),gr=gfx->color565(50,20,95),orb=gfx->color565(255,110,40);
        canvas.clear(bg); uint8_t off=(millis()/150)%6;
        for(uint8_t x=off;x<PixelCanvas::kLogicalWidth;x+=6)canvas.fillRect(x,0,1,PixelCanvas::kLogicalHeight,gr);
        for(uint8_t y=off;y<PixelCanvas::kLogicalHeight;y+=6)canvas.fillRect(0,y,PixelCanvas::kLogicalWidth,1,gr);
        for(uint8_t i=0;i<kParticleCount;i++)canvas.fillRect(particles[i].x,particles[i].y,2,2,orb);
    }
}

void renderClockToCanvas() {
    if(!clockNow.valid)return;
    int h12=clockNow.hour%12;if(h12==0)h12=12;
    char tb[8]; snprintf(tb,sizeof(tb),"%d%c%02d",h12,(clockNow.second%2==0)?':':' ',clockNow.minute);
    uint16_t sh=gfx->color565(0,0,0),ink=gfx->color565(250,250,250),acc=gfx->color565(255,180,50);
    canvas.drawTextCentered(tb,PixelCanvas::kLogicalWidth/2+1,33,sh,3,2);
    canvas.drawTextCentered(tb,PixelCanvas::kLogicalWidth/2,32,ink,3,2);
    canvas.drawTextCentered((clockNow.hour>=12)?"PM":"AM",PixelCanvas::kLogicalWidth/2,52,acc,2,1);
}

void renderWatchStatusBar() {
    char db[24]; snprintf(db,sizeof(db),"%04d-%02d-%02d %02d:%02d:%02d",clockNow.year,clockNow.month,clockNow.day,clockNow.hour,clockNow.minute,clockNow.second);
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_CYAN, RGB565_BLACK); gfx->setCursor(8,8); gfx->print(kWatchFaceNames[settings.watchFace]);
    gfx->setTextColor(RGB565_WHITE, RGB565_BLACK); gfx->setCursor(8,482); gfx->print(settings.watchFace == 0 ? "tap top:style" : "tap top:face");
    gfx->setCursor(270,482); gfx->print("bottom:menu");
    gfx->setCursor(70,458); gfx->print(db);
    gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK); gfx->setCursor(300,8); gfx->print(timeStatus);
    if(pmuReady&&power.isBatteryConnect()){gfx->setTextColor(RGB565_GREEN,RGB565_BLACK);gfx->setCursor(350,8);gfx->printf("%u%%",power.getBatteryPercent());}
    else{gfx->setTextColor(RGB565_RED,RGB565_BLACK);gfx->setCursor(352,8);gfx->print("BAT?");}
}

void renderWatchFrame() {
    switch (settings.watchFace) {
        case 0: renderStyleBackground(); renderClockToCanvas(); canvas.render(*gfx); break;
        case 1: renderCleanWatchFace(); break;
        case 2: renderStubWatchFace(); break;
    }
    renderWatchStatusBar();
}

void fetchNwsForecast() {
    const unsigned long now = millis();
    if (!hasNwsLocation(settings)) {
        cleanWatchForecast = "";
        cleanWatchForecastNextAttemptMs = 0;
        return;
    }
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
        if (!cleanWatchForecast.length()) {
            cleanWatchForecast = "Weather offline";
            cleanWatchDirty = true;
        }
        cleanWatchForecastNextAttemptMs = now + WEATHER_RETRY_MS;
        return;
    }
    if (cleanWatchForecastNextAttemptMs && now < cleanWatchForecastNextAttemptMs) return;

    auto failForecast = [&](const String &msg) {
        cleanWatchForecast = msg;
        cleanWatchForecastNextAttemptMs = millis() + WEATHER_RETRY_MS;
        cleanWatchDirty = true;
        GW_LOGF("[NWS] %s; retry in %lu ms\n", msg.c_str(), WEATHER_RETRY_MS);
    };

    const unsigned long fetchStartMs = millis();
    GW_LOGLN("[NWS] forecast fetch start");

    String pointsPath = "https://api.weather.gov/points/" + String(settings.latitude) + "," + String(settings.longitude);
    HTTPClient http;
    http.setTimeout(4000);
    if (!http.begin(pointsPath)) { failForecast("NWS begin fail"); return; }
    http.addHeader("User-Agent", "(GroqWatch, coreymillia@gmail.com)");
    int code = http.GET();
    if (code != 200) { http.end(); failForecast("NWS HTTP " + String(code)); return; }

    JsonDocument ptsDoc;
    if (deserializeJson(ptsDoc, http.getStream())) { http.end(); failForecast("NWS parse"); return; }
    http.end();
    String forecastUrl = String(ptsDoc["properties"]["forecast"] | "");
    if (!forecastUrl.length()) { failForecast("No NWS fcst"); return; }

    HTTPClient http2;
    http2.setTimeout(4000);
    if (!http2.begin(forecastUrl)) { failForecast("NWS url fail"); return; }
    http2.addHeader("User-Agent", "(GroqWatch, coreymillia@gmail.com)");
    code = http2.GET();
    if (code != 200) { http2.end(); failForecast("NWS fcst " + String(code)); return; }

    JsonDocument fcDoc;
    if (deserializeJson(fcDoc, http2.getStream())) { http2.end(); failForecast("NWS fcst parse"); return; }
    http2.end();

    JsonArray periods = fcDoc["properties"]["periods"].as<JsonArray>();
    if (!periods || periods.size() < 2) { failForecast("No NWS periods"); return; }

    String fc = "";
    for (int i = 0; i < min(4, (int)periods.size()); i++) {
        String name  = String(periods[i]["name"]  | "");
        String temp  = String(periods[i]["temperature"] | 0);
        String unit  = String(periods[i]["temperatureUnit"] | "F");
        String sFore = String(periods[i]["shortForecast"] | "");
        if (i > 0) fc += " | ";
        fc += name + ": " + temp + unit + " " + sFore;
    }
    cleanWatchForecast = fc;
    cleanWatchForecastMs = now;
    cleanWatchForecastNextAttemptMs = now + WEATHER_REFRESH_MS;
    cleanWatchDirty = true;
    GW_LOGF("[NWS] forecast updated in %lu ms\n", millis() - fetchStartMs);
}

void renderCleanWatchFace() {
    const int minuteKey = clockNow.hour * 60 + clockNow.minute;
    if (!cleanWatchDirty && minuteKey == cleanWatchLastMinute) return;
    cleanWatchDirty = false;
    cleanWatchLastMinute = minuteKey;

    gfx->fillScreen(RGB565_BLACK);
    if (!clockNow.valid) return;

    int h12 = clockNow.hour % 12; if (h12 == 0) h12 = 12;
    char tb[12]; snprintf(tb, sizeof(tb), "%d:%02d", h12, clockNow.minute);
    char db[24]; snprintf(db, sizeof(db), "%04d-%02d-%02d", clockNow.year, clockNow.month, clockNow.day);

    // Large time at top
    gfx->setTextSize(5);
    gfx->setTextColor(gfx->color565(250, 250, 250), RGB565_BLACK);
    int tw = strlen(tb) * 30;
    gfx->setCursor((LCD_WIDTH - tw) / 2, 30);
    gfx->print(tb);

    // Date below time
    gfx->setTextSize(2);
    gfx->setTextColor(gfx->color565(255, 180, 50), RGB565_BLACK);
    int dw = strlen(db) * 12;
    gfx->setCursor((LCD_WIDTH - dw) / 2, 105);
    gfx->print(db);

    gfx->setTextSize(1);
    gfx->setTextColor(gfx->color565(180, 200, 220), RGB565_BLACK);
    gfx->setCursor((LCD_WIDTH - 24) / 2, 130);
    gfx->print(clockNow.hour >= 12 ? "PM" : "AM");

    // NWS forecast section
    const int forecastY = 155;
    gfx->fillRoundRect(8, forecastY, LCD_WIDTH - 16, 200, 8, gfx->color565(10, 20, 40));
    gfx->drawRoundRect(8, forecastY, LCD_WIDTH - 16, 200, 8, gfx->color565(60, 120, 200));
    gfx->setTextSize(1);
    gfx->setTextColor(gfx->color565(60, 120, 200), gfx->color565(10, 20, 40));
    gfx->setCursor(18, forecastY + 8);
    gfx->print("NWS Forecast");

    if (cleanWatchForecast.length()) {
        gfx->setTextSize(2);
        gfx->setTextColor(RGB565_WHITE, gfx->color565(10, 20, 40));
        gfx->setCursor(18, forecastY + 30);
        // Word-wrap the forecast
        String fc = cleanWatchForecast; fc.trim();
        const int maxChars = 24;
        int cy = forecastY + 30;
        size_t pos = 0;
        while (pos < fc.length() && cy < forecastY + 190) {
            while (pos < fc.length() && fc[pos] == ' ') pos++;
            if (pos >= fc.length()) break;
            size_t end = min(fc.length(), pos + maxChars);
            int sep = fc.indexOf(" | ", pos);
            if (sep >= 0 && (size_t)sep < end) end = sep;
            else if (end < fc.length()) {
                int sp = fc.lastIndexOf(' ', (int)end);
                if (sp >= 0 && (size_t)sp > (int)pos) end = (size_t)sp;
            }
            String line = fc.substring(pos, end); line.trim();
            gfx->setCursor(18, cy);
            gfx->print(line);
            cy += 21;
            pos = end;
            if (pos < fc.length() && fc[pos] == '|') pos += 2;
        }
    } else if (!hasNwsLocation(settings)) {
        gfx->setTextSize(1);
        gfx->setTextColor(gfx->color565(120, 140, 160), gfx->color565(10, 20, 40));
        gfx->setCursor(18, forecastY + 40);
        gfx->print("Set lat/lon in Setup for weather.");
    } else {
        gfx->setTextSize(1);
        gfx->setTextColor(gfx->color565(120, 140, 160), gfx->color565(10, 20, 40));
        gfx->setCursor(18, forecastY + 40);
        gfx->print("Fetching weather data...");
        cleanWatchForecastMs = 0;  // force immediate retry
    }
}

void renderStubWatchFace() {
    // Placeholder for future analog / weather face
    gfx->fillScreen(gfx->color565(12, 18, 32));
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_CYAN, RGB565_BLACK);
    gfx->setCursor(60, 200);
    gfx->print("Watch Face #3");
    gfx->setTextSize(1);
    gfx->setCursor(40, 240);
    gfx->print("(analog / weather -- coming soon)");
}

void cycleWatchFace() {
    settings.watchFace = (settings.watchFace + 1) % kWatchFaceCount;
    saveSettings(settings);
    if (settings.watchFace == 0) initParticlesForStyle(settings.watchStyle);
    applyWatchFaceConnectivity();
}

// ── Mode rendering ─────────────────────────────────────────────────────
void renderAiFrame() {
    if (!aiNeedsRedraw) return;

    if (!aiModeActive || (!aiCurrentSlide.buffer && !aiCurrentSlide.fromSd)) {
        gfx->fillScreen(RGB565_BLACK);
        gfx->setTextColor(RGB565_CYAN, RGB565_BLACK); gfx->setTextSize(2);
        gfx->setCursor(28,180); gfx->print(aiStatus.c_str());
        gfx->setTextSize(1);
        gfx->setCursor(20,220); gfx->print(aiCacheSummary().c_str());
        if (aiLastError.length()) {
            gfx->setCursor(20,245); gfx->print(aiLastError.c_str());
        }
        aiAckRedraw();
        return;
    }

    aiDrawCurrentSlide(*gfx);

    // Status overlay
    char db[24]; snprintf(db,sizeof(db),"%02d:%02d",clockNow.hour,clockNow.minute);
    gfx->setTextSize(1); gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
    gfx->setCursor(8,8); gfx->print("AI SHOW");
    gfx->setCursor(340,8); gfx->print(db);
    gfx->setCursor(8,482); gfx->print("tap: next  |  bottom:back");
    if(pmuReady&&power.isBatteryConnect()){gfx->setTextColor(RGB565_GREEN,RGB565_BLACK);gfx->setCursor(350,24);gfx->printf("%u%%",power.getBatteryPercent());}
}

// ── Bot runtime ──────────────────────────────────────────────────────
static constexpr unsigned long BOT_STATE_POLL_MS = 1500;
static constexpr unsigned long BOT_ACTION_COOLDOWN_MS = 800;
static constexpr int BOT_BTN_Y = LCD_HEIGHT - 92;
static constexpr int BOT_BTN_H = 32;
static constexpr int BOT_BTN_W = 92;
static constexpr int BOT_BTN_GAP = 8;
static constexpr int BOT_BTN_REC_X = 9;
static constexpr int BOT_BTN_STOP_X = BOT_BTN_REC_X + BOT_BTN_W + BOT_BTN_GAP;
static constexpr int BOT_BTN_HOLD_X = BOT_BTN_STOP_X + BOT_BTN_W + BOT_BTN_GAP;
static constexpr int BOT_BTN_BACK_X = BOT_BTN_HOLD_X + BOT_BTN_W + BOT_BTN_GAP;

BotState botState = BotState::Idle;
String botStatus = "Set Whisplay URL in Setup";
String botLastUser = "";
String botLastReply = "";
String botRemoteEmoji = "";
String botGeneratedImagesRevision = "";
bool botRemoteReady = false;
bool botRemoteTextInputEnabled = false;
bool botDirty = true;
bool botBtnPrev = false;
bool botTouchHoldActive = false;
unsigned long botTimedStopMs = 0;
unsigned long botTouchResumeMs = 0;
unsigned long botUiLastMs = 0;
unsigned long botLastPollMs = 0;
unsigned long botLastActionMs = 0;
int botLastRenderMinuteKey = -1;
unsigned long botLastRenderTick = 0xFFFFFFFFUL;

static String botNormalizeBaseUrl(String value) {
    value.trim();
    while (value.endsWith("/")) value.remove(value.length() - 1);
    return value;
}

static bool botPointInRect(uint16_t px, uint16_t py, int x, int y, int w, int h) {
    return px >= static_cast<uint16_t>(x) && px <= static_cast<uint16_t>(x + w) &&
           py >= static_cast<uint16_t>(y) && py <= static_cast<uint16_t>(y + h);
}

static bool botPostJson(const char *path, const char *jsonBody, String *responseOut = nullptr) {
    if (!hasWhisplayUrl(settings)) {
        if (responseOut) *responseOut = "Whisplay URL missing";
        return false;
    }
    if (!ensureWifiConnected()) {
        if (responseOut) *responseOut = "WiFi disconnected";
        return false;
    }

    String endpoint = botNormalizeBaseUrl(String(settings.whisplayUrl)) + path;
    HTTPClient http;
    if (!http.begin(endpoint)) {
        if (responseOut) *responseOut = "HTTP begin failed";
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);
    int code = http.POST(jsonBody ? jsonBody : "{}");
    String response = http.getString();
    http.end();
    if (responseOut) *responseOut = response;
    return code >= 200 && code < 300;
}

static void drawBotButton(int x, const char *label, uint16_t border, bool active = false) {
    const uint16_t fill = active ? gfx->color565(30, 60, 100) : gfx->color565(12, 20, 36);
    gfx->fillRoundRect(x, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H, 6, fill);
    gfx->drawRoundRect(x, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H, 6, border);
    gfx->setTextSize(2);
    gfx->setTextColor(border, fill);
    int textX = x + (BOT_BTN_W - static_cast<int>(strlen(label)) * 12) / 2;
    gfx->setCursor(textX, BOT_BTN_Y + 9);
    gfx->print(label);
}

static void drawWrappedText(const String &text, int x, int y, int w, int h,
                            uint16_t fg, uint16_t bg, uint8_t textSize = 2) {
    const int charW = 6 * textSize;
    const int lineH = 8 * textSize + 6;
    const int maxChars = max(1, w / charW);
    gfx->setTextSize(textSize);
    gfx->setTextColor(fg, bg);

    size_t pos = 0;
    int cy = y;
    while (pos < text.length() && cy + lineH <= y + h) {
        while (pos < text.length() && text[pos] == ' ') pos++;
        if (pos >= text.length()) break;

        size_t end = min(text.length(), pos + static_cast<size_t>(maxChars));
        int nl = text.indexOf('\n', pos);
        if (nl >= 0 && static_cast<size_t>(nl) < end) {
            end = static_cast<size_t>(nl);
        } else if (end < text.length()) {
            int space = text.lastIndexOf(' ', static_cast<int>(end));
            if (space >= 0 && static_cast<size_t>(space) > pos) end = static_cast<size_t>(space);
        }

        String line = text.substring(pos, end);
        line.trim();
        gfx->setCursor(x, cy);
        gfx->print(line);
        cy += lineH;

        pos = end;
        if (pos < text.length() && text[pos] == '\n') pos++;
    }
}

static uint8_t botReplyTextSize() {
    if (botLastReply.length() <= 90) return 3;
    return 2;
}

static bool botFetchRemoteState(bool force = false) {
    if (!hasWhisplayUrl(settings)) {
        bool changed = botStatus != "Set Whisplay URL in Setup" || botLastReply.length() || botLastUser.length();
        botRemoteReady = false;
        botRemoteTextInputEnabled = false;
        botRemoteEmoji = "";
        botGeneratedImagesRevision = "";
        botLastUser = "Whisplay URL missing";
        botLastReply = "Open Setup and enter the Whisplay base URL to mirror the hat chat here.";
        botStatus = "Set Whisplay URL in Setup";
        botState = BotState::Idle;
        if (changed) botDirty = true;
        return false;
    }

    unsigned long now = millis();
    if (!force && botLastPollMs != 0 && now - botLastPollMs < BOT_STATE_POLL_MS) return true;
    botLastPollMs = now;

    if (!ensureWifiConnected()) {
        botRemoteReady = false;
        botState = BotState::Idle;
        botStatus = "WiFi disconnected";
        botDirty = true;
        return false;
    }

    String endpoint = botNormalizeBaseUrl(String(settings.whisplayUrl)) + "/api/state";
    HTTPClient http;
    if (!http.begin(endpoint)) {
        botRemoteReady = false;
        botState = BotState::Idle;
        botStatus = "HTTP begin failed";
        botDirty = true;
        return false;
    }
    http.setTimeout(5000);
    int code = http.GET();
    String response = http.getString();
    http.end();

    if (code != 200) {
        botRemoteReady = false;
        botState = BotState::Idle;
        botStatus = response.length() ? response : (String("HTTP ") + String(code));
        botDirty = true;
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, response)) {
        botRemoteReady = false;
        botState = BotState::Idle;
        botStatus = "State JSON parse failed";
        botDirty = true;
        return false;
    }

    const bool nextReady = doc["ready"] | false;
    const bool nextTextInputEnabled = doc["text_input_enabled"] | false;
    String nextStatus = String(doc["status"] | "idle");
    nextStatus.trim();
    String nextText = String(doc["text"] | "");
    nextText.trim();
    String nextEmoji = String(doc["emoji"] | "");
    nextEmoji.trim();
    String nextRevision = String(doc["generated_images_revision"] | "");

    String nextStateLine = nextEmoji;
    if (nextStatus.length()) {
        if (nextStateLine.length()) nextStateLine += " ";
        nextStateLine += nextStatus;
    }
    if (!nextStateLine.length()) nextStateLine = nextReady ? "Whisplay online" : "Whisplay not ready";
    if (!nextText.length()) nextText = nextReady ? "Waiting for the next Whisplay reply." : "Whisplay not ready.";

    bool changed =
        nextReady != botRemoteReady ||
        nextTextInputEnabled != botRemoteTextInputEnabled ||
        nextEmoji != botRemoteEmoji ||
        nextRevision != botGeneratedImagesRevision ||
        nextStateLine != botLastUser ||
        nextText != botLastReply;

    botRemoteReady = nextReady;
    botRemoteTextInputEnabled = nextTextInputEnabled;
    botRemoteEmoji = nextEmoji;
    botGeneratedImagesRevision = nextRevision;
    botLastUser = nextStateLine;
    botLastReply = nextText;
    botState = nextReady ? BotState::ShowingReply : BotState::Idle;

    if (nextReady) {
        botStatus = nextStatus.length() ? (String("Whisplay: ") + nextStatus) : "Whisplay connected";
    } else {
        botStatus = nextStatus.length() ? (String("Whisplay: ") + nextStatus) : "Whisplay not ready";
    }

    if (changed) botDirty = true;
    return nextReady;
}

static bool botRepeatRemoteReply() {
    if (millis() - botLastActionMs < BOT_ACTION_COOLDOWN_MS) return false;
    String response;
    if (!botPostJson("/api/companion/action", "{\"action\":\"repeat\"}", &response)) {
        botStatus = response.length() ? response : "Repeat failed";
        botDirty = true;
        return false;
    }
    botLastActionMs = millis();
    botState = BotState::Syncing;
    botStatus = "Repeat requested";
    botDirty = true;
    botLastPollMs = 0;
    return true;
}

static bool botResetRemoteChat() {
    if (millis() - botLastActionMs < BOT_ACTION_COOLDOWN_MS) return false;
    String response;
    if (!botPostJson("/api/chat/reset", "{}", &response)) {
        botStatus = response.length() ? response : "Reset failed";
        botDirty = true;
        return false;
    }
    botLastActionMs = millis();
    botRemoteReady = true;
    botState = BotState::Syncing;
    botLastUser = "Chat reset requested";
    botLastReply = "Waiting for the next Whisplay reply.";
    botStatus = "New chat requested";
    botDirty = true;
    botLastPollMs = 0;
    return true;
}

void renderBotFrame() {
    gfx->fillScreen(RGB565_BLACK);

    const uint16_t hdr = gfx->color565(10, 30, 60);
    const uint16_t panel = gfx->color565(8, 20, 40);

    gfx->fillRect(0, 0, LCD_WIDTH, 34, hdr);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_CYAN, hdr);
    gfx->setCursor(8, 8); gfx->print("WHISPLAY");

    char db[24]; snprintf(db, sizeof(db), "%02d:%02d", clockNow.hour, clockNow.minute);
    gfx->setTextSize(1);
    gfx->setCursor(360, 10); gfx->print(db);

    gfx->setTextColor(wifiConnected ? RGB565_GREEN : RGB565_RED, hdr);
    gfx->setCursor(92, 12); gfx->print(wifiConnected ? "WiFi OK" : "WiFi OFF");

    if (pmuReady && power.isBatteryConnect()) {
        gfx->setTextColor(RGB565_GREEN, hdr);
        gfx->setCursor(164, 12); gfx->printf("BAT %u%%", power.getBatteryPercent());
    }

    int contentY = 40;
    if (botLastUser.length()) {
        gfx->setTextSize(2);
        gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK);
        gfx->setCursor(8, contentY);
        gfx->print("STATE");
        drawWrappedText(botLastUser, 76, contentY - 2, LCD_WIDTH - 84, 62,
                        RGB565_WHITE, RGB565_BLACK, 2);
        contentY += 66;
    }

    const int replyBoxY = contentY;
    const int replyBoxH = BOT_BTN_Y - replyBoxY - 10;
    gfx->fillRoundRect(4, replyBoxY, LCD_WIDTH - 8, replyBoxH, 8, panel);
    gfx->drawRoundRect(4, replyBoxY, LCD_WIDTH - 8, replyBoxH, 8, RGB565_CYAN);
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_CYAN, panel);
    gfx->setCursor(12, replyBoxY + 8);
    gfx->print("Latest reply");

    if (!hasWhisplayUrl(settings)) {
        drawWrappedText("Set the Whisplay URL in Setup. This mode mirrors the hat chat and lets you repeat/reset it over Wi-Fi.",
                        16, replyBoxY + 34, LCD_WIDTH - 32, replyBoxH - 60,
                        gfx->color565(180, 190, 220), panel, 2);
    } else if (botState == BotState::Syncing) {
        gfx->setTextSize(2); gfx->setTextColor(RGB565_YELLOW, panel);
        gfx->setCursor(112, replyBoxY + replyBoxH / 2 - 8); gfx->print("Syncing...");
    } else {
        drawWrappedText(botLastReply.length() ? botLastReply : "Waiting for the next Whisplay reply.",
                        12, replyBoxY + 28, LCD_WIDTH - 24, replyBoxH - 36,
                        RGB565_WHITE, panel, botReplyTextSize());
    }

    drawBotButton(BOT_BTN_REC_X,  "NEW",    RGB565_GREEN,  false);
    drawBotButton(BOT_BTN_STOP_X, "REPEAT", RGB565_CYAN,   false);
    drawBotButton(BOT_BTN_HOLD_X, "AI",     RGB565_YELLOW, false);
    drawBotButton(BOT_BTN_BACK_X, "BACK",   RGB565_WHITE,  false);

    gfx->fillRect(0, LCD_HEIGHT - 44, LCD_WIDTH, 44, gfx->color565(10, 20, 40));
    gfx->setTextSize(1); gfx->setTextColor(RGB565_WHITE, gfx->color565(10, 20, 40));
    gfx->setCursor(8, LCD_HEIGHT - 38); gfx->print(botStatus.c_str());
    gfx->setCursor(8, LCD_HEIGHT - 18); gfx->print("Mirror + control Whisplay chat   |   NEW / REPEAT / AI / BACK");

    botDirty = false;
    botUiLastMs = millis();
    botLastRenderMinuteKey = clockNow.hour * 60 + clockNow.minute;
    botLastRenderTick = 0xFFFFFFFFUL;
}

void renderCurrentMode() {
    switch (currentMode) {
        case AppMode::Watch: renderWatchFrame(); break;
        case AppMode::AiScreensaver: renderAiFrame(); break;
        case AppMode::Bot: renderBotFrame(); break;
        case AppMode::Settings: settingsMenu.draw(*gfx); break;
    }
}

// ── Settings tile action ───────────────────────────────────────────────
void handleSettingsTile(int tileIndex) {
    // If in watch settings sub-menu, handle those tiles
    if (settingsMenu.inWatchSettings()) {
        switch (tileIndex) {
            case 7: // BACK
                settingsMenu.closeWatchSettings();
                break;
        }
        return;
    }

    // Main settings tiles
    switch (tileIndex) {
        case 0: // SETUP
            disconnectWiFi();
            runSetupPortalModal(gfx, settings);
            break;
        case 1: // FACE - cycle
            cycleWatchFace();
            settingsMenu.rebuild();
            renderCurrentMode();
            break;
        case 2: // WATCH
            setMode(AppMode::Watch);
            break;
        case 3: // BOOT
            cycleBootMode();
            settingsMenu.rebuild();
            renderCurrentMode();
            break;
        case 4: // BOT
            setMode(AppMode::Bot);
            break;
        case 5: // AI
            if (hasWhisplayUrl(settings) || aiCanRunOffline()) setMode(AppMode::AiScreensaver);
            break;
        case 7: // BACK
            setMode(AppMode::Watch);
            break;
    }
}

// ── Touch handling ─────────────────────────────────────────────────────
static void handleBotTouch(const TouchState &touch) {
    if (!touch.justPressed) return;

    if (millis() - lastTouchLogMs > 250) {
        GW_LOGF("touch x=%u y=%u mode=%s\n", touch.x, touch.y, modeLabel(currentMode));
        lastTouchLogMs = millis();
    }

    if (botPointInRect(touch.x, touch.y, BOT_BTN_BACK_X, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H)) {
        botTouchHoldActive = false;
        setMode(AppMode::Watch);
        return;
    }

    if (botPointInRect(touch.x, touch.y, BOT_BTN_REC_X, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H)) {
        botResetRemoteChat();
        return;
    }

    if (botPointInRect(touch.x, touch.y, BOT_BTN_STOP_X, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H)) {
        botRepeatRemoteReply();
        return;
    }

    if (botPointInRect(touch.x, touch.y, BOT_BTN_HOLD_X, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H)) {
        if (hasWhisplayUrl(settings) || aiCanRunOffline()) {
            setMode(AppMode::AiScreensaver);
        }
        return;
    }
}

void handleTouch(const TouchState &touch) {
    if (!touch.pressed && !touch.justPressed && !touch.justReleased) return;
    lastActivityMs = millis();
    if (currentMode == AppMode::Bot) {
        handleBotTouch(touch);
        return;
    }

    if (!touch.justPressed) return;
    if (millis() - lastTouchLogMs > 250) {
        GW_LOGF("touch x=%u y=%u mode=%s\n", touch.x, touch.y, modeLabel(currentMode));
        lastTouchLogMs = millis();
    }

    if (currentMode == AppMode::Settings) {
        int tile = settingsMenu.tileAtPoint(touch.x, touch.y);
        if (tile >= 0) handleSettingsTile(tile);
        return;
    }

    if (currentMode == AppMode::AiScreensaver) {
        if (touch.y > 440) { // bottom = back to watch
            setMode(AppMode::Watch);
            return;
        }
        // Tap anywhere else = next slide
        aiModeActive = aiShowNextSlide(settings.whisplayUrl, false);
        aiFrameLastMs = millis();
        return;
    }

    // Watch mode touch zones
    if (touch.y < 80) {
        if (settings.watchFace == 0) {
            // Face 0: cycle particle sub-style on top tap
            settings.watchStyle = clampStyle((settings.watchStyle + 1) % 3);
            saveSettings(settings);
            initParticlesForStyle(settings.watchStyle);
        } else {
            // Other faces: cycle to next watch face on top tap
            cycleWatchFace();
        }
        return;
    }
    if (touch.y > 430) {
        setMode(AppMode::Settings);
        return;
    }
}

// ── Hardware init ──────────────────────────────────────────────────────
void initHardware() {
    Serial.begin(115200); delay(100);
#if defined(GROQWATCH_LOW_LOG_BUILD) && GROQWATCH_LOW_LOG_BUILD
    Serial.setDebugOutput(false);
    esp_log_level_set("*", ESP_LOG_NONE);
#endif
    GW_LOGLN();
    GW_LOGLN("GroqWatch boot");
    GW_LOGF("psramFound=%s size=%u free=%u heap=%u largest=%u\n",
            psramFound() ? "yes" : "no",
            static_cast<unsigned>(ESP.getPsramSize()),
            static_cast<unsigned>(ESP.getFreePsram()),
            static_cast<unsigned>(ESP.getFreeHeap()),
            static_cast<unsigned>(ESP.getMaxAllocHeap()));
    Wire.begin(IIC_SDA, IIC_SCL);

    displayReady = gfx->begin();
    if (!displayReady) { GW_LOGLN("display fail"); return; }
    gfx->fillScreen(RGB565_BLACK);
    drawStatusScreen("GroqWatch","Display ready","Bringing up hardware...");

    touchReady = touchDev->begin();
    if (touchReady) {
        touchDev->IIC_Write_Device_State(Arduino_IIC_Touch::Device::TOUCH_POWER_MODE, Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
        GW_LOGLN("touch ready");
    } else GW_LOGLN("touch fail");

    rtcReady = rtc.begin(Wire, IIC_SDA, IIC_SCL);
    if (rtcReady) {
        RTC_DateTime dt=rtc.getDateTime(); rtcHasValidTime=rtcDateTimeLooksValid(dt);
        if(rtcHasValidTime){setFallbackFromRtc(dt);timeStatus="RTC";}
        GW_LOGF("rtc ready valid=%s\n",rtcHasValidTime?"yes":"no");
    } else GW_LOGLN("rtc fail");

    pmuReady = power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
    if (pmuReady) {
        power.enableBattDetection(); power.enableBattVoltageMeasure();
        power.enableVbusVoltageMeasure(); power.enableSystemVoltageMeasure();
        power.enableTemperatureMeasure();
        power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
        power.clearIrqStatus();
        power.enableIRQ(
            XPOWERS_AXP2101_PKEY_SHORT_IRQ |
            XPOWERS_AXP2101_PKEY_LONG_IRQ |
            XPOWERS_AXP2101_PKEY_POSITIVE_IRQ |
            XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ
        );
        GW_LOGLN("pmu ready");
    } else GW_LOGLN("pmu fail");
}

}  // namespace

// ── Arduino entry points ────────────────────────────────────────────────
void setup() {
    fallbackEpoch=compileTimeEpoch(); fallbackEpochMs=millis();
    loadSettings(settings);
    initHardware();
    if (!displayReady) return;

    AppMode bootMode = bootModeFromString(settings.bootMode);

    if (watchBootButtonHeld()) {
        GW_LOGLN("boot held -> setup");
        runSetupPortalModal(gfx, settings);
    }
    if (bootMode != AppMode::Watch && !hasWiFi(settings)) {
        GW_LOGLN("no wifi -> setup");
        runSetupPortalModal(gfx, settings);
    }

    if (bootMode != AppMode::Watch) {
        connectWiFi();
    }
    pollClock();
    lastActivityMs = millis();
    screenBlanked = false;
    settings.watchStyle=clampStyle(settings.watchStyle);
    initParticlesForStyle(settings.watchStyle);

    // Honor the saved boot mode.
    GW_LOGF("startup boot mode=%s\n", settings.bootMode);
    setMode(bootMode);
    renderCurrentMode();
}

// ── Screen timeout helpers ─────────────────────────────────────────────
void dirtyAllModes() {
    cleanWatchDirty = true;
    cleanWatchLastMinute = -1;
    botDirty = true;
    aiMarkNeedsRedraw();
}

void setScreenPowerState(bool on) {
    if (on) {
        if (!screenBlanked) return;
        gfx->displayOn();
        screenBlanked = false;
        lastActivityMs = millis();
        wakeInputIgnoreUntilMs = millis() + 500;
        wakeBootWaitRelease = digitalRead(WATCH_BOOT_BUTTON_PIN) == LOW;
        aiBootDown = false;
        aiBootLongHandled = false;
        aiBootDownMs = 0;
        resetTouchState();
        cleanWatchDirty = true;
        cleanWatchLastMinute = -1;
        botDirty = true;
        aiMarkNeedsRedraw();
        GW_LOGLN("[Screen] PWR wake");
        renderCurrentMode();
        return;
    }

    if (screenBlanked) return;
    resetTouchState();
    gfx->displayOff();
    screenBlanked = true;
    GW_LOGLN("[Screen] PWR sleep");
}

void manualScreenSleep() {
    GW_LOGLN("[Screen] manual sleep disabled until wake is proven reliable");
}

void loop() {
    if (!displayReady) { delay(1000); return; }

    if (cpIsRunning()) {
        cpRunPortal();
    }

    bool pwrShortPress = false;
    if (pmuReady) {
        const uint64_t pmuIrq = power.getIrqStatus();
        if (pmuIrq) {
            if (power.isPekeyShortPressIrq())  { GW_LOGLN("[PMU] PWR short press"); pwrShortPress = true; }
            if (power.isPekeyLongPressIrq())   GW_LOGLN("[PMU] PWR long press");
            if (power.isPekeyPositiveIrq())    GW_LOGLN("[PMU] PWR positive edge");
            if (power.isPekeyNegativeIrq())    GW_LOGLN("[PMU] PWR negative edge");
            power.clearIrqStatus();
        }
    }

    // ── Screen power toggle via PWR short press ────────────────────
    if (screenBlanked) {
        const bool bootWake = digitalRead(WATCH_BOOT_BUTTON_PIN) == LOW;
        if (pwrShortPress || bootWake) {
            setScreenPowerState(true);
        } else {
            delay(20);
        }
        return;
    }
    if (pwrShortPress) {
        setScreenPowerState(false);
        return;
    }

    const bool bootNow = digitalRead(WATCH_BOOT_BUTTON_PIN) == LOW;
    if (bootNow) lastActivityMs = millis();

    if (wakeBootWaitRelease) {
        if (!bootNow) {
            wakeBootWaitRelease = false;
            aiBootDown = false;
            aiBootLongHandled = false;
            aiBootDownMs = 0;
        }
    }

    if (currentMode == AppMode::Bot) {
        MicAudio::poll();
    }

    if (touchReady) {
        if (botTouchReadSuspended() || millis() < wakeInputIgnoreUntilMs) {
            if (!touchPollingSuspended) {
                resetTouchState();
                touchPollingSuspended = true;
            }
        } else {
            touchPollingSuspended = false;
            TouchState touch;
            if (pollTouch(touch)) {
                handleTouch(touch);
            }
        }
    }

    // AI mode: BOOT shortcuts are also active. Short press = next slide, long hold = back to Watch.
    if (currentMode == AppMode::AiScreensaver && !wakeBootWaitRelease && millis() >= wakeInputIgnoreUntilMs) {
        if (bootNow && !aiBootDown) {
            aiBootDown = true;
            aiBootLongHandled = false;
            aiBootDownMs = millis();
        }
        if (bootNow && aiBootDown && !aiBootLongHandled && millis() - aiBootDownMs >= AI_BOOT_LONG_MS) {
            aiBootLongHandled = true;
            GW_LOGLN("[AI] BOOT long -> Watch");
            setMode(AppMode::Watch);
            return;
        }
        if (!bootNow && aiBootDown) {
            const unsigned long heldMs = millis() - aiBootDownMs;
            aiBootDown = false;
            if (!aiBootLongHandled && heldMs >= 30) {
                GW_LOGLN("[AI] BOOT short -> next slide");
                aiModeActive = aiShowNextSlide(settings.whisplayUrl, false);
                aiFrameLastMs = millis();
                lastActivityMs = millis();
            }
            aiBootLongHandled = false;
            aiBootDownMs = 0;
        }
    }

    // Per-frame work
    if (millis() - lastFrameMs >= 90) {
        lastFrameMs = millis();
        pollClock();
        checkWifiAndFallback();

        if (currentMode == AppMode::Watch) {
            updateParticles();
            // Fetch NWS weather for the Clean watch face (face 1)
            if (settings.watchFace == 1 && hasNwsLocation(settings)) {
                fetchNwsForecast();
            }
            renderWatchFrame();
        } else if (currentMode == AppMode::AiScreensaver) {
            // Rescue behavior: no automatic slide advance for now.
            // AI mode changes only on explicit user action so BOOT handling stays responsive.
            if (aiNeedsRedraw) {
                renderAiFrame();
            }
        } else if (currentMode == AppMode::Bot) {
            // BOOT button: REPEAT when Whisplay is online
            bool btnNow = digitalRead(WATCH_BOOT_BUTTON_PIN) == LOW;
            if (btnNow && !botBtnPrev) {
                if (botRemoteReady) {
                    botRepeatRemoteReply();
                }
            }
            botBtnPrev = btnNow;

            // Poll Whisplay state periodically
            botFetchRemoteState();

            bool needsBotRender = botDirty;
            const int minuteKey = clockNow.hour * 60 + clockNow.minute;
            if (minuteKey != botLastRenderMinuteKey) {
                needsBotRender = true;
            }

            if (needsBotRender) {
                renderBotFrame();
            }
        } else if (currentMode == AppMode::Settings) {
            settingsMenu.draw(*gfx);
        }
    }

    delay(2);
}
