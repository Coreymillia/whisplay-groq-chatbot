import fs from "fs";
import path from "path";
import {
  getCurrentPersonalityPresetId,
  getMatchingPersonalityPreset,
  type PersonalityPreset,
} from "./personality-presets";
import {
  DEFAULT_TEXT_LLM_MODEL,
  normalizeTextLlmModel,
} from "./text-llm-models";
import {
  normalizeEsp32AgentErrorPersonalityPrompt,
  normalizeEsp32AgentPersonalityPrompt,
} from "./esp32-agent-personality";
import {
  GEMINI_IMAGE_PRESET_OPTIONS,
  normalizeGeminiImagePreset,
  type GeminiImagePresetId,
} from "./gemini-image-presets";

export type VoiceMode = "text-only" | "speak-on-demand" | "voice-chat";
export type UITheme = "default" | "matrix" | "plasma" | "amber-terminal";
export type CameraSource = "pi-camera" | "esp32-cam";
export type HatTextColor =
  | "white"
  | "green"
  | "cyan"
  | "amber"
  | "pink"
  | "purple"
  | "blue"
  | "multi-line";
export type HeaderMode =
  | "emoji"
  | "matrix"
  | "matrix-binary"
  | "matrix-blue"
  | "retro-geometry"
  | "plasma"
  | "neon-rain"
  | "vu-bars"
  | "vu-scope"
  | "vu-wave";
export type GroqHeaderBadgeMode = "model" | "rpd-remaining";
export type GeminiImageModel =
  | "gemini-3.1-flash-lite-image"
  | "gemini-3.1-flash-image"
  | "gemini-3-pro-image"
  | "gemini-2.5-flash-image";
export type ScreensaverMode =
  | "off"
  | "ai-gallery"
  | "camera-roll"
  | "matrix"
  | "matrix-binary"
  | "matrix-blue"
  | "retro-geometry"
  | "plasma"
  | "neon-rain"
  | "random-shift"
  | "bouncing-balls"
  | "kaleidoscope"
  | "tetris-rain";

export type HatFontSize = "small" | "medium" | "large";
export type HatFontFamily =
  | "default"
  | "sans"
  | "mono"
  | "noto-mono"
  | "liberation-sans"
  | "liberation-mono"
  | "jetbrains-mono"
  | "ibm-plex-mono"
  | "press-start-2p";

export interface RuntimeSettings {
  groqApiKey: string;
  geminiApiKey: string;
  geminiImageModel: GeminiImageModel;
  geminiImagePreset: GeminiImagePresetId;
  geminiImageEditConfirmMode: boolean;
  geminiImagePromptHelperEnabled: boolean;
  geminiImagePromptHelperTokenLimit: number;
  geminiLowTierImageBalanceUsd: number;
  geminiLowTierAutoReloadEnabled: boolean;
  geminiLowTierAutoReloadThresholdUsd: number;
  geminiLowTierAutoReloadAmountUsd: number;
  llmModel: string;
  personalityPrompt: string;
  esp32AgentPersonalityPrompt: string;
  esp32AgentErrorPersonalityPrompt: string;
  savedPersonalityPresets: PersonalityPreset[];
  musicShuffle: boolean;
  volumeLevel: number;
  scrollSpeedLevel: number;
  hatScrollSpeedLevel: number;
  hatFontSize: HatFontSize;
  hatFontFamily: HatFontFamily;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
  cameraSource: CameraSource;
  esp32CamUrl: string;
  hatTextColor: HatTextColor;
  piCameraRotationDeg: number;
  esp32CamRotationDeg: number;
  manualRecordMaxSec: number;
  headerMode: HeaderMode;
  groqHeaderBadgeMode: GroqHeaderBadgeMode;
  screensaverMode: ScreensaverMode;
  idleTimeoutSec: number;
  screenBlankTimeoutSec: number;
  roomMonitorIntervalSec: number;
  weatherLatitude: number | null;
  weatherLongitude: number | null;
}

export interface RuntimeSettingsUpdate {
  groqApiKey?: string;
  clearGroqApiKey?: boolean;
  geminiApiKey?: string;
  geminiImageModel?: string;
  geminiImagePreset?: string;
  geminiImageEditConfirmMode?: boolean;
  geminiImagePromptHelperEnabled?: boolean;
  geminiImagePromptHelperTokenLimit?: number;
  geminiLowTierImageBalanceUsd?: number;
  geminiLowTierAutoReloadEnabled?: boolean;
  geminiLowTierAutoReloadThresholdUsd?: number;
  geminiLowTierAutoReloadAmountUsd?: number;
  llmModel?: string;
  personalityPrompt?: string;
  esp32AgentPersonalityPrompt?: string;
  esp32AgentErrorPersonalityPrompt?: string;
  savedPersonalityPresets?: PersonalityPreset[];
  musicShuffle?: boolean;
  volumeLevel?: number;
  scrollSpeedLevel?: number;
  hatScrollSpeedLevel?: number;
  hatFontSize?: HatFontSize;
  hatFontFamily?: HatFontFamily;
  voiceMode?: string;
  uiTheme?: string;
  cameraSource?: string;
  esp32CamUrl?: string;
  hatTextColor?: string;
  piCameraRotationDeg?: number;
  esp32CamRotationDeg?: number;
  manualRecordMaxSec?: number;
  headerMode?: string;
  groqHeaderBadgeMode?: string;
  screensaverMode?: string;
  idleTimeoutSec?: number;
  screenBlankTimeoutSec?: number;
  roomMonitorIntervalSec?: number;
  weatherLatitude?: number | null;
  weatherLongitude?: number | null;
}

