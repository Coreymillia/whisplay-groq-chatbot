#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "AppSettings.h"

namespace GroqWatch {

bool watchBootButtonHeld();
[[noreturn]] void runSetupPortalModal(Arduino_GFX *gfx, const AppSettings &currentSettings);

}  // namespace GroqWatch
