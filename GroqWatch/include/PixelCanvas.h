#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

class PixelCanvas {
public:
    static constexpr uint16_t kLogicalWidth = 82;
    static constexpr uint16_t kLogicalHeight = 100;
    static constexpr uint8_t kScale = 5;
    static constexpr uint16_t kOffsetX = 0;
    static constexpr uint16_t kOffsetY = 1;
    static constexpr uint16_t kBlack = 0x0000;

    PixelCanvas() {
        clear(kBlack);
    }

    void clear(uint16_t color) {
        for (uint32_t i = 0; i < kLogicalWidth * kLogicalHeight; i++) {
            pixels_[i] = color;
        }
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x < 0 || y < 0 || x >= static_cast<int16_t>(kLogicalWidth) ||
            y >= static_cast<int16_t>(kLogicalHeight)) {
            return;
        }
        pixels_[y * kLogicalWidth + x] = color;
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        if (w <= 0 || h <= 0) return;
        for (int16_t yy = 0; yy < h; yy++) {
            for (int16_t xx = 0; xx < w; xx++) {
                drawPixel(x + xx, y + yy, color);
            }
        }
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        if (w <= 0 || h <= 0) return;
        fillRect(x, y, w, 1, color);
        fillRect(x, y + h - 1, w, 1, color);
        fillRect(x, y, 1, h, color);
        fillRect(x + w - 1, y, 1, h, color);
    }

    void drawText(const String &text, int16_t x, int16_t y, uint16_t color,
                  uint8_t scale = 1, int8_t spacing = 1) {
        int16_t cursorX = x;
        for (size_t i = 0; i < text.length(); i++) {
            const char c = text.charAt(i);
            drawChar(c, cursorX, y, color, scale);
            cursorX += (glyphWidth(c) * scale) + spacing;
        }
    }

    int16_t textWidth(const String &text, uint8_t scale = 1, int8_t spacing = 1) const {
        if (!text.length()) return 0;
        int16_t width = 0;
        for (size_t i = 0; i < text.length(); i++) {
            width += (glyphWidth(text.charAt(i)) * scale);
            if (i + 1 < text.length()) width += spacing;
        }
        return width;
    }

    void drawTextCentered(const String &text, int16_t centerX, int16_t y, uint16_t color,
                          uint8_t scale = 1, int8_t spacing = 1) {
        drawText(text, centerX - textWidth(text, scale, spacing) / 2, y, color, scale, spacing);
    }

    void render(Arduino_GFX &gfx) {
        gfx.startWrite();
        gfx.fillRect(0, 0, kLogicalWidth * kScale, 1, kBlack);
        gfx.fillRect(0, kOffsetY + (kLogicalHeight * kScale), kLogicalWidth * kScale, 1, kBlack);

        for (uint16_t y = 0; y < kLogicalHeight; y++) {
            uint16_t runColor = pixels_[y * kLogicalWidth];
            uint16_t runStart = 0;
            for (uint16_t x = 1; x <= kLogicalWidth; x++) {
                const bool boundary = (x == kLogicalWidth);
                const uint16_t color = boundary ? 0xFFFF : pixels_[y * kLogicalWidth + x];
                if (boundary || color != runColor) {
                    const uint16_t runLen = x - runStart;
                    gfx.fillRect(kOffsetX + runStart * kScale,
                                 kOffsetY + y * kScale,
                                 runLen * kScale,
                                 kScale,
                                 runColor);
                    runStart = x;
                    runColor = color;
                }
            }
        }
        gfx.endWrite();
    }

