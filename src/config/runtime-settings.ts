import fs from "fs";
import path from "path";

export type VoiceMode = "text-only" | "speak-on-demand" | "voice-chat";

export interface RuntimeSettings {
  groqApiKey: string;
  personalityPrompt: string;
  voiceMode: VoiceMode;
}

export interface RuntimeSettingsUpdate {
  groqApiKey?: string;
  clearGroqApiKey?: boolean;
  personalityPrompt?: string;
  voiceMode?: string;
}

const SETTINGS_PATH = path.resolve(
  __dirname,
  "../..",
  ".whisplay-groqhat-settings.json",
);

const DEFAULT_VOICE_MODE: VoiceMode = "text-only";
const VALID_VOICE_MODES = new Set<VoiceMode>([
  "text-only",
  "speak-on-demand",
  "voice-chat",
]);

function normalizeVoiceMode(value: unknown): VoiceMode {
  if (typeof value === "string" && VALID_VOICE_MODES.has(value as VoiceMode)) {
    return value as VoiceMode;
  }
  return DEFAULT_VOICE_MODE;
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

  writeSettingsFile(next);
  return next;
}

export function getPublicRuntimeSettings(): {
  groqApiKeyConfigured: boolean;
  personalityPrompt: string;
  voiceMode: VoiceMode;
} {
  const settings = loadSettingsFile();
  return {
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    personalityPrompt: settings.personalityPrompt,
    voiceMode: settings.voiceMode,
  };
}
