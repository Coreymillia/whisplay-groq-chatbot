#pragma once

#include <algorithm>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <img_converters.h>
#include <jpeg_decoder.h>
#include <JPEGDEC.h>
#include <vector>

#include "AppSettings.h"
#include "pin_config.h"

static constexpr unsigned long AI_LIST_REFRESH_MS = 30000;
static constexpr unsigned long AI_SLIDE_INTERVAL_MS = 18000;
static constexpr size_t AI_MAX_IMAGE_BYTES = 300 * 1024;
static constexpr size_t AI_MAX_DECODE_BYTES = 200 * 1024;
static constexpr char AI_CACHE_DIR[] = "/ai-cache";
static constexpr uint64_t AI_CACHE_MIN_FREE_BYTES = 256ULL * 1024ULL * 1024ULL;
static constexpr uint64_t AI_CACHE_UNKNOWN_WRITE_BUDGET = 1024ULL * 1024ULL;
static constexpr int AI_RENDER_W = 410;
static constexpr int AI_RENDER_H = 502;
static constexpr int AI_FETCH_W = AI_RENDER_W;
static constexpr int AI_FETCH_H = 502;
static constexpr bool AI_SWAP_COLOR_BYTES = false;
static constexpr bool AI_DRAW_BIG_ENDIAN = false;

struct AiSlideState {
    String fileName;
    String cachePath;
    uint8_t *buffer = nullptr;
    size_t bufferLen = 0;
    bool fromSd = false;
    uint16_t *decoded565 = nullptr;
    uint16_t decodedW = 0;
    uint16_t decodedH = 0;
};

struct AiCacheEntry {
    String fileName;
    String path;
    size_t sizeBytes = 0;
    time_t lastWrite = 0;
};

static std::vector<String> aiRemoteFiles;
static std::vector<AiCacheEntry> aiCachedFiles;
static int aiSlideIndex = -1;
static int aiCachedSlideIndex = -1;
static unsigned long aiLastListRefreshMs = 0;
static unsigned long aiLastSlideMs = 0;
static unsigned long aiLastCacheScanMs = 0;
static String aiStatus = "No AI images loaded.";
static String aiLastError;
static AiSlideState aiCurrentSlide;
static bool aiSdChecked = false;
static bool aiSdReady = false;
static bool aiNeedsRedraw = true;
static bool aiRecoveryInProgress = false;
static String aiLastBaseUrl;
static uint64_t aiSdTotalBytes = 0;
static uint64_t aiSdUsedBytes = 0;
static uint16_t aiScaleLine[AI_RENDER_W];
static JPEGDEC aiJpeg;
static File aiJpegFile;
static Arduino_GFX *aiJpegGfx = nullptr;
static int aiJpegX = 0;
static int aiJpegY = 0;
static int aiJpegXBound = 0;
static int aiJpegYBound = 0;

static void aiMarkNeedsRedraw() { aiNeedsRedraw = true; }
static void aiAckRedraw() { aiNeedsRedraw = false; }

static String aiUrlEncode(const String &value) {
    String out;
    out.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); i++) {
        unsigned char c = (unsigned char)value.charAt(i);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

static String aiNormalizeUrl(const String &raw) {
    String u = raw;
    u.trim();
    if (!u.startsWith("http://") && !u.startsWith("https://")) u = "http://" + u;
    while (u.endsWith("/")) u.remove(u.length() - 1);
    return u;
}

static String aiBaseName(const String &path) {
    const int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.substring(slash + 1) : path;
}

static bool aiIsJpegFileName(const String &name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".jpg") || lower.endsWith(".jpeg");
}

static void aiRefreshSdStats() {
    if (!aiSdReady) {
        aiSdTotalBytes = 0;
        aiSdUsedBytes = 0;
        return;
    }
    aiSdTotalBytes = SD_MMC.totalBytes();
    aiSdUsedBytes = SD_MMC.usedBytes();
}

