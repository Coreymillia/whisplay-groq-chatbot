import fs from "fs";
import path from "path";
import { getCurrentPersonalityPresetId } from "./personality-presets";

export type VoiceMode = "text-only" | "speak-on-demand" | "voice-chat";
export type UITheme = "default" | "matrix" | "plasma" | "amber-terminal";
export type HeaderMode =
  | "emoji"
  | "matrix"
  | "matrix-binary"
  | "matrix-blue"
  | "retro-geometry"
  | "plasma"
  | "neon-rain";
export type ScreensaverMode =
  | "off"
  | "matrix"
  | "matrix-binary"
  | "matrix-blue"
  | "retro-geometry"
  | "plasma"
  | "neon-rain";

export interface RuntimeSettings {
  groqApiKey: string;
  personalityPrompt: string;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
  manualRecordMaxSec: number;
  headerMode: HeaderMode;
  screensaverMode: ScreensaverMode;
  idleTimeoutSec: number;
}

export interface RuntimeSettingsUpdate {
  groqApiKey?: string;
  clearGroqApiKey?: boolean;
  personalityPrompt?: string;
  voiceMode?: string;
  uiTheme?: string;
  manualRecordMaxSec?: number;
  headerMode?: string;
  screensaverMode?: string;
  idleTimeoutSec?: number;
}

const SETTINGS_PATH = path.resolve(
  __dirname,
  "../..",
  ".whisplay-groqhat-settings.json",
);

const DEFAULT_VOICE_MODE: VoiceMode = "text-only";
const DEFAULT_UI_THEME: UITheme = "default";
const DEFAULT_MANUAL_RECORD_MAX_SEC = 15;
const DEFAULT_HEADER_MODE: HeaderMode = "emoji";
const DEFAULT_SCREENSAVER_MODE: ScreensaverMode = "retro-geometry";
const DEFAULT_IDLE_TIMEOUT_SEC = 120;
export const RECORD_TIMEOUT_OPTIONS = [10, 15, 20, 30, 45, 60];
export const IDLE_TIMEOUT_OPTIONS = [0, 30, 60, 120, 300, 600];
export const VOICE_MODES: VoiceMode[] = [
  "text-only",
  "speak-on-demand",
  "voice-chat",
];
export const UI_THEMES: UITheme[] = [
  "default",
  "matrix",
  "plasma",
  "amber-terminal",
];
export const HEADER_MODES: HeaderMode[] = [
  "emoji",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
];
export const SCREENSAVER_MODES: ScreensaverMode[] = [
  "off",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
];
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
const VALID_HEADER_MODES = new Set<HeaderMode>([
  "emoji",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
]);
const VALID_SCREENSAVER_MODES = new Set<ScreensaverMode>([
  "off",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
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

function normalizeHeaderMode(value: unknown): HeaderMode {
  if (
    typeof value === "string" &&
    VALID_HEADER_MODES.has(value as HeaderMode)
  ) {
    return value as HeaderMode;
  }
  return DEFAULT_HEADER_MODE;
}

function normalizeScreensaverMode(value: unknown): ScreensaverMode {
  if (
    typeof value === "string" &&
    VALID_SCREENSAVER_MODES.has(value as ScreensaverMode)
  ) {
    return value as ScreensaverMode;
  }
  return DEFAULT_SCREENSAVER_MODE;
}

function normalizeManualRecordMaxSec(value: unknown): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_MANUAL_RECORD_MAX_SEC;
  }
  return Math.max(5, Math.min(120, Math.round(numeric)));
}

function normalizeIdleTimeoutSec(value: unknown): number {
  const numeric =
    typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_IDLE_TIMEOUT_SEC;
  }
  return Math.max(0, Math.min(3600, Math.round(numeric)));
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
    manualRecordMaxSec: normalizeManualRecordMaxSec(input?.manualRecordMaxSec),
    headerMode: normalizeHeaderMode(input?.headerMode),
    screensaverMode: normalizeScreensaverMode(input?.screensaverMode),
    idleTimeoutSec: normalizeIdleTimeoutSec(input?.idleTimeoutSec),
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

  if (typeof update.manualRecordMaxSec === "number") {
    next.manualRecordMaxSec = normalizeManualRecordMaxSec(
      update.manualRecordMaxSec,
    );
  }

  if (typeof update.headerMode === "string") {
    next.headerMode = normalizeHeaderMode(update.headerMode);
  }

  if (typeof update.screensaverMode === "string") {
    next.screensaverMode = normalizeScreensaverMode(update.screensaverMode);
  }

  if (typeof update.idleTimeoutSec === "number") {
    next.idleTimeoutSec = normalizeIdleTimeoutSec(update.idleTimeoutSec);
  }

  writeSettingsFile(next);
  return next;
}

export function getVoiceModeLabel(value: string): string {
  switch (value) {
    case "speak-on-demand":
      return "On demand";
    case "voice-chat":
      return "Voice chat";
    case "text-only":
    default:
      return "Text only";
  }
}

export function getUIThemeLabel(value: string): string {
  switch (value) {
    case "matrix":
      return "Matrix";
    case "plasma":
      return "Plasma";
    case "amber-terminal":
      return "Amber";
    case "default":
    default:
      return "Default";
  }
}

export function getHeaderModeLabel(value: string): string {
  switch (value) {
    case "matrix-binary":
      return "Binary Matrix";
    case "matrix-blue":
      return "Blue Matrix";
    case "retro-geometry":
      return "Retro Geometry";
    case "plasma":
      return "Plasma";
    case "neon-rain":
      return "Neon Rain";
    case "matrix":
      return "Matrix";
    case "emoji":
    default:
      return "Emoji";
  }
}

export function getScreensaverModeLabel(value: string): string {
  switch (value) {
    case "matrix-binary":
      return "Binary Matrix";
    case "matrix-blue":
      return "Blue Matrix";
    case "retro-geometry":
      return "Retro Geometry";
    case "plasma":
      return "Plasma";
    case "neon-rain":
      return "Neon Rain";
    case "matrix":
      return "Matrix";
    case "off":
    default:
      return "Off";
  }
}

export function getIdleTimeoutLabel(value: number): string {
  return value <= 0 ? "Off" : `${value}s`;
}

export function getPublicRuntimeSettings(): {
  groqApiKeyConfigured: boolean;
  personalityPrompt: string;
  personalityPresetId: string;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
  manualRecordMaxSec: number;
  headerMode: HeaderMode;
  screensaverMode: ScreensaverMode;
  idleTimeoutSec: number;
} {
  const settings = loadSettingsFile();
  return {
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    personalityPrompt: settings.personalityPrompt,
    personalityPresetId: getCurrentPersonalityPresetId(
      settings.personalityPrompt,
    ),
    voiceMode: settings.voiceMode,
    uiTheme: settings.uiTheme,
    manualRecordMaxSec: settings.manualRecordMaxSec,
    headerMode: settings.headerMode,
    screensaverMode: settings.screensaverMode,
    idleTimeoutSec: settings.idleTimeoutSec,
  };
}
