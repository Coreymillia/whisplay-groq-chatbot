#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <JPEGDEC.h>
#include <XPT2046_Touchscreen.h>

#include "Portal.h"

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
SPIClass sdSPI(HSPI);

#define SD_CS 5
#define SD_SCK 18
#define SD_MOSI 23
#define SD_MISO 19

static const uint16_t COLOR_BG = RGB565_BLACK;
static const uint16_t COLOR_TEXT = RGB565_WHITE;
static const uint16_t COLOR_DIM = 0x7BEF;
static const uint16_t COLOR_ACCENT = 0x07FF;
static const uint16_t COLOR_WARN = 0xFFE0;
static const uint16_t COLOR_ERROR = 0xF800;
static const uint16_t COLOR_PANEL = 0x18C3;

static constexpr char AI_SLIDESHOW_DIR[] = "/whisplay-ai";
static constexpr char AI_PREFS_NAMESPACE[] = "compcyd";
static constexpr char AI_PREF_KEY_FILE[] = "aislide";
static constexpr char AI_PREF_KEY_INDEX[] = "aiidx";
static constexpr char PNG_RENDER_CACHE_PATH[] = "/png-render-cache.jpg";
static constexpr size_t MAX_SLIDES = 512;
static constexpr size_t MAX_SLIDE_FILE_NAME = 96;
static constexpr size_t REMOTE_PAGE_SIZE = 12;
static constexpr unsigned long TOUCH_DEBOUNCE_MS = 220;
static constexpr unsigned long BOOT_PORTAL_HOLD_MS = 1200;
static constexpr unsigned long STATUS_OVERLAY_MS = 2400;
static constexpr unsigned long SD_SCAN_MS = 10000;
static constexpr unsigned long AI_SYNC_STEP_MS = 3000;
static constexpr unsigned long AI_SLIDE_ADVANCE_MS = 8000;
static constexpr unsigned long SD_RETRY_MS = 5000;
static constexpr uint32_t SD_SPI_FREQUENCY = 4000000;
static constexpr int AI_SLIDE_RENDER_WIDTH = 320;
static constexpr int AI_SLIDE_RENDER_HEIGHT = 240;

struct RemotePhoto {
  String fileName;
  String imageUrl;
  String companionImageUrl;
};

struct RemotePage {
  size_t count = 0;
  size_t total = 0;
  bool hasMore = false;
  RemotePhoto photos[REMOTE_PAGE_SIZE];
};

static JPEGDEC previewJpeg;
static File previewJpegFile;
static bool sdReady = false;
static bool renderDirty = true;
static bool touchWasDown = false;
static bool bootButtonWasDown = false;
static bool bootPortalOpened = false;
static unsigned long lastTouchMs = 0;
static unsigned long lastSlideAdvanceMs = 0;
static unsigned long lastSdScanMs = 0;
static unsigned long lastSyncStepMs = 0;
static unsigned long lastSdRetryMs = 0;
static unsigned long lastBootHoldStartMs = 0;
static unsigned long statusUntilMs = 0;
static bool lastStatusOverlayVisible = false;
static size_t slideCount = 0;
static size_t slideIndex = 0;
static size_t syncOffset = 0;
static String slideFileNames[MAX_SLIDES];
static String currentSlideFileName;
static String persistedAiSlideFileName;
static String statusMessage;
static String lastErrorMessage;

static String apiUrlForPath(const String &path);
static String remoteCompanionPathForFileName(const String &fileName);
static bool downloadFileToSpiffs(const String &remotePath, const char *localPath, String *errorOut = nullptr);

static int32_t previewFileRead(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
  (void)handle;
  return static_cast<int32_t>(previewJpegFile.read(buffer, length));
}

static int32_t previewFileSeek(JPEGFILE *handle, int32_t position) {
  (void)handle;
  return previewJpegFile.seek(position) ? position : 0;
}

static void previewFileClose(void *handle) {
  (void)handle;
  previewJpegFile.close();
}

