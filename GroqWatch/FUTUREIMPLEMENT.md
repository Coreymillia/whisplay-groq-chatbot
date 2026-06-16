# Future Implementations — GroqWatch

Ordered from safest/most achievable to riskiest/most complex.

---

## 1. Watch mode polish

### 1a. Fix particle animations
**Risk:** Very low.  
**Dependency:** none.

The current Stars / Bubbles / Grid renderer uses a double-buffered `PixelCanvas`. It works but has visual bugs (flicker, compositing glitches, particle reset issues on mode switch). Cleanup is pure rendering work — no hardware or driver concerns.

### 1b. Last AI image as clock background
**Risk:** Low.  
**Dependency:** 1a.

Instead of a black background with particles, use the last-viewed AI slideshow image as the watch face background. The clock digits, status bar, and particles would render on top.

Implementation:
- Keep `aiCurrentSlide` alive when leaving AI mode
- In `renderWatchFrame()`, if a slide background is available, draw it first instead of the particle canvas
- Add a "clear background" option or timeout so the user isn't stuck with one image forever

### 1c. WiFi auto-fallback to Watch mode
**Risk:** Low.  
**Dependency:** none.

Already partially implemented in `checkWifiAndFallback()`. Need to:
- When WiFi drops, save the current mode (`Bot` or `AiScreensaver`) as `lastOnlineMode`
- Switch to `Watch` mode
- Periodically scan for WiFi return
- When WiFi reconnects, restore `lastOnlineMode`
- If the user manually changed modes while offline, respect the manual choice

---

## 2. Background AI polling ("captive art portal")

**Risk:** Low–Medium.  
**Dependency:** 1c (WiFi awareness).

The AI screensaver currently only polls Whisplay and downloads images when the AI mode is active and visible. The "captive art portal" idea means:

- AI image polling/downloading runs **in the background** even when the user is in Watch mode
- New images are silently cached to SD
- The last-downloaded image is available for the watch background (1b)
- When the user enters AI mode, slides are already fresh

Implementation:
- Move `aiShowNextSlide` polling into a non-blocking background task
- Decouple "fetch and cache" from "display current slide"
- Respect SD space limits as the existing cache pruning does

---

## 3. Tetris

**Risk:** Medium.  
**Dependency:** none.

Port one of your existing ESP32 Tetris builds to the GroqWatch hardware. Key concerns:

| Concern | Assessment |
|---------|-----------|
| Display | Arduino_GFX on 410×502 — plenty of room, easy to adapt |
| Input | Touch screen (swipe/ tap zones) or BOOT button (rotate) |
| Memory | Game state is tiny; fits easily even without PSRAM |
| Audio | No audio needed — no codec dependency |

A simple block-dropper with touch controls is straightforward. The main work is adapting drawing calls to use Arduino_GFX instead of whatever display library the original Tetris builds used.

---

## 4. MP3 player

**Risk:** Medium–High.  
**Dependency:** ES8311 speaker path must work.

The **Waveshare 08_ES8311 Arduino demo** proves the ES8311 speaker output path functions in Arduino. That's a strong signal. However:

### What we know works
- ES8311 I2C control (already in our codebase at `src/es8311.c` / `.h`)
- Shared I2S bus (BCLK=41, LRCK=45, MCLK=16)
- DOUT=40 for speaker data output
- The demo plays PCM audio through ES8311

### What we need to add
- MP3 decoder library (e.g. ESP8266Audio, or a lightweight MAD/helix port)
- SD card file browser to select MP3 files
- I2S output configuration (opposite direction from our mic attempts)
- Volume control and playback UI

### Main risk
The ES8311 speaker path may have subtle driver issues similar to what we hit with the ES7210 mic path. The Waveshare demo works, but it may depend on specific I2S init order, clock config, or codec register sequence that could conflict with other device usage.

**Recommendation:** Prove the ES8311 speaker path first with a minimal test before building the full player.

---

## 5. Streaming low-bitrate radio

**Risk:** High.  
**Dependency:** 4 (MP3 / audio output must work).

Would layer on top of the MP3 player:

| Component | Feasibility |
|-----------|------------|
| HTTP stream fetch | Straightforward — ESP32 WiFiClient can handle a shoutcast/Icecast stream |
| Low-bitrate codec | Many streams use MP3 or AAC at 32-64kbps — manageable |
| Continuous buffering | Needs a ring buffer and careful memory management |
| UI | Simple — channel list, play/stop, volume |

### Main risks
- **Same audio output risk as MP3 player** — streaming radio inherits all the same codec driver concerns
- **Sustained WiFi + audio streaming** can be taxing on the ESP32-S3; we'd want PSRAM for larger buffers
- **Power draw** would be significant for a wrist-worn device streaming over WiFi while driving audio and display

This is the least certain feature and should only be attempted after the MP3 player is proven stable.

---

## Summary

| # | Feature | Risk | Est. effort | Blocker |
|---|---------|------|-------------|---------|
| 1a | Fix particle animations | Very Low | Small | none |
| 1b | AI image as clock background | Low | Small | 1a |
| 1c | WiFi auto-fallback | Low | Small | none |
| 2 | Background art portal | Low-Med | Medium | 1c |
| 3 | Tetris | Medium | Medium | none |
| 4 | MP3 player | Med-High | Large | ES8311 speaker must work |
| 5 | Streaming radio | High | Large | 4 must be stable |

### Recommended order
```
1a → 1b → 1c → 2 → 3 → (validate ES8311 speaker) → 4 → 5
```
