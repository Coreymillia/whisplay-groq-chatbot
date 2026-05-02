#!/bin/bash

# if graphical interface is enabled, ask user whether to disable graphical interface
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

# Get user info
TARGET_USER=$(whoami)
TARGET_GROUP=$(id -gn "$TARGET_USER")
USER_HOME=$HOME
TARGET_UID=$(id -u $TARGET_USER)
REPO_DIR=$(cd "$(dirname "$0")" && pwd)
LOG_PATH="$REPO_DIR/chatbot.log"

# Make sure we do not return roon (in case user called the script with sudo)
if [ "$TARGET_USER" == "root" ]; then
    echo "Error: Please run this script as your normal user (WITHOUT sudo)."
    echo "The script will ask for sudo permissions only when writing the service file."
    exit 1
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

# start the service
echo "Service file created. Reloading Systemd..."
sudo systemctl daemon-reload
sudo systemctl enable chatbot.service
sudo systemctl restart chatbot.service

echo "Done! Chatbot is starting..."
echo "Checking status..."
sleep 2
sudo systemctl status chatbot.service --no-pager
