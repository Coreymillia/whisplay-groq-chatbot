# Groqputer

Groqputer is a standalone **M5Cardputer** Groq chatbot firmware built inside the WhisplayGroqHat repo.

It is designed to be **100% free to start** with Groq's very generous request limits, which makes it a good fit for an always-available pocket chatbot build.

It is meant to be a small, useful Cardputer chat build first:

- direct **Groq chat**
- direct **Groq Whisper** transcription
- local **Wi-Fi setup AP**
- editable **personality prompt**
- on-device **model/personality** switching
- optional **16x2 I2C LCD** companion display
- early **same-LAN Whisplay relay** testing

## Why it exists

Groqputer started as a pivot from the larger Whisplay project so the Cardputer could become its own simple Groq bot instead of depending on a Raspberry Pi host.

The long-term hope is still to let Groqputer connect back into the Whisplay ecosystem more cleanly, but the first goal is a dependable standalone Cardputer chatbot.

## Current status

Working now:

- standalone keyboard chat
- hold-to-record voice input
- direct Groq Whisper transcription
- direct Groq replies
- saved local chat history
- split incoming/outgoing chat views
- reduced Cardputer redraw flicker
- on-device bot settings
- setup AP with saved Wi-Fi and Groq settings
- optional 16x2 I2C LCD output
- merged M5Burner-ready firmware image

Current experimental feature:

- **Connected Device / LAN mode** can send prompts to a Whisplay node over the local network and read back the displayed reply

## Included firmware image

This folder includes a merged firmware image for easier flashing:

- **`Groqputer_M5Cardputer-MERGED.bin`**

This image includes the bootloader, partitions, boot app, and main firmware in one file.

## Setup

1. Flash the firmware.
2. Boot the Cardputer.
3. On a fresh flash, the Cardputer now shows a **setup instruction screen** instead of sitting blank.
4. Join the setup AP shown on-screen:
   - **`Groqputer-Setup`**
5. Open:
   - **`http://192.168.4.1`**
6. Open the setup AP later with **Fn+A** if needed.
7. Save:
   - Wi-Fi SSID/password
   - Groq API key
   - chat model
   - personality prompt
   - max record seconds
   - optional **This Device URL**
   - optional **Connected Device URL**
8. Save and let the Cardputer reboot.

### Groq API key setup

1. Create or sign in to your Groq account.
2. Open the Groq API keys page.
3. Create a new key.
4. Paste that key into the **Groq API Key** field in the setup AP.

After that, Groqputer can use direct Groq chat and direct Groq Whisper transcription.

### Important first-boot note

If Groqputer is not configured yet, it does **not** mean the firmware is broken.

On first boot it should now show setup instructions on the Cardputer screen and wait in the AP setup flow until Wi-Fi and the Groq key are saved.

For Whisplay relay testing, the connected device URL should be the Whisplay browser base URL, for example:

```text
http://10.160.0.136:17880
```

## Hotkeys

- **Enter** = send typed message
- **Hold BtnA** = record voice message
- **Fn+A** = open setup AP
- **Fn+M** = incoming view
- **Fn+O** = outgoing view
- **Fn+S** = settings screen
- **Fn+B** = bot settings
- **Fn+C** = connected-device / LAN mode on or off
- **Fn+N** = new chat
- **Fn+;** / **Fn+.** = scroll current chat view
- **Fn+,** / **Fn+/** = LCD marquee slower / faster
- **Fn+1** / **Fn+2** = LCD backlight off / on
- **Fn++** / **Fn+-** = Cardputer text size up / down

### Bot settings controls

When **Bot Settings** is open:

- **Fn+;** / **Fn+.** = move between **Model** and **Personality**
- **Fn+,** / **Fn+/** = cycle selected value and save immediately

## Personality presets

Groqputer currently includes the same built-in presets used by the Whisplay bot:

- Neutral
- Friendly
- Cranky
- Roast Bot
- Sleepy Pi
- Affirmation
- Philosopher
- Mythic Oracle
- Joke Bot
- Tutor
- Detective
- Zen

## Optional 16x2 I2C LCD

Current target:

- **HD44780 16x2 LCD with I2C backpack**

Current behavior:

- line 1 = compact status
- line 2 = scrolling **incoming** reply text

Current note:

- backlight can be toggled from the Cardputer
- true brightness control is **not** supported by the current LCD backpack library
- battery readability on the tested 1602 display appears to be a **hardware limitation**, so a future **1.3 inch I2C display** is a likely next direction

## Connected-device / LAN mode

This mode is for early testing against another bot on the same LAN, especially Whisplay.

Current behavior:

- Groqputer sends the prompt to the configured connected device
- the peer device answers using its own normal chatbot/personality flow
- Groqputer reads that reply back and shows it locally

This is currently a **simple relay path**, not the full BotNet conversation engine yet.

## Future direction

Planned or likely future work:

- local daily message / reply counter
- better peer / BotNet integration
- improved external display support
- possible **1.3 inch I2C display** support for better battery behavior
- tighter Whisplay-side integration after the standalone firmware is more mature
