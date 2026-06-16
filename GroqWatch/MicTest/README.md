# GroqWatch MicTest

Minimal microphone diagnostic firmware for the Waveshare ESP32-S3-Touch-AMOLED-2.06.

Purpose:
- avoid the full chatbot/watch app
- reduce possible hardware/software conflicts
- probe I2C audio devices
- test several I2S port/pin configurations
- print left/right audio energy stats over Serial

## What it does
- scans I2C after enabling the audio codec rail
- reports whether addresses `0x18` and `0x40` exist
- initializes ES8311 if present
- runs one audio test each time you press **BOOT**
- cycles through multiple I2S configs:
  - Port 1 / demo pins
  - Port 1 / demo pins with continuous TX silence
  - Port 1 / schematic pins
  - Port 1 / schematic pins with continuous TX silence
  - Port 0 / demo pins
  - Port 0 / schematic pins
- captures raw stereo 16-bit data and prints:
  - bytes captured
  - zero-read count
  - zero-write count
  - left/right average magnitude
  - left/right peak magnitude

## Expected use
1. Flash firmware
2. Open serial monitor
3. Press BOOT to run the shown test
4. Speak clearly for ~1.5 seconds during capture
5. Compare results between tests

## Commands
```bash
cd /home/coreymillia/Documents/GroqWatch/MicTest
pio run -t upload
pio device monitor -b 115200
```

## Notes
- This firmware intentionally does **not** initialize the display, touch UI, Wi-Fi, or chatbot flow.
- That is deliberate so we can isolate audio and possible hardware conflicts.
