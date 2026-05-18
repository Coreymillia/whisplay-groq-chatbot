#!/bin/bash

# Get user info
TARGET_USER=$(whoami)
TARGET_GROUP=$(id -gn "$TARGET_USER")
USER_HOME=$HOME
TARGET_UID=$(id -u $TARGET_USER)
REPO_DIR=$(cd "$(dirname "$0")" && pwd)
LOG_PATH="$REPO_DIR/chatbot.log"
ENV_FILE="$REPO_DIR/.env"
AUTOSTART_DIR="$USER_HOME/.config/autostart"
AUTOSTART_FILE="$AUTOSTART_DIR/whisplay-hdmi-kiosk.desktop"

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

# Make sure we do not return roon (in case user called the script with sudo)
if [ "$TARGET_USER" == "root" ]; then
    echo "Error: Please run this script as your normal user (WITHOUT sudo)."
    echo "The script will ask for sudo permissions only when writing the service file."
    exit 1
fi

HDMI_KIOSK_ENABLED=$(normalize_bool "$(get_env_value "WHISPLAY_HDMI_KIOSK_ENABLED")")

# if graphical interface is enabled, ask user whether to disable graphical interface
if [ "$HDMI_KIOSK_ENABLED" == "true" ]; then
    if [ "$(systemctl get-default)" != "graphical.target" ]; then
        echo "WHISPLAY_HDMI_KIOSK_ENABLED=true requires the graphical desktop to start on boot."
        read -p "Enable graphical.target so the HDMI kiosk can launch? (Y/n) " enable_gui
        if [[ ! "$enable_gui" =~ ^[Nn]$ ]]; then
            echo "Enabling graphical interface..."
            sudo systemctl set-default graphical.target
        else
            echo "Keeping the non-graphical boot target. HDMI kiosk launch will remain disabled until graphical.target is restored."
        fi
    else
        echo "Graphical interface is currently enabled for HDMI kiosk mode."
    fi
else
    if [ "$(systemctl get-default)" == "graphical.target" ]; then
        echo "Graphical interface is currently enabled."
        read -p "Disabling graphical interface is recommended for a headless setup. Do you want to disable the graphical interface? (y/n) " disable_gui
        if [[ "$disable_gui" == "y" ]]; then
            echo "Disabling graphical interface..."
            sudo systemctl set-default multi-user.target
            echo "Graphical interface disabled. You can re-enable it later with 'sudo systemctl set-default graphical.target'."
        else
            echo "Keeping graphical interface enabled."
        fi
    else
        echo "Graphical interface is currently disabled."
    fi
fi

echo "----------------------------------------"
echo "Detected User: $TARGET_USER"
echo "Detected Group: $TARGET_GROUP"
echo "Detected Home: $USER_HOME"
echo "Detected UID:  $TARGET_UID"
echo "Detected Repo: $REPO_DIR"

NODE_FOLDER=""
if NODE_BIN=$(command -v node 2>/dev/null); then
    NODE_FOLDER=$(dirname "$NODE_BIN")
    echo "Found Node at: $NODE_FOLDER"
else
    echo "Node is not on PATH in this shell. The service will rely on NVM_DIR=$USER_HOME/.nvm."
fi
echo "----------------------------------------"

chmod +x "$REPO_DIR/scripts/launch-hdmi-kiosk.sh"

# Create the service file
echo "Creating systemd service file..."
sudo tee /etc/systemd/system/chatbot.service > /dev/null <<EOF
[Unit]
Description=Chatbot Service
After=network-online.target sound.target
Wants=network-online.target sound.target

[Service]
Type=simple
User=$TARGET_USER
Group=$TARGET_GROUP
SupplementaryGroups=audio video gpio i2c spi input

WorkingDirectory=$REPO_DIR
ExecStart=/bin/bash $REPO_DIR/run_chatbot.sh

# Inject runtime environment
Environment=PATH=$NODE_FOLDER:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
Environment=HOME=$USER_HOME
Environment=XDG_RUNTIME_DIR=/run/user/$TARGET_UID
Environment=NODE_ENV=production
Environment=NVM_DIR=$USER_HOME/.nvm

# Audio permissions
PrivateDevices=no

# Logs
StandardOutput=append:$LOG_PATH
StandardError=append:$LOG_PATH

Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

if [ "$HDMI_KIOSK_ENABLED" == "true" ]; then
    echo "Installing HDMI kiosk autostart entry..."
    mkdir -p "$AUTOSTART_DIR"
    cat > "$AUTOSTART_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=Whisplay HDMI Kiosk
Comment=Launch the Whisplay HDMI mirror in Chromium
Exec=/bin/bash -lc 'exec "$REPO_DIR/scripts/launch-hdmi-kiosk.sh"'
Terminal=false
StartupNotify=false
X-GNOME-Autostart-enabled=true
EOF
else
    rm -f "$AUTOSTART_FILE"
fi

# start the service
echo "Service file created. Reloading Systemd..."
sudo systemctl daemon-reload
sudo systemctl enable chatbot.service
sudo systemctl restart chatbot.service

echo "Done! Chatbot is starting..."
echo "Checking status..."
sleep 2
sudo systemctl status chatbot.service --no-pager
