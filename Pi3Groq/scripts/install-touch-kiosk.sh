#!/usr/bin/env bash
set -euo pipefail

APP_DIR=$(cd "$(dirname "$0")/.." && pwd)
SERVICE_NAME="pi3groq"
HDMI_SERVICE_NAME="pi3groq-hdmi"
SERVICE_USER="${SUDO_USER:-$USER}"
SERVICE_GROUP=$(id -gn "$SERVICE_USER")
SERVICE_HOME=$(getent passwd "$SERVICE_USER" | cut -d: -f6)
USER_BIN_DIR="$SERVICE_HOME/bin"
HDMI_SCRIPT="$USER_BIN_DIR/start-pi3groq-hdmi-kiosk.sh"
HDMI_XSESSION_SCRIPT="$USER_BIN_DIR/start-pi3groq-hdmi-xsession.sh"
CONFIG_PATH="/boot/firmware/config.txt"
XORG_CONF_DIR="/etc/X11/xorg.conf.d"
XORG_CONF_PATH="$XORG_CONF_DIR/99-pi3groq-fbdev.conf"
XWRAPPER_CONF_PATH="/etc/X11/Xwrapper.config"
V3D_CONF_PATH="$XORG_CONF_DIR/99-v3d.conf"
CUSTOM_TFT_OVERLAY_NAME="tft35a"
CUSTOM_TFT_OVERLAY_SOURCE="$APP_DIR/scripts/tft35a-overlay.dts"
CUSTOM_TFT_OVERLAY_TARGET="/boot/firmware/overlays/${CUSTOM_TFT_OVERLAY_NAME}.dtbo"
TFT_PROFILE="${PI3GROQ_TFT_PROFILE:-tft35a}"
TFT_SPEED="${PI3GROQ_TFT_SPEED:-16000000}"
TFT_ROTATE="${PI3GROQ_TFT_ROTATE:-270}"
TFT_EXTRA_PARAMS="${PI3GROQ_TFT_EXTRA_PARAMS:-swapxy=1}"

if [[ -z "$SERVICE_HOME" ]]; then
  echo "Could not resolve the home directory for user: $SERVICE_USER"
  exit 1
fi

if [[ "$SERVICE_USER" == "root" ]]; then
  echo "Run this script as your normal user, not root."
  exit 1
fi

echo "Preparing Pi3Groq touch kiosk in: $APP_DIR"
echo "Using TFT profile: $TFT_PROFILE"

sudo apt-get update
sudo apt-get install -y \
  chromium-browser \
  curl \
  device-tree-compiler \
  openbox \
  x11-xserver-utils \
  xinit \
  xserver-xorg \
  xserver-xorg-input-libinput \
  xserver-xorg-video-fbdev

if [[ "$TFT_PROFILE" == "$CUSTOM_TFT_OVERLAY_NAME" ]]; then
  if [[ ! -f "$CUSTOM_TFT_OVERLAY_SOURCE" ]]; then
    echo "Missing overlay source: $CUSTOM_TFT_OVERLAY_SOURCE" >&2
    exit 1
  fi
  sudo dtc -@ -I dts -O dtb -o "$CUSTOM_TFT_OVERLAY_TARGET" "$CUSTOM_TFT_OVERLAY_SOURCE"
fi

mkdir -p "$USER_BIN_DIR"

cat >"$HDMI_SCRIPT" <<EOF
#!/usr/bin/env bash
set -euo pipefail
exec "$APP_DIR/scripts/launch-hdmi-kiosk.sh"
EOF
chmod +x "$HDMI_SCRIPT"
chown "$SERVICE_USER:$SERVICE_GROUP" "$HDMI_SCRIPT"

cat >"$HDMI_XSESSION_SCRIPT" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

export DISPLAY=:0
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$HOME/.cache/pi3groq-runtime}"

mkdir -p "$XDG_RUNTIME_DIR"

if command -v openbox-session >/dev/null 2>&1; then
  openbox-session >/tmp/pi3groq-openbox.log 2>&1 &
  sleep 2
