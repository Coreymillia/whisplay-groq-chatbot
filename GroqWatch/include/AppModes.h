#pragma once

#include <Arduino.h>
#include "AppSettings.h"

namespace GroqWatch {

inline AppMode bootModeFromString(const char *str) {
    String s = str;
    s.trim();
    s.toLowerCase();
    if (s == "bot" || s == "chatbot") return AppMode::Bot;
    if (s == "ai" || s == "screensaver") return AppMode::AiScreensaver;
    return AppMode::Watch;
}

inline const char *bootModeToString(AppMode mode) {
    switch (mode) {
        case AppMode::Bot: return "bot";
        case AppMode::AiScreensaver: return "ai";
        default: return "watch";
    }
}

inline const char *modeLabel(AppMode mode) {
    switch (mode) {
        case AppMode::Bot: return "Bot";
        case AppMode::AiScreensaver: return "AI Show";
        case AppMode::Watch: return "Watch";
        case AppMode::Settings: return "Settings";
        default: return "?";
    }
}

}  // namespace GroqWatch
