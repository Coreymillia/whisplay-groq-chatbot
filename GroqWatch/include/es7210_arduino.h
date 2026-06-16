#pragma once

#include <Arduino.h>
#include <Wire.h>

// ── Minimal Arduino ES7210 mic ADC driver ──────────────────────────────
// Extracted from esp_codec_dev/device/es7210/es7210.c
// I2C: 7-bit address 0x40 (ESP-ADF uses 8-bit 0x80)

namespace Es7210 {

static constexpr uint8_t I2C_ADDR = 0x40;

enum Reg : uint8_t {
    CHIP_RESET = 0x00, CLOCK_OFF_7210 = 0x01, MAINCLK = 0x02, MASTER_CLK = 0x03,
    LRCK_DIVH = 0x04, LRCK_DIVL = 0x05, PWR_DOWN = 0x06, OSR = 0x07,
    MODE_CONFIG = 0x08, TIME_CTRL0 = 0x09, TIME_CTRL1 = 0x0A,
    SDP_INTERFACE1 = 0x11, SDP_INTERFACE2 = 0x12,
    ADC_AUTOMUTE = 0x13, ADC34_MUTERANGE = 0x14,
    ADC34_HPF2 = 0x20, ADC34_HPF1 = 0x21, ADC12_HPF1 = 0x22, ADC12_HPF2 = 0x23,
    ANA_CTRL = 0x40, MIC12_BIAS = 0x41, MIC34_BIAS = 0x42,
    MIC1_GAIN = 0x43, MIC2_GAIN = 0x44, MIC3_GAIN = 0x45, MIC4_GAIN = 0x46,
    MIC1_POWER = 0x47, MIC2_POWER = 0x48, MIC3_POWER = 0x49, MIC4_POWER = 0x4A,
    MIC12_POWER = 0x4B, MIC34_POWER = 0x4C,
};

enum Gain : uint8_t {
    GAIN_0DB = 0, GAIN_3DB = 1, GAIN_6DB = 2, GAIN_9DB = 3,
    GAIN_12DB = 4, GAIN_15DB = 5, GAIN_18DB = 6, GAIN_21DB = 7,
    GAIN_24DB = 8, GAIN_27DB = 9, GAIN_30DB = 10,
    GAIN_33DB = 11, GAIN_34_5DB = 12, GAIN_36DB = 13, GAIN_37_5DB = 14,
};

inline bool g_init = false;
inline uint8_t g_clockOffSaved = 0x3F;

inline bool writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}
inline bool readReg(uint8_t reg, uint8_t &val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(I2C_ADDR, (uint8_t)1) != 1) return false;
    val = Wire.read();
    return true;
}
inline bool updateReg(uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t v = 0;
    if (!readReg(reg, v)) return false;
    v = (v & ~mask) | (mask & val);
    return writeReg(reg, v);
}

inline bool enableMics(uint8_t gain) {
    bool ok = true;
    // Match Waveshare/ESP-GMF more closely: MIC1 + MIC2 + MIC3 enabled
    ok &= writeReg(Reg::MIC12_POWER, 0xFF);
    ok &= writeReg(Reg::MIC34_POWER, 0xFF);
    ok &= updateReg(Reg::CLOCK_OFF_7210, 0x0B, 0x00);
    ok &= updateReg(Reg::CLOCK_OFF_7210, 0x15, 0x00);

    // MIC1
    ok &= writeReg(Reg::MIC12_POWER, 0x00);
    ok &= updateReg(Reg::MIC1_GAIN, 0x10, 0x10);
    ok &= updateReg(Reg::MIC1_GAIN, 0x0F, gain);

    // MIC2
    ok &= updateReg(Reg::MIC2_GAIN, 0x10, 0x10);
    ok &= updateReg(Reg::MIC2_GAIN, 0x0F, gain);

    // MIC3
    ok &= writeReg(Reg::MIC34_POWER, 0x00);
    ok &= updateReg(Reg::MIC3_GAIN, 0x10, 0x10);
    ok &= updateReg(Reg::MIC3_GAIN, 0x0F, gain);

    // MIC4 off
    ok &= updateReg(Reg::MIC4_GAIN, 0x10, 0x00);
    return ok;
}

