# Core1Display

Core1Display is a **wireless external chat screen** for the **M5Stack Core / Core1 form factor**.

It now supports both:

- **Groqputer** via `/api/companion/chat`
- **Whisplay** via `/api/state`

## What it does

- connects to Wi-Fi with its own setup AP
- polls one or two chat backends
- can follow **the last backend that changed** in **Auto** mode
- displays the latest user prompt and bot reply locally on the Core screen
- keeps a simple button-driven reader UI
- includes a lightweight idle screensaver system with Matrix, Ripple, and Entropy modes

## Setup

1. Build and flash the firmware to the Core1/Core device.
2. On first boot, join:
   - **`Core1Display-Setup`**
3. Open:
   - **`http://192.168.4.1`**
4. Save:
   - Wi-Fi SSID/password
   - Groqputer base URL (optional), for example:

```text
http://10.160.0.203
```

   - Whisplay base URL (optional), for example:

```text
http://10.160.0.136:17880
```

5. The Core1 firmware will reboot and start polling:

```text
Groqputer -> /api/companion/chat
Whisplay  -> /api/state
```

## Controls

- **Button A** = scroll up in the active pane
- **Button B** = scroll down in the active pane
- **Hold Button B** = cycle backend mode: `Auto -> Groqputer -> Whisplay`
- **Button C** = switch focus between prompt and reply
- **Hold Button A** = open on-device settings
- **Hold Button C** = reopen the setup AP

The on-device settings now include:

- border color
- font color
- auto scroll speed
- font size
- font family (`Built-in`, `Sans`, `SansBold`, `Mono`, `MonoBold`)
- screensaver mode (`Off`, `Matrix`, `Ripple`, `Entropy`)
- screensaver idle-on timeout
- screen-off timeout after the screensaver starts
- split/full reply display mode

## Companion API expectations

### Groqputer

Groqputer should be reachable on the same LAN and provide:

- latest user prompt
- latest bot reply
- compact status such as `thinking`, `reply_ready`, or `error`
- model tag and persona label

### Whisplay

Whisplay should be reachable on the same LAN and provide:

- `/api/state`
- current status text
- current emoji/status marker
- current reply/state text suitable for a live companion view

## Notes

- Any button press or a newly changed backend reply wakes the screen.
- The imported Matrix project was used as the **board/project starting point** and remains useful as a source of later screensaver ideas, but the current saver implementation is a compact native rewrite rather than a full engine import.
- Wireless polling is intentionally preferred over Grove-to-Grove display transport for safety, simplicity, and easier recovery.
- In **Auto** mode, Core1 shows whichever configured backend changed most recently from the Core1's point of view.
