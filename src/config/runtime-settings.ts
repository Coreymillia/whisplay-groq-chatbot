import fs from "fs";
import path from "path";
import { getCurrentPersonalityPresetId } from "./personality-presets";

export type VoiceMode = "text-only" | "speak-on-demand" | "voice-chat";
export type UITheme = "default" | "matrix" | "plasma" | "amber-terminal";
export type CameraSource = "pi-camera" | "esp32-cam";
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
  geminiApiKey: string;
  personalityPrompt: string;
  volumeLevel: number;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
  cameraSource: CameraSource;
  esp32CamUrl: string;
  manualRecordMaxSec: number;
  headerMode: HeaderMode;
  screensaverMode: ScreensaverMode;
  idleTimeoutSec: number;
}

export interface RuntimeSettingsUpdate {
  groqApiKey?: string;
  clearGroqApiKey?: boolean;
  geminiApiKey?: string;
  personalityPrompt?: string;
  volumeLevel?: number;
  voiceMode?: string;
  uiTheme?: string;
  cameraSource?: string;
  esp32CamUrl?: string;
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
const DEFAULT_VOLUME_LEVEL = 9;
const DEFAULT_UI_THEME: UITheme = "default";
const DEFAULT_CAMERA_SOURCE: CameraSource = "pi-camera";
const DEFAULT_ESP32_CAM_URL = "http://esp32-cam.local";
const DEFAULT_MANUAL_RECORD_MAX_SEC = 15;
const DEFAULT_HEADER_MODE: HeaderMode = "emoji";
const DEFAULT_SCREENSAVER_MODE: ScreensaverMode = "retro-geometry";
const DEFAULT_IDLE_TIMEOUT_SEC = 120;
export const RECORD_TIMEOUT_OPTIONS = [10, 15, 20, 30, 45, 60];
export const VOLUME_LEVEL_OPTIONS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
export const IDLE_TIMEOUT_OPTIONS = [0, 60, 120, 180, 240, 300, 360, 420, 480, 540, 600];
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
export const CAMERA_SOURCES: CameraSource[] = [
  "pi-camera",
  "esp32-cam",
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
const VALID_CAMERA_SOURCES = new Set<CameraSource>([
  "pi-camera",
  "esp32-cam",
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

function normalizeVolumeLevel(value: unknown): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_VOLUME_LEVEL;
  }
  return Math.max(1, Math.min(10, Math.round(numeric)));
}

function normalizeUITheme(value: unknown): UITheme {
  if (typeof value === "string" && VALID_UI_THEMES.has(value as UITheme)) {
    return value as UITheme;
  }
  return DEFAULT_UI_THEME;
}

function normalizeCameraSource(value: unknown): CameraSource {
  if (
    typeof value === "string" &&
    VALID_CAMERA_SOURCES.has(value as CameraSource)
  ) {
    return value as CameraSource;
  }
  return DEFAULT_CAMERA_SOURCE;
}

function normalizeEsp32CamUrl(value: unknown): string {
  if (typeof value !== "string") {
    return DEFAULT_ESP32_CAM_URL;
  }
  const trimmed = value.trim();
  if (!trimmed) {
    return DEFAULT_ESP32_CAM_URL;
  }
  const withProtocol = /^https?:\/\//i.test(trimmed)
    ? trimmed
    : `http://${trimmed}`;
  return withProtocol.replace(/\/+$/, "");
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
    geminiApiKey:
      typeof input?.geminiApiKey === "string" ? input.geminiApiKey.trim() : "",
    personalityPrompt:
      typeof input?.personalityPrompt === "string"
        ? input.personalityPrompt.trim()
        : "",
    volumeLevel: normalizeVolumeLevel(input?.volumeLevel),
    voiceMode: normalizeVoiceMode(input?.voiceMode),
    uiTheme: normalizeUITheme(input?.uiTheme),
    cameraSource: normalizeCameraSource(input?.cameraSource),
    esp32CamUrl: normalizeEsp32CamUrl(input?.esp32CamUrl),
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

  if (typeof update.geminiApiKey === "string") {
    const trimmed = update.geminiApiKey.trim();
    if (trimmed) {
      next.geminiApiKey = trimmed;
    }
  }

  if (typeof update.personalityPrompt === "string") {
    next.personalityPrompt = update.personalityPrompt.trim();
  }

  if (typeof update.volumeLevel === "number") {
    next.volumeLevel = normalizeVolumeLevel(update.volumeLevel);
  }

  if (typeof update.voiceMode === "string") {
    next.voiceMode = normalizeVoiceMode(update.voiceMode);
  }

  if (typeof update.uiTheme === "string") {
    next.uiTheme = normalizeUITheme(update.uiTheme);
  }

  if (typeof update.cameraSource === "string") {
    next.cameraSource = normalizeCameraSource(update.cameraSource);
  }

  if (typeof update.esp32CamUrl === "string") {
    next.esp32CamUrl = normalizeEsp32CamUrl(update.esp32CamUrl);
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

export function getVolumeLevelLabel(value: number): string {
  const normalized = normalizeVolumeLevel(value);
  return `${normalized}/10`;
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

export function getCameraSourceLabel(value: string): string {
  switch (value) {
    case "esp32-cam":
      return "ESP32-CAM";
    case "pi-camera":
    default:
      return "Pi Camera";
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
  if (value <= 0) {
    return "Off";
  }
  const minutes = Math.round(value / 60);
  return `${minutes} min`;
}

export function getPublicRuntimeSettings(): {
  groqApiKeyConfigured: boolean;
  geminiApiKeyConfigured: boolean;
  personalityPrompt: string;
  personalityPresetId: string;
  volumeLevel: number;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
  cameraSource: CameraSource;
  esp32CamUrl: string;
  manualRecordMaxSec: number;
  headerMode: HeaderMode;
  screensaverMode: ScreensaverMode;
  idleTimeoutSec: number;
} {
  const settings = loadSettingsFile();
  return {
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    geminiApiKeyConfigured: Boolean(settings.geminiApiKey),
    personalityPrompt: settings.personalityPrompt,
    personalityPresetId: getCurrentPersonalityPresetId(
      settings.personalityPrompt,
    ),
    volumeLevel: settings.volumeLevel,
    voiceMode: settings.voiceMode,
    uiTheme: settings.uiTheme,
    cameraSource: settings.cameraSource,
    esp32CamUrl: settings.esp32CamUrl,
    manualRecordMaxSec: settings.manualRecordMaxSec,
    headerMode: settings.headerMode,
    screensaverMode: settings.screensaverMode,
    idleTimeoutSec: settings.idleTimeoutSec,
  };
}