static int jpegDrawCallback(JPEGDRAW *draw) {
  gfx->draw16bitBeRGBBitmap(draw->x, draw->y, draw->pPixels, draw->iWidth, draw->iHeight);
  return 1;
}

static void mapTouch(uint16_t rawX, uint16_t rawY, int &screenX, int &screenY) {
  screenX = map(rawX, 200, 3800, 0, 320);
  screenY = map(rawY, 200, 3800, 0, 240);
  screenX = constrain(screenX, 0, 319);
  screenY = constrain(screenY, 0, 239);
}

static String trimTail(const String &value, size_t maxLen) {
  if (value.length() <= maxLen) {
    return value;
  }
  if (maxLen <= 3) {
    return value.substring(0, maxLen);
  }
  return value.substring(0, maxLen - 3) + "...";
}

static String leafName(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash >= 0 && slash + 1 < static_cast<int>(path.length())) {
    return path.substring(slash + 1);
  }
  return path;
}

static bool isJpegFileName(const String &fileName) {
  String lower = fileName;
  lower.toLowerCase();
  return lower.endsWith(".jpg") || lower.endsWith(".jpeg");
}

static bool isPngFileName(const String &fileName) {
  String lower = fileName;
  lower.toLowerCase();
  return lower.endsWith(".png");
}

static bool isRenderableSlideFileName(const String &fileName) {
  return isJpegFileName(fileName) || isPngFileName(fileName);
}

static String slidePathForFileName(const String &fileName) {
  return String(AI_SLIDESHOW_DIR) + "/" + fileName;
}

static void showStatus(const String &message) {
  statusMessage = message;
  statusUntilMs = millis() + STATUS_OVERLAY_MS;
  renderDirty = true;
}

static void loadAiSlidePrefs() {
  Preferences prefs;
  if (!prefs.begin(AI_PREFS_NAMESPACE, true)) {
    return;
  }
  persistedAiSlideFileName = prefs.getString(AI_PREF_KEY_FILE, "");
  slideIndex = prefs.getULong(AI_PREF_KEY_INDEX, 0);
  prefs.end();
}

static void saveAiSlidePrefs() {
  Preferences prefs;
  if (!prefs.begin(AI_PREFS_NAMESPACE, false)) {
    return;
  }
  String fileName = slideCount ? slideFileNames[slideIndex] : String();
  prefs.putString(AI_PREF_KEY_FILE, fileName);
  prefs.putULong(AI_PREF_KEY_INDEX, slideIndex);
  prefs.end();
  persistedAiSlideFileName = fileName;
}

static bool ensureSdCardReady() {
  if (sdReady) {
    return true;
  }
  unsigned long now = millis();
  if (lastSdRetryMs != 0 && now - lastSdRetryMs < SD_RETRY_MS) {
    return false;
  }
  lastSdRetryMs = now;
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI, SD_SPI_FREQUENCY);
  if (!sdReady || SD.cardType() == CARD_NONE) {
    sdReady = false;
    lastErrorMessage = "SD card not ready";
    renderDirty = true;
    return false;
  }
  SD.mkdir(AI_SLIDESHOW_DIR);
  lastErrorMessage = "";
  renderDirty = true;
  return true;
}

