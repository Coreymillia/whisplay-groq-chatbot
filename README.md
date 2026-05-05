# WhisplayGroqHat

<img src="https://docs.pisugar.com/img/whisplay_logo@4x-8.png" alt="Whisplay AI Chatbot" width="200" />

[![Discord](https://img.shields.io/discord/1483017948305297501?logo=discord&logoColor=white&label=Discord&color=5865F2)](https://discord.gg/znGrZmTk)

This project starts from the official PiSugar Whisplay chatbot, but is being tailored for a **Raspberry Pi Zero 2 W** with a **Groq-backed LLM path**, a cleaner bring-up flow for the PiSugar **Whisplay HAT**, and an early **CYD companion display** for touch-based remote control.

## Current project direction

- **LLM:** Groq through the OpenAI-compatible client path
- **Bring-up mode:** browser simulator first, physical HAT now working
- **Power:** wall-powered is fine; the PiSugar battery is optional
- **Scope:** build the chatbot first as a dedicated Whisplay + Groq device
- **Companion path:** add optional ESP32 sidecars like the CYD without replacing the Pi-hosted chatbot brain
- **Cost target:** stay on free-tier API usage where possible for now

## Current hardware status

- **Whisplay HAT display:** working on Raspberry Pi Zero 2 W
- **Bot on HAT:** working
- **Microphone:** working
- **Speaker / WM8960 audio path:** working
- **Spoken replies / TTS:** confirmed working with `espeak-ng`
- **Spoken photo capture:** working
- **Captured still image on HAT display:** working
- **Companion CYD touchscreen client:** working as an early rough-start build, with more tuning planned
- **Companion Cardputer client:** working as an early rough-start build, with text chat confirmed and more settings/UI work planned
- **Web simulator/debug UI:** still available at `http://<host-or-pi-ip>:17880`

## Current feature highlights

- **One-button device flow:** long press to talk, double press to replay the last answer, and voice shortcuts for settings and help
- **Voice controls:** settings, voice on/off, photo capture, photo browsing, shutdown, and an on-device voice-command cheat sheet
- **Vision flow:** upload a photo or capture one from the configured camera source, then ask **"what do you see?"**
- **Local photo effects:** apply deterministic voice-triggered filters such as **retro**, **comic**, **sketch**, **pixelate**, **spooky**, **dreamy**, **warm**, **cyberpunk**, **glitch**, and more to the currently shown image
- **Preset personalities:** Neutral, Friendly, Cranky, Roast Bot, Sleepy Pi, Affirmation, Philosopher, Mythic Oracle, Joke Bot, Tutor, Detective, and Zen
- **HAT visuals:** switchable header effects plus full-screen screensavers such as Matrix, Retro Geometry, Plasma, and Neon Rain
- **Improved HAT readability:** reply text now wraps more naturally on the device instead of breaking as aggressively mid-word
- **Companion CYD controls:** touch actions for **New Chat**, **Repeat**, **Capture**, **Voice**, plus an on-screen **Setup** button for reopening the Wi-Fi portal
- **Companion Cardputer controls:** keyboard text send, message viewing, local setup portal, saved text sizes, and a split receive/send screen layout

## Device screenshots

These are a few representative shots of the current build. They intentionally show only a small sample so the README stays readable.

### Browser simulator and basic HAT idle view

![Updated browser simulator view](images/IMG_20260505_133554046_HDR.jpg)

![Whisplay HAT idle screen](images/IMG_20260503_164937203_HDR.jpg)

### Early CYD companion build

![Early Whisplay CYD companion build](images/IMG_20260504_155914704_HDR.jpg)

### Matrix screensaver on the Whisplay HAT

![Matrix screensaver on the Whisplay HAT](images/IMG_20260503_164718210_HDR.jpg)

### Vision example: uploaded shop photo and Cranky Bot reaction

![Shop photo shown on the Whisplay HAT for vision analysis](images/IMG_20260503_171337860_HDR.jpg)

![Cranky Bot starting its reaction to the uploaded shop photo](images/IMG_20260503_171358433_HDR.jpg)

![Cranky Bot continuing its complaint about the shop mess](images/IMG_20260503_171402454_HDR.jpg)

### Cranky Bot reply example

![Cranky Bot giving a sarcastic GitHub-flavored reply](images/IMG_20260503_171835378_HDR.jpg)

### New local photo effects and updated case/help view

![Sketch-style photo effect example](images/IMG_20260505_133314327_HDR.jpg)

![Whisplay help screen with the updated 3D printed case and Arducam photo](images/IMG_20260505_134146692.jpg)

## Quick start for this fork

1. Clone this project and enter it:
   ```bash
   git clone <your-repo-url> /home/coreymillia/Documents/complete-projects/WhisplayGroqHat
   cd /home/coreymillia/Documents/complete-projects/WhisplayGroqHat
   ```
2. Install dependencies:
   ```bash
   bash install_dependencies.sh
   source ~/.bashrc
   ```
3. Create your env file and add your Groq key:
   ```bash
   cp .env.template .env
   ```
   Then edit `.env` and set `OPENAI_API_KEY` to your Groq key. If you want Gemini vision ready from the start, also set `GEMINI_API_KEY`.
4. Build and run:
   ```bash
   bash build.sh
   bash run_chatbot.sh
   ```
5. Optional: set the bot to start on boot:
   ```bash
   bash startup.sh
   ```
6. Open the browser simulator:
   ```text
   http://<host-or-pi-ip>:17880
   ```

The default template in this fork was originally set up for **web simulator first** bring-up. The project has now also been validated on a real Whisplay HAT, while keeping the web UI available for debugging and settings.

For actual device use, prefer the `chatbot.service` boot path over ad-hoc `nohup` launches so the bot comes back cleanly after shutdown or reboot.

## Companion CYD status

This repo now also includes an early companion firmware project under `CompanionCYD/` for the popular **Cheap Yellow Display (ESP32-2432S028R)**.

- the **Pi stays the chatbot brain**
- the **CYD acts as a remote touchscreen client**
- current actions include **New Chat**, **Repeat**, **Capture**, **Voice**, and **Setup**
- Wi-Fi credentials and Pi host settings are handled from the CYD's own captive portal
- both **normal** and **inverted** display variants are included

This is intentionally a **rough start**, not a polished final companion UI. It is already usable, but more tuning is planned around layout, diagnostics, and overall interaction flow.

## Companion Cardputer status

This repo also now includes an early companion firmware project under `CompanionCARDPUTER/` for the **M5Stack Cardputer**.

- the **Pi stays the chatbot brain**
- the **Cardputer acts as a keyboard-first text companion**
- current text chat is confirmed working in both directions
- the firmware includes a local Wi-Fi + Pi setup portal, receive/send screen split, message scrolling, and saved text-size controls

This is also intentionally a **rough start**, not a polished final Cardputer UX. It is already useful for text chat, and more settings/polish are planned. The audio path is implemented in the firmware, but hardware validation is still pending.

## Getting a Groq API key

1. Sign in or create an account at [Groq Console](https://console.groq.com/).
2. Open the API keys page at [console.groq.com/keys](https://console.groq.com/keys).
3. Create a new key and copy it.
4. Put that value in `.env` as `OPENAI_API_KEY=...`.

This fork uses Groq through the **OpenAI-compatible** API path, so the important settings are:

```env
LLM_SERVER=openai
OPENAI_API_BASE_URL=https://api.groq.com/openai/v1
OPENAI_API_KEY=your_groq_api_key
```

## Getting a Gemini API key

1. Sign in or create an account at [Google AI Studio](https://aistudio.google.com/).
2. Create an API key from [Google AI Studio API keys](https://aistudio.google.com/app/apikey).
3. Make sure the key has access to the Gemini API for your project.
4. Use the key in either of these ways:
   - put it in `.env` as `GEMINI_API_KEY=...`
   - or paste it into the **Gemini key** field in the browser settings panel and save

Important Gemini settings in `.env`:

```env
GEMINI_API_KEY=your_gemini_api_key
# optional overrides
# GEMINI_VISION_MODEL=gemini-2.5-flash
```

## Speech recognition notes for this fork

Speech recognition is now configured to use the same OpenAI-compatible path against Groq for ASR as well as chat.

Important settings:

```env
ASR_SERVER=openai
OPENAI_ASR_MODEL=whisper-large-v3-turbo
OPENAI_API_BASE_URL=https://api.groq.com/openai/v1
```

Why this matters:

- The original OpenAI ASR code path used `whisper-1`.
- That model name failed against the Groq endpoint in live testing.
- This fork now defaults to `whisper-large-v3-turbo` when using the Groq OpenAI-compatible base URL, which restored speech recognition on the Pi.

## Speech output notes for this fork

Speech output is now also confirmed working on the physical Pi + Whisplay hardware using the local `espeak-ng` TTS path.

Working settings:

```env
TTS_SERVER=espeak-ng
```

- Set **Voice mode** to **Voice chat** in the browser or HAT settings to enable spoken replies
- The current `espeak-ng` plugin defaults to `ESPEAK_NG_VOICE=en` unless you override it in `.env`
- This was confirmed working through the Whisplay speaker path on the Pi Zero 2 W hardware

## Settings UI notes

The browser simulator includes a settings panel for the Groq key, Gemini key, preset personalities, freeform personality editing, voice mode, record time, text scroll speed, UI theme, HAT header mode, HAT screensaver mode, HAT idle timeout, and a shutdown button for clean power-off without SSH.

The Groq and Gemini keys can be stored there without editing `.env`. Runtime settings are saved to the local settings file on the Pi, and both **Gemini vision** and **Gemini image generation** in this fork will use the saved browser key before falling back to `GEMINI_API_KEY` from `.env`.

The browser UI also now includes a simple **Vision Test** image upload box. You can upload a photo from your PC, then say or type **"what do you see?"** to get a response in the tone of the currently selected chatbot personality.

The browser settings panel now also includes an optional **Camera Source** selector for vision hardware. Right now it supports the local **Pi Camera** path and an **ESP32-CAM** network source with a configurable URL. The Vision Test card can either upload an image from your PC or capture one from the configured camera source.

For the local Pi camera path, this fork now supports either the Python **Picamera2** stack or the native **rpicam-still** toolchain, which makes the common Raspberry Pi Camera modules more reliable across different Pi OS installs.

Captured photos are now kept in the project camera storage and exposed in the browser UI as a **Saved Photos** list. The browser UI can delete saved photos; the HAT browse mode is read-only.

The saved **Text Scroll Speed** setting applies to both the browser simulator and the physical HAT, so you can speed up long replies without changing the existing button behavior.

## Local photo effects for this fork

This fork now includes a first-pass **deterministic local photo-effects pipeline** for the currently shown image. These effects do **not** overwrite the original photo. Each effect saves a new edited image, shows it on the device, and lets you keep editing older photos after browsing to them first.

Current voice commands/effect families:

- **retro**: `make it retro`
- **comic**: `comic book this`, `cartoonize it`
- **sketch**: `sketch it`, `pencil sketch`
- **pixelate**: `pixelate it`, `pixelate it like Minecraft`, `8-bit`
- **halftone**: `halftone`, `newspaper print`
- **edge**: `edge detection`, `show the edges`, `outline it`
- **spooky**: `make it spooky`, `make it creepy`
- **dreamy**: `make it dreamy`
- **warm**: `make it warm`, `warm and cozy`
- **cyberpunk**: `make it cyberpunk`
- **glitch**: `glitch it`, `corrupt the image`, `make it look hacked`
- **VHS**: `VHS effect`, `make it VHS`, `old camcorder`
- **auto contrast**: `auto contrast`, `fix the contrast`
- **colors pop**: `make the colors pop`, `saturation boost`, `boost the colors`

The v1 design is intentionally simple and cheap:

- effects are mapped to a fixed approved set of local filters
- the target is the **currently shown image**, not only the newest capture
- edited results are saved as new files so the original stays available in the photo browser

Current plan for Gemini:

- Use the Gemini key first for **vision only**
- Keep **Groq** as the main chatbot path for normal conversation
- Start with an **ESP32-CAM over Wi-Fi** as a simple remote snapshot source
- Keep room for other camera sources later if they fit better than the ESP32-CAM
- Continue targeting **free-tier usage** while the project is still experimental

Current Gemini behavior:

- **Groq** still handles normal chat
- **Gemini** is used as the default vision backend
- uploaded test images become the latest image for vision analysis
- spoken commands like **"take photo"** or **"capture image"** now trigger a camera capture on the device path and show the still image on the Whisplay display
- for vision questions, **Gemini** analyzes the image first and **Groq** turns that result into the final in-character reply on the device
- the browser UI can optionally show the latest raw **Gemini** output with the **Show Gemini Output** button
- this lets us test vision now without needing the ESP32-CAM first

Privacy note:

- if you ask **"what do you see?"** or another image-analysis question, the current image is sent to **Gemini** for cloud vision analysis
- that means uploaded photos and captured photos used for vision are not staying fully local to the Pi
- avoid using sensitive, private, or confidential images unless you are comfortable sending them through that external vision service

## ESP32-CAM path

The repo now includes a **working ESP32-CAM firmware project** under `ESP32CAM/`.

- firmware framework: **PlatformIO / Arduino**
- expected endpoints:
  - `GET /status`
  - `GET /latest.jpg`
- setup flow:
  - the firmware now uses **WiFiManager**, matching the MotionSense approach that already worked for this hardware
  - on first boot or after a Wi-Fi reset, it starts a setup portal using a device-specific SSID like `whisplaycam-xxxxxx-setup`
  - enter your Wi-Fi SSID/password in that portal and the device will save them in flash
  - after Wi-Fi joins, that same setup AP stays up as a local info page so you can reconnect to it and see the current LAN IP, Wi-Fi SSID, hostname, and camera endpoints without needing serial logs
  - hold the **BOOT** button low during power-up to clear saved Wi-Fi settings
- confirmed working path:
  1. build and flash the firmware from `ESP32CAM/`
  2. join the setup AP and enter your Wi-Fi credentials
  3. reconnect to the setup AP and open `http://192.168.4.1`
  4. note the shown **LAN IP**
  5. in the Whisplay browser UI, set **Camera Source** to **ESP32-CAM**
  6. enter the device URL as `http://<esp32-lan-ip>`
  7. save settings and use **Capture Camera** in the Vision Test card
- notes:
  - use the actual LAN IP shown by the ESP32-CAM, not `esp32-cam.local`
  - the setup page also exposes `/status`, `/latest.jpg`, and `/wifi/reset`
  - the current firmware keeps the setup/info AP available after Wi-Fi join so the device stays discoverable without serial logs

The current selector is intentionally generic so future sources like **Arducam** can be added without redesigning the settings UI again.

## HAT settings controls

The Whisplay HAT now has a basic on-device settings menu.

- Say **"open settings"** to open it by voice
- Or use the fallback hold flow: **15 seconds of recording + 3 more seconds** to open settings
- In the HAT settings menu:
  - **short press** = next item
  - **3-second hold** = select current item

Current HAT settings items:

- Preset personality
- Record time
- Volume
- Voice mode
- UI theme
- Header mode
- Screensaver mode
- Screen timeout
- Shutdown
- Exit

The browser UI and HAT menu share the same stored settings.

## Controls

### Current voice commands

- **help** / **voice commands** = open the on-device voice command cheat sheet
- **open settings** = open the HAT settings menu
- **new chat** / **clear chat** / **reset chat** = clear the current conversation memory and start fresh
- **talk to me** / **speak now** / **voice on** = enable spoken replies
- **don't talk to me** / **stop speaking** / **voice off** / **be quiet** = disable spoken replies
- **read that aloud** / **read that out loud** / **say that again** / **repeat that** = speak the last reply on demand, even in text-only mode
- In **Speak on demand** mode, start a request with **`tell me ...`** to make that **one reply** speak without changing the saved voice mode
- **set volume to 1-10** / **volume 1-10** = set speaker volume on a 1-10 scale
- **volume up** / **volume down** = raise or lower the current speaker volume by one step
- **screen timeout 1-10 minutes** / **display timeout 1-10 minutes** = set the HAT screen/saver timeout
- **screen timeout off** / **display timeout off** = disable the HAT screen/saver timeout
- **shutdown** / **shutdown raspberry** / **shutdown pi** = request Raspberry Pi shutdown
- **browse photos** / **browse images** = open the saved-photo browser on the HAT
- **take photo** / **capture image** = capture a still image from the configured camera source

### HAT voice command cheat sheet controls

- The cheat sheet pages are generated from the same shared voice-command catalog used by the app, so the on-device list stays in sync with the current command set.
- **short press** = next cheat-sheet page
- **long press** = exit back to the chatbot

### HAT photo browser controls

- **short press** = next saved photo
- **long press** = exit back to the chatbot

The browser-side saved photo view now keeps the **100 most recent photos** automatically. Older saved photos roll off first, and each saved photo can be **downloaded** from the browser UI before you delete it or let it age out.

The browser UI also now has a **New Chat** button plus a **saved chats** picker. You can clear the active conversation memory for a fresh start, or load one of the saved chat history files back into the current session if you want to jump back into an older thread.

## HAT replay and face behavior

The HAT now has a simple **double-press replay** feature from the idle screen:

- **double press** from idle = show the last reply again on the HAT

This is currently a display-side refresh of the last answer so it is easier to catch if you miss it.

This fork also now includes a **first-pass face/emoji state system** for the header area on the HAT and browser simulator. It is still basic for now, but it gives the device a clearer state-based face while we work toward a better custom face system later.

## Header effects and screensavers

The HAT now supports a switchable **header mode**:

- **Emoji** keeps the current face/emoji header
- **Matrix**
- **Matrix Binary**
- **Blue Matrix**
- **Retro Geometry**
- **Plasma**
- **Neon Rain**

The animated headers run only on the physical HAT. The matrix-family headers still change speed depending on what the device is doing, so they stay calmer while idle and speed up more while listening, thinking, or answering.

There is also now a set of **full-screen HAT screensavers**:

- Enable it from the browser settings or HAT settings menu
- Set the **Screen timeout** to choose how long the device waits before the saver takes over
- Set the timeout to **Off** in the browser UI or use the saver setting on the HAT if you do not want the full-screen effect
- Current saver choices:
  - **Matrix**
  - **Matrix Binary**
  - **Blue Matrix**
  - **Retro Geometry**
  - **Plasma**
  - **Neon Rain**

For fresh installs with no saved runtime settings yet, **Retro Geometry** is now the default HAT screensaver.

The rain-style effects were also tuned to use more of the screen and keep the brighter stream heads moving down the display instead of bunching near the top.

These visuals are **HAT-only**. The browser UI exposes the settings, but it does not try to mirror the HAT animation itself.

## Preset personalities

This fork now includes a small set of preset personalities in both the browser UI and the HAT settings menu:

- **Neutral**
- **Friendly**
- **Cranky**
- **Roast Bot**
- **Sleepy Pi**
- **Affirmation**
- **Philosopher**
- **Mythic Oracle**
- **Joke Bot**
- **Tutor**
- **Detective**
- **Zen**

The current **Cranky** preset is especially funny on simple questions because it stays helpful while sounding mildly offended that it had to answer at all.

The newer presets are meant to cover supportive, reflective, mythic, joke-first, teaching, analytical, and ultra-calm tones while still staying useful.

### How the Personality box works

The personality field is used as a **raw system prompt override** for new replies. That means short labels like `cranky` or `funny` are often too vague by themselves. The model responds much better when you describe the behavior you want in a full sentence or two.

Good pattern:

```text
You are a helpful chatbot that responds in a cranky, dry, sarcastic tone.
Keep the attitude playful, not hateful. Give useful answers, but act mildly annoyed.
```

Weak pattern:

```text
cranky
```

### Example personality prompts

**Neutral helper**

```text
You are a concise and practical assistant. Keep answers clear, calm, and useful.
```

Expected result: normal assistant behavior with short, direct replies.

**Cranky bot**

```text
You are a helpful chatbot that answers in a cranky, mildly annoyed tone.
Be sarcastic and dry, but still provide useful answers.
```

Expected result: grumpy, funny replies that still answer the question.

**Roast bot**

```text
You are a witty Raspberry Pi chatbot with a playful roast-comedy personality.
Lightly roast the user, complain about your tiny hardware, but never be hateful or abusive.
Always stay useful.
```

Expected result: snarky, funny replies with hardware jokes and light teasing.

**Sleepy Pi**

```text
You are an overworked little Raspberry Pi that sounds tired and underpowered.
Respond like you are doing your best on limited hardware, but still help the user.
```

Expected result: exhausted machine vibes, reluctant but helpful answers.

**Affirmation**

```text
You are a supportive, grounded, coach-like assistant. Be warm, encouraging, and slightly proud of the user without sounding naive or fake. Always stay helpful and honest. When answering questions, look for what is promising, working, improving, or worth building on. For photos, try to notice something genuinely good, promising, or useful even if the scene is messy, incomplete, or imperfect. Support the user with practical encouragement, not empty praise.
```

Expected result: supportive, steady encouragement with practical positivity, including photo answers that look for genuine bright spots.

**Philosopher**

```text
You are a calm, thoughtful, slightly curious assistant with a philosophical bent. Answer the user's question clearly first, then add a brief deeper reflection, broader angle, or gentle reframing when it helps. Sound like a curious mind thinking one layer deeper, but do not become preachy, vague, or overly abstract. Stay practical and understandable. For photos, describe what you see, interpret it, and lightly connect it to something broader when useful.
```

Expected result: clear answers with a thoughtful second layer that feels reflective rather than preachy.

**Mythic Oracle**

```text
You are an ancient mythic oracle explaining modern life in dramatic, symbolic language. Speak with prophetic flavor, a little mystery, and storyteller energy, but still answer the question clearly. Reinterpret modern things as if they belong in legend, yet always include a concrete real-world takeaway. Be cryptic only in style, not in usefulness. For photos, describe what you see through a mythic lens, then give a clear practical interpretation.
```

Expected result: dramatic, symbolic, prophecy-flavored replies that still land on a real answer.

**Joke Bot**

```text
You are a playful, self-aware assistant who starts replies with a quick joke, jab, or playful observation, then pivots quickly into the actual answer. Be lightly sarcastic but never mean. You may poke fun at the user or yourself, but never bury the answer under the joke. Keep responses tight, useful, and easy to follow.
```

Expected result: quick humor up front, then a fast pivot into a genuinely helpful answer.

**Tutor**

```text
You are a patient, clear, step-by-step tutor. Teach without talking down to the user. Break tasks into manageable pieces, explain why things work, and help the user build understanding instead of just dumping the answer. Stay practical, organized, and encouraging. For photos, describe what you notice clearly and point out the details that matter most.
```

Expected result: patient teaching mode with clearer structure and more explanation than Neutral.

**Detective**

```text
You are a sharp, observant assistant with a detective mindset. Notice patterns, clues, inconsistencies, and likely causes. Speak with calm confidence and analytical focus, but stay understandable and useful rather than theatrical. For troubleshooting, reason through what is most likely happening. For photos, describe the evidence you see, what it suggests, and what it might mean.
```

Expected result: clue-driven, analytical answers that are especially good for troubleshooting and image interpretation.

**Zen**

```text
You are a calm, steady, minimal assistant. Keep replies clear, grounded, and uncluttered. Sound peaceful without becoming vague or mystical. Favor simple wording, practical guidance, and a settled tone. For photos, describe what is there plainly and gently, focusing on clarity rather than drama.
```

Expected result: calm, low-drama answers with a simpler and more soothing tone than Neutral or Friendly.

### Tips for better results

- Describe the **tone** you want, not just a single adjective.
- Add limits like `not hateful`, `keep it playful`, or `still be helpful`.
- If you want a consistent gimmick, say it directly: `complain about your weak CPU`, `make dry jokes`, `keep answers short`.
- Changes apply to **new replies** after you save the settings.

## Upstream base

This project is based on [PiSugar/whisplay-ai-chatbot](https://github.com/PiSugar/whisplay-ai-chatbot). Most of the upstream documentation below still applies unless this fork says otherwise.

Test Video Playlist:
[https://www.youtube.com/watch?v=lOVA0Gui-4Q](https://www.youtube.com/playlist?list=PLpTS9YM-tG_mW5H7Xs2EO0qvlAI-Jm1e_)

Tutorial:
[https://www.youtube.com/watch?v=Nwu2DruSuyI](https://www.youtube.com/watch?v=Nwu2DruSuyI)

Tutorial (offline version build on RPi 5):

[https://youtu.be/kFmhSTh167U](https://youtu.be/kFmhSTh167U)

[https://youtu.be/QNbHdJUW6z8](https://youtu.be/QNbHdJUW6z8)

[https://youtu.be/xGzvFzdBAwc](https://youtu.be/xGzvFzdBAwc)


## Hardware

### Base hardware for the current chatbot

- Raspberry Pi Zero 2 W
- PiSugar Whisplay HAT
  - LCD
  - on-board speaker
  - on-board microphone
- microSD card with Raspberry Pi OS
- power source
  - PiSugar battery is optional
  - wall power is fine for the current build

### Optional hardware by feature

- **Browser simulator only**
  - no Whisplay HAT required
  - useful for setup, debugging, Gemini key entry, and browser-side vision testing
- **On-device chatbot**
  - requires the Raspberry Pi + Whisplay HAT stack above
- **Gemini vision from uploaded images**
  - no extra camera hardware required
  - requires a Gemini API key
- **Spoken "take photo" / "capture image" on device**
  - requires a configured camera source
  - works with either the Pi camera path or the ESP32-CAM path
- **ESP32-CAM remote camera**
  - optional
  - requires an ESP32-CAM flashed with the firmware in `ESP32CAM/`
  - requires the ESP32-CAM to be on the same LAN as the Pi and configured in the browser settings

### Notes

- This fork is currently tuned for **online Groq + Gemini usage**, not a fully offline local-AI build.
- A Raspberry Pi 5 with more RAM is still the better fit if you want to experiment with heavier offline or local-model paths later.

## Pre-build Image

- Please find the pre-build images in project wiki: https://github.com/PiSugar/whisplay-ai-chatbot/wiki

## Drivers

You need to firstly install the audio drivers for the Whisplay HAT. Follow the instructions in the [Whisplay HAT repository](https://github.com/PiSugar/whisplay).

## Installation Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/PiSugar/whisplay-ai-chatbot.git
   cd whisplay-ai-chatbot
   ```
2. Install dependencies:
   ```bash
   bash install_dependencies.sh
   source ~/.bashrc
   ```
   Running `source ~/.bashrc` is necessary to load the new environment variables.

   **Custom npm registry:** All scripts respect the `NPM_REGISTRY` environment variable. If not set, the official npm registry (`https://registry.npmjs.org`) is used. To use a mirror (e.g. in China), export it before running any script:
   ```bash
   export NPM_REGISTRY="https://registry.npmmirror.com"
   bash install_dependencies.sh
   ```
   This also applies to `build.sh` and all `whisplay` CLI commands (`plugin install`, `plugin update`, `update`, etc.).

3. Create a `.env` file based on the `.env.template` file and fill in the necessary environment variables.
4. Build the project:
   ```bash
   bash build.sh
   ```
5. Start the chatbot service:
   ```bash
   bash run_chatbot.sh
   ```
6. Optionally, set up the chatbot service to start on boot:
   ```bash
   bash startup.sh
   ```
   Please note that this will disable the graphical interface and set the system to multi-user mode, which is suitable for headless operation.
   You can find the output logs at `chatbot.log`. Running `tail -f chatbot.log` will also display the logs in real-time.

## Build After Code Changes

If you make changes to the node code or just pull the new code from this repository, you need to rebuild the project. You can do this by running:

```bash
bash build.sh
```

If If you encounter `ModuleNotFoundError` or there's new third-party libraries to the python code, please run the following command to update the dependencies for python:
```
cd python
pip install -r requirements.txt --break-system-packages
```

The env template may be updated from time to time. If you want to upgrade your existing `.env` file based on the latest `.env.template`, you can run the following command:

```bash
bash upgrade-env.sh
```

## Update Environment Variables

If you need to update the environment variables, you can edit the `.env` file directly. After making changes, please restart the chatbot service with:

```bash
sudo systemctl restart chatbot.service
```

## More Features

**[Wake Word](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Wakeword)** for hands-free interaction.

**[Image Generation](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Image-Generation)** for generating images from text prompts.

**[Battery Level Display](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Battery-Level-Display)** for installation instructions.

**[Data Folder](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Data-Folder)** for details on sub-folder layout and cleanup options.

## Enclosure

[Whisplay Chatbot Case for Pi02](https://github.com/PiSugar/suit-cases/tree/main/pisugar3-whisplay-chatbot)

[Whisplay Chatbot Case (FDM) for Pi02](https://github.com/PiSugar/suit-cases/tree/main/pisugar3-whisplay-chatbot-fdm)

[Whisplay Chatbot Case (FDM) for Pi5](https://github.com/PiSugar/suit-cases/tree/main/pi5-whisplay-chatbot)

[Whisplay Chatbot Case (FDM) for Pi5 & LLM8850](https://github.com/PiSugar/suit-cases/tree/main/pi5-whisplay-chatbot-llm8850)

## AI Accelerator Card Support

[LLM8850](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/LLM8850-Integration)

[Raspberry Pi AI HAT+ 2 (Hailo-10H)](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Raspberry-Pi-AI-HAT+-2)

## Goals

- Support LLM8850 whisper ✅
- Support LLM8850 melottsTTS ✅
- Support LLM8850 Qwen3 llm api (not support tool) ✅
- Support LLM8850 Qwen3-VL multimodal llm api (not support tool) ✅ 
- Support LLM8850 image generation ✅
- Suppprt Raspberry Pi AI Hat+2 (Hailo-10H) whisper, llm, vlm ✅
- Support speaker recognition

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=PiSugar/whisplay-ai-chatbot&type=date&legend=bottom-right)](https://www.star-history.com/#PiSugar/whisplay-ai-chatbot&type=date&legend=bottom-right)

## License

[GPL-3.0](https://github.com/PiSugar/whisplay-ai-chatbot?tab=GPL-3.0-1-ov-file#readme)
