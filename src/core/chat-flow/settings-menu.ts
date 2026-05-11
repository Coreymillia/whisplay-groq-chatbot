import {
  getCurrentPersonalityPresetLabel,
  getNextPersonalityPreset,
} from "../../config/personality-presets";
import {
  HEADER_MODES,
  IDLE_TIMEOUT_OPTIONS,
  getHeaderModeLabel,
  getIdleTimeoutLabel,
  getRuntimeSettings,
  RECORD_TIMEOUT_OPTIONS,
  SCREENSAVER_MODES,
  getScreensaverModeLabel,
  saveRuntimeSettings,
  SCROLL_SPEED_OPTIONS,
  getScrollSpeedLevelLabel,
  VOLUME_LEVEL_OPTIONS,
  getVolumeLevelLabel,
  VOICE_MODES,
  UI_THEMES,
  getVoiceModeLabel,
  getUIThemeLabel,
} from "../../config/runtime-settings";
import { setVolumeByLevel } from "../../utils/volume";

export const SETTINGS_SELECT_HOLD_MS = 3_000;
export const SETTINGS_OPEN_GRACE_MS = 3_000;

export type SettingsMenuItemId =
  | "personality"
  | "record-time"
  | "scroll-speed"
  | "volume"
  | "voice-mode"
  | "ui-theme"
  | "header-mode"
  | "screensaver-mode"
  | "idle-timeout"
  | "shutdown"
  | "exit";

interface SettingsMenuItem {
  id: SettingsMenuItemId;
  label: string;
  value: string;
}

function getCompactVoiceLabel(value: string): string {
  switch (value) {
    case "speak-on-demand":
      return "OnDemand";
    case "voice-chat":
      return "Voice";
    case "text-only":
    default:
      return "Text";
  }
}

function getCompactThemeLabel(value: string): string {
  switch (value) {
    case "amber-terminal":
      return "Amber";
    case "matrix":
      return "Matrix";
    case "plasma":
      return "Plasma";
    case "default":
    default:
      return "Default";
  }
}

function getCompactHeaderLabel(value: string): string {
  switch (value) {
    case "vu-bars":
      return "VU Bars";
    case "vu-scope":
      return "VU Scope";
    case "vu-wave":
      return "VU Wave";
    case "matrix-binary":
      return "Binary";
    case "matrix-blue":
      return "Blue";
    case "retro-geometry":
      return "Retro";
    case "plasma":
      return "Plasma";
    case "neon-rain":
      return "Neon";
    case "matrix":
      return "Matrix";
    default:
      return "Emoji";
  }
}

function getCompactScreensaverLabel(value: string): string {
  switch (value) {
    case "matrix-binary":
      return "Binary";
    case "matrix-blue":
      return "Blue";
    case "retro-geometry":
      return "Retro";
    case "plasma":
      return "Plasma";
    case "neon-rain":
      return "Neon";
    case "matrix":
      return "Matrix";
    default:
      return "Off";
  }
}

export function buildSettingsMenuItems(): SettingsMenuItem[] {
  const settings = getRuntimeSettings();
  return [
    {
      id: "personality",
      label: "Preset",
      value: getCurrentPersonalityPresetLabel(
        settings.personalityPrompt,
        settings.savedPersonalityPresets,
      ),
    },
    {
      id: "record-time",
      label: "Record",
      value: `${settings.manualRecordMaxSec}s`,
    },
    {
      id: "scroll-speed",
      label: "Scroll",
      value: getScrollSpeedLevelLabel(settings.scrollSpeedLevel),
    },
    {
      id: "volume",
      label: "Volume",
      value: getVolumeLevelLabel(settings.volumeLevel),
    },
    {
      id: "voice-mode",
      label: "Voice",
      value: getCompactVoiceLabel(settings.voiceMode),
    },
    {
      id: "ui-theme",
      label: "Theme",
      value: getCompactThemeLabel(settings.uiTheme),
    },
    {
      id: "header-mode",
      label: "Header",
      value: getCompactHeaderLabel(settings.headerMode),
    },
    {
      id: "screensaver-mode",
      label: "Saver",
      value: getCompactScreensaverLabel(settings.screensaverMode),
    },
    {
      id: "idle-timeout",
      label: "Screen",
      value: getIdleTimeoutLabel(settings.idleTimeoutSec),
    },
    {
      id: "shutdown",
      label: "Shutdown",
      value: "",
    },
    {
      id: "exit",
      label: "Exit",
      value: "",
    },
  ];
}

export function renderSettingsMenu(selectedIndex: number, message = ""): string {
  const items = buildSettingsMenuItems();
  const lines = items.map((item, index) => {
    const marker = index === selectedIndex ? ">" : " ";
    const suffix = item.value ? ` ${item.value}` : "";
    return `${marker} ${item.label}${suffix}`;
  });
  const header = ["SETTINGS", "Tap=next Hold=ok"];
  if (message) {
    header.push(message);
  }
  return `${header.join("\n")}\n\n${lines.join("\n")}`;
}

function getNextRecordTimeoutSec(current: number): number {
  const currentIndex = RECORD_TIMEOUT_OPTIONS.findIndex(
    (value) => value === current,
  );
  if (currentIndex === -1) {
    return RECORD_TIMEOUT_OPTIONS[0];
  }
  return RECORD_TIMEOUT_OPTIONS[
    (currentIndex + 1) % RECORD_TIMEOUT_OPTIONS.length
  ];
}

