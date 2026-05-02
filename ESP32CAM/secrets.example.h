#pragma once

// Compile-time Wi-Fi credentials are no longer required.
// The ESP32-CAM firmware now uses WiFiManager and stores credentials in flash.
// This file is kept only if you want to override the hostname prefix.
#define DEVICE_HOSTNAME_PREFIX "whisplaycam"