const SETTINGS_PATH = path.resolve(
  __dirname,
  "../..",
  ".whisplay-groqhat-settings.json",
);

const DEFAULT_VOICE_MODE: VoiceMode = "text-only";
const DEFAULT_GEMINI_IMAGE_MODEL: GeminiImageModel = "gemini-2.5-flash-image";
const DEFAULT_GEMINI_IMAGE_PRESET: GeminiImagePresetId = "none";
const DEFAULT_GEMINI_IMAGE_EDIT_CONFIRM_MODE = false;
const DEFAULT_GEMINI_IMAGE_PROMPT_HELPER_ENABLED = false;
const DEFAULT_GEMINI_IMAGE_PROMPT_HELPER_TOKEN_LIMIT = 120;
export const GEMINI_LOW_TIER_IMAGE_COST_USD = 0.04;
const DEFAULT_GEMINI_LOW_TIER_IMAGE_BALANCE_USD = 0;
const DEFAULT_GEMINI_LOW_TIER_AUTO_RELOAD_ENABLED = false;
const DEFAULT_GEMINI_LOW_TIER_AUTO_RELOAD_THRESHOLD_USD = 1;
const DEFAULT_GEMINI_LOW_TIER_AUTO_RELOAD_AMOUNT_USD = 10;
const DEFAULT_LLM_MODEL = DEFAULT_TEXT_LLM_MODEL;
const DEFAULT_MUSIC_SHUFFLE = false;
const DEFAULT_VOLUME_LEVEL = 9;
const DEFAULT_SCROLL_SPEED_LEVEL = 5;
const DEFAULT_HAT_SCROLL_SPEED_LEVEL = 5;
const DEFAULT_HAT_FONT_SIZE: HatFontSize = "medium";
const DEFAULT_HAT_FONT_FAMILY: HatFontFamily = "default";
const DEFAULT_UI_THEME: UITheme = "default";
const DEFAULT_CAMERA_SOURCE: CameraSource = "pi-camera";
const DEFAULT_ESP32_CAM_URL = "http://esp32-cam.local";
const DEFAULT_HAT_TEXT_COLOR: HatTextColor = "white";
const DEFAULT_PI_CAMERA_ROTATION_DEG = 0;
const DEFAULT_ESP32_CAM_ROTATION_DEG = 0;
const DEFAULT_MANUAL_RECORD_MAX_SEC = 15;
const DEFAULT_HEADER_MODE: HeaderMode = "emoji";
const DEFAULT_GROQ_HEADER_BADGE_MODE: GroqHeaderBadgeMode = "model";
const DEFAULT_SCREENSAVER_MODE: ScreensaverMode = "retro-geometry";
const DEFAULT_IDLE_TIMEOUT_SEC = 120;
const DEFAULT_SCREEN_BLANK_TIMEOUT_SEC = 0;
const DEFAULT_ROOM_MONITOR_INTERVAL_SEC = 0;
const DEFAULT_WEATHER_LATITUDE = null;
const DEFAULT_WEATHER_LONGITUDE = null;
export const RECORD_TIMEOUT_OPTIONS = [10, 15, 20, 30, 45, 60];
export const VOLUME_LEVEL_OPTIONS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
export const SCROLL_SPEED_OPTIONS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
export const HAT_SCROLL_SPEED_OPTIONS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
export const HAT_FONT_SIZE_OPTIONS: HatFontSize[] = ["small", "medium", "large"];
export const HAT_FONT_FAMILY_OPTIONS: HatFontFamily[] = [
  "default",
  "sans",
  "mono",
  "noto-mono",
  "liberation-sans",
  "liberation-mono",
  "jetbrains-mono",
  "ibm-plex-mono",
  "press-start-2p",
];
export const IDLE_TIMEOUT_OPTIONS = [0, 60, 120, 180, 240, 300, 360, 420, 480, 540, 600];
export const SCREEN_BLANK_TIMEOUT_OPTIONS = [...IDLE_TIMEOUT_OPTIONS];
export const ROOM_MONITOR_INTERVAL_OPTIONS = [0, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600];
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
export const HAT_TEXT_COLORS: HatTextColor[] = [
  "white",
  "green",
  "cyan",
  "amber",
  "pink",
  "purple",
  "blue",
  "multi-line",
];
export const HEADER_MODES: HeaderMode[] = [
  "emoji",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
  "vu-bars",
  "vu-scope",
  "vu-wave",
];
export const GROQ_HEADER_BADGE_MODES: GroqHeaderBadgeMode[] = [
  "model",
  "rpd-remaining",
];
export const GEMINI_IMAGE_MODEL_OPTIONS: Array<{
  id: GeminiImageModel;
  label: string;
}> = [
  { id: "gemini-3.1-flash-lite-image", label: "Gemini 3.1 Flash Lite Image" },
  { id: "gemini-3.1-flash-image", label: "Gemini 3.1 Flash Image" },
  { id: "gemini-3-pro-image", label: "Gemini 3 Pro Image" },
  { id: "gemini-2.5-flash-image", label: "Gemini 2.5 Flash Image" },
];
export { GEMINI_IMAGE_PRESET_OPTIONS };
export const SCREENSAVER_MODES: ScreensaverMode[] = [
  "off",
  "ai-gallery",
  "camera-roll",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
  "random-shift",
  "bouncing-balls",
  "kaleidoscope",
  "tetris-rain",
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
const VALID_HAT_TEXT_COLORS = new Set<HatTextColor>([
  "white",
  "green",
  "cyan",
  "amber",
  "pink",
  "purple",
  "blue",
  "multi-line",
]);
const VALID_HEADER_MODES = new Set<HeaderMode>([
  "emoji",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
  "vu-bars",
  "vu-scope",
  "vu-wave",
]);
const VALID_SCREENSAVER_MODES = new Set<ScreensaverMode>([
  "off",
  "ai-gallery",
  "camera-roll",
  "matrix",
  "matrix-binary",
  "matrix-blue",
  "retro-geometry",
  "plasma",
  "neon-rain",
  "random-shift",
  "bouncing-balls",
  "kaleidoscope",
  "tetris-rain",
]);
const GEMINI_IMAGE_MODEL_ALIASES: Record<string, GeminiImageModel> = {
  "gemini-3.1-flash-image-preview": "gemini-3.1-flash-image",
  "gemini-3-pro-image-preview": "gemini-3-pro-image",
};

const VALID_GEMINI_IMAGE_MODELS = new Set<GeminiImageModel>(
  GEMINI_IMAGE_MODEL_OPTIONS.map((option) => option.id),
);
const VALID_HAT_FONT_SIZES = new Set<HatFontSize>([
  "small",
  "medium",
  "large",
]);
const VALID_HAT_FONT_FAMILIES = new Set<HatFontFamily>([
  "default",
  "sans",
  "mono",
  "noto-mono",
  "liberation-sans",
  "liberation-mono",
  "jetbrains-mono",
  "ibm-plex-mono",
  "press-start-2p",
]);

function normalizeVoiceMode(value: unknown): VoiceMode {
  if (typeof value === "string" && VALID_VOICE_MODES.has(value as VoiceMode)) {
    return value as VoiceMode;
  }
  return DEFAULT_VOICE_MODE;
}

function normalizePersonalityLabel(value: unknown): string {
  if (typeof value !== "string") {
    return "";
  }
  return value.trim().replace(/\s+/g, " ").slice(0, 40);
}

function normalizePersonalityPrompt(value: unknown): string {
  if (typeof value !== "string") {
    return "";
  }
  return value.trim();
}

function createSavedPersonalityId(label: string, existing: PersonalityPreset[]): string {
  const base =
    label
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "") || "favorite";
  let candidate = `saved-${base}`;
  let suffix = 2;
  const existingIds = new Set(existing.map((preset) => preset.id));
  while (existingIds.has(candidate)) {
    candidate = `saved-${base}-${suffix}`;
    suffix += 1;
  }
  return candidate;
}

