import fs from "fs";
import path from "path";

export type VoiceMode = "text-only" | "speak-on-demand" | "voice-chat";
export type UITheme = "default" | "matrix" | "plasma" | "amber-terminal";

export interface RuntimeSettings {
  groqApiKey: string;
  personalityPrompt: string;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
}

export interface RuntimeSettingsUpdate {
  groqApiKey?: string;
  clearGroqApiKey?: boolean;
  personalityPrompt?: string;
  voiceMode?: string;
  uiTheme?: string;
}

const SETTINGS_PATH = path.resolve(
  __dirname,
  "../..",
  ".whisplay-groqhat-settings.json",
);

const DEFAULT_VOICE_MODE: VoiceMode = "text-only";
const DEFAULT_UI_THEME: UITheme = "default";
const VALID_VOICE_MODES = new Set<VoiceMode>([
  "text-only",
  "speak-on-demand",
  "voice-chat",
]);
const VALID_UI_THEMES = new Set<UITheme>([
  "default",
  "matrix",
  "plasma",
  "amber-terminal",
]);

function normalizeVoiceMode(value: unknown): VoiceMode {
  if (typeof value === "string" && VALID_VOICE_MODES.has(value as VoiceMode)) {
    return value as VoiceMode;
  }
  return DEFAULT_VOICE_MODE;
}

function normalizeUITheme(value: unknown): UITheme {
  if (typeof value === "string" && VALID_UI_THEMES.has(value as UITheme)) {
    return value as UITheme;
  }
  return DEFAULT_UI_THEME;
}

function sanitizeSettings(input: Partial<RuntimeSettings> | null | undefined): RuntimeSettings {
  return {
    groqApiKey:
      typeof input?.groqApiKey === "string" ? input.groqApiKey.trim() : "",
    personalityPrompt:
      typeof input?.personalityPrompt === "string"
        ? input.personalityPrompt.trim()
        : "",
    voiceMode: normalizeVoiceMode(input?.voiceMode),
    uiTheme: normalizeUITheme(input?.uiTheme),
  };
}

function loadSettingsFile(): RuntimeSettings {
  try {
    if (!fs.existsSync(SETTINGS_PATH)) {
      return sanitizeSettings({});
    }
    const raw = fs.readFileSync(SETTINGS_PATH, "utf8");
    return sanitizeSettings(JSON.parse(raw));
  } catch (error) {
    console.warn("[settings] Failed to load runtime settings:", error);
    return sanitizeSettings({});
  }
}

function writeSettingsFile(settings: RuntimeSettings): void {
  fs.writeFileSync(SETTINGS_PATH, `${JSON.stringify(settings, null, 2)}\n`, "utf8");
}

export function getRuntimeSettings(): RuntimeSettings {
  return loadSettingsFile();
}

export function saveRuntimeSettings(
  update: RuntimeSettingsUpdate,
): RuntimeSettings {
  const current = loadSettingsFile();
  const next: RuntimeSettings = { ...current };

  if (update.clearGroqApiKey) {
    next.groqApiKey = "";
  } else if (typeof update.groqApiKey === "string") {
    const trimmed = update.groqApiKey.trim();
    if (trimmed) {
      next.groqApiKey = trimmed;
    }
  }

  if (typeof update.personalityPrompt === "string") {
    next.personalityPrompt = update.personalityPrompt.trim();
  }

  if (typeof update.voiceMode === "string") {
    next.voiceMode = normalizeVoiceMode(update.voiceMode);
  }

  if (typeof update.uiTheme === "string") {
    next.uiTheme = normalizeUITheme(update.uiTheme);
  }

  writeSettingsFile(next);
  return next;
}

export function getPublicRuntimeSettings(): {
  groqApiKeyConfigured: boolean;
  personalityPrompt: string;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
} {
  const settings = loadSettingsFile();
  return {
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    personalityPrompt: settings.personalityPrompt,
    voiceMode: settings.voiceMode,
    uiTheme: settings.uiTheme,
  };
}