static void aiFreeDecoded() {
    if (aiCurrentSlide.decoded565) free(aiCurrentSlide.decoded565);
    aiCurrentSlide.decoded565 = nullptr;
    aiCurrentSlide.decodedW = 0;
    aiCurrentSlide.decodedH = 0;
}

static void aiClearCurrentSlide() {
    if (aiCurrentSlide.buffer) free(aiCurrentSlide.buffer);
    aiCurrentSlide.buffer = nullptr;
    aiCurrentSlide.bufferLen = 0;
    aiCurrentSlide.fileName = "";
    aiCurrentSlide.cachePath = "";
    aiCurrentSlide.fromSd = false;
    aiFreeDecoded();
    aiMarkNeedsRedraw();
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
    if (sanitized.length() > 60) sanitized = sanitized.substring(0, 60);
    return sanitized;
}

static String aiCachePathForFile(const String &fileName) {
    return String(AI_CACHE_DIR) + "/" + aiSanitizeCacheName(fileName);
}

static bool aiRefreshCachedFiles() {
    aiCachedFiles.clear();
    if (!aiSdReady) return false;

    File dir = SD_MMC.open(AI_CACHE_DIR);
    if (!dir || !dir.isDirectory()) return false;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            const String fullPath = String(file.name());
            const String fileName = aiBaseName(fullPath);
            if (aiIsJpegFileName(fileName)) {
                AiCacheEntry entry;
                entry.fileName = fileName;
                entry.path = fullPath;
                entry.sizeBytes = (size_t)file.size();
                entry.lastWrite = file.getLastWrite();
                aiCachedFiles.push_back(entry);
            }
        }
        file = dir.openNextFile();
    }

    std::sort(aiCachedFiles.begin(), aiCachedFiles.end(), [](const AiCacheEntry &a, const AiCacheEntry &b) {
        if (a.lastWrite == b.lastWrite) return a.fileName < b.fileName;
        return a.lastWrite > b.lastWrite;
    });
    aiLastCacheScanMs = millis();
    return !aiCachedFiles.empty();
}

static bool aiEnsureSdCache() {
    if (aiSdChecked) return aiSdReady;
    aiSdChecked = true;

    Serial.println("[AI] checking SD cache...");
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
    if (!SD_MMC.begin("/sdcard", true, false)) {
        aiSdReady = false;
        Serial.println("[AI] SD_MMC begin failed");
        return false;
    }
    if (SD_MMC.cardType() == CARD_NONE) {
        aiSdReady = false;
        Serial.println("[AI] no SD card detected");
        return false;
    }
    if (!SD_MMC.exists(AI_CACHE_DIR)) SD_MMC.mkdir(AI_CACHE_DIR);
    aiSdReady = SD_MMC.exists(AI_CACHE_DIR);
    aiRefreshSdStats();
    aiRefreshCachedFiles();
    aiMarkNeedsRedraw();
    Serial.printf("[AI] SD cache %s, total=%lluMB used=%lluMB files=%u\n",
                  aiSdReady ? "ready" : "unavailable",
                  (unsigned long long)(aiSdTotalBytes / (1024ULL * 1024ULL)),
                  (unsigned long long)(aiSdUsedBytes / (1024ULL * 1024ULL)),
                  (unsigned)aiCachedFiles.size());
    return aiSdReady;
}

static bool aiHasOfflineCache() {
    if (!aiEnsureSdCache()) return false;
    if (aiCachedFiles.empty() || millis() - aiLastCacheScanMs > AI_LIST_REFRESH_MS) aiRefreshCachedFiles();
    return !aiCachedFiles.empty();
}

