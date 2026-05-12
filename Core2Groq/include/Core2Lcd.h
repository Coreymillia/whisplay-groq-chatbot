#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "Portal.h"

static constexpr uint8_t C2_LCD_COLS = 16;
static constexpr uint8_t C2_LCD_ROWS = 2;
static constexpr uint8_t C2_LCD_SDA_PIN = 33;
static constexpr uint8_t C2_LCD_SCL_PIN = 32;
static constexpr uint8_t C2_LCD_BACKLIGHT = 0x08;
static constexpr uint8_t C2_LCD_ENABLE = 0x04;
static constexpr uint8_t C2_LCD_RW = 0x02;
static constexpr uint8_t C2_LCD_RS = 0x01;

static TwoWire &c2_lcd_wire = Wire;
static int c2_lcd_address = -1;
static bool c2_lcd_ready = false;
static bool c2_lcd_backlight = true;
static bool c2_lcd_probe_attempted = false;
static String c2_lcd_status_line = "";
static String c2_lcd_message_source = "";
static unsigned long c2_lcd_last_scroll_ms = 0;
static unsigned long c2_lcd_last_status_ms = 0;
static size_t c2_lcd_scroll_offset = 0;
static unsigned long c2_lcd_probe_after_ms = 0;

static String c2PadOrTrimLcd(const String &value) {
    String text = value;
    if (text.length() > C2_LCD_COLS) {
        text = text.substring(0, C2_LCD_COLS);
    }
    while (text.length() < C2_LCD_COLS) {
        text += ' ';
    }
    return text;
}

static void c2LcdWriteRaw(uint8_t value) {
    if (c2_lcd_address < 0) return;
    c2_lcd_wire.beginTransmission(c2_lcd_address);
    c2_lcd_wire.write(value | (c2_lcd_backlight ? C2_LCD_BACKLIGHT : 0x00));
    c2_lcd_wire.endTransmission();
}

static void c2LcdPulse(uint8_t value) {
    c2LcdWriteRaw(value | C2_LCD_ENABLE);
    delayMicroseconds(1);
    c2LcdWriteRaw(value & ~C2_LCD_ENABLE);
    delayMicroseconds(50);
}

static void c2LcdWrite4Bits(uint8_t value) {
    c2LcdWriteRaw(value);
    c2LcdPulse(value);
}

static void c2LcdSend(uint8_t value, uint8_t mode) {
    uint8_t high = (value & 0xF0) | mode;
    uint8_t low = ((value << 4) & 0xF0) | mode;
    c2LcdWrite4Bits(high);
    c2LcdWrite4Bits(low);
}

static void c2LcdCommand(uint8_t value) {
    c2LcdSend(value, 0);
}

static void c2LcdWriteChar(uint8_t value) {
    c2LcdSend(value, C2_LCD_RS);
}

static void c2LcdSetCursor(uint8_t col, uint8_t row) {
    static const uint8_t rowOffsets[] = {0x00, 0x40};
    c2LcdCommand(0x80 | (col + rowOffsets[min<uint8_t>(row, C2_LCD_ROWS - 1)]));
}

static void c2LcdPrint(const String &value) {
    for (size_t i = 0; i < value.length(); i++) {
        c2LcdWriteChar(static_cast<uint8_t>(value.charAt(i)));
    }
}

static int c2DetectLcdAddress() {
    const uint8_t candidates[] = {0x27, 0x3F};
    for (uint8_t address : candidates) {
        c2_lcd_wire.beginTransmission(address);
        if (c2_lcd_wire.endTransmission() == 0) {
            return address;
        }
    }
    return -1;
}

static void c2InitLcd() {
    c2_lcd_wire.setTimeOut(10);
    c2_lcd_wire.begin(C2_LCD_SDA_PIN, C2_LCD_SCL_PIN);
    c2_lcd_address = c2DetectLcdAddress();
    if (c2_lcd_address < 0) {
        c2_lcd_ready = false;
        return;
    }

    delay(50);
    c2LcdWrite4Bits(0x30);
    delay(5);
    c2LcdWrite4Bits(0x30);
    delayMicroseconds(150);
    c2LcdWrite4Bits(0x30);
    c2LcdWrite4Bits(0x20);

    c2LcdCommand(0x28);
    c2LcdCommand(0x0C);
    c2LcdCommand(0x06);
    c2LcdCommand(0x01);
    delay(2);

    c2_lcd_ready = true;
    c2_lcd_last_scroll_ms = 0;
    c2_lcd_last_status_ms = 0;
    c2_lcd_status_line = "";
    c2_lcd_message_source = "";
    c2_lcd_scroll_offset = 0;

    c2LcdSetCursor(0, 0);
    c2LcdPrint("Core2Groq LCD   ");
    c2LcdSetCursor(0, 1);
    c2LcdPrint("Booting...      ");
}