function getNextVolumeLevel(current: number): number {
  const currentIndex = VOLUME_LEVEL_OPTIONS.findIndex((value) => value === current);
  if (currentIndex === -1) {
    return VOLUME_LEVEL_OPTIONS[0];
  }
  return VOLUME_LEVEL_OPTIONS[
    (currentIndex + 1) % VOLUME_LEVEL_OPTIONS.length
  ];
}

function getNextScrollSpeedLevel(current: number): number {
  const currentIndex = SCROLL_SPEED_OPTIONS.findIndex((value) => value === current);
  if (currentIndex === -1) {
    return SCROLL_SPEED_OPTIONS[0];
  }
  return SCROLL_SPEED_OPTIONS[
    (currentIndex + 1) % SCROLL_SPEED_OPTIONS.length
  ];
}

function getNextVoiceMode(current: string): string {
  const currentIndex = VOICE_MODES.findIndex((mode) => mode === current);
  if (currentIndex === -1) {
    return VOICE_MODES[0];
  }
  return VOICE_MODES[(currentIndex + 1) % VOICE_MODES.length];
}

function getNextUITheme(current: string): string {
  const currentIndex = UI_THEMES.findIndex((theme) => theme === current);
  if (currentIndex === -1) {
    return UI_THEMES[0];
  }
  return UI_THEMES[(currentIndex + 1) % UI_THEMES.length];
}

function getNextHeaderMode(current: string): string {
  const currentIndex = HEADER_MODES.findIndex((mode) => mode === current);
  if (currentIndex === -1) {
    return HEADER_MODES[0];
  }
  return HEADER_MODES[(currentIndex + 1) % HEADER_MODES.length];
}

function getNextScreensaverMode(current: string): string {
  const currentIndex = SCREENSAVER_MODES.findIndex((mode) => mode === current);
  if (currentIndex === -1) {
    return SCREENSAVER_MODES[0];
  }
  return SCREENSAVER_MODES[(currentIndex + 1) % SCREENSAVER_MODES.length];
}

function getNextIdleTimeout(current: number): number {
  const currentIndex = IDLE_TIMEOUT_OPTIONS.findIndex((value) => value === current);
  if (currentIndex === -1) {
    return IDLE_TIMEOUT_OPTIONS[0];
  }
  return IDLE_TIMEOUT_OPTIONS[
    (currentIndex + 1) % IDLE_TIMEOUT_OPTIONS.length
  ];
}

export function applySettingsMenuAction(id: SettingsMenuItemId): {
  message: string;
  shouldExit: boolean;
} {
  const settings = getRuntimeSettings();

  switch (id) {
    case "personality": {
      const nextPreset = getNextPersonalityPreset(
        settings.personalityPrompt,
        settings.savedPersonalityPresets,
      );
      saveRuntimeSettings({ personalityPrompt: nextPreset.prompt });
      return {
        message: `Preset ${nextPreset.label}`,
        shouldExit: false,
      };
    }
    case "record-time": {
      const nextValue = getNextRecordTimeoutSec(settings.manualRecordMaxSec);
      saveRuntimeSettings({ manualRecordMaxSec: nextValue });
      return {
        message: `Record ${nextValue}s`,
        shouldExit: false,
      };
    }
    case "scroll-speed": {
      const nextValue = getNextScrollSpeedLevel(settings.scrollSpeedLevel);
      saveRuntimeSettings({ scrollSpeedLevel: nextValue });
      return {
        message: `Scroll ${getScrollSpeedLevelLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "volume": {
      const nextValue = getNextVolumeLevel(settings.volumeLevel);
      saveRuntimeSettings({ volumeLevel: nextValue });
      setVolumeByLevel(nextValue);
      return {
        message: `Volume ${getVolumeLevelLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "voice-mode": {
      const nextValue = getNextVoiceMode(settings.voiceMode);
      saveRuntimeSettings({ voiceMode: nextValue });
      return {
        message: `Voice ${getCompactVoiceLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "ui-theme": {
      const nextValue = getNextUITheme(settings.uiTheme);
      saveRuntimeSettings({ uiTheme: nextValue });
      return {
        message: `Theme ${getCompactThemeLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "header-mode": {
      const nextValue = getNextHeaderMode(settings.headerMode);
      saveRuntimeSettings({ headerMode: nextValue });
      return {
        message: `Header ${getHeaderModeLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "screensaver-mode": {
      const nextValue = getNextScreensaverMode(settings.screensaverMode);
      saveRuntimeSettings({ screensaverMode: nextValue });
      return {
        message: `Saver ${getScreensaverModeLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "idle-timeout": {
      const nextValue = getNextIdleTimeout(settings.idleTimeoutSec);
      saveRuntimeSettings({ idleTimeoutSec: nextValue });
      return {
        message: `Screen ${getIdleTimeoutLabel(nextValue)}`,
        shouldExit: false,
      };
    }
    case "shutdown":
      return {
        message: "Hold again to shut down",
        shouldExit: false,
      };
    case "exit":
      return {
        message: "Closing",
        shouldExit: true,
      };
  }
}