static int indexOfSlide(const String &fileName) {
  for (size_t i = 0; i < slideCount; i++) {
    if (slideFileNames[i] == fileName) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

static bool slideExistsOnSd(const String &fileName) {
  if (!sdReady || !fileName.length()) {
    return false;
  }
  return SD.exists(slidePathForFileName(fileName).c_str());
}

static void scanSlidesFromSd() {
  if (!ensureSdCardReady()) {
    slideCount = 0;
    return;
  }

  const String preferredFile = currentSlideFileName.length() ? currentSlideFileName : persistedAiSlideFileName;
  size_t previousCount = slideCount;
  File dir = SD.open(AI_SLIDESHOW_DIR, "r");
  if (!dir || !dir.isDirectory()) {
    slideCount = 0;
    renderDirty = true;
    return;
  }

  slideCount = 0;
  while (slideCount < MAX_SLIDES) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }
    String fileName = leafName(String(entry.name()));
    entry.close();
    if (!isRenderableSlideFileName(fileName)) {
      continue;
    }
    slideFileNames[slideCount++] = fileName;
  }
  dir.close();

  if (slideCount > 1) {
    for (size_t i = 0; i + 1 < slideCount; i++) {
      for (size_t j = i + 1; j < slideCount; j++) {
        if (slideFileNames[j].compareTo(slideFileNames[i]) > 0) {
          String swapValue = slideFileNames[i];
          slideFileNames[i] = slideFileNames[j];
          slideFileNames[j] = swapValue;
        }
      }
    }
  }

  if (!slideCount) {
    slideIndex = 0;
    currentSlideFileName = "";
  } else if (preferredFile.length()) {
    int restoredIndex = indexOfSlide(preferredFile);
    if (restoredIndex >= 0) {
      slideIndex = static_cast<size_t>(restoredIndex);
    } else if (slideIndex >= slideCount) {
      slideIndex = 0;
    }
  } else if (slideIndex >= slideCount) {
    slideIndex = 0;
  }

  if (slideCount) {
    currentSlideFileName = slideFileNames[slideIndex];
  }
  if (slideCount != previousCount) {
    renderDirty = true;
  }
  lastSdScanMs = millis();
  saveAiSlidePrefs();
}

static int jpegScaleOptionForBounds(int width, int height, int maxW, int maxH, int &scaledW, int &scaledH) {
  int option = 0;
  scaledW = width;
  scaledH = height;

  if (scaledW > maxW || scaledH > maxH) {
    option = JPEG_SCALE_HALF;
    scaledW = max(1, width / 2);
    scaledH = max(1, height / 2);
  }
  if (scaledW > maxW || scaledH > maxH) {
    option = JPEG_SCALE_QUARTER;
    scaledW = max(1, width / 4);
    scaledH = max(1, height / 4);
  }
  if (scaledW > maxW || scaledH > maxH) {
    option = JPEG_SCALE_EIGHTH;
    scaledW = max(1, width / 8);
    scaledH = max(1, height / 8);
  }
  return option;
}

static bool drawJpegFromFs(fs::FS &fileSystem, const char *localPath, String *errorOut = nullptr) {
  previewJpegFile = fileSystem.open(localPath, "r");
  if (!previewJpegFile) {
    if (errorOut) {
      *errorOut = "Open fail";
    }
    return false;
  }

  int openResult = previewJpeg.open(
    static_cast<void *>(&previewJpegFile),
    previewJpegFile.size(),
    previewFileClose,
    previewFileRead,
    previewFileSeek,
    jpegDrawCallback
  );
  if (!openResult) {
    int jpegError = previewJpeg.getLastError();
    previewJpegFile.close();
    if (errorOut) {
      *errorOut = "JPEG open " + String(jpegError);
    }
    return false;
  }

  previewJpeg.setPixelType(RGB565_BIG_ENDIAN);
  int scaledW = previewJpeg.getWidth();
  int scaledH = previewJpeg.getHeight();
  int decodeOption = jpegScaleOptionForBounds(
    previewJpeg.getWidth(),
    previewJpeg.getHeight(),
    AI_SLIDE_RENDER_WIDTH,
    AI_SLIDE_RENDER_HEIGHT,
    scaledW,
    scaledH
  );
  int drawX = max(0, (AI_SLIDE_RENDER_WIDTH - scaledW) / 2);
  int drawY = max(0, (AI_SLIDE_RENDER_HEIGHT - scaledH) / 2);

  gfx->fillScreen(COLOR_BG);
  bool ok = previewJpeg.decode(drawX, drawY, decodeOption) != 0;
  previewJpeg.close();
  if (!ok && errorOut) {
    *errorOut = "JPEG decode " + String(previewJpeg.getLastError());
  }
  return ok;
}