static bool aiEnsureFreeSpaceForCache(uint64_t incomingBytes, String &error) {
    error = "";
    if (!aiEnsureSdCache()) {
        error = "SD cache unavailable.";
        return false;
    }

    aiRefreshSdStats();
    uint64_t freeBytes = aiSdTotalBytes > aiSdUsedBytes ? (aiSdTotalBytes - aiSdUsedBytes) : 0;
    const uint64_t requiredFree = AI_CACHE_MIN_FREE_BYTES + incomingBytes;
    if (freeBytes >= requiredFree) return true;

    aiRefreshCachedFiles();
    std::vector<AiCacheEntry> oldestFirst = aiCachedFiles;
    std::sort(oldestFirst.begin(), oldestFirst.end(), [](const AiCacheEntry &a, const AiCacheEntry &b) {
        if (a.lastWrite == b.lastWrite) return a.fileName < b.fileName;
        return a.lastWrite < b.lastWrite;
    });

    for (const auto &entry : oldestFirst) {
        if (aiCurrentSlide.fromSd && entry.path == aiCurrentSlide.cachePath) continue;
        Serial.println(String("[AI] pruning cached slide: ") + entry.path);
        SD_MMC.remove(entry.path);
        aiRefreshSdStats();
        freeBytes = aiSdTotalBytes > aiSdUsedBytes ? (aiSdTotalBytes - aiSdUsedBytes) : 0;
        if (freeBytes >= requiredFree) {
            aiRefreshCachedFiles();
            return true;
        }
    }

    aiRefreshCachedFiles();
    error = "SD cache full.";
    return false;
}

static bool aiFetchRemoteList(const char *baseUrl, String &error) {
    error = "";
    std::vector<String> nextFiles;
    String reqUrl = aiNormalizeUrl(String(baseUrl)) + "/api/generated-images";
    Serial.println(String("[AI] fetching list: ") + reqUrl);
    HTTPClient http;
    http.begin(reqUrl);
    http.setTimeout(7000);
    int code = http.GET();
    if (code != 200) {
        error = code > 0 ? "HTTP " + String(code) : "List failed.";
        Serial.println(String("[AI] list fetch failed: ") + error);
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["photos"][0]["fileName"] = true;

    JsonDocument doc;
    DeserializationError jsonErr = deserializeJson(doc, *http.getStreamPtr(), DeserializationOption::Filter(filter));
    http.end();
    if (jsonErr) {
        error = String("List parse failed: ") + jsonErr.c_str();
        return false;
    }

    JsonArray photos = doc["photos"].as<JsonArray>();
    if (photos.isNull()) {
        error = "No image list.";
        return false;
    }

    nextFiles.reserve(photos.size());
    for (JsonVariant pv : photos) {
        JsonObject po = pv.as<JsonObject>();
        String fn = String(po["fileName"] | "");
        if (fn.length()) nextFiles.push_back(fn);
    }
    if (nextFiles.empty()) {
        error = "No AI images yet.";
        return false;
    }

    aiRemoteFiles.swap(nextFiles);
    aiLastListRefreshMs = millis();
    Serial.printf("[AI] remote photo count=%u\n", (unsigned)aiRemoteFiles.size());
    return true;
}

static bool aiSetCurrentSlideFromMemory(const String &fileName, uint8_t *buffer, size_t length) {
    aiClearCurrentSlide();
    aiCurrentSlide.fileName = fileName;
    aiCurrentSlide.buffer = buffer;
    aiCurrentSlide.bufferLen = length;
    aiCurrentSlide.fromSd = false;
    aiLastSlideMs = millis();
    aiStatus = "Showing AI gallery";
    aiLastError = "";
    aiMarkNeedsRedraw();
    Serial.println(String("[AI] slide ready from RAM: ") + fileName);
    return true;
}

static bool aiSetCurrentSlideFromSd(const String &fileName, const String &cachePath) {
    aiClearCurrentSlide();
    aiCurrentSlide.fileName = fileName;
    aiCurrentSlide.cachePath = cachePath;
    aiCurrentSlide.fromSd = true;
    aiLastSlideMs = millis();
    aiStatus = "Showing AI gallery";
    aiLastError = "";
    aiMarkNeedsRedraw();
    Serial.println(String("[AI] slide ready from SD: ") + cachePath);
    return true;
}

static bool aiBuildCompanionUrl(const char *baseUrl, size_t index, String &reqUrl, String &error) {
    error = "";
    if (index >= aiRemoteFiles.size()) {
        error = "Index out of range.";
        return false;
    }
    reqUrl = aiNormalizeUrl(String(baseUrl)) + "/api/generated-images/companion/" +
             aiUrlEncode(aiRemoteFiles[index]) + "?width=" + String(AI_FETCH_W) + "&height=" + String(AI_FETCH_H);
    return true;
}

static bool aiDownloadSlideToMemory(const char *baseUrl, size_t index, String &error) {
    error = "";
    String reqUrl;
    if (!aiBuildCompanionUrl(baseUrl, index, reqUrl, error)) return false;

    const String &fn = aiRemoteFiles[index];
    Serial.println(String("[AI] downloading to RAM: ") + fn);

    HTTPClient http;
    http.begin(reqUrl);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) {
        error = code > 0 ? "HTTP " + String(code) : "Download failed.";
        http.end();
        return false;
    }

    int contentLen = http.getSize();
    size_t cap = contentLen > 0 ? (size_t)contentLen : 32768;
    if (cap > AI_MAX_IMAGE_BYTES) {
        http.end();
        error = "Too large.";
        return false;
    }

    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) {
        http.end();
        error = "Alloc failed.";
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t total = 0;
    unsigned long last = millis();
    while (http.connected() && (contentLen < 0 || (int)total < contentLen || stream->available())) {
        size_t avail = stream->available();
        if (!avail) {
            if (millis() - last > 2000) break;
            delay(1);
            continue;
        }
        last = millis();
        if (total + avail > cap) {
            size_t nc = cap;
            while (total + avail > nc) nc *= 2;
            if (nc > AI_MAX_IMAGE_BYTES) {
                free(buf);
                http.end();
                error = "Exceeded cap.";
                return false;
            }
            uint8_t *g = (uint8_t *)realloc(buf, nc);
            if (!g) {
                free(buf);
                http.end();
                error = "Realloc failed.";
                return false;
            }
            buf = g;
            cap = nc;
        }
        size_t hint = contentLen > 0 ? (size_t)(contentLen - (int)total) : avail;
        size_t r = stream->readBytes(buf + total, min(avail, hint));
        if (!r) break;
        total += r;
    }
    http.end();

    if (!total || (contentLen > 0 && (int)total != contentLen)) {
        free(buf);
        error = "Read failed.";
        return false;
    }

    aiSlideIndex = (int)index;
    return aiSetCurrentSlideFromMemory(fn, buf, total);
}