function normalizeSavedPersonalityPresets(value: unknown): PersonalityPreset[] {
  if (!Array.isArray(value)) {
    return [];
  }
  const normalized: PersonalityPreset[] = [];
  const seenIds = new Set<string>();
  const seenPrompts = new Set<string>();
  for (const item of value) {
    const label = normalizePersonalityLabel((item as PersonalityPreset)?.label);
    const prompt = normalizePersonalityPrompt((item as PersonalityPreset)?.prompt);
    const id = typeof (item as PersonalityPreset)?.id === "string"
      ? (item as PersonalityPreset).id.trim()
      : "";
    if (!label || !prompt || !id || seenIds.has(id) || seenPrompts.has(prompt)) {
      continue;
    }
    seenIds.add(id);
    seenPrompts.add(prompt);
    normalized.push({ id, label, prompt });
  }
  return normalized;
}

function normalizeVolumeLevel(value: unknown): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_VOLUME_LEVEL;
  }
  return Math.max(1, Math.min(10, Math.round(numeric)));
}

function normalizeScrollSpeedLevel(value: unknown): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_SCROLL_SPEED_LEVEL;
  }
  return Math.max(1, Math.min(10, Math.round(numeric)));
}

function normalizeHatScrollSpeedLevel(value: unknown): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_HAT_SCROLL_SPEED_LEVEL;
  }
  return Math.max(1, Math.min(10, Math.round(numeric)));
}

