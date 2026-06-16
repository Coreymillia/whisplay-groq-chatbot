# GroqWatch ESP-IDF audio probe

Location:
- `/home/coreymillia/Documents/GroqWatch/ESP-IDFtest`

Purpose:
- test the Waveshare board with the **board BSP / ESP-IDF codec path** instead of the ad-hoc Arduino path
- bring up **speaker first**, then **mic**, to match the reported full-duplex startup order
- verify whether mic energy rises during known speaker tone windows

## What this test does
- uses the Waveshare managed BSP component:
  - `waveshare/esp32_s3_touch_amoled_2_06`
- uses `esp_codec_dev`
- initializes:
  - `bsp_audio_codec_speaker_init()`
  - `bsp_audio_codec_microphone_init()`
- opens the **speaker first** at `16 kHz / 16-bit / 2ch`
- writes several silence chunks to prime the audio clocks
- opens the **mic second**
- runs a full-duplex test with alternating windows:
  - tone ON
  - tone OFF
- prints left/right mic energy for each window

## Expected result
If the hardware path is correct, you should see:
- higher `avgL/avgR` and/or `peakL/peakR` during `tone=ON`
- lower values during `tone=OFF`
- meaningful response when you also speak near the mic

## Files
- `CMakeLists.txt`
- `sdkconfig.defaults`
- `main/CMakeLists.txt`
- `main/idf_component.yml`
- `main/main.c`

## Build / flash
First make sure ESP-IDF is installed and exported in your shell.
If `idf.py` is not found, source your ESP-IDF environment first.

Example:
```bash
cd ~/esp/esp-idf
source export.sh
```

Then build the probe:
```bash
cd /home/coreymillia/Documents/GroqWatch/ESP-IDFtest
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Adjust the port if needed.

## How to use
- On boot, the probe runs once automatically.
- Press **BOOT** to run it again.
- During the test, speak near the mic and watch the serial logs.

## Notes
- This project is intentionally serial-only.
- It avoids the full watch/chatbot UI to reduce possible hardware conflicts.
- It is designed around the idea that this board may require the BSP-managed full-duplex codec startup order to work reliably.
