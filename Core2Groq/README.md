# Core2Groq 📻🤖

Unified **M5Stack Core2** firmware that keeps the OTR radio project and now adds a **Groq chatbot mode** beside it.

The radio side still comes from [M5RadioStream](../M5RadioStream) (Core1 Basic), upgraded with **16-bit I2S audio** for clean standalone playback on the Core2 speaker.

Original concept: **[winRadio by Volos Projects](https://github.com/VolosR/WaveshareRadioStream)**

---

## Demo

![Core2Groq demo](YouCut_20260512_122958855%20(1).gif)

---

## Key Upgrade Over Core1 Version

| | Core1 Basic | **Core2** |
|---|---|---|
| Audio output | 8-bit internal DAC | **16-bit I2S → NS4168 amp** |
| Background hiss | Constant (hardware floor) | **Dramatically reduced** |
| Touch input | Physical buttons only | **Capacitive touch + physical** |
| Haptic feedback | None | **Vibration motor on every tap** |
| PSRAM | None | **4MB** |

---

## Current controls

### Radio mode

Both the **on-screen touch footer** and **physical virtual buttons** work:

| Touch Zone | Button | Normal Mode | Settings Mode |
|---|---|---|---|
| [SET] | BtnA (short) | Open sound settings | — |
| [STA] | BtnB | Cycle station (1.5s debounce) | Select next parameter |
| [VOL] | BtnC (short) | Cycle volume 0–10 (0=mute) | Increase value |
| — | **BtnC (hold 1s)** | **Toggle screen on/off** | — |
| [BACK] | BtnA | — | Exit settings |

### Bot mode

- Boots into **Bot mode** by default when a Groq key is configured
- bottom capacitive **REC** button records for the configured max time
- bottom capacitive **STOP** button stops an active recording
- bottom capacitive **HOLD** button records while held and stops on release
- top-center **BOT / YOU** chip toggles between the latest bot reply and the latest user transcript
- on-screen **SET** opens the bot settings menu
- settings menu currently includes:
  - **Setup**
  - **Personality** preset cycling
  - **Model** cycling
  - **Auto-scroll speed**
- on-screen **RADIO** switches into radio mode
- on-screen **NEW** clears the current bot chat
- long replies now **auto-scroll and repeat**
- tapping the reply panel can still manually bump the scroll position if needed
- in radio mode, tap the **BOT** button in the header to return

---

## Hardware

| Component | Details |
|-----------|---------|
| Board | M5Stack Core2 |
| MCU | ESP32 (dual-core, 240 MHz) |
| Display | ILI9341 320×240, capacitive touch |
| Audio amp | I2S → NS4168 (BCK=GPIO12, LRC=GPIO0, DOUT=GPIO2) |
| Speaker | 1W, 8Ω onboard |
| PSRAM | 4MB |
| Optional LCD | 16x2 I2C character LCD *(0x27 / 0x3F typical)* |

---

## Optional 16x2 I2C LCD Wiring

The same style of 16x2 LCD used on **Groqputer** can also be connected here for an external status and message marquee.

| LCD pin | Core2Groq connection |
|---|---|
| VCC | 5V |
| GND | GND |
| SDA | GPIO33 |
| SCL | GPIO32 |

Notes:

- this follows the requested mapping of **G33 = Cardputer G1** and **G32 = Cardputer G2**
- the firmware scans common backpack addresses **0x27** and **0x3F**
- line 1 shows mode / recording / connectivity status
- line 2 scrolls the current bot reply, user transcript, or radio title using the same auto-scroll speed setting as the bot screen

---

## Stations (ROKiT Radio Network — OTR classics, 48 kbps MP3)

| # | Station | Highlights |
|---|---|---|
| 1 | 1940s Radio | Big band, wartime era |
| 2 | American Comedy | Fibber McGee & Molly, Jack Benny, You Bet Your Life |
| 3 | American Classics | Drama anthology |
| 4 | Jazz Central | Swing & jazz |
| 5 | Comedy Gold | Burns & Allen, Red Skelton |
| 6 | Mystery Radio | Suspense, Inner Sanctum |
| 7 | Crime & Suspense | Dragnet, Philip Marlowe |
| 8 | Crime Radio | Sam Spade, Boston Blackie |
| 9 | Adventure Stories | The Lone Ranger, Zorro |
| 10 | Drama Radio | Lux Radio Theatre |
| 11 | Nostalgia Lane | Mixed OTR variety |
| 12 | Science Fiction | X Minus One, Dimension X |

---

## Build & Flash

```bash
# Build and upload directly
pio run --target upload

# Serial monitor
pio device monitor --baud 115200
```

### Flash with M5Burner
Use **`Core2Groq_M5Core2-MERGED.bin`** — flash to offset `0x0`.

This merged image includes the bootloader, partitions, boot app, and the current Core2Groq firmware in one file.

---

## First Boot / Setup AP

On first boot (or hold **BtnA** during the 3-second splash), a captive portal opens:

1. Connect phone/PC to WiFi network **`Core2Groq_Setup`**
2. Open browser → `192.168.4.1`
3. Enter:
   - 2.4 GHz WiFi credentials
   - Groq API key for bot mode
   - default boot mode (**Bot** or **Radio**)
4. Save and reboot

Settings are stored in NVS and survive power cycles.

---

## Planned Features

- continue polishing the touchscreen bot UI
- keep radio playback simple and reliable as a standalone speaker-based player

---

## Credits

- **Original project:** [winRadio by Volos Projects](https://github.com/VolosR/WaveshareRadioStream)
- **Core2 port & skin:** CoreyMillia / GitHub Copilot — 2026