static bool aiDownloadSlideToSd(const char *baseUrl, size_t index, String &error) {
    error = "";
    if (index >= aiRemoteFiles.size()) {
        error = "Index out of range.";
        return false;
    }

    const String &fn = aiRemoteFiles[index];
    const String cachePath = aiCachePathForFile(fn);
    if (aiEnsureSdCache() && SD_MMC.exists(cachePath)) {
        aiSlideIndex = (int)index;
        aiRefreshCachedFiles();
        return aiSetCurrentSlideFromSd(fn, cachePath);
    }

    String reqUrl;
    if (!aiBuildCompanionUrl(baseUrl, index, reqUrl, error)) return false;
    Serial.println(String("[AI] downloading to SD: ") + fn + " -> " + cachePath);

    HTTPClient http;
    http.begin(reqUrl);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) {
        error = code > 0 ? "HTTP " + String(code) : "Download failed.";
        http.end();
        return false;
    }

    const int expectedSize = http.getSize();
    String spaceError;
    if (!aiEnsureFreeSpaceForCache(expectedSize > 0 ? (uint64_t)expectedSize : AI_CACHE_UNKNOWN_WRITE_BUDGET,
                                   spaceError)) {
        http.end();
        error = spaceError.length() ? spaceError : "SD low space.";
        return false;
    }

    SD_MMC.remove(cachePath);
    File file = SD_MMC.open(cachePath, FILE_WRITE);
    if (!file) {
        http.end();
        error = "SD open failed.";
        return false;
    }

    const int written = http.writeToStream(&file);
    file.close();
    http.end();

    size_t finalSize = 0;
    File verify = SD_MMC.open(cachePath, FILE_READ);
    if (verify) {
        finalSize = verify.size();
        verify.close();
    }

    const bool ok = written >= 0 && finalSize > 0 &&
                    (expectedSize < 0 || (written == expectedSize && (int)finalSize == expectedSize));
    if (!ok) {
        SD_MMC.remove(cachePath);
        error = "SD write failed.";
        return false;
    }

    aiRefreshSdStats();
    aiRefreshCachedFiles();
    aiSlideIndex = (int)index;
    return aiSetCurrentSlideFromSd(fn, cachePath);
}

