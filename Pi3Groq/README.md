# Pi3Groq

Pi3Groq is the early Pi 3 companion project for WhisplayGroqHat.

## First-pass goal

- Pi 3B acts as a **keyboard-first companion**
- Whisplay remains the **chatbot brain**
- Pi3Groq talks to Whisplay over HTTP using:
  - `GET /api/state`
  - `POST /api/input/text`
- Pi3Groq serves its own local browser UI and a local `/hdmi` touch-display page

## Current wiring target

- SCK: `GPIO11`
- MOSI: `GPIO10`
- CS: `GPIO8`
- DC: `GPIO24`
- RESET: `GPIO25`

The first software pass is browser-first so the local companion flow can be tested before the SPI TFT rendering layer is added.

## Current touch display target

- 3.5-inch portrait display
- `320 x 480`
- XPT2046 touch controller
- Pi3Groq `/hdmi` is now the local touch-display page for this screen
- tested/default TFT path uses the LCDWiki-style `tft35a` overlay generated from `scripts/tft35a-overlay.dts`
  - ILI9486 panel with the board-specific init sequence
  - `reset=GPIO25`
  - `dc=GPIO24`
  - `pendown=GPIO17`
  - `regwidth=16`
  - `rotate=270`
  - `speed=16000000`

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
- `touchDisplayMode` - `mirror` or `slideshow-chat` for the touch screen
- `slideshowEnabled` - whether idle AI slideshow mode runs on the touch display
- `slideshowIntervalSec` - touch-display AI slide interval
- `chatReturnTimeoutSec` - how long the touch display stays on chat text before returning to the slideshow

## Touch display / HDMI page

Run the local server, then launch the touch-display page in Chromium:

```bash
cd Pi3Groq
bash scripts/launch-hdmi-kiosk.sh
```

By default it opens:

```text
http://127.0.0.1:18600/hdmi
```

Behavior:

- `mirror` mode keeps the full live status / emoji / text / image mirror on the TFT
- `slideshow-chat` mode shows fullscreen AI gallery slides while idle, then switches to chat text while Whisplay is active
- the return from chat text back to the slideshow is set in the browser UI with `chatReturnTimeoutSec`
- touch-friendly `Prev`, `Refresh`, and `Next` controls appear on the TFT while the slideshow is active
- the TFT slideshow and mirrored image views now use a Pi-side `320x480` composed frame so square Whisplay images are pre-fit for the portrait panel instead of leaving that sizing to browser CSS alone

## SPI TFT kiosk install

To install the physical 320x480 TFT path on a Pi:

```bash
cd Pi3Groq
bash scripts/install-touch-kiosk.sh
sudo reboot
```

To try a different TFT controller profile without editing the script:

```bash
PI3GROQ_TFT_PROFILE=hx8357d bash scripts/install-touch-kiosk.sh
sudo reboot
```

Useful alternatives for 3.5-inch SPI panels on this Pi image:

- `tft35a`
- `ili9486`
- `tontec35_9486`
- `hx8357d`

This installs:

- Chromium kiosk dependencies
- an Xorg fbdev config for the SPI framebuffer
- a `pi3groq-hdmi.service` kiosk service
- persistent `/boot/firmware/config.txt` overlays for:
  - the TFT panel on `spi0-0`
  - the XPT2046 touch controller on `spi0-1`

## Near-term follow-up

- add keyboard handling tuned for the Pi companion hardware
- keep browser UI available even after the TFT path is added
- later add the dual-mode setup path:
  - companion mode by saved Whisplay URL
  - standalone mode by local API keys