function normalizeHatFontSize(value: unknown): HatFontSize {
  if (typeof value === "string" && VALID_HAT_FONT_SIZES.has(value as HatFontSize)) {
    return value as HatFontSize;
  }
  return DEFAULT_HAT_FONT_SIZE;
}

function normalizeHatFontFamily(value: unknown): HatFontFamily {
  if (
    typeof value === "string" &&
    VALID_HAT_FONT_FAMILIES.has(value as HatFontFamily)
  ) {
    return value as HatFontFamily;
  }
  return DEFAULT_HAT_FONT_FAMILY;
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

function normalizeHatTextColor(value: unknown): HatTextColor {
  if (typeof value === "string" && VALID_HAT_TEXT_COLORS.has(value as HatTextColor)) {
    return value as HatTextColor;
  }
  return DEFAULT_HAT_TEXT_COLOR;
}

function normalizeCameraRotationDeg(
  value: unknown,
  defaultValue: number,
): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return defaultValue;
  }
  const normalized = ((Math.round(numeric / 90) * 90) % 360 + 360) % 360;
  return normalized;
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

function normalizeGroqHeaderBadgeMode(value: unknown): GroqHeaderBadgeMode {
  if (typeof value !== "string") {
    return DEFAULT_GROQ_HEADER_BADGE_MODE;
  }
  return GROQ_HEADER_BADGE_MODES.includes(value as GroqHeaderBadgeMode)
    ? (value as GroqHeaderBadgeMode)
    : DEFAULT_GROQ_HEADER_BADGE_MODE;
}

export function normalizeGeminiImageModel(value: unknown): GeminiImageModel {
  if (typeof value === "string") {
    const normalizedValue =
      GEMINI_IMAGE_MODEL_ALIASES[value] || value;
    if (VALID_GEMINI_IMAGE_MODELS.has(normalizedValue as GeminiImageModel)) {
      return normalizedValue as GeminiImageModel;
    }
  }
  return DEFAULT_GEMINI_IMAGE_MODEL;
}

function normalizeGeminiImagePromptHelperTokenLimit(value: unknown): number {
  const numeric = typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_GEMINI_IMAGE_PROMPT_HELPER_TOKEN_LIMIT;
  }
  return Math.max(32, Math.min(512, Math.round(numeric)));
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

function normalizeScreenBlankTimeoutSec(value: unknown): number {
  const numeric =
    typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_SCREEN_BLANK_TIMEOUT_SEC;
  }
  return Math.max(0, Math.min(3600, Math.round(numeric)));
}

function normalizeRoomMonitorIntervalSec(value: unknown): number {
  const numeric =
    typeof value === "number" ? value : parseInt(String(value), 10);
  if (!Number.isFinite(numeric)) {
    return DEFAULT_ROOM_MONITOR_INTERVAL_SEC;
  }
  return Math.max(0, Math.min(86400, Math.round(numeric)));
}

function normalizeCoordinate(
  value: unknown,
  min: number,
  max: number,
): number | null {
  if (value === null || value === undefined || value === "") {
    return null;
  }
  const numeric = typeof value === "number" ? value : parseFloat(String(value));
  if (!Number.isFinite(numeric)) {
    return null;
  }
  return Math.max(min, Math.min(max, numeric));
}

function roundCurrency(value: number): number {
  return Math.round(value * 100) / 100;
}

function normalizeCurrencyValue(
  value: unknown,
  fallbackValue: number,
): number {
  const numeric = typeof value === "number" ? value : parseFloat(String(value));
  if (!Number.isFinite(numeric)) {
    return roundCurrency(fallbackValue);
  }
  return roundCurrency(Math.max(0, numeric));
}

function normalizeLlmModel(value: unknown): string {
  return normalizeTextLlmModel(value ?? DEFAULT_LLM_MODEL);
}

