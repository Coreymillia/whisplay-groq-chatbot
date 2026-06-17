#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "AppSettings.h"
#include "AppModes.h"
#include "SetupPortal.h"

namespace GroqWatch {

static constexpr uint16_t SCREEN_TIMEOUT_OPTIONS[] = {0, 15, 30, 60, 120, 300, 600};

struct SettingsTile {
    int16_t x, y, w, h;
    const char *label;
    String value;
    uint16_t accent;
    int index;
};

class SettingsMenu {
public:
    static constexpr int kCols = 2;
    static constexpr int kRows = 4;
    static constexpr int kTileW = 190;
    static constexpr int kTileH = 100;
    static constexpr int kGap = 8;
    static constexpr int kGridX = 8;
    static constexpr int kGridY = 34;

    void begin(AppSettings &settings, AppMode &currentMode) {
        settings_ = &settings;
        mode_ = &currentMode;
        watchSettingsOpen_ = false;
        dirty_ = true;
        buildTiles();
    }

    bool inWatchSettings() const { return watchSettingsOpen_; }

    void openWatchSettings() {
        watchSettingsOpen_ = true;
        buildWatchTiles();
        dirty_ = true;
    }

    void closeWatchSettings() {
        watchSettingsOpen_ = false;
        buildTiles();
        dirty_ = true;
    }

    void draw(Arduino_GFX &gfx) {
        if (!dirty_) return;
        gfx.fillScreen(RGB565_BLACK);

        if (watchSettingsOpen_) {
            gfx.setCursor(12, 8);
            gfx.setTextColor(RGB565_CYAN, RGB565_BLACK);
            gfx.setTextSize(2);
            gfx.print("WATCH SETTINGS");
            for (int i = 0; i < kCols * kRows; i++) {
                if (tile_[i].label[0]) drawTile(gfx, tile_[i]);
            }
        } else {
            gfx.setCursor(12, 8);
            gfx.setTextColor(RGB565_CYAN, RGB565_BLACK);
            gfx.setTextSize(2);
            gfx.print("SETTINGS");
            gfx.setCursor(240, 8);
            gfx.setTextColor(RGB565_YELLOW, RGB565_BLACK);
            gfx.setTextSize(1);
            gfx.print("tap tile to act");
            for (int i = 0; i < kCols * kRows; i++) {
                if (tile_[i].label[0]) drawTile(gfx, tile_[i]);
            }
        }
        dirty_ = false;
    }

    int tileAtPoint(uint16_t px, uint16_t py) {
        for (int i = 0; i < kCols * kRows; i++) {
            const auto &t = tile_[i];
            if (!t.label[0]) continue;
            if (px >= (uint16_t)t.x && px <= (uint16_t)(t.x + t.w) &&
                py >= (uint16_t)t.y && py <= (uint16_t)(t.y + t.h))
                return t.index;
        }
        return -1;
    }

    void rebuild() {
        if (watchSettingsOpen_) buildWatchTiles();
        else buildTiles();
        dirty_ = true;
    }

private:
    AppSettings *settings_;
    AppMode *mode_;
    SettingsTile tile_[kCols * kRows];
    bool dirty_ = false;
    bool watchSettingsOpen_ = false;

    static int findTimeoutIndex(uint16_t val) {
        for (int i = 0; i < 7; i++)
            if (SCREEN_TIMEOUT_OPTIONS[i] == val) return i;
        return 1;
    }

    void buildTiles() {
        for (int i = 0; i < kCols * kRows; i++) {
            tile_[i].label = "";  // blank by default
            tile_[i].index = i;
            int col = i % kCols;
            int row = i / kCols;
            tile_[i].x = kGridX + col * (kTileW + kGap);
            tile_[i].y = kGridY + row * (kTileH + kGap);
            tile_[i].w = kTileW;
            tile_[i].h = kTileH;
        }

        // 0: SETUP
        tile_[0].label = "SETUP"; tile_[0].value = "AP portal"; tile_[0].accent = RGB565_CYAN;
        // 1: FACE
        tile_[1].label = "FACE"; tile_[1].value = kWatchFaceNames[settings_->watchFace]; tile_[1].accent = gfx_Color565(120, 220, 255);
        // 2: WATCH
        tile_[2].label = "WATCH"; tile_[2].value = "Launch now"; tile_[2].accent = gfx_Color565(180, 140, 255);
        // 3: BOOT
        tile_[3].label = "BOOT"; tile_[3].value = settings_->bootMode; tile_[3].accent = gfx_Color565(255, 100, 40);
        // 4: BOT
        tile_[4].label = "BOT"; tile_[4].value = hasWhisplayUrl(*settings_) ? "Launch now" : "Needs URL";
        tile_[4].accent = hasWhisplayUrl(*settings_) ? RGB565_GREEN : RGB565_RED;
        // 5: AI
        tile_[5].label = "AI"; tile_[5].value = (hasWhisplayUrl(*settings_) || aiCanRunOffline()) ? "Launch now" : "Needs URL";
        tile_[5].accent = (hasWhisplayUrl(*settings_) || aiCanRunOffline()) ? RGB565_CYAN : RGB565_RED;
        // 6: empty
        // 7: BACK
        tile_[7].label = "BACK"; tile_[7].value = "Close"; tile_[7].accent = RGB565_WHITE;
    }

    void buildWatchTiles() {
        for (int i = 0; i < kCols * kRows; i++) {
            tile_[i].label = "";
            tile_[i].index = i;
            int col = i % kCols;
            int row = i / kCols;
            tile_[i].x = kGridX + col * (kTileW + kGap);
            tile_[i].y = kGridY + row * (kTileH + kGap);
            tile_[i].w = kTileW;
            tile_[i].h = kTileH;
        }

        // Sleep is intentionally disabled until wake is proven 100% reliable.
        // Leave this submenu effectively empty except for BACK.

        // 0: unused
        // 1: unused

        // 4: (spare — draw empty for layout)
        // 5: (spare)

        // 6: (spare)

        // 7: BACK
        tile_[7].label = "BACK"; tile_[7].value = "Settings"; tile_[7].accent = RGB565_WHITE;
    }

    void drawTile(Arduino_GFX &gfx, const SettingsTile &t) {
        if (!t.label[0]) return;
        gfx.fillRoundRect(t.x, t.y, t.w, t.h, 8, 0x1082);
        gfx.drawRoundRect(t.x, t.y, t.w, t.h, 8, t.accent);
        gfx.setTextColor(t.accent, 0x1082);
        gfx.setTextSize(2);
        int16_t lx = t.x + (t.w - strlen(t.label) * 12) / 2;
        gfx.setCursor(lx, t.y + 12);
        gfx.print(t.label);
        gfx.setTextColor(RGB565_WHITE, 0x1082);
        gfx.setTextSize(2);
        gfx.setCursor(t.x + 8, t.y + 52);
        gfx.print(t.value.c_str());
    }

    static String shorten(const char *str, int maxLen) {
        String s = str;
        if (s.length() <= (size_t)maxLen) return s;
        return s.substring(0, maxLen - 1) + ".";
    }

    static uint16_t gfx_Color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
};

}  // namespace GroqWatch
