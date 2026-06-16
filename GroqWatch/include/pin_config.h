#pragma once

#define XPOWERS_CHIP_AXP2101

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_RESET 8
#define LCD_WIDTH 410
#define LCD_HEIGHT 502

// Touch + shared I2C bus
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 38
#define TP_RESET 9

// SD pins kept for later phases
static constexpr int SDMMC_CLK = 2;
static constexpr int SDMMC_CMD = 1;
static constexpr int SDMMC_DATA = 3;
static constexpr int SDMMC_CS = 17;

// Optional BOOT/strap button for forcing setup portal.
// Also used as PTT (push-to-talk) in Bot mode.
static constexpr int WATCH_BOOT_BUTTON_PIN = 0;

// ── Audio / ES8311 codec ──────────────────────────────────────────────
// Use the exact pin order from Waveshare's 08_ES8311 demo:
//   i2s.setPins(41, 45, 40, 42, 16)
// Signature is setPins(bclk, ws, dout, din, mclk)
static constexpr int I2S_BCLK = 41;
static constexpr int I2S_LRCK = 45;
static constexpr int I2S_DOUT = 40;
static constexpr int I2S_DIN  = 42;
static constexpr int I2S_MCLK = 16;
static constexpr int ES8311_EN = 46; // codec enable (HIGH = active)
static constexpr int ES8311_I2C_ADDR = 0x18;
