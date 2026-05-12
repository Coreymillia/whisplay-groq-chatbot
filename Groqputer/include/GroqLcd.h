#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>

#include "GroqPortal.h"

static constexpr uint8_t GP_LCD_COLS = 16;
static constexpr uint8_t GP_LCD_ROWS = 2;
static constexpr uint8_t GP_LCD_SDA_PIN = 1;
static constexpr uint8_t GP_LCD_SCL_PIN = 2;

static LiquidCrystal_I2C *gp_lcd = nullptr;
static bool gp_lcd_ready = false;
static String gp_lcd_status_line = "";
static String gp_lcd_message_source = "";
static unsigned long gp_lcd_last_scroll_ms = 0;
static unsigned long gp_lcd_last_status_ms = 0;
static size_t gp_lcd_scroll_offset = 0;

static String gpPadOrTrimLcd(const String &value) {
  String text = value;
  if (text.length() > GP_LCD_COLS) {
    text = text.substring(0, GP_LCD_COLS);
  }
  while (text.length() < GP_LCD_COLS) {
    text += ' ';
  }
  return text;
}

static int gpDetectLcdAddress() {
  const uint8_t candidates[] = {0x27, 0x3F};
  for (uint8_t address : candidates) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      return address;
    }
  }
  return -1;
}

static void gpInitLcd() {
  Wire.begin(GP_LCD_SDA_PIN, GP_LCD_SCL_PIN);
  int address = gpDetectLcdAddress();
  if (address < 0) {
    gp_lcd_ready = false;
    return;
  }

  gp_lcd = new LiquidCrystal_I2C(address, GP_LCD_COLS, GP_LCD_ROWS);
  gp_lcd->init();
  gp_lcd->backlight();
  gp_lcd->clear();
  gp_lcd->setCursor(0, 0);
  gp_lcd->print("Groqputer LCD  ");
  gp_lcd->setCursor(0, 1);
  gp_lcd->print("Booting...      ");
  gp_lcd_ready = true;
  if (gp_lcd_backlight_enabled) {
    gp_lcd->backlight();
  } else {
    gp_lcd->noBacklight();
  }
  gp_lcd_last_scroll_ms = millis();
  gp_lcd_last_status_ms = 0;
}

static bool gpIsLcdReady() {
  return gp_lcd_ready;
}

static String gpBuildLcdStatusLine(
  bool recordingActive,
  unsigned long recordingStartedMs,
  uint8_t recordSeconds
) {
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
  } else if (WiFi.status() == WL_CONNECTED) {
    line = "WiFi OK ";
    String model = gp_model[0] ? gp_model : GP_DEFAULT_MODEL;
    if (model.startsWith("llama-3.1")) {
      line += "L31";
    } else if (model.startsWith("llama-3.3")) {
      line += "L33";
    } else if (model.startsWith("qwen/")) {
      line += "QWN";
    } else if (model.startsWith("groq/")) {
      line += "CMP";
    } else if (model.startsWith("openai/")) {
      line += "GPT";
    } else {
      line += "BOT";
    }
  } else {
    line = "No WiFi Setup";
  }
  return gpPadOrTrimLcd(line);
}

static void gpSetLcdIncomingMessage(const String &message) {
  String normalized = message;
  normalized.replace("\r", " ");
  normalized.replace("\n", " ");
  normalized.trim();
  if (!normalized.length()) {
    normalized = "Waiting for reply...";
  }
  if (normalized != gp_lcd_message_source) {
    gp_lcd_message_source = normalized;
    gp_lcd_scroll_offset = 0;
    gp_lcd_last_scroll_ms = 0;
  }
}

static String gpCurrentLcdMessageWindow() {
  String source = gp_lcd_message_source.length()
    ? gp_lcd_message_source
    : String("Waiting for reply...");
  if (source.length() <= GP_LCD_COLS) {
    return gpPadOrTrimLcd(source);
  }

  String marquee = source + "   ";
  if (gp_lcd_scroll_offset >= marquee.length()) {
    gp_lcd_scroll_offset = 0;
  }

  String doubled = marquee + marquee;
  return doubled.substring(gp_lcd_scroll_offset, gp_lcd_scroll_offset + GP_LCD_COLS);
}

static void gpUpdateLcd(
  bool recordingActive,
  unsigned long recordingStartedMs,
  uint8_t recordSeconds
) {
  if (!gp_lcd_ready || !gp_lcd) return;

  if (gp_lcd_backlight_enabled) {
    gp_lcd->backlight();
  } else {
    gp_lcd->noBacklight();
  }

  unsigned long now = millis();
  String nextStatus = gpBuildLcdStatusLine(recordingActive, recordingStartedMs, recordSeconds);
  if (nextStatus != gp_lcd_status_line || now - gp_lcd_last_status_ms >= 1000) {
    gp_lcd->setCursor(0, 0);
    gp_lcd->print(nextStatus);
    gp_lcd_status_line = nextStatus;
    gp_lcd_last_status_ms = now;
  }

  bool advanceScroll = false;
  if (gp_lcd_message_source.length() > GP_LCD_COLS) {
    if (gp_lcd_last_scroll_ms == 0 || now - gp_lcd_last_scroll_ms >= gp_lcd_scroll_ms) {
      advanceScroll = true;
      gp_lcd_last_scroll_ms = now;
    }
  }
  if (advanceScroll) {
    gp_lcd_scroll_offset += 1;
  }

  gp_lcd->setCursor(0, 1);
  gp_lcd->print(gpCurrentLcdMessageWindow());
}
