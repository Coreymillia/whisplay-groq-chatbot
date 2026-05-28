# Core2Groq 🤖📻

**Core2Groq** is a chatbot-first **M5Stack Core2** firmware built for the **M5Burner** style of install: flash one merged image, save Wi-Fi and your Groq key, then use the device as a handheld Groq bot.

The main focus is the **touchscreen chatbot experience**:

- direct **Groq chat**
- direct **Groq Whisper** voice transcription
- **Whisplay AI screensaver** mode that pulls generated images from a Whisplay URL
- local **setup AP**
- editable **personality prompt**
- on-device **personality / model / scroll / boot mode** controls
- optional **radio mode** as a side feature when you want it

---

## Demo

![Core2Groq demo](YouCut_20260512_122958855%20(1).gif)

---

## What it does

- boots into **Bot mode** by default when a Groq key is configured
- can also boot directly into **AI Screensaver** mode or **Radio** mode
- uses the Core2 touchscreen for day-to-day chat control
- records voice from the device and sends it through Groq Whisper
- shows the latest **bot** reply or **you** transcript on the main screen
- keeps local settings in NVS so the setup only has to be done once
- lets you open **Radio mode** when you want a simple side feature without replacing the bot
- lets you save a **Whisplay URL** and use the Core2 as a dedicated AI slideshow screen

---

## Flash with M5Burner

Use **`Core2Groq_M5Core2-MERGED.bin`** and flash it to offset **`0x0`**.

This merged image includes the bootloader, partitions, boot app, and the current Core2Groq firmware in one file.

### Quick setup after flashing

1. Boot the Core2.
2. On first boot, or by holding **BtnA** during the splash, open the setup AP.
3. Connect your phone or computer to:
   - **`Core2Groq_Setup`**
4. Open:
   - **`http://192.168.4.1`**
5. Save:
   - 2.4 GHz Wi-Fi credentials
   - Groq API key
   - optional Whisplay URL for AI screensaver mode
   - default boot mode (**Bot**, **AI Screensaver**, or **Radio**)
6. Reboot and start chatting.

Settings are stored locally and survive power cycles.

---

## Bot controls

- bottom capacitive **REC** button records for the configured max time
- bottom capacitive **STOP** button stops an active recording
- bottom capacitive **HOLD** button records while held and stops on release
- top-center **BOT / YOU** chip toggles between the latest bot reply and the latest user transcript
- on-screen **SET** opens the bot settings menu
- bot settings currently include:
  - **Setup**
  - **Personality**
  - **Model**
  - **Auto-scroll speed**
  - **Boot mode**
  - **Launch selected boot mode**
- on-screen **NEW** clears the current chat
- on-screen **RADIO** switches into radio mode
- long replies auto-scroll, and tapping the reply area can still manually bump the scroll position
- the settings screen now uses large touch tiles instead of the earlier thin row targets

---

## Writing a good persona

The best personas are usually **short, specific, and useful**.

### Good persona structure

1. **Role** - who the bot is
2. **Tone** - how it should sound
3. **Answer style** - concise, detailed, step-by-step, playful, etc.
4. **Limits** - what it should avoid
5. **Special behavior** - how it should handle troubleshooting, images, or encouragement

### Reliable pattern

```text
You are a practical handheld assistant.
Sound calm, clear, and helpful.
Keep replies concise unless the user asks for more detail.
When troubleshooting, give the most likely cause first.
Do not ramble or bury the answer in character flavor.
```

### Tips

- give the bot a **job**, not just a vibe
- ask for a clear **reply length or style**
- add one or two **hard limits** like:
  - do not ramble
  - do not be rude
  - do not get so in-character that the answer becomes unclear
- if you want humor or personality, say that it should still stay **useful first**

### Avoid

- long backstories that do not change behavior
- too many conflicting traits in one prompt
- vague prompts like **"be cool"** or **"be funny"** with no guidance on how to help

### Example personas

**Encouraging builder**

```text
You are a warm electronics project coach.
Sound grounded, supportive, and honest.
Keep replies concise and practical.
Notice what is already working before suggesting the next fix.
Do not use fake hype or empty praise.
```

**Pocket technician**

```text
You are a pocket troubleshooting assistant.
Sound direct, capable, and calm.
Keep replies short and step-by-step.
When debugging, start with the most likely failure point.
Do not ramble or over-explain simple checks.
```

---

## Current controls

### Bot mode

- **REC** = record for the configured max time
- **STOP** = stop an active recording
- **HOLD** = record while held, stop on release
- **BOT / YOU** = swap between the current reply and your latest transcript
- **SET** = open the settings menu
- **NEW** = clear the current chat
- **RADIO** = switch into radio mode

### Settings menu

- **Setup**
- **Personality**
- **Model**
- **Auto-scroll speed**
- **Boot mode**
- **Launch**
- **AI Show**
- **Radio**

### AI Screensaver mode

- uses the saved **Whisplay URL** only
- polls only the Whisplay **generated image gallery**
- does **not** mirror chat text or assistant status
- caches slideshow images on the **SD card** when available so the Core2 can rotate through many stored slides
- falls back to a single in-memory image if SD caching is unavailable
- uses the display **fullscreen** with no on-screen controls layered over the image
- bottom-left button/zone opens **setup**
- bottom-middle button/zone advances to the **next** image

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

## Build & Flash from source

```bash
# Build and upload directly
pio run --target upload

# Serial monitor
pio device monitor --baud 115200
```

---

## Radio mode

Core2Groq still includes a simple **radio mode**, but it is now a side feature instead of the main focus.

- tap **RADIO** from bot mode to switch over
- tap **BOT** in the header to come back
- radio mode is meant to be lightweight and easy to use, not the center of the project
- if you never use the radio, Core2Groq still makes sense as a chatbot-only handheld

Basic radio controls:

- **SET** = sound settings
- **STA** = next station
- **VOL** = volume
- **BACK** = leave radio settings

The radio side was originally inspired by **[winRadio by Volos Projects](https://github.com/VolosR/WaveshareRadioStream)**, but this Core2 firmware is now being documented and positioned primarily as a **bot project first**.

---

## Credits

- **Original project:** [winRadio by Volos Projects](https://github.com/VolosR/WaveshareRadioStream)
- **Core2 port & skin:** CoreyMillia / GitHub Copilot — 2026