static void c2ScheduleLcdInit(unsigned long delayMs = 4000) {
    c2_lcd_probe_attempted = false;
    c2_lcd_probe_after_ms = millis() + delayMs;
}

static void c2EnsureLcdInit() {
    if (c2_lcd_ready || c2_lcd_probe_attempted) return;
    if (millis() < c2_lcd_probe_after_ms) return;
    c2_lcd_probe_attempted = true;
    c2InitLcd();
}

static bool c2IsLcdReady() {
    return c2_lcd_ready;
}

static String c2BuildLcdStatusLine(bool radioMode, bool recordingActive,
                                   unsigned long recordingStartedMs, uint8_t recordSeconds) {
    String line;
    if (recordingActive) {
        unsigned long elapsedTenths = (millis() - recordingStartedMs) / 100;
        line = "REC ";
        line += String(elapsedTenths / 10);
        line += ".";
        line += String(elapsedTenths % 10);
        line += "/";
        line += String(recordSeconds);
        line += "s";
    } else if (radioMode) {
        line = WiFi.status() == WL_CONNECTED ? "Radio WiFi OK" : "Radio No WiFi";
    } else if (WiFi.status() == WL_CONNECTED) {
        line = "Bot ";
        if (String(rd_groq_model).startsWith("llama-3.1")) {
            line += "L31";
        } else if (String(rd_groq_model).startsWith("llama-3.3")) {
            line += "L33";
        } else if (String(rd_groq_model).startsWith("qwen/")) {
            line += "QWN";
        } else if (String(rd_groq_model).startsWith("groq/")) {
            line += "CMP";
        } else if (String(rd_groq_model).startsWith("openai/")) {
            line += "GPT";
        } else {
            line += "Ready";
        }
    } else {
        line = "Bot No WiFi";
    }
    return c2PadOrTrimLcd(line);
}

static void c2SetLcdMessage(const String &message) {
    String normalized = message;
    normalized.replace("\r", " ");
    normalized.replace("\n", " ");
    normalized.trim();
    if (!normalized.length()) {
        normalized = "Waiting...";
    }
    if (normalized != c2_lcd_message_source) {
        c2_lcd_message_source = normalized;
        c2_lcd_scroll_offset = 0;
        c2_lcd_last_scroll_ms = 0;
    }
}

static String c2CurrentLcdMessageWindow() {
    String source = c2_lcd_message_source.length() ? c2_lcd_message_source : String("Waiting...");
    if (source.length() <= C2_LCD_COLS) {
        return c2PadOrTrimLcd(source);
    }

    String marquee = source + "   ";
    if (c2_lcd_scroll_offset >= marquee.length()) {
        c2_lcd_scroll_offset = 0;
    }
    String doubled = marquee + marquee;
    return doubled.substring(c2_lcd_scroll_offset, c2_lcd_scroll_offset + C2_LCD_COLS);
}

static void c2UpdateLcd(bool radioMode, bool recordingActive,
                        unsigned long recordingStartedMs, uint8_t recordSeconds) {
    if (!c2_lcd_ready) return;

    unsigned long now = millis();
    String nextStatus =
        c2BuildLcdStatusLine(radioMode, recordingActive, recordingStartedMs, recordSeconds);
    if (nextStatus != c2_lcd_status_line || now - c2_lcd_last_status_ms >= 1000) {
        c2LcdSetCursor(0, 0);
        c2LcdPrint(nextStatus);
        c2_lcd_status_line = nextStatus;
        c2_lcd_last_status_ms = now;
    }

    if (c2_lcd_message_source.length() > C2_LCD_COLS &&
        (c2_lcd_last_scroll_ms == 0 || now - c2_lcd_last_scroll_ms >= rd_scroll_ms)) {
        c2_lcd_scroll_offset += 1;
        c2_lcd_last_scroll_ms = now;
    }

    c2LcdSetCursor(0, 1);
    c2LcdPrint(c2CurrentLcdMessageWindow());
}