static bool aiShowNextCachedSlide() {
    if (!aiHasOfflineCache()) {
        aiStatus = aiSdReady ? "No AI images on SD yet." : "No SD card detected.";
        aiLastError = aiStatus;
        aiMarkNeedsRedraw();
        return false;
    }

    if (aiCurrentSlide.fromSd && aiCurrentSlide.cachePath.length()) {
        for (size_t i = 0; i < aiCachedFiles.size(); i++) {
            if (aiCachedFiles[i].path == aiCurrentSlide.cachePath) {
                aiCachedSlideIndex = (int)i;
                break;
            }
        }
    }

    size_t next = aiCachedFiles.empty() ? 0 : (size_t)((aiCachedSlideIndex + 1 + (int)aiCachedFiles.size()) % (int)aiCachedFiles.size());
    for (size_t attempt = 0; attempt < aiCachedFiles.size(); attempt++) {
        const auto &entry = aiCachedFiles[next];
        if (SD_MMC.exists(entry.path)) {
            aiCachedSlideIndex = (int)next;
            aiStatus = "Showing AI gallery (offline)";
            aiLastError = "";
            return aiSetCurrentSlideFromSd(entry.fileName, entry.path);
        }
        next = (next + 1) % aiCachedFiles.size();
    }

    aiStatus = "No AI images on SD yet.";
    aiLastError = aiStatus;
    aiMarkNeedsRedraw();
    return false;
}

static bool aiShowNextSlide(const char *baseUrl, bool forceRefresh = false) {
    const bool wifiUp = WiFi.status() == WL_CONNECTED;
    String err;

    if (baseUrl && baseUrl[0]) aiLastBaseUrl = aiNormalizeUrl(String(baseUrl));
    aiEnsureSdCache();

    if (wifiUp && baseUrl && baseUrl[0]) {
        if (forceRefresh || aiRemoteFiles.empty() || millis() - aiLastListRefreshMs >= AI_LIST_REFRESH_MS) {
            if (!aiFetchRemoteList(baseUrl, err)) {
                Serial.println(String("[AI] remote list unavailable, falling back to SD: ") + err);
                if (aiShowNextCachedSlide()) return true;
                aiClearCurrentSlide();
                aiStatus = err.length() ? err : "No AI images.";
                aiLastError = aiStatus;
                aiMarkNeedsRedraw();
                return false;
            }
        }

        if (!aiRemoteFiles.empty()) {
            size_t next = (size_t)((aiSlideIndex + 1) % (int)aiRemoteFiles.size());
            for (size_t attempt = 0; attempt < aiRemoteFiles.size(); attempt++) {
                if (aiDownloadSlideToSd(baseUrl, next, err)) return true;
                Serial.println(String("[AI] SD download failed, trying RAM fallback: ") + err);
                if (aiDownloadSlideToMemory(baseUrl, next, err)) return true;
                next = (next + 1) % aiRemoteFiles.size();
            }
        }
    }

    if (aiShowNextCachedSlide()) return true;

    aiClearCurrentSlide();
    aiStatus = err.length() ? err : (aiSdReady ? "No AI images on SD yet." : "No AI images.");
    aiLastError = aiStatus;
    aiMarkNeedsRedraw();
    return false;
}