function sanitizeSettings(input: Partial<RuntimeSettings> | null | undefined): RuntimeSettings {
  return {
    groqApiKey:
      typeof input?.groqApiKey === "string" ? input.groqApiKey.trim() : "",
    geminiApiKey:
      typeof input?.geminiApiKey === "string" ? input.geminiApiKey.trim() : "",
    geminiImageModel: normalizeGeminiImageModel(input?.geminiImageModel),
    geminiImagePreset: normalizeGeminiImagePreset(input?.geminiImagePreset),
    geminiImageEditConfirmMode:
      typeof input?.geminiImageEditConfirmMode === "boolean"
        ? input.geminiImageEditConfirmMode
        : DEFAULT_GEMINI_IMAGE_EDIT_CONFIRM_MODE,
    geminiImagePromptHelperEnabled:
      typeof input?.geminiImagePromptHelperEnabled === "boolean"
        ? input.geminiImagePromptHelperEnabled
        : DEFAULT_GEMINI_IMAGE_PROMPT_HELPER_ENABLED,
    geminiImagePromptHelperTokenLimit: normalizeGeminiImagePromptHelperTokenLimit(
      input?.geminiImagePromptHelperTokenLimit,
    ),
    geminiLowTierImageBalanceUsd: normalizeCurrencyValue(
      input?.geminiLowTierImageBalanceUsd,
      DEFAULT_GEMINI_LOW_TIER_IMAGE_BALANCE_USD,
    ),
    geminiLowTierAutoReloadEnabled:
      typeof input?.geminiLowTierAutoReloadEnabled === "boolean"
        ? input.geminiLowTierAutoReloadEnabled
        : DEFAULT_GEMINI_LOW_TIER_AUTO_RELOAD_ENABLED,
    geminiLowTierAutoReloadThresholdUsd: normalizeCurrencyValue(
      input?.geminiLowTierAutoReloadThresholdUsd,
      DEFAULT_GEMINI_LOW_TIER_AUTO_RELOAD_THRESHOLD_USD,
    ),
    geminiLowTierAutoReloadAmountUsd: normalizeCurrencyValue(
      input?.geminiLowTierAutoReloadAmountUsd,
      DEFAULT_GEMINI_LOW_TIER_AUTO_RELOAD_AMOUNT_USD,
    ),
    llmModel: normalizeLlmModel(input?.llmModel),
    personalityPrompt:
      typeof input?.personalityPrompt === "string"
        ? input.personalityPrompt.trim()
        : "",
    esp32AgentPersonalityPrompt: normalizeEsp32AgentPersonalityPrompt(
      input?.esp32AgentPersonalityPrompt,
    ),
    esp32AgentErrorPersonalityPrompt: normalizeEsp32AgentErrorPersonalityPrompt(
      input?.esp32AgentErrorPersonalityPrompt,
    ),
    savedPersonalityPresets: normalizeSavedPersonalityPresets(
      input?.savedPersonalityPresets,
    ),
    musicShuffle:
      typeof input?.musicShuffle === "boolean"
        ? input.musicShuffle
        : DEFAULT_MUSIC_SHUFFLE,
    volumeLevel: normalizeVolumeLevel(input?.volumeLevel),
    scrollSpeedLevel: normalizeScrollSpeedLevel(input?.scrollSpeedLevel),
    hatScrollSpeedLevel: normalizeHatScrollSpeedLevel(input?.hatScrollSpeedLevel),
    hatFontSize: normalizeHatFontSize(input?.hatFontSize),
    hatFontFamily: normalizeHatFontFamily(input?.hatFontFamily),
    voiceMode: normalizeVoiceMode(input?.voiceMode),
    uiTheme: normalizeUITheme(input?.uiTheme),
    cameraSource: normalizeCameraSource(input?.cameraSource),
    esp32CamUrl: normalizeEsp32CamUrl(input?.esp32CamUrl),
    hatTextColor: normalizeHatTextColor(input?.hatTextColor),
    piCameraRotationDeg: normalizeCameraRotationDeg(
      input?.piCameraRotationDeg,
      DEFAULT_PI_CAMERA_ROTATION_DEG,
    ),
    esp32CamRotationDeg: normalizeCameraRotationDeg(
      input?.esp32CamRotationDeg,
      DEFAULT_ESP32_CAM_ROTATION_DEG,
    ),
    manualRecordMaxSec: normalizeManualRecordMaxSec(input?.manualRecordMaxSec),
    headerMode: normalizeHeaderMode(input?.headerMode),
    groqHeaderBadgeMode: normalizeGroqHeaderBadgeMode(input?.groqHeaderBadgeMode),
    screensaverMode: normalizeScreensaverMode(input?.screensaverMode),
    idleTimeoutSec: normalizeIdleTimeoutSec(input?.idleTimeoutSec),
    screenBlankTimeoutSec: normalizeScreenBlankTimeoutSec(input?.screenBlankTimeoutSec),
    roomMonitorIntervalSec: normalizeRoomMonitorIntervalSec(input?.roomMonitorIntervalSec),
    weatherLatitude: normalizeCoordinate(input?.weatherLatitude, -90, 90) ?? DEFAULT_WEATHER_LATITUDE,
    weatherLongitude: normalizeCoordinate(input?.weatherLongitude, -180, 180) ?? DEFAULT_WEATHER_LONGITUDE,
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

  if (typeof update.geminiImageModel === "string") {
    next.geminiImageModel = normalizeGeminiImageModel(update.geminiImageModel);
  }

  if (typeof update.geminiImagePreset === "string") {
    next.geminiImagePreset = normalizeGeminiImagePreset(update.geminiImagePreset);
  }

  if (typeof update.geminiImageEditConfirmMode === "boolean") {
    next.geminiImageEditConfirmMode = update.geminiImageEditConfirmMode;
  }

  if (typeof update.geminiImagePromptHelperEnabled === "boolean") {
    next.geminiImagePromptHelperEnabled = update.geminiImagePromptHelperEnabled;
  }

  if (typeof update.geminiImagePromptHelperTokenLimit === "number") {
    next.geminiImagePromptHelperTokenLimit = normalizeGeminiImagePromptHelperTokenLimit(
      update.geminiImagePromptHelperTokenLimit,
    );
  }

  if (typeof update.geminiLowTierImageBalanceUsd === "number") {
    next.geminiLowTierImageBalanceUsd = normalizeCurrencyValue(
      update.geminiLowTierImageBalanceUsd,
      current.geminiLowTierImageBalanceUsd,
    );
  }

  if (typeof update.geminiLowTierAutoReloadEnabled === "boolean") {
    next.geminiLowTierAutoReloadEnabled = update.geminiLowTierAutoReloadEnabled;
  }

  if (typeof update.geminiLowTierAutoReloadThresholdUsd === "number") {
    next.geminiLowTierAutoReloadThresholdUsd = normalizeCurrencyValue(
      update.geminiLowTierAutoReloadThresholdUsd,
      current.geminiLowTierAutoReloadThresholdUsd,
    );
  }

  if (typeof update.geminiLowTierAutoReloadAmountUsd === "number") {
    next.geminiLowTierAutoReloadAmountUsd = normalizeCurrencyValue(
      update.geminiLowTierAutoReloadAmountUsd,
      current.geminiLowTierAutoReloadAmountUsd,
    );
  }

  if (typeof update.llmModel === "string") {
    next.llmModel = normalizeLlmModel(update.llmModel);
  }

  if (typeof update.personalityPrompt === "string") {
    next.personalityPrompt = update.personalityPrompt.trim();
  }

  if (typeof update.esp32AgentPersonalityPrompt === "string") {
    next.esp32AgentPersonalityPrompt = normalizeEsp32AgentPersonalityPrompt(
      update.esp32AgentPersonalityPrompt,
    );
  }

  if (typeof update.esp32AgentErrorPersonalityPrompt === "string") {
    next.esp32AgentErrorPersonalityPrompt =
      normalizeEsp32AgentErrorPersonalityPrompt(
        update.esp32AgentErrorPersonalityPrompt,
      );
  }

  if (Array.isArray(update.savedPersonalityPresets)) {
    next.savedPersonalityPresets = normalizeSavedPersonalityPresets(
      update.savedPersonalityPresets,
    );
  }

  if (typeof update.musicShuffle === "boolean") {
    next.musicShuffle = update.musicShuffle;
  }

  if (typeof update.volumeLevel === "number") {
    next.volumeLevel = normalizeVolumeLevel(update.volumeLevel);
  }

  if (typeof update.scrollSpeedLevel === "number") {
    next.scrollSpeedLevel = normalizeScrollSpeedLevel(update.scrollSpeedLevel);
  }

  if (typeof update.hatScrollSpeedLevel === "number") {
    next.hatScrollSpeedLevel = normalizeHatScrollSpeedLevel(update.hatScrollSpeedLevel);
  }

  if (typeof update.hatFontSize === "string") {
    next.hatFontSize = normalizeHatFontSize(update.hatFontSize);
  }

  if (typeof update.hatFontFamily === "string") {
    next.hatFontFamily = normalizeHatFontFamily(update.hatFontFamily);
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

  if (typeof update.hatTextColor === "string") {
    next.hatTextColor = normalizeHatTextColor(update.hatTextColor);
  }

  if (typeof update.piCameraRotationDeg === "number") {
    next.piCameraRotationDeg = normalizeCameraRotationDeg(
      update.piCameraRotationDeg,
      DEFAULT_PI_CAMERA_ROTATION_DEG,
    );
  }

  if (typeof update.esp32CamRotationDeg === "number") {
    next.esp32CamRotationDeg = normalizeCameraRotationDeg(
      update.esp32CamRotationDeg,
      DEFAULT_ESP32_CAM_ROTATION_DEG,
    );
  }

  if (typeof update.manualRecordMaxSec === "number") {
    next.manualRecordMaxSec = normalizeManualRecordMaxSec(
      update.manualRecordMaxSec,
    );
  }

  if (typeof update.headerMode === "string") {
    next.headerMode = normalizeHeaderMode(update.headerMode);
  }

  if (typeof update.groqHeaderBadgeMode === "string") {
    next.groqHeaderBadgeMode = normalizeGroqHeaderBadgeMode(
      update.groqHeaderBadgeMode,
    );
  }

  if (typeof update.screensaverMode === "string") {
    next.screensaverMode = normalizeScreensaverMode(update.screensaverMode);
  }

  if (typeof update.idleTimeoutSec === "number") {
    next.idleTimeoutSec = normalizeIdleTimeoutSec(update.idleTimeoutSec);
  }

  if (typeof update.screenBlankTimeoutSec === "number") {
    next.screenBlankTimeoutSec = normalizeScreenBlankTimeoutSec(update.screenBlankTimeoutSec);
  }

  if (typeof update.roomMonitorIntervalSec === "number") {
    next.roomMonitorIntervalSec = normalizeRoomMonitorIntervalSec(update.roomMonitorIntervalSec);
  }

  if ("weatherLatitude" in update) {
    next.weatherLatitude = normalizeCoordinate(update.weatherLatitude, -90, 90);
  }

  if ("weatherLongitude" in update) {
    next.weatherLongitude = normalizeCoordinate(update.weatherLongitude, -180, 180);
  }

  const sanitized = sanitizeSettings(next);
  writeSettingsFile(sanitized);
  return sanitized;
}

export function saveNamedPersonalityPreset(
  label: string,
  prompt: string,
): RuntimeSettings {
  const normalizedLabel = normalizePersonalityLabel(label);
  const normalizedPrompt = normalizePersonalityPrompt(prompt);
  if (!normalizedLabel) {
    throw new Error("Enter a personality name first.");
  }
  if (!normalizedPrompt) {
    throw new Error("Enter a personality prompt first.");
  }

  const current = loadSettingsFile();
  const builtinMatch = getMatchingPersonalityPreset(normalizedPrompt);
  if (builtinMatch) {
    throw new Error(
      `That prompt already matches the built-in ${builtinMatch.label} preset.`,
    );
  }

  const nextSaved = [...current.savedPersonalityPresets];
  const existingByPrompt = nextSaved.find(
    (preset) => preset.prompt.trim() === normalizedPrompt,
  );
  const existingByLabel = nextSaved.find(
    (preset) => preset.label.trim().toLowerCase() === normalizedLabel.toLowerCase(),
  );

  if (existingByPrompt) {
    existingByPrompt.label = normalizedLabel;
  } else if (existingByLabel) {
    existingByLabel.label = normalizedLabel;
    existingByLabel.prompt = normalizedPrompt;
  } else {
    nextSaved.push({
      id: createSavedPersonalityId(normalizedLabel, nextSaved),
      label: normalizedLabel,
      prompt: normalizedPrompt,
    });
  }

  const next = sanitizeSettings({
    ...current,
    personalityPrompt: normalizedPrompt,
    savedPersonalityPresets: nextSaved,
  });
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

export function getScrollSpeedLevelLabel(value: number): string {
  const normalized = normalizeScrollSpeedLevel(value);
  return `${normalized}/10`;
}

export function getScrollSpeedFactor(value: number): number {
  const normalized = normalizeScrollSpeedLevel(value);
  return 0.4 + (normalized - 1) * 0.1777777778;
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
    case "vu-bars":
      return "VU Bars";
    case "vu-scope":
      return "VU Scope";
    case "vu-wave":
      return "VU Wave";
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

export function getHatTextColorLabel(value: string): string {
  switch (value) {
    case "green":
      return "Green";
    case "cyan":
      return "Cyan";
    case "amber":
      return "Amber";
    case "pink":
      return "Pink";
    case "purple":
      return "Purple";
    case "blue":
      return "Blue";
    case "multi-line":
      return "Multi Color (Per Line)";
    case "white":
    default:
      return "White";
  }
}

export function getCameraRotationLabel(value: number): string {
  const normalized = normalizeCameraRotationDeg(value, 0);
  return `${normalized}°`;
}

export function getScreensaverModeLabel(value: string): string {
  switch (value) {
    case "ai-gallery":
      return "AI Screensaver";
    case "camera-roll":
      return "Camera Roll";
    case "random-shift":
      return "Random Shift";
    case "bouncing-balls":
      return "Bouncing Balls";
    case "kaleidoscope":
      return "Kaleidoscope";
    case "tetris-rain":
      return "Tetris Rain";
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

export function getRoomMonitorIntervalLabel(value: number): string {
  if (value <= 0) {
    return "Off";
  }
  if (value < 60) {
    return `${value} sec`;
  }
  const minutes = value / 60;
  return `${minutes % 1 === 0 ? minutes : minutes.toFixed(1)} min`;
}

export function getPublicRuntimeSettings(): {
  groqApiKeyConfigured: boolean;
  geminiApiKeyConfigured: boolean;
  geminiImageModel: GeminiImageModel;
  geminiImagePreset: GeminiImagePresetId;
  geminiImageEditConfirmMode: boolean;
  geminiImagePromptHelperEnabled: boolean;
  geminiImagePromptHelperTokenLimit: number;
  geminiLowTierImageBalanceUsd: number;
  geminiLowTierAutoReloadEnabled: boolean;
  geminiLowTierAutoReloadThresholdUsd: number;
  geminiLowTierAutoReloadAmountUsd: number;
  llmModel: string;
  personalityPrompt: string;
  esp32AgentPersonalityPrompt: string;
  esp32AgentErrorPersonalityPrompt: string;
  personalityPresetId: string;
  musicShuffle: boolean;
  volumeLevel: number;
  scrollSpeedLevel: number;
  hatScrollSpeedLevel: number;
  hatFontSize: HatFontSize;
  hatFontFamily: HatFontFamily;
  voiceMode: VoiceMode;
  uiTheme: UITheme;
  cameraSource: CameraSource;
  esp32CamUrl: string;
  hatTextColor: HatTextColor;
  piCameraRotationDeg: number;
  esp32CamRotationDeg: number;
  manualRecordMaxSec: number;
  headerMode: HeaderMode;
  groqHeaderBadgeMode: GroqHeaderBadgeMode;
  screensaverMode: ScreensaverMode;
  idleTimeoutSec: number;
  screenBlankTimeoutSec: number;
  roomMonitorIntervalSec: number;
  weatherLatitude: number | null;
  weatherLongitude: number | null;
} {
  const settings = loadSettingsFile();
  return {
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    geminiApiKeyConfigured: Boolean(settings.geminiApiKey),
    geminiImageModel: settings.geminiImageModel,
    geminiImagePreset: settings.geminiImagePreset || DEFAULT_GEMINI_IMAGE_PRESET,
    geminiImageEditConfirmMode: Boolean(settings.geminiImageEditConfirmMode),
    geminiImagePromptHelperEnabled: Boolean(
      settings.geminiImagePromptHelperEnabled,
    ),
    geminiImagePromptHelperTokenLimit:
      settings.geminiImagePromptHelperTokenLimit,
    geminiLowTierImageBalanceUsd: settings.geminiLowTierImageBalanceUsd,
    geminiLowTierAutoReloadEnabled: settings.geminiLowTierAutoReloadEnabled,
    geminiLowTierAutoReloadThresholdUsd: settings.geminiLowTierAutoReloadThresholdUsd,
    geminiLowTierAutoReloadAmountUsd: settings.geminiLowTierAutoReloadAmountUsd,
    llmModel: settings.llmModel,
    personalityPrompt: settings.personalityPrompt,
    esp32AgentPersonalityPrompt: settings.esp32AgentPersonalityPrompt,
    esp32AgentErrorPersonalityPrompt:
      settings.esp32AgentErrorPersonalityPrompt,
    personalityPresetId: getCurrentPersonalityPresetId(
      settings.personalityPrompt,
      settings.savedPersonalityPresets,
    ),
    musicShuffle: settings.musicShuffle,
    volumeLevel: settings.volumeLevel,
    scrollSpeedLevel: settings.scrollSpeedLevel,
    hatScrollSpeedLevel: settings.hatScrollSpeedLevel,
    hatFontSize: settings.hatFontSize,
    hatFontFamily: settings.hatFontFamily,
    voiceMode: settings.voiceMode,
    uiTheme: settings.uiTheme,
    cameraSource: settings.cameraSource,
    esp32CamUrl: settings.esp32CamUrl,
    hatTextColor: settings.hatTextColor,
    piCameraRotationDeg:
      typeof settings.piCameraRotationDeg === "number"
        ? settings.piCameraRotationDeg
        : DEFAULT_PI_CAMERA_ROTATION_DEG,
    esp32CamRotationDeg:
      typeof settings.esp32CamRotationDeg === "number"
        ? settings.esp32CamRotationDeg
        : DEFAULT_ESP32_CAM_ROTATION_DEG,
    manualRecordMaxSec: settings.manualRecordMaxSec,
    headerMode: settings.headerMode,
    groqHeaderBadgeMode: settings.groqHeaderBadgeMode,
    screensaverMode: settings.screensaverMode,
    idleTimeoutSec: settings.idleTimeoutSec,
    screenBlankTimeoutSec:
      typeof settings.screenBlankTimeoutSec === "number"
        ? settings.screenBlankTimeoutSec
        : DEFAULT_SCREEN_BLANK_TIMEOUT_SEC,
    roomMonitorIntervalSec:
      typeof settings.roomMonitorIntervalSec === "number"
        ? settings.roomMonitorIntervalSec
        : DEFAULT_ROOM_MONITOR_INTERVAL_SEC,
    weatherLatitude: settings.weatherLatitude,
    weatherLongitude: settings.weatherLongitude,
  };
}

export function formatGeminiLowTierImageBalanceText(balanceUsd: number): string {
  return `$${normalizeCurrencyValue(balanceUsd, 0).toFixed(2)}`;
}

export function applyGeminiLowTierImageCharge(
  chargeUsd = GEMINI_LOW_TIER_IMAGE_COST_USD,
): RuntimeSettings {
  const current = loadSettingsFile();
  const next: RuntimeSettings = {
    ...current,
    geminiLowTierImageBalanceUsd: roundCurrency(
      Math.max(0, current.geminiLowTierImageBalanceUsd - Math.max(0, chargeUsd)),
    ),
  };
  if (
    next.geminiLowTierAutoReloadEnabled &&
    next.geminiLowTierAutoReloadAmountUsd > 0
  ) {
    let guard = 0;
    while (
      next.geminiLowTierImageBalanceUsd < next.geminiLowTierAutoReloadThresholdUsd &&
      guard < 8
    ) {
      next.geminiLowTierImageBalanceUsd = roundCurrency(
        next.geminiLowTierImageBalanceUsd + next.geminiLowTierAutoReloadAmountUsd,
      );
      guard += 1;
    }
  }
  const sanitized = sanitizeSettings(next);
  writeSettingsFile(sanitized);
  return sanitized;
}