fi

exec "$HOME/bin/start-pi3groq-hdmi-kiosk.sh"
EOF
chmod +x "$HDMI_XSESSION_SCRIPT"
chown "$SERVICE_USER:$SERVICE_GROUP" "$HDMI_XSESSION_SCRIPT"

sudo mkdir -p "$XORG_CONF_DIR"
sudo tee "$XORG_CONF_PATH" >/dev/null <<'EOF'
Section "ServerFlags"
    Option "AutoAddGPU" "off"
    Option "AutoBindGPU" "off"
EndSection

Section "Device"
    Identifier "Pi3GroqFB"
    Driver "fbdev"
    Option "fbdev" "/dev/fb1"
EndSection

Section "Screen"
    Identifier "Pi3GroqScreen"
    Device "Pi3GroqFB"
EndSection
EOF

if [[ -f "$V3D_CONF_PATH" ]]; then
  sudo mv "$V3D_CONF_PATH" "${V3D_CONF_PATH}.pi3groq-disabled"
fi

sudo tee "$XWRAPPER_CONF_PATH" >/dev/null <<'EOF'
allowed_users=anybody
needs_root_rights=yes
EOF

TMP_CONFIG=$(mktemp)
awk '
  BEGIN { skip = 0 }
  /^# >>> Pi3Groq TFT >>>$/ { skip = 1; next }
  /^# <<< Pi3Groq TFT <<</ { skip = 0; next }
  /^dtoverlay=vc4-kms-v3d/ { next }
  /^dtoverlay=vc4-fkms-v3d/ { next }
  skip == 0 { print }
' "$CONFIG_PATH" >"$TMP_CONFIG"

cat >>"$TMP_CONFIG" <<EOF

# >>> Pi3Groq TFT >>>
dtparam=spi=on
EOF

if [[ "$TFT_PROFILE" == "$CUSTOM_TFT_OVERLAY_NAME" ]]; then
cat >>"$TMP_CONFIG" <<EOF
dtoverlay=${CUSTOM_TFT_OVERLAY_NAME},rotate=${TFT_ROTATE},speed=${TFT_SPEED}${TFT_EXTRA_PARAMS:+,${TFT_EXTRA_PARAMS}}
EOF
else
cat >>"$TMP_CONFIG" <<EOF
dtoverlay=fbtft,spi0-0,${TFT_PROFILE},reset_pin=24,dc_pin=25,rotate=${TFT_ROTATE},speed=${TFT_SPEED}${TFT_EXTRA_PARAMS:+,${TFT_EXTRA_PARAMS}}
dtoverlay=ads7846,cs=1,penirq=17,speed=2000000,xohms=100,swapxy=1
EOF
fi

cat >>"$TMP_CONFIG" <<'EOF'
# <<< Pi3Groq TFT <<<
EOF

sudo cp "$TMP_CONFIG" "$CONFIG_PATH"
rm -f "$TMP_CONFIG"

sudo tee "/etc/systemd/system/${HDMI_SERVICE_NAME}.service" >/dev/null <<EOF
[Unit]
Description=Pi3Groq touch display kiosk
After=network-online.target ${SERVICE_NAME}.service
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
WorkingDirectory=${SERVICE_HOME}
StandardOutput=journal
StandardError=journal
Environment=HOME=${SERVICE_HOME}
Environment=FRAMEBUFFER=/dev/fb1
Environment=PI3GROQ_HDMI_URL=http://127.0.0.1:18600/hdmi
ExecStartPre=/usr/bin/curl --silent --show-error --max-time 3 http://127.0.0.1:18600/api/settings
ExecStart=/usr/bin/xinit ${HDMI_XSESSION_SCRIPT} -- :0 -nocursor
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "${HDMI_SERVICE_NAME}.service"

echo
echo "Pi3Groq TFT overlay, Xorg fbdev config, and kiosk service are installed."
echo "Reboot the Pi to load the SPI display overlay:"
echo "  sudo reboot"
