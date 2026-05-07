#!/usr/bin/env bash
set -euo pipefail

APP_DIR=$(cd "$(dirname "$0")/.." && pwd)
SERVICE_NAME="groqbotnet-hub"
SERVICE_USER="${SUDO_USER:-$USER}"
SERVICE_GROUP=$(id -gn "$SERVICE_USER")
LOG_PATH="$APP_DIR/groqbotnet-hub.log"
DATA_DIR="$APP_DIR/data"

if [[ "$SERVICE_USER" == "root" ]]; then
  echo "Run this script as your normal user, not root."
  exit 1
fi

echo "Preparing GroqBotNetHub in: $APP_DIR"

sudo apt-get update
sudo apt-get install -y nodejs npm

NODE_BIN=$(command -v node || true)
NPM_BIN=$(command -v npm || true)
if [[ -z "$NODE_BIN" || -z "$NPM_BIN" ]]; then
  echo "Node.js and npm are required but were not found on PATH after install."
  exit 1
fi

mkdir -p "$DATA_DIR"

cd "$APP_DIR"
npm install --omit=dev

sudo tee "/etc/systemd/system/${SERVICE_NAME}.service" >/dev/null <<EOF
[Unit]
Description=GroqBotNet hosted relay hub
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
WorkingDirectory=${APP_DIR}
Environment=PORT=18991
Environment=HOST=0.0.0.0
Environment=GROQBOTNET_HUB_DATA_DIR=${DATA_DIR}
ExecStart=${NPM_BIN} start
Restart=always
RestartSec=5
StandardOutput=append:${LOG_PATH}
StandardError=append:${LOG_PATH}

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}.service"
sudo systemctl restart "${SERVICE_NAME}.service"

echo
echo "GroqBotNetHub installed."
echo "Local health check: http://127.0.0.1:18991/api/v1/botnet/health"
echo "Service status:"
sudo systemctl status "${SERVICE_NAME}.service" --no-pager
