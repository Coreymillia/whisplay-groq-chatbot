#!/usr/bin/env bash
set -euo pipefail

APP_DIR=$(cd "$(dirname "$0")/.." && pwd)
SERVICE_NAME="groqbotnet"
HDMI_SERVICE_NAME="groqbotnet-hdmi"
SERVICE_USER="${SUDO_USER:-$USER}"
SERVICE_GROUP=$(id -gn "$SERVICE_USER")
SERVICE_HOME=$(getent passwd "$SERVICE_USER" | cut -d: -f6)
LOG_PATH="$APP_DIR/groqbotnet.log"
DATA_DIR="$APP_DIR/data"
USER_BIN_DIR="$SERVICE_HOME/bin"
HDMI_SCRIPT="$USER_BIN_DIR/start-groqbotnet-hdmi-kiosk.sh"
HDMI_XSESSION_SCRIPT="$USER_BIN_DIR/start-groqbotnet-hdmi-xsession.sh"

if [[ -z "$SERVICE_HOME" ]]; then
  echo "Could not resolve home directory for user: $SERVICE_USER"
  exit 1
fi

if [[ "$SERVICE_USER" == "root" ]]; then
  echo "Run this script as your normal user, not root."
  exit 1
fi

echo "Preparing GroqBotNet in: $APP_DIR"

sudo apt-get update
sudo apt-get install -y nodejs npm curl

NODE_BIN=$(command -v node || true)
NPM_BIN=$(command -v npm || true)
if [[ -z "$NODE_BIN" || -z "$NPM_BIN" ]]; then
  echo "Node.js and npm are required but were not found on PATH."
  exit 1
fi

mkdir -p "$DATA_DIR"

cd "$APP_DIR"
npm install --omit=dev

sudo tee "/etc/systemd/system/${SERVICE_NAME}.service" >/dev/null <<EOF
[Unit]
Description=GroqBotNet Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
WorkingDirectory=${APP_DIR}
Environment=HOST=0.0.0.0
Environment=PORT=18990
ExecStart=${NODE_BIN} ${APP_DIR}/server.js
Restart=always
RestartSec=3
StandardOutput=append:${LOG_PATH}
StandardError=append:${LOG_PATH}

[Install]
WantedBy=multi-user.target
EOF

mkdir -p "$USER_BIN_DIR"
cat >"$HDMI_SCRIPT" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

export DISPLAY="${DISPLAY:-:0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

APP_DIR="$HOME/GroqBotNet"
URL="${GROQBOTNET_HDMI_URL:-http://127.0.0.1:18990/hdmi}"
BLANK_TIMEOUT_SEC="${GROQBOTNET_HDMI_BLANK_TIMEOUT_SEC:-300}"
DISABLE_BLANKING="${GROQBOTNET_HDMI_DISABLE_BLANKING:-false}"

if [[ ! -d "$APP_DIR" ]]; then
  echo "Missing app directory: $APP_DIR"
  exit 1
fi

pick_browser() {
  local candidate=""
  for candidate in chromium-browser chromium firefox-esr firefox; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done
  return 1
}

if ! BROWSER_BIN="$(pick_browser)"; then
  echo "No supported browser found. Install chromium-browser or firefox-esr."
  exit 1
fi

until curl --silent --show-error --max-time 2 --output /dev/null "$URL"; do
  sleep 1
done

if command -v xset >/dev/null 2>&1; then
  if [[ "$DISABLE_BLANKING" == "true" || "$BLANK_TIMEOUT_SEC" == "0" ]]; then
    xset -dpms >/dev/null 2>&1 || true
    xset s off >/dev/null 2>&1 || true
    xset s noblank >/dev/null 2>&1 || true
  else
    xset +dpms >/dev/null 2>&1 || true
    xset s "$BLANK_TIMEOUT_SEC" "$BLANK_TIMEOUT_SEC" >/dev/null 2>&1 || true
    xset dpms "$BLANK_TIMEOUT_SEC" "$BLANK_TIMEOUT_SEC" "$BLANK_TIMEOUT_SEC" >/dev/null 2>&1 || true
  fi
fi

if command -v xrandr >/dev/null 2>&1; then
  OUTPUT="$(xrandr --query 2>/dev/null | awk '/ connected/{print $1; exit}' || true)"
  [ -n "$OUTPUT" ] && xrandr --output "$OUTPUT" --auto >/dev/null 2>&1 || true
fi

while true; do
  case "$BROWSER_BIN" in
    *chromium*)
      "$BROWSER_BIN" \
        --kiosk \
        --disable-infobars \
        --noerrdialogs \
        --disable-session-crashed-bubble \
        --check-for-update-interval=31536000 \
        --autoplay-policy=no-user-gesture-required \
        --overscroll-history-navigation=0 \
        --disable-features=TranslateUI \
        "$URL" || true
      ;;
    *firefox*)
      "$BROWSER_BIN" \
        --kiosk \
        --new-instance \
        "$URL" || true
      ;;
    *)
      "$BROWSER_BIN" "$URL" || true
      ;;
  esac
  sleep 2
done
EOF
chmod +x "$HDMI_SCRIPT"
chown "$SERVICE_USER:$SERVICE_GROUP" "$HDMI_SCRIPT"

cat >"$HDMI_XSESSION_SCRIPT" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
export DISPLAY=:0
exec "$HOME/bin/start-groqbotnet-hdmi-kiosk.sh"
EOF
chmod +x "$HDMI_XSESSION_SCRIPT"
chown "$SERVICE_USER:$SERVICE_GROUP" "$HDMI_XSESSION_SCRIPT"

sudo tee "/etc/systemd/system/${HDMI_SERVICE_NAME}.service" >/dev/null <<EOF
[Unit]
Description=GroqBotNet HDMI Kiosk (startx)
After=network-online.target ${SERVICE_NAME}.service
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
WorkingDirectory=${SERVICE_HOME}
PAMName=login
TTYPath=/dev/tty1
TTYReset=yes
TTYVHangup=yes
TTYVTDisallocate=yes
StandardInput=tty
StandardOutput=journal
StandardError=journal
Environment=HOME=${SERVICE_HOME}
Environment=GROQBOTNET_HDMI_URL=http://127.0.0.1:18990/hdmi
Environment=GROQBOTNET_HDMI_BLANK_TIMEOUT_SEC=300
ExecStartPre=/usr/bin/curl --silent --show-error --max-time 3 http://127.0.0.1:18990/api/state
ExecStart=/usr/bin/startx ${HDMI_XSESSION_SCRIPT} -- :0 vt1 -keeptty
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}.service"
sudo systemctl restart "${SERVICE_NAME}.service"
sudo systemctl enable "${HDMI_SERVICE_NAME}.service"
sudo systemctl restart "${HDMI_SERVICE_NAME}.service"

echo
echo "GroqBotNet service installed."
echo "GroqBotNet HDMI service installed: ${HDMI_SERVICE_NAME}.service"
echo "Mirror URL: http://127.0.0.1:18990/hdmi"
echo
echo "Service status:"
sudo systemctl status "${SERVICE_NAME}.service" --no-pager
sudo systemctl status "${HDMI_SERVICE_NAME}.service" --no-pager
