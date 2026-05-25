# Pi3Groq

Pi3Groq is the early Pi 3 companion project for WhisplayGroqHat.

## First-pass goal

- Pi 3B acts as a **keyboard-first companion**
- Whisplay remains the **chatbot brain**
- Pi3Groq talks to Whisplay over HTTP using:
  - `GET /api/state`
  - `POST /api/input/text`
- Pi3Groq serves its own local browser UI and a local `/hdmi` mirror page

## Current wiring target

- SCK: `GPIO11`
- MOSI: `GPIO10`
- CS: `GPIO8`
- DC: `GPIO25`
- RESET: `GPIO24`

The first software pass is browser-first so the local companion flow can be tested before the SPI TFT rendering layer is added.

## Current app layout

- `app.py` - small local web server for companion mode
- `web/` - Pi3Groq browser UI and HDMI mirror page
- `data/settings.json` - saved local Pi3Groq settings
- `scripts/launch-hdmi-kiosk.sh` - local HDMI kiosk launcher

## Local run

```bash
cd Pi3Groq
python3 app.py
```

Default URL:

```text
http://127.0.0.1:18600
```

## Saved settings

Pi3Groq currently saves:

- `mode` - reserved for future companion vs standalone support
- `companionBaseUrl` - Whisplay base URL, for example `http://10.160.0.136:17880`
- `pollIntervalMs` - browser polling interval

## HDMI mirror

Run the local server, then launch the companion mirror in Chromium:

```bash
cd Pi3Groq
bash scripts/launch-hdmi-kiosk.sh
```

By default it opens:

```text
http://127.0.0.1:18600/hdmi
```

## Near-term follow-up

- add the SPI TFT display adapter
- add keyboard handling tuned for the Pi companion hardware
- keep browser UI available even after the TFT path is added
- later add the dual-mode setup path:
  - companion mode by saved Whisplay URL
  - standalone mode by local API keys
