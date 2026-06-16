# GroqWatch

A **Whisplay companion** for the **Waveshare ESP32-S3 Touch AMOLED 2.06** — mirror the Whisplay hat chat and browse its AI-generated images right from your wrist.

## What it does

GroqWatch connects to a Whisplay Pi over Wi‑Fi and gives you three modes:

| Mode | What you see |
|------|-------------|
| **Watch** | Animated clock face with particle styles (Stars / Bubbles / Grid) |
| **Whisplay Bot** | Mirrors the latest Whisplay chat reply, with on-screen REPEAT / NEW CHAT / BACK controls |
| **AI Screensaver** | Full-screen AI slideshow from the Whisplay image gallery, with SD cache |

### Whisplay Bot mode

- polls `GET /api/state` from the Whisplay Pi
- shows the latest assistant reply as readable text
- shows the hat's current status and emoji
- **BOOT button** triggers REPEAT on the hat
- on-screen buttons:
  - **NEW** — `POST /api/chat/reset`
  - **REPEAT** — `POST /api/companion/action`
  - **AI** — jump to the AI slideshow
  - **BACK** — return to watch mode

### AI Screensaver mode

- polls `GET /api/generated-images` from Whisplay
- downloads companion-sized JPEGs and saves them to SD under `/ai-cache`
- renders images full-screen centered via JPEGDEC
- **BOOT button** advances to the next slide
- auto-advances every ~18 seconds
- works offline from SD cache when Wi‑Fi is unavailable

---

## Hardware

- **Board:** Waveshare ESP32-S3 Touch AMOLED 2.06
- **Display:** 410×502 AMOLED
- **Touch:** FT3x68 capacitive
- **RTC:** PCF85063
- **PMU:** AXP2101 (battery percentage in header)
- **SD:** microSD via `SD_MMC` (used for AI image cache)

---

## Pins

Key pin assignments in `include/pin_config.h`:

| Function | GPIO |
|----------|------|
| LCD QSPI | 4, 5, 6, 7, 11, 12, 8 |
| I2C SDA | 15 |
| I2C SCL | 14 |
| Touch INT | 38 |
| Touch RST | 9 |
| SD CLK/CMD/D0 | 2 / 1 / 3 |
| BOOT button | 0 |

---

## Setup

1. Flash the firmware
2. The device starts in **Watch** mode
3. Hold **BOOT** at startup to enter the Setup AP portal
4. In the portal, enter:
   - Wi‑Fi SSID and password
   - **Whisplay URL** (e.g. `http://10.160.0.136:17880`)
   - optional: timezone, default boot mode
5. Save — the device reboots and connects

The Groq API key section is hidden behind a `<details>` expander in Setup since Bot mode now uses the Whisplay Pi as the chat brain.

---

## Build and flash

```bash
cd GroqWatch
pio run
pio run -t upload
```

Serial monitor:
```bash
pio device monitor -b 115200
```

---

## Project structure

```text
GroqWatch/
├── README.md
├── platformio.ini
├── include/
│   ├── AiScreensaver.h      # AI slideshow polling, download, SD cache, JPEG render
│   ├── AppModes.h           # mode enum and labels
│   ├── AppSettings.h        # Preferences-backed settings struct
│   ├── GroqApi.h            # leftover Groq API helpers (not used in companion mode)
│   ├── PixelCanvas.h        # double-buffered pixel canvas for watch animations
│   ├── SettingsMenu.h       # on-device settings tile UI
│   ├── SetupPortal.h        # captive-portal Wi‑Fi setup
│   └── pin_config.h         # hardware pin map
└── src/
    ├── main.cpp             # app loop, mode switching, Whisplay polling, UI
    └── SetupPortal.cpp      # AP setup web form
```

---

## Known issues

- AI slideshow occasionally shows an image decode failure on one slide and recovers on the next
- touch responsiveness during AI mode can lag while a large JPEG is downloading
- older cached images from earlier builds may render at mismatched sizes — clearing `/ai-cache` on the SD card and re-downloading fixes this

---

## Relation to the rest of WhisplayGroqHat

GroqWatch is one of several companion surfaces in this repo:

| Companion | Role |
|-----------|------|
| **CYD** | Full touch companion with chat, capture, gallery, settings |
| **Cardputer** | Keyboard-based companion |
| **Core1Display** | Lightweight polling text mirror |
| **Core2Groq** | Standalone Groq bot on M5Stack Core2 |
| **GroqWatch** | Wrist-worn polling mirror + AI slideshow |

All companions talk to the same Whisplay Pi over the local network.