inline bool begin() {
    if (g_init) return true;
    bool ok = true;

    ok &= writeReg(Reg::CHIP_RESET, 0xFF); delay(1);
    ok &= writeReg(Reg::CHIP_RESET, 0x41); delay(1);
    ok &= writeReg(Reg::CLOCK_OFF_7210, 0x3F);
    ok &= writeReg(Reg::TIME_CTRL0, 0x30);
    ok &= writeReg(Reg::TIME_CTRL1, 0x30);
    ok &= writeReg(Reg::ADC12_HPF2, 0x2A);
    ok &= writeReg(Reg::ADC12_HPF1, 0x0A);
    ok &= writeReg(Reg::ADC34_HPF2, 0x0A);
    ok &= writeReg(Reg::ADC34_HPF1, 0x2A);
    ok &= writeReg(Reg::MODE_CONFIG, 0x00);
    ok &= writeReg(Reg::ANA_CTRL, 0x43);
    ok &= writeReg(Reg::MIC12_BIAS, 0x70);
    ok &= writeReg(Reg::MIC34_BIAS, 0x70);
    ok &= writeReg(Reg::OSR, 0x20);
    ok &= writeReg(Reg::MAINCLK, 0xC1);
    ok &= enableMics(Gain::GAIN_30DB);
    // 3 enabled mics also triggers ES7210 TDM framing in Espressif's driver
    ok &= writeReg(Reg::SDP_INTERFACE2, 0x02);
    ok &= writeReg(Reg::SDP_INTERFACE1, 0x60);
    readReg(Reg::CLOCK_OFF_7210, g_clockOffSaved);

    if (!ok) { Serial.println("[ES7210] I2C init FAILED"); return false; }

    ok &= writeReg(Reg::CLOCK_OFF_7210, g_clockOffSaved);
    ok &= writeReg(Reg::PWR_DOWN, 0x00);
    ok &= writeReg(Reg::ANA_CTRL, 0x43);
    ok &= writeReg(Reg::MIC1_POWER, 0x08);
    ok &= writeReg(Reg::MIC2_POWER, 0x08);
    ok &= writeReg(Reg::MIC3_POWER, 0x08);
    ok &= writeReg(Reg::MIC4_POWER, 0x08);
    ok &= enableMics(Gain::GAIN_30DB);
    ok &= writeReg(Reg::ANA_CTRL, 0x43);
    ok &= writeReg(Reg::CHIP_RESET, 0x71); delay(1);
    ok &= writeReg(Reg::CHIP_RESET, 0x41);

    if (!ok) { Serial.println("[ES7210] Start FAILED"); return false; }
    // Readback verification
    {
        uint8_t v = 0;
        if (readReg(Reg::CHIP_RESET, v))
            Serial.printf("[ES7210] Read-back REG0=%02X\n", v);
        else
            Serial.println("[ES7210] WARNING: read-back failed — chip may not be responding");
    }
    g_init = true;
    Serial.println("[ES7210] Ready (MIC1+MIC2+MIC3, vendor-like config, 30dB)");
    return true;
}

inline void end() {
    if (!g_init) return;
    writeReg(Reg::MIC1_POWER, 0xFF);
    writeReg(Reg::MIC2_POWER, 0xFF);
    writeReg(Reg::MIC3_POWER, 0xFF);
    writeReg(Reg::MIC4_POWER, 0xFF);
    writeReg(Reg::MIC12_POWER, 0xFF);
    writeReg(Reg::MIC34_POWER, 0xFF);
    writeReg(Reg::ANA_CTRL, 0xC0);
    writeReg(Reg::CLOCK_OFF_7210, 0x7F);
    writeReg(Reg::PWR_DOWN, 0x07);
    g_init = false;
    Serial.println("[ES7210] Powered down");
}

} // namespace Es7210