static bool aiCanRunOffline() {
    return aiHasOfflineCache();
}

static String aiCacheSummary() {
    if (!aiSdReady) return "SD: none";
    aiRefreshSdStats();
    const uint64_t freeBytes = aiSdTotalBytes > aiSdUsedBytes ? (aiSdTotalBytes - aiSdUsedBytes) : 0;
    return String("SD files:") + aiCachedFiles.size() + " free:" +
           String((unsigned long)(freeBytes / (1024ULL * 1024ULL))) + "MB";
}

static void *aiJpegOpenFile(const char *szFilename, int32_t *pFileSize) {
    aiJpegFile = SD_MMC.open(szFilename, FILE_READ);
    if (!aiJpegFile) {
        *pFileSize = 0;
        return nullptr;
    }
    *pFileSize = (int32_t)aiJpegFile.size();
    return &aiJpegFile;
}

static void aiJpegCloseFile(void *pHandle) {
    File *f = static_cast<File *>(pHandle);
    if (f) f->close();
}

static int32_t aiJpegReadFile(JPEGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    File *f = static_cast<File *>(pFile->fHandle);
    return f ? (int32_t)f->read(pBuf, iLen) : 0;
}

static int32_t aiJpegSeekFile(JPEGFILE *pFile, int32_t iPosition) {
    File *f = static_cast<File *>(pFile->fHandle);
    if (!f) return 0;
    f->seek(iPosition);
    return iPosition;
}

static int aiJpegDrawCallback(JPEGDRAW *pDraw) {
    if (!aiJpegGfx) return 0;
    if (AI_DRAW_BIG_ENDIAN) {
        aiJpegGfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    } else {
        aiJpegGfx->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    }
    return 1;
}

static bool aiRenderCurrentSlideJpegDec(Arduino_GFX &gfx, String &error) {
    error = "";
    aiJpegGfx = &gfx;
    aiJpegX = 0;
    aiJpegY = 0;
    aiJpegXBound = AI_RENDER_W - 1;
    aiJpegYBound = AI_RENDER_H - 1;

    bool opened = false;
    if (aiCurrentSlide.fromSd && aiCurrentSlide.cachePath.length()) {
        Serial.println(String("[AI] JPEGDEC open SD: ") + aiCurrentSlide.cachePath);
        opened = aiJpeg.open(aiCurrentSlide.cachePath.c_str(), aiJpegOpenFile, aiJpegCloseFile,
                             aiJpegReadFile, aiJpegSeekFile, aiJpegDrawCallback);
    } else if (aiCurrentSlide.buffer && aiCurrentSlide.bufferLen) {
        Serial.printf("[AI] JPEGDEC open RAM: %s bytes=%u\n", aiCurrentSlide.fileName.c_str(), (unsigned)aiCurrentSlide.bufferLen);
        opened = aiJpeg.openRAM(aiCurrentSlide.buffer, aiCurrentSlide.bufferLen, aiJpegDrawCallback);
    }

    if (!opened) {
        error = "JPEGDEC open failed.";
        return false;
    }

    int width = aiJpeg.getWidth();
    int height = aiJpeg.getHeight();
    float ratio = (float)height / AI_RENDER_H;
    int scale;
    int maxMCUs;
    if (ratio <= 1) {
        scale = 0;
        maxMCUs = max(26, (width + 15) / 16);
    } else if (ratio <= 2) {
        scale = JPEG_SCALE_HALF;
        maxMCUs = max(52, (width + 7) / 8);
    } else if (ratio <= 4) {
        scale = JPEG_SCALE_QUARTER;
        maxMCUs = max(103, (width + 3) / 4);
    } else {
        scale = JPEG_SCALE_EIGHTH;
        maxMCUs = max(206, (width + 1) / 2);
    }

    aiJpeg.setMaxOutputSize(maxMCUs);
    if (AI_DRAW_BIG_ENDIAN) aiJpeg.setPixelType(RGB565_BIG_ENDIAN);

    const int scaledW = scale == JPEG_SCALE_HALF ? (width / 2) :
                        scale == JPEG_SCALE_QUARTER ? (width / 4) :
                        scale == JPEG_SCALE_EIGHTH ? (width / 8) : width;
    const int scaledH = scale == JPEG_SCALE_HALF ? (height / 2) :
                        scale == JPEG_SCALE_QUARTER ? (height / 4) :
                        scale == JPEG_SCALE_EIGHTH ? (height / 8) : height;
    const int drawX = max(0, (AI_RENDER_W - scaledW) / 2);
    const int drawY = max(0, (AI_RENDER_H - scaledH) / 2);

    gfx.fillScreen(RGB565_BLACK);
    Serial.printf("[AI] JPEGDEC decode %s src=%dx%d scale=%d draw=%d,%d scaled=%dx%d\n",
                  aiCurrentSlide.fileName.c_str(), width, height, scale, drawX, drawY, scaledW, scaledH);
    int decodeResult = aiJpeg.decode(drawX, drawY, scale);
    aiJpeg.close();

    if (decodeResult == 0) {
        error = "JPEGDEC decode failed.";
        return false;
    }

    Serial.printf("[AI] JPEGDEC rendered %s\n", aiCurrentSlide.fileName.c_str());
    return true;
}

