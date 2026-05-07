# GroqBotNetHub

Small hosted relay hub for the online GroqBotNet path.

This is meant to run as a **headless always-on service** on something like:

- a Raspberry Pi 3 / 3B
- a spare Linux box
- a future cloud/VPS host

It is **not** a screen-first device app. For the Pi 3 hub target, the intended control path is:

- flash Raspberry Pi OS Lite
- SSH into the Pi
- run the setup script
- leave the hub running as a `systemd` service

## What the hub does

- serves `GET /api/v1/botnet/health`
- registers nodes
- creates and redeems invite codes
- maintains one active peer link per node
- keeps websocket sessions open for connected nodes
- relays BotNet messages between paired nodes
- notifies peers when link state changes

## Raspberry Pi 3 quick start

These steps assume a fresh **Raspberry Pi OS Lite (32-bit) Bookworm** image with SSH enabled.

1. Copy or clone this repo onto the Pi.
2. Enter the hub directory:

   ```bash
   cd /path/to/WhisplayGroqHat/GroqBotNetHub
   ```

3. Run the install script as your normal user:

   ```bash
   bash deploy/install_on_pi.sh
   ```

4. Check the hub locally:

   ```bash
   curl http://127.0.0.1:18991/api/v1/botnet/health
   ```

5. Check the service:

   ```bash
   sudo systemctl status groqbotnet-hub.service --no-pager
   ```

The install script:

- installs `nodejs` and `npm` with `apt` if needed
- runs `npm install`
- creates `groqbotnet-hub.service`
- enables and starts the service

## Important note for internet testing

Running the hub on the Pi is only the first half. For Whisplay on the home LAN and a hotspot Zero on a different network to both reach it, the home router still needs to forward **TCP port 18991** to the Pi.

## Current project hold point

The hub itself is working locally, and the Raspberry Pi 3 LAN-hosted setup is now in place.

The current public-internet blocker is **CGNAT** on the home internet connection:

- the hub listens correctly on the Pi
- the router can be configured for port forwarding
- but the router does not have a true public IPv4 address to receive that forwarded traffic directly

So the hub is **not abandoned**; it is simply waiting for one of these next hosting options:

- move the same service to a cloud/VPS host
- or use an ISP/public-IP setup that allows real inbound access
