#!/usr/bin/env bash
set -eu

PORT="${PI3GROQ_PORT:-18600}"
URL="${PI3GROQ_HDMI_URL:-http://127.0.0.1:${PORT}/hdmi}"
DISPLAY_VALUE="${DISPLAY:-:0}"
XAUTHORITY_VALUE="${XAUTHORITY:-$HOME/.Xauthority}"

pick_browser() {
  local candidate=""
  for candidate in chromium-browser chromium google-chrome-stable google-chrome x-www-browser; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done
  return 1
}

if ! BROWSER_BIN="$(pick_browser)"; then
  echo "[Pi3GroqHDMI] No supported browser found."
  exit 1
fi

export DISPLAY="$DISPLAY_VALUE"
export XAUTHORITY="$XAUTHORITY_VALUE"

if command -v xset >/dev/null 2>&1; then
  xset s off >/dev/null 2>&1 || true
  xset -dpms >/dev/null 2>&1 || true
  xset s noblank >/dev/null 2>&1 || true
fi

if command -v curl >/dev/null 2>&1; then
  ATTEMPT=1
  until curl --silent --show-error --max-time 2 --output /dev/null "$URL"; do
    if [ "$ATTEMPT" -ge 90 ]; then
      echo "[Pi3GroqHDMI] Timed out waiting for $URL"
      exit 1
    fi
    ATTEMPT=$((ATTEMPT + 1))
    sleep 1
  done
else
  sleep 10
fi

exec "$BROWSER_BIN" \
  --kiosk \
  --disable-infobars \
  --noerrdialogs \
  --disable-session-crashed-bubble \
  --check-for-update-interval=31536000 \
  --autoplay-policy=no-user-gesture-required \
  --overscroll-history-navigation=0 \
  --disable-features=TranslateUI \
  "$URL"
