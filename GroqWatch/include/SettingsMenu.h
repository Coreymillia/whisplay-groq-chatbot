#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "AppSettings.h"
#include "AppModes.h"
#include "SetupPortal.h"

namespace GroqWatch {

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
        dirty_ = true;
        buildTiles();
    }

    void draw(Arduino_GFX &gfx) {
        if (!dirty_) return;
        gfx.fillScreen(RGB565_BLACK);

        // Header
        gfx.setCursor(12, 8);
        gfx.setTextColor(RGB565_CYAN, RGB565_BLACK);
        gfx.setTextSize(2);
        gfx.print("SETTINGS");

        gfx.setCursor(280, 8);
        gfx.setTextColor(RGB565_YELLOW, RGB565_BLACK);
        gfx.setTextSize(1);
        gfx.print("tap tile to act");

        for (int i = 0; i < kCols * kRows; i++) {
            drawTile(gfx, tile_[i]);
        }
        dirty_ = false;
    }

    int tileAtPoint(uint16_t px, uint16_t py) {
        for (int i = 0; i < kCols * kRows; i++) {
            const auto &t = tile_[i];
            if (px >= (uint16_t)t.x && px <= (uint16_t)(t.x + t.w) &&
                py >= (uint16_t)t.y && py <= (uint16_t)(t.y + t.h))
                return t.index;
        }
        return -1;
    }

    void rebuild() {
        buildTiles();
        dirty_ = true;
    }

private:
    AppSettings *settings_;
    AppMode *mode_;
    SettingsTile tile_[kCols * kRows];
    bool dirty_ = false;

    void buildTiles() {
        for (int i = 0; i < kCols * kRows; i++) {
            int col = i % kCols;
            int row = i / kCols;
            tile_[i].x = kGridX + col * (kTileW + kGap);
            tile_[i].y = kGridY + row * (kTileH + kGap);
            tile_[i].w = kTileW;
            tile_[i].h = kTileH;
            tile_[i].index = i;
        }

        // 0: SETUP
        tile_[0].label = "SETUP";
        tile_[0].value = "AP portal";
        tile_[0].accent = RGB565_CYAN;

        // 1: MODE
        tile_[1].label = "MODE";
        tile_[1].value = modeLabel(*mode_);
        tile_[1].accent = RGB565_GREEN;

        // 2: PERSONA
        tile_[2].label = "PERSONA";
        tile_[2].value = personaShortLabel();
        tile_[2].accent = gfx_Color565(255, 180, 50);

        // 3: MODEL
        tile_[3].label = "MODEL";
        tile_[3].value = shorten(settings_->model, 14);
        tile_[3].accent = gfx_Color565(255, 215, 0);

        // 4: BOOT
        tile_[4].label = "BOOT";
        tile_[4].value = settings_->bootMode;
        tile_[4].accent = gfx_Color565(255, 100, 40);

        // 5: LAUNCH
        tile_[5].label = "LAUNCH";
        tile_[5].value = modeLabel(bootModeFromString(settings_->bootMode));
        tile_[5].accent = RGB565_GREEN;

        // 6: AI SHOW
        tile_[6].label = "AI SHOW";
        tile_[6].value = hasWhisplayUrl(*settings_) ? "Launch now" : "Needs URL";
        tile_[6].accent = hasWhisplayUrl(*settings_) ? RGB565_CYAN : RGB565_RED;

        // 7: BACK
        tile_[7].label = "BACK";
        tile_[7].value = modeLabel(*mode_);
        tile_[7].accent = RGB565_WHITE;
    }

    void drawTile(Arduino_GFX &gfx, const SettingsTile &t) {
        gfx.fillRoundRect(t.x, t.y, t.w, t.h, 8, 0x1082);
        gfx.drawRoundRect(t.x, t.y, t.w, t.h, 8, t.accent);
        gfx.setTextColor(t.accent, 0x1082);
        gfx.setTextSize(2);
        int16_t lx = t.x + (t.w - strlen(t.label) * 12) / 2;
        gfx.setCursor(lx, t.y + 12);
        gfx.print(t.label);
        gfx.setTextColor(RGB565_WHITE, 0x1082);
        gfx.setTextSize(1);
        gfx.setCursor(t.x + 8, t.y + 44);
        gfx.print(t.value.c_str());
    }

    const char *personaShortLabel() {
        if (!settings_->personaPrompt[0]) return "Default";
        String p = settings_->personaPrompt;
        if (p.indexOf("concise") >= 0 && p.indexOf("practical") >= 0) return "Neutral";
        if (p.indexOf("warm") >= 0) return "Friendly";
        if (p.indexOf("cranky") >= 0) return "Cranky";
        if (p.indexOf("roast") >= 0) return "Roast";
        if (p.indexOf("sleepy") >= 0 || p.indexOf("overworked") >= 0) return "Sleepy";
        if (p.indexOf("coach") >= 0 || p.indexOf("supportive") >= 0) return "Coach";
        if (p.indexOf("philosopher") >= 0) return "Philos.";
        if (p.indexOf("oracle") >= 0 || p.indexOf("mythic") >= 0) return "Oracle";
        if (p.indexOf("joke") >= 0) return "Joke";
        if (p.indexOf("tutor") >= 0) return "Tutor";
        if (p.indexOf("detective") >= 0) return "Detectv";
        if (p.indexOf("zen") >= 0) return "Zen";
        return "Custom";
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
