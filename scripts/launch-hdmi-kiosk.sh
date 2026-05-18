#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="$REPO_DIR/.env"

get_env_value() {
  local key="$1"
  if [ -f "$ENV_FILE" ] && grep -Eq "^[[:space:]]*$key[[:space:]]*=" "$ENV_FILE"; then
    local val
    val=$(grep -E "^[[:space:]]*$key[[:space:]]*=" "$ENV_FILE" | tail -n1 | cut -d'=' -f2-)
    echo "$(echo "$val" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e 's/^"//' -e 's/"$//' -e "s/^'//" -e "s/'$//")"
  else
    echo ""
  fi
}

normalize_bool() {
  case "$(echo "${1:-}" | tr '[:upper:]' '[:lower:]')" in
    1|true|yes|on) echo "true" ;;
    *) echo "false" ;;
  esac
}

pick_browser() {
  local requested_browser="$1"
  if [ -n "$requested_browser" ]; then
    if command -v "$requested_browser" >/dev/null 2>&1; then
      command -v "$requested_browser"
      return 0
    fi
    if [ -x "$requested_browser" ]; then
      echo "$requested_browser"
      return 0
    fi
  fi

  local candidate=""
  for candidate in chromium-browser chromium google-chrome-stable google-chrome x-www-browser; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done

  return 1
}

HDMI_KIOSK_ENABLED="$(normalize_bool "$(get_env_value "WHISPLAY_HDMI_KIOSK_ENABLED")")"
if [ "$HDMI_KIOSK_ENABLED" != "true" ]; then
  echo "[HDMIKiosk] WHISPLAY_HDMI_KIOSK_ENABLED is false; nothing to launch."
  exit 0
fi

PORT="$(get_env_value "WHISPLAY_WEB_PORT")"
if [ -z "$PORT" ]; then
  PORT="17880"
fi

URL="$(get_env_value "WHISPLAY_HDMI_KIOSK_URL")"
if [ -z "$URL" ]; then
  URL="http://127.0.0.1:${PORT}/hdmi"
fi

DISPLAY_VALUE="$(get_env_value "WHISPLAY_HDMI_KIOSK_DISPLAY")"
if [ -z "$DISPLAY_VALUE" ]; then
  DISPLAY_VALUE="${DISPLAY:-:0}"
fi

XAUTHORITY_VALUE="$(get_env_value "WHISPLAY_HDMI_KIOSK_XAUTHORITY")"
if [ -z "$XAUTHORITY_VALUE" ]; then
  XAUTHORITY_VALUE="${XAUTHORITY:-$HOME/.Xauthority}"
fi

BROWSER_VALUE="$(get_env_value "WHISPLAY_HDMI_KIOSK_BROWSER")"
if ! BROWSER_BIN="$(pick_browser "$BROWSER_VALUE")"; then
  echo "[HDMIKiosk] No supported browser found. Install chromium-browser or set WHISPLAY_HDMI_KIOSK_BROWSER."
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
      echo "[HDMIKiosk] Timed out waiting for $URL"
      exit 1
    fi
    ATTEMPT=$((ATTEMPT + 1))
    sleep 1
  done
else
  sleep 15
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
