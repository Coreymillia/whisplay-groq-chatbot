import {
  getCurrentPersonalityPresetLabel,
  getNextPersonalityPreset,
} from "../../config/personality-presets";
import {
  getRuntimeSettings,
  RECORD_TIMEOUT_OPTIONS,
  saveRuntimeSettings,
  VOICE_MODES,
  UI_THEMES,
  getVoiceModeLabel,
  getUIThemeLabel,
} from "../../config/runtime-settings";

export const SETTINGS_SELECT_HOLD_MS = 3_000;
export const SETTINGS_OPEN_GRACE_MS = 3_000;

export type SettingsMenuItemId =
  | "personality"
  | "record-time"
  | "voice-mode"
  | "ui-theme"
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

export function buildSettingsMenuItems(): SettingsMenuItem[] {
  const settings = getRuntimeSettings();
  return [
    {
      id: "personality",
      label: "Preset",
      value: getCurrentPersonalityPresetLabel(settings.personalityPrompt),
    },
    {
      id: "record-time",
      label: "Record",
      value: `${settings.manualRecordMaxSec}s`,
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

export function applySettingsMenuAction(id: SettingsMenuItemId): {
  message: string;
  shouldExit: boolean;
} {
  const settings = getRuntimeSettings();

  switch (id) {
    case "personality": {
      const nextPreset = getNextPersonalityPreset(settings.personalityPrompt);
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
    case "exit":
      return {
        message: "Closing",
        shouldExit: true,
      };
  }
}
