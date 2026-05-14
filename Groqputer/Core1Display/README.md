# Core1Display

Core1Display is a **wireless external chat screen** for Groqputer, built for the **M5Stack Core / Core1 form factor**.

Instead of trying to mirror the Cardputer display over Grove, this firmware joins the same Wi-Fi network and polls Groqputer's lightweight companion API:

```text
/api/companion/chat
```

## What it does

- connects to Wi-Fi with its own setup AP
- polls the Groqputer companion endpoint
- displays the latest user prompt and bot reply locally on the Core screen
- keeps a simple button-driven reader UI
- leaves room for future idle/screensaver reuse from the original Matrix demo project

## Setup

1. Build and flash the firmware to the Core1/Core device.
2. On first boot, join:
   - **`Core1Display-Setup`**
3. Open:
   - **`http://192.168.4.1`**
4. Save:
   - Wi-Fi SSID/password
   - Groqputer base URL, for example:

```text
http://10.160.0.203
```

5. The Core1 firmware will reboot and start polling:

```text
http://10.160.0.203/api/companion/chat
```

## Controls

- **Button A** = scroll up in the active pane
- **Button B** = scroll down in the active pane
- **Button C** = switch focus between prompt and reply
- **Hold Button C** = reopen the setup AP

## Companion API expectations

Groqputer should be reachable on the same LAN and provide:

- latest user prompt
- latest bot reply
- compact status such as `thinking`, `reply_ready`, or `error`
- model tag and persona label

## Notes

- This is the **first-pass chat viewer firmware**, not the final idle/screensaver build.
- The imported Matrix project was used as the **board/project starting point** and remains useful as a source of later screensaver ideas.
- Wireless polling is intentionally preferred over Grove-to-Grove display transport for safety, simplicity, and easier recovery.