private:
    uint16_t pixels_[kLogicalWidth * kLogicalHeight];

    static uint8_t glyphWidth(char c) {
        switch (c) {
            case ':': return 1;
            case 'I': return 1;
            default: return 3;
        }
    }

    void drawChar(char raw, int16_t x, int16_t y, uint16_t color, uint8_t scale) {
        char c = static_cast<char>(toupper(static_cast<unsigned char>(raw)));
        const char *rows[5] = {"000", "000", "000", "000", "000"};
        switch (c) {
            case '0': rows[0] = "111"; rows[1] = "101"; rows[2] = "101"; rows[3] = "101"; rows[4] = "111"; break;
            case '1': rows[0] = "010"; rows[1] = "110"; rows[2] = "010"; rows[3] = "010"; rows[4] = "111"; break;
            case '2': rows[0] = "111"; rows[1] = "001"; rows[2] = "111"; rows[3] = "100"; rows[4] = "111"; break;
            case '3': rows[0] = "111"; rows[1] = "001"; rows[2] = "111"; rows[3] = "001"; rows[4] = "111"; break;
            case '4': rows[0] = "101"; rows[1] = "101"; rows[2] = "111"; rows[3] = "001"; rows[4] = "001"; break;
            case '5': rows[0] = "111"; rows[1] = "100"; rows[2] = "111"; rows[3] = "001"; rows[4] = "111"; break;
            case '6': rows[0] = "111"; rows[1] = "100"; rows[2] = "111"; rows[3] = "101"; rows[4] = "111"; break;
            case '7': rows[0] = "111"; rows[1] = "001"; rows[2] = "010"; rows[3] = "010"; rows[4] = "010"; break;
            case '8': rows[0] = "111"; rows[1] = "101"; rows[2] = "111"; rows[3] = "101"; rows[4] = "111"; break;
            case '9': rows[0] = "111"; rows[1] = "101"; rows[2] = "111"; rows[3] = "001"; rows[4] = "111"; break;
            case 'A': rows[0] = "010"; rows[1] = "101"; rows[2] = "111"; rows[3] = "101"; rows[4] = "101"; break;
            case 'B': rows[0] = "110"; rows[1] = "101"; rows[2] = "110"; rows[3] = "101"; rows[4] = "110"; break;
            case 'E': rows[0] = "111"; rows[1] = "100"; rows[2] = "110"; rows[3] = "100"; rows[4] = "111"; break;
            case 'F': rows[0] = "111"; rows[1] = "100"; rows[2] = "110"; rows[3] = "100"; rows[4] = "100"; break;
            case 'H': rows[0] = "101"; rows[1] = "101"; rows[2] = "111"; rows[3] = "101"; rows[4] = "101"; break;
            case 'I': rows[0] = "1"; rows[1] = "1"; rows[2] = "1"; rows[3] = "1"; rows[4] = "1"; break;
            case 'M': rows[0] = "101"; rows[1] = "111"; rows[2] = "111"; rows[3] = "101"; rows[4] = "101"; break;
            case 'N': rows[0] = "101"; rows[1] = "111"; rows[2] = "111"; rows[3] = "111"; rows[4] = "101"; break;
            case 'O': rows[0] = "111"; rows[1] = "101"; rows[2] = "101"; rows[3] = "101"; rows[4] = "111"; break;
            case 'P': rows[0] = "111"; rows[1] = "101"; rows[2] = "111"; rows[3] = "100"; rows[4] = "100"; break;
            case 'R': rows[0] = "110"; rows[1] = "101"; rows[2] = "110"; rows[3] = "101"; rows[4] = "101"; break;
            case 'S': rows[0] = "111"; rows[1] = "100"; rows[2] = "111"; rows[3] = "001"; rows[4] = "111"; break;
            case 'T': rows[0] = "111"; rows[1] = "010"; rows[2] = "010"; rows[3] = "010"; rows[4] = "010"; break;
            case 'U': rows[0] = "101"; rows[1] = "101"; rows[2] = "101"; rows[3] = "101"; rows[4] = "111"; break;
            case 'W': rows[0] = "101"; rows[1] = "101"; rows[2] = "111"; rows[3] = "111"; rows[4] = "101"; break;
            case 'Y': rows[0] = "101"; rows[1] = "101"; rows[2] = "010"; rows[3] = "010"; rows[4] = "010"; break;
            case ':': rows[0] = "0"; rows[1] = "1"; rows[2] = "0"; rows[3] = "1"; rows[4] = "0"; break;
            case '-': rows[0] = "000"; rows[1] = "000"; rows[2] = "111"; rows[3] = "000"; rows[4] = "000"; break;
            default: break;
        }

        for (uint8_t row = 0; row < 5; row++) {
            const size_t rowLen = strlen(rows[row]);
            for (uint8_t col = 0; col < rowLen; col++) {
                if (rows[row][col] != '1') continue;
                fillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
};