static void aiDrawCurrentSlide(Arduino_GFX &gfx) {
    String decodeError;
    if (!aiRenderCurrentSlideJpegDec(gfx, decodeError)) {
        const bool recoverableCacheIssue =
            decodeError.startsWith("SD file") ||
            decodeError.startsWith("JPEG read") ||
            decodeError.startsWith("Bad JPG hdr") ||
            decodeError.startsWith("JPEGDEC open");

        if (!aiRecoveryInProgress && recoverableCacheIssue && aiCurrentSlide.fromSd && aiCurrentSlide.cachePath.length() &&
            aiLastBaseUrl.length() && WiFi.status() == WL_CONNECTED && aiSlideIndex >= 0) {
            aiRecoveryInProgress = true;
            Serial.println(String("[AI] purging bad cached slide and retrying: ") + aiCurrentSlide.cachePath + " reason=" + decodeError);
            SD_MMC.remove(aiCurrentSlide.cachePath);
            aiRefreshCachedFiles();
            aiClearCurrentSlide();
            String retryError;
            if (aiDownloadSlideToSd(aiLastBaseUrl.c_str(), (size_t)aiSlideIndex, retryError)) {
                aiRecoveryInProgress = false;
                aiDrawCurrentSlide(gfx);
                return;
            }
            Serial.println(String("[AI] retry download failed: ") + retryError);
            decodeError = retryError.length() ? retryError : decodeError;
            aiRecoveryInProgress = false;
        }

        gfx.fillScreen(RGB565_BLACK);
        gfx.setTextColor(RGB565_RED, RGB565_BLACK);
        gfx.setTextSize(3);
        gfx.setCursor(26, 120);
        gfx.print("DECODE FAIL");
        gfx.setTextColor(RGB565_WHITE, RGB565_BLACK);
        gfx.setTextSize(2);
        gfx.setCursor(20, 180);
        gfx.print(aiCurrentSlide.fromSd ? "Source: SD" : "Source: RAM");
        gfx.setCursor(20, 215);
        gfx.print(aiCurrentSlide.fileName.substring(0, 24).c_str());
        gfx.setCursor(20, 250);
        gfx.print(decodeError.substring(0, 26).c_str());
        gfx.setTextSize(1);
        gfx.setCursor(20, 300);
        gfx.print(aiCacheSummary().c_str());
        aiLastError = decodeError;
        aiAckRedraw();
        return;
    }

    aiAckRedraw();
}