static void drawCenteredMessage(const String &title, const String &detail, uint16_t titleColor) {
  gfx->fillScreen(COLOR_BG);
  gfx->setTextColor(titleColor, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(16, 84);
  gfx->print(trimTail(title, 24));
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setCursor(16, 114);
  gfx->print(trimTail(detail, 44));
}

static void drawStatusOverlay() {
  const bool visible = statusUntilMs > millis() || lastErrorMessage.length() || !slideCount;
  if (!visible) {
    return;
  }

  gfx->fillRect(0, 0, 320, 28, COLOR_PANEL);
  gfx->setTextColor(COLOR_ACCENT, COLOR_PANEL);
  gfx->setCursor(8, 10);
  gfx->print("AI");
  gfx->setTextColor(COLOR_TEXT, COLOR_PANEL);
  gfx->setCursor(36, 10);
  if (slideCount) {
    gfx->print(String(slideIndex + 1) + "/" + String(slideCount));
  } else {
    gfx->print("0/0");
  }

  String wifiLabel = WiFi.status() == WL_CONNECTED ? "WiFi" : "No WiFi";
  int wifiX = 320 - (wifiLabel.length() * 6) - 8;
  gfx->setCursor(wifiX, 10);
  gfx->setTextColor(WiFi.status() == WL_CONNECTED ? COLOR_ACCENT : COLOR_WARN, COLOR_PANEL);
  gfx->print(wifiLabel);

  gfx->fillRect(0, 218, 320, 22, COLOR_PANEL);
  gfx->setTextColor(lastErrorMessage.length() ? COLOR_ERROR : COLOR_TEXT, COLOR_PANEL);
  gfx->setCursor(8, 225);
  String footer = lastErrorMessage.length() ? lastErrorMessage : statusMessage;
  if (!footer.length()) {
    footer = slideCount ? trimTail(slideFileNames[slideIndex], 40) : String("Waiting for SD images in /whisplay-ai");
  }
  gfx->print(trimTail(footer, 50));
}

static void renderScreen() {
  if (!slideCount) {
    String detail = ensureSdCardReady()
      ? "Copy JPG/PNG files to /whisplay-ai or wait for sync."
      : "Insert SD card or hold BOOT for setup.";
    drawCenteredMessage("AI slideshow", detail, COLOR_ACCENT);
    drawStatusOverlay();
    lastStatusOverlayVisible = true;
    renderDirty = false;
    return;
  }

  const String fileName = slideFileNames[slideIndex];
  const String path = slidePathForFileName(fileName);
  String drawError;
  if (!slideExistsOnSd(fileName)) {
    lastErrorMessage = "Missing: " + trimTail(fileName, 28);
    drawCenteredMessage("SD image missing", fileName, COLOR_ERROR);
  } else if (isPngFileName(fileName)) {
    if (!ccEnsureWifiConnected()) {
      lastErrorMessage = "PNG needs WiFi render";
      drawCenteredMessage("WiFi needed", trimTail(fileName, 28), COLOR_WARN);
    } else if (
      !downloadFileToSpiffs(remoteCompanionPathForFileName(fileName), PNG_RENDER_CACHE_PATH, &drawError) ||
      !drawJpegFromFs(SPIFFS, PNG_RENDER_CACHE_PATH, &drawError)
    ) {
      lastErrorMessage = drawError.length() ? drawError : String("PNG render failed");
      drawCenteredMessage("PNG render error", trimTail(fileName, 28), COLOR_ERROR);
    } else {
      currentSlideFileName = fileName;
      if (!statusUntilMs || statusUntilMs <= millis()) {
        statusMessage = "";
      }
      if (!lastErrorMessage.startsWith("Sync")) {
        lastErrorMessage = "";
      }
    }
  } else if (!drawJpegFromFs(SD, path.c_str(), &drawError)) {
    lastErrorMessage = drawError.length() ? drawError : String("JPEG draw failed");
    drawCenteredMessage("JPEG error", trimTail(fileName, 28), COLOR_ERROR);
  } else {
    currentSlideFileName = fileName;
    if (!statusUntilMs || statusUntilMs <= millis()) {
      statusMessage = "";
    }
    if (!lastErrorMessage.startsWith("Sync")) {
      lastErrorMessage = "";
    }
  }

  drawStatusOverlay();
  lastStatusOverlayVisible = statusUntilMs > millis() || lastErrorMessage.length() || !slideCount;
  renderDirty = false;
}

static void moveSlide(int delta) {
  if (!slideCount) {
    showStatus("No images on SD yet");
    return;
  }
  int nextIndex = static_cast<int>(slideIndex) + delta;
  while (nextIndex < 0) {
    nextIndex += static_cast<int>(slideCount);
  }
  slideIndex = static_cast<size_t>(nextIndex % static_cast<int>(slideCount));
  currentSlideFileName = slideFileNames[slideIndex];
  lastSlideAdvanceMs = millis();
  statusMessage = trimTail(currentSlideFileName, 40);
  statusUntilMs = millis() + 1200;
  lastErrorMessage = "";
  saveAiSlidePrefs();
  renderDirty = true;
}

static String apiUrlForPath(const String &path) {
  if (path.startsWith("http://") || path.startsWith("https://")) {
    return path;
  }
  if (!cc_pi_host[0]) {
    return String();
  }
  return String("http://") + cc_pi_host + ":" + String(cc_pi_port) + path;
}

static bool fetchGeneratedImagesPage(RemotePage &page, size_t offset, size_t limit, String *errorOut = nullptr) {
  page.count = 0;
  page.total = 0;
  page.hasMore = false;
  if (!cc_pi_host[0]) {
    if (errorOut) {
      *errorOut = "Pi host not set";
    }
    return false;
  }

  const String url = apiUrlForPath(
    String("/api/generated-images?offset=") + String(offset) + "&limit=" + String(limit)
  );
  HTTPClient http;
  http.setConnectTimeout(1200);
  http.setTimeout(1500);
  http.setReuse(false);
  if (!http.begin(url)) {
    if (errorOut) {
      *errorOut = "HTTP begin failed";
    }
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    if (errorOut) {
      *errorOut = "HTTP " + String(code);
    }
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    if (errorOut) {
      *errorOut = String("JSON ") + error.c_str();
    }
    return false;
  }

  page.total = doc["total"] | 0;
  JsonArray photos = doc["photos"].as<JsonArray>();
  for (JsonObject photoObject : photos) {
    if (page.count >= REMOTE_PAGE_SIZE) {
      break;
    }
    RemotePhoto &photo = page.photos[page.count++];
    photo.fileName = String(photoObject["fileName"] | "");
    photo.imageUrl = String(photoObject["imageUrl"] | "");
    photo.companionImageUrl = String(photoObject["companionImageUrl"] | "");
  }
  page.hasMore = (offset + page.count) < page.total;
  return true;
}

static String remotePathForPhoto(const RemotePhoto &photo) {
  if (photo.companionImageUrl.length()) {
    return photo.companionImageUrl +
      "?width=" + String(AI_SLIDE_RENDER_WIDTH) +
      "&height=" + String(AI_SLIDE_RENDER_HEIGHT);
  }
  return photo.imageUrl;
}

static String remoteCompanionPathForFileName(const String &fileName) {
  return String("/api/generated-images/companion/") + fileName +
    "?width=" + String(AI_SLIDE_RENDER_WIDTH) +
    "&height=" + String(AI_SLIDE_RENDER_HEIGHT);
}

static bool downloadFileToSd(const String &remotePath, const String &localPath, String *errorOut = nullptr) {
  if (!ensureSdCardReady()) {
    if (errorOut) {
      *errorOut = "SD not ready";
    }
    return false;
  }
  const String url = apiUrlForPath(remotePath);
  if (!url.length()) {
    if (errorOut) {
      *errorOut = "URL build failed";
    }
    return false;
  }

  String tempPath = localPath + ".tmp";
  SD.remove(tempPath.c_str());
  File target = SD.open(tempPath.c_str(), "w");
  if (!target) {
    if (errorOut) {
      *errorOut = "Open temp failed";
    }
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(6000);
  http.setReuse(false);
  if (!http.begin(url)) {
    target.close();
    SD.remove(tempPath.c_str());
    if (errorOut) {
      *errorOut = "HTTP begin failed";
    }
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    target.close();
    SD.remove(tempPath.c_str());
    http.end();
    if (errorOut) {
      *errorOut = "HTTP " + String(code);
    }
    return false;
  }

  size_t written = http.writeToStream(&target);
  target.close();
  http.end();
  if (written == 0) {
    SD.remove(tempPath.c_str());
    if (errorOut) {
      *errorOut = "Write failed";
    }
    return false;
  }

  SD.remove(localPath.c_str());
  if (!SD.rename(tempPath.c_str(), localPath.c_str())) {
    SD.remove(tempPath.c_str());
    if (errorOut) {
      *errorOut = "Rename failed";
    }
    return false;
  }
  return true;
}

static bool downloadFileToSpiffs(const String &remotePath, const char *localPath, String *errorOut) {
  const String url = apiUrlForPath(remotePath);
  if (!url.length()) {
    if (errorOut) {
      *errorOut = "URL build failed";
    }
    return false;
  }

  String tempPath = String(localPath) + ".tmp";
  SPIFFS.remove(tempPath.c_str());
  File target = SPIFFS.open(tempPath.c_str(), "w");
  if (!target) {
    if (errorOut) {
      *errorOut = "Open temp failed";
    }
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(6000);
  http.setReuse(false);
  if (!http.begin(url)) {
    target.close();
    SPIFFS.remove(tempPath.c_str());
    if (errorOut) {
      *errorOut = "HTTP begin failed";
    }
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    target.close();
    SPIFFS.remove(tempPath.c_str());
    http.end();
    if (errorOut) {
      *errorOut = "HTTP " + String(code);
    }
    return false;
  }

  size_t written = http.writeToStream(&target);
  target.close();
  http.end();
  if (written == 0) {
    SPIFFS.remove(tempPath.c_str());
    if (errorOut) {
      *errorOut = "Write failed";
    }
    return false;
  }

  SPIFFS.remove(localPath);
  if (!SPIFFS.rename(tempPath.c_str(), localPath)) {
    SPIFFS.remove(tempPath.c_str());
    if (errorOut) {
      *errorOut = "Rename failed";
    }
    return false;
  }
  return true;
}

static void syncSlidesIfDue(bool force = false) {
  if (!ccEnsureWifiConnected() || !ensureSdCardReady()) {
    return;
  }

  unsigned long now = millis();
  if (!force && now - lastSyncStepMs < AI_SYNC_STEP_MS) {
    return;
  }
  lastSyncStepMs = now;

  RemotePage page;
  String syncError;
  if (!fetchGeneratedImagesPage(page, syncOffset, REMOTE_PAGE_SIZE, &syncError)) {
    lastErrorMessage = "Sync: " + trimTail(syncError, 34);
    renderDirty = true;
    return;
  }

  bool pageComplete = true;
  bool changed = false;
  for (size_t i = 0; i < page.count; i++) {
    const RemotePhoto &photo = page.photos[i];
    if (!photo.fileName.length() || !isJpegFileName(photo.fileName) || slideExistsOnSd(photo.fileName)) {
      continue;
    }

    String downloadError;
    if (!downloadFileToSd(remotePathForPhoto(photo), slidePathForFileName(photo.fileName), &downloadError)) {
      lastErrorMessage = "Sync: " + trimTail(downloadError, 34);
      renderDirty = true;
      return;
    }

    changed = true;
    pageComplete = false;
    break;
  }

  if (changed) {
    scanSlidesFromSd();
  }

  if (pageComplete) {
    if (page.hasMore && page.count > 0) {
      syncOffset += page.count;
    } else {
      syncOffset = 0;
    }
  }
}

static void handleTouch() {
  bool touched = ts.touched();
  if (!touched) {
    touchWasDown = false;
    return;
  }
  if (touchWasDown || millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) {
    return;
  }

  TS_Point point = ts.getPoint();
  int screenX = 0;
  int screenY = 0;
  mapTouch(point.x, point.y, screenX, screenY);
  touchWasDown = true;
  lastTouchMs = millis();

  if (screenX < 107) {
    moveSlide(-1);
    return;
  }
  if (screenX > 213) {
    moveSlide(1);
    return;
  }

  scanSlidesFromSd();
  syncSlidesIfDue(true);
  showStatus("Refresh triggered");
}

static void openSetupPortal() {
  drawCenteredMessage("Opening setup", "Connect to the CYD AP to configure WiFi.", COLOR_WARN);
  delay(400);
  ccRunPortal();
}

static void handleBootButton() {
  const bool pressed = digitalRead(BOOT_BTN) == LOW;
  if (pressed) {
    if (!bootButtonWasDown) {
      bootButtonWasDown = true;
      bootPortalOpened = false;
      lastBootHoldStartMs = millis();
    } else if (!bootPortalOpened && millis() - lastBootHoldStartMs >= BOOT_PORTAL_HOLD_MS) {
      bootPortalOpened = true;
      openSetupPortal();
    }
    return;
  }

  if (!bootButtonWasDown) {
    return;
  }

  bool shortPress = !bootPortalOpened &&
    lastBootHoldStartMs > 0 &&
    millis() - lastBootHoldStartMs < BOOT_PORTAL_HOLD_MS;
  bootButtonWasDown = false;
  bootPortalOpened = false;
  lastBootHoldStartMs = 0;
  if (shortPress) {
    moveSlide(1);
  }
}

static void advanceSlideIfDue() {
  if (!slideCount) {
    return;
  }
  unsigned long now = millis();
  if (lastSlideAdvanceMs != 0 && now - lastSlideAdvanceMs < AI_SLIDE_ADVANCE_MS) {
    return;
  }
  moveSlide(1);
}

void setup() {
  Serial.begin(115200);
  pinMode(GFX_BL, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  digitalWrite(GFX_BL, HIGH);

  gfx->begin();
  gfx->fillScreen(COLOR_BG);
  gfx->setTextColor(COLOR_ACCENT, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(18, 84);
  gfx->print("Whisplay CYD");
  gfx->setTextSize(1);
  gfx->setCursor(82, 108);
  gfx->print("AI Slideshow");

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  SPIFFS.begin(true);
  loadAiSlidePrefs();
  ensureSdCardReady();
  scanSlidesFromSd();

  bool forcePortal = digitalRead(BOOT_BTN) == LOW;
  ccConnect(forcePortal);
  analogWrite(GFX_BL, cc_brightness);

  lastSlideAdvanceMs = millis();
  showStatus(slideCount ? trimTail(slideFileNames[slideIndex], 40) : String("Waiting for AI images"));
  renderScreen();
}

void loop() {
  handleBootButton();
  handleTouch();
  ccEnsureWifiConnected();

  unsigned long now = millis();
  if (ensureSdCardReady() && (lastSdScanMs == 0 || now - lastSdScanMs >= SD_SCAN_MS)) {
    scanSlidesFromSd();
  }

  advanceSlideIfDue();
  syncSlidesIfDue();

  bool statusVisible = statusUntilMs > now || lastErrorMessage.length() || !slideCount;
  if (!statusVisible && lastStatusOverlayVisible) {
    renderDirty = true;
  }

  if (renderDirty) {
    renderScreen();
  }
}
