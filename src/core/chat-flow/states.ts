import fs from "fs";
import path from "path";
import moment from "moment";
import { compact, noop } from "lodash";
import {
  onButtonPressed,
  onButtonReleased,
  onButtonDoubleClick,
  display,
  getCurrentStatus,
  onCameraCapture,
  onTextInput,
  isButtonDown,
  recordConversationTurn,
  setEmitPressImmediatelyWithDoubleClick,
} from "../../device/display";
import {
  recordAudio,
  recordAudioManually,
  recordFileFormat,
  getDynamicVoiceDetectLevel,
} from "../../device/audio";
import { chatWithLLMStream, resetChatHistory } from "../../cloud-api/server";
import { isImMode } from "../../cloud-api/llm";
import {
  createPlainTextModelResponse,
  streamPlainTextModelResponse,
} from "../../cloud-api/text-model-router";
import { getSystemPromptWithKnowledge } from "../Knowledge";
import { enableRAG } from "../../cloud-api/knowledge";
import { getSystemPrompt } from "../../config/llm-config";
import { cameraDir, imageDir } from "../../utils/dir";
import {
  activateInteractiveImage,
  clearInteractiveImage,
  clearPendingCapturedImgForChat,
  getInteractiveImage,
  getLatestCapturedImg,
  hasInteractiveImage,
  listCapturedImgs,
  listGeneratedImgs,
  getLatestGenImg,
  getLatestDisplayImg,
  getLatestShowedImage,
  queueDisplayImage,
  setLatestCapturedImg,
  setPendingCapturedImgForChat,
  showLatestCapturedImg,
} from "../../utils/image";
import { sendWhisplayIMMessage } from "../../cloud-api/whisplay-im/whisplay-im";
import { ChatFlowContext, FlowName, FlowStateHandler } from "./types";
import {
  enterCameraMode,
  handleCameraModePress,
  handleCameraModeRelease,
  onCameraModeExit,
  resetCameraModeControl,
} from "./camera-mode";
import { DEFAULT_EMOJI } from "../../utils";
import {
  isMusicPlaying,
  getCurrentTrackTitle,
  stopMusicPlayback,
  startPendingMusicPlayback,
  onMusicTrackChange,
  onMusicPlaybackEnd,
  getManagedMusicPlayer,
} from "../../device/music-player";
import { autoSaveExchange } from "../../config/mempalace";
import { STATE_EMOJIS } from "../../config/state-emojis";
import { llmFuncMap } from "../../config/llm-tools";
import {
  CAMERA_SOURCES,
  getCameraSourceLabel,
  getHeaderModeLabel,
  getHatTextColorLabel,
  getIdleTimeoutLabel,
  getRuntimeSettings,
  HEADER_MODES,
  HAT_TEXT_COLORS,
  HAT_SCROLL_SPEED_OPTIONS,
  getVoiceModeLabel,
  saveRuntimeSettings,
  VOLUME_LEVEL_OPTIONS,
} from "../../config/runtime-settings";
import type { GroqHeaderBadgeView } from "../../device/display";
import {
  SETTINGS_OPEN_GRACE_MS,
  SETTINGS_SELECT_HOLD_MS,
} from "./settings-menu";
import { ToolReturnTag } from "../../type";
import { setLatestVisionAnalysis } from "../../utils/vision-analysis";
import { clearLatestVisionAnalysis } from "../../utils/vision-analysis";
import { captureCameraImage } from "../../device/camera-daemon";
import {
  buildVoiceCommandHelpPages,
} from "./voice-command-catalog";
import {
  setVolumeByLevel,
} from "../../utils/volume";
import {
  applyImageEffect,
  getImageEffectLabel,
  type ImageEffectId,
} from "../../device/image-effects";
import { recordRpdMessage } from "../../status/rpd-counter";
import {
  fetchWeatherSnapshot,
  isWeatherConfigured,
} from "../../device/weather";
import { getBotNetManager } from "../../device/botnet";
import { getBotNetModelLabel } from "../../config/botnet-models";
import { getTextLlmProvider } from "../../config/text-llm-models";

const imageIntentPatterns = [
  /\bwhat do you see\b/i,
  /\bdescribe (this|the) (image|photo|picture)\b/i,
  /\banaly[sz]e (this|the) (image|photo|picture)\b/i,
  /\bwhat(?:'s| is) in (this|the) (image|photo|picture)\b/i,
  /\bdo you see\b/i,
  /\bread (the )?text\b/i,
  /\bocr\b/i,
];

const weatherIntentPatterns = [
  /^\s*(?:what(?:'s| is)\s+the\s+weather|weather|weather forecast|forecast)\s*[.!?]*$/i,
  /^\s*(?:weather alerts|alerts|any alerts|are there any alerts)\s*[.!?]*$/i,
  /^\s*(?:is it going to snow|is snow coming|snow forecast)\s*[.!?]*$/i,
];

const imageGenerationIntentPatterns = [
  /^\s*(?:draw|illustrate|paint)\b/i,
  /\b(?:generate|create|make)\s+(?:me\s+)?(?:an?\s+)?(?:image|picture|photo|drawing|illustration|artwork|logo|poster|wallpaper)\b/i,
  /^\s*edit\s+(?:this|the|that)\s+(?:image|photo|picture)\b/i,
  /\b(?:edit|restyle|transform|change)\s+(?:this|the|that|my)?\s*(?:image|photo|picture)\b/i,
  /\b(?:make|turn|give|apply)\s+(?:this|the|that|my)?\s*(?:image|photo|picture)\b/i,
  /\b(?:surprise\s+me|work\s+your\s+magic)\b[\s\S]{0,80}\b(?:image|photo|picture)\b/i,
  /\b(?:do\s+your\s+thing|favorite\s+style|personal\s+style)\b[\s\S]{0,80}\b(?:image|photo|picture)\b/i,
  /\bmake\s+(?:this|the|that|my)?\s*(?:image|photo|picture)\s+artistic\b/i,
];

const imageGenerationContextPattern =
  /\b(?:edit|restyle|transform|change|make|turn|give|apply|surprise|work|favorite|personal)\b[\s\S]{0,80}\b(?:this|the|that|my)?\s*(?:image|picture|photo)\b/i;

const showInteractiveImageIntentPatterns = [
  /^\s*(?:show|display|open)\s+(?:me\s+)?(?:the\s+)?(?:image|photo|picture)\s*[.!?]*$/i,
  /^\s*(?:show|display|open)\s+(?:this|that|my)\s+(?:image|photo|picture)\s*[.!?]*$/i,
];

const editInteractiveImageIntentPatterns = [
  /^\s*edit\s+(?:the\s+|this\s+|that\s+|my\s+)?(?:image|photo|picture)\s*[.!?]*$/i,
];

const imageEffectCommandMatchers: Array<{
  effect: ImageEffectId;
  patterns: RegExp[];
}> = [
  {
    effect: "retro",
    patterns: [
      /\bmake (?:it|this)\s+retro\b/i,
      /\bmake (?:it|this)\s+vintage\b/i,
      /\bapply (?:a\s+)?retro\b/i,
    ],
  },
  {
    effect: "comic",
    patterns: [
      /\bcomic(?:\s+book)?\s+(?:this|it)\b/i,
      /\bmake (?:it|this)\s+(?:a\s+)?comic(?:\s+book)?\b/i,
      /\bcartoon(?:ize)?\s+(?:this|it)\b/i,
    ],
  },
  {
    effect: "sketch",
    patterns: [
      /\bsketch(?:\s+it|\s+this)?\b/i,
      /\bpencil\s+sketch\b/i,
      /\bmake (?:it|this)\s+(?:a\s+)?sketch\b/i,
    ],
  },
  {
    effect: "pixelate",
    patterns: [
      /\bpixelate(?:\s+it|\s+this)?\b/i,
      /\bminecraft\b/i,
      /\b8[\s-]?bit\b/i,
    ],
  },
  {
    effect: "halftone",
    patterns: [
      /\bhalftone\b/i,
      /\bnewspaper\s+print\b/i,
      /\bdot\s+screen\b/i,
    ],
  },
  {
    effect: "edge",
    patterns: [
      /\bedge\s+detection\b/i,
      /\bshow (?:the\s+)?edges\b/i,
      /\boutline (?:it|this)\b/i,
    ],
  },
  {
    effect: "spooky",
    patterns: [
      /\bmake (?:it|this)\s+spooky\b/i,
      /\bmake (?:it|this)\s+creepy\b/i,
      /\bhaunted\b/i,
    ],
  },
  {
    effect: "dreamy",
    patterns: [
      /\bmake (?:it|this)\s+dreamy\b/i,
      /\bdreamy\b/i,
      /\bethereal\b/i,
    ],
  },
  {
    effect: "warm",
    patterns: [
      /\bmake (?:it|this)\s+warm\b/i,
      /\bwarm and cozy\b/i,
      /\bcozy\b/i,
    ],
  },
  {
    effect: "cyberpunk",
    patterns: [
      /\bmake (?:it|this)\s+cyberpunk\b/i,
      /\bcyberpunk\b/i,
      /\bneon\b/i,
    ],
  },
  {
    effect: "glitch",
    patterns: [
      /\bglitch(?:\s+it|\s+this)?\b/i,
      /\bcorrupt (?:it|this|the image)\b/i,
      /\bmake (?:it|this)\s+look\s+hacked\b/i,
    ],
  },
  {
    effect: "vhs",
    patterns: [
      /\bvhs\b/i,
      /\btape\b/i,
      /\bold\s+camcorder\b/i,
    ],
  },
  {
    effect: "auto-contrast",
    patterns: [
      /\bauto\s+contrast\b/i,
      /\bfix (?:the\s+)?contrast\b/i,
    ],
  },
  {
    effect: "colors-pop",
    patterns: [
      /\bmake (?:the\s+)?colors pop\b/i,
      /\bsaturation boost\b/i,
      /\bboost (?:the\s+)?saturation\b/i,
      /\bboost (?:the\s+)?colors\b/i,
    ],
  },
];

const captureIntentPatterns = [
  /\btake (?:a )?(?:photo|picture)\b/i,
  /\bcapture (?:an? )?(?:image|photo|picture)\b/i,
  /\bsnap (?:a )?(?:photo|picture)\b/i,
];

const browseIntentPatterns = [
  /^\s*browse\s+(?:photos|images)\s*[.!?]*$/i,
  /^\s*open\s+gallery\s*[.!?]*$/i,
  /^\s*(?:show|open)\s+(?:the\s+)?gallery\s*[.!?]*$/i,
];

const shutdownIntentPatterns = [
  /^\s*(?:shutdown|shut\s*down)(?:\s+(?:raspberry(?:\s*pi)?|pi))?\s*[.!?]*$/i,
];

const settingsIntentPatterns = [
  /^\s*(?:settings|open\s+settings|settings\s+menu)\s*[.!?]*$/i,
];

const styleFontIntentPatterns = [
  /^\s*(?:change|switch|cycle)\s+(?:the\s+)?font\s*[.!?]*$/i,
  /^\s*(?:change|switch|cycle)\s+(?:the\s+)?text\s+(?:style|color)\s*[.!?]*$/i,
  /^\s*(?:change|switch|cycle)\s+(?:the\s+)?display\s+style\s*[.!?]*$/i,
];

const styleThemeIntentPatterns = [
  /^\s*(?:change|switch|cycle)\s+(?:the\s+)?theme\s*[.!?]*$/i,
  /^\s*(?:change|switch|cycle)\s+(?:the\s+)?header\s*[.!?]*$/i,
  /^\s*(?:change|switch|cycle)\s+(?:the\s+)?header\s+theme\s*[.!?]*$/i,
];

const voiceHelpIntentPatterns = [
  /^\s*(?:help|voice\s+commands|show\s+voice\s+commands|voice\s+command\s+list)\s*[.!?]*$/i,
];

const voiceOnIntentPatterns = [
  /^\s*(?:talk\s+to\s+me|speak\s+now|start\s+(?:speaking|talking)|voice\s+on|turn\s+(?:the\s+)?voice\s+on|enable\s+(?:voice|speech))\s*[.!?]*$/i,
];

const voiceOffIntentPatterns = [
  /^\s*(?:don't\s+talk\s+to\s+me|do\s+not\s+talk\s+to\s+me|stop\s+(?:speaking|talking)|don't\s+speak|do\s+not\s+speak|be\s+quiet|voice\s+off|turn\s+(?:the\s+)?voice\s+off|disable\s+(?:voice|speech)|mute(?:\s+voice)?)\s*[.!?]*$/i,
];

const speakOnDemandIntentPattern = /^\s*tell\s+me\b/i;

const replaySpeechIntentPatterns = [
  /^\s*(?:read(?:\s+that)?\s+aloud|read(?:\s+that)?\s+out\s+loud|say\s+that\s+again|repeat\s+that|repeat\s+the\s+last\s+answer)\s*[.!?]*$/i,
];

const clearChatIntentPatterns = [
  /^\s*(?:new\s+chat|clear\s+chat|reset\s+chat|start\s+(?:a\s+)?new\s+chat)\s*[.!?]*$/i,
];

const cameraPreviewIntentPatterns = [
  /^\s*(?:live\s+preview|camera\s+preview)\s*[.!?]*$/i,
  /^\s*(?:open|start|show)\s+(?:the\s+)?(?:live\s+|camera\s+)?preview\s*[.!?]*$/i,
  /^\s*(?:open|start|show)\s+(?:the\s+)?camera\s*[.!?]*$/i,
];

const volumeUpIntentPatterns = [
  /^\s*(?:volume\s+up|turn\s+(?:the\s+)?volume\s+up|increase\s+(?:the\s+)?volume|louder)\s*[.!?]*$/i,
];

const volumeDownIntentPatterns = [
  /^\s*(?:volume\s+down|turn\s+(?:the\s+)?volume\s+down|decrease\s+(?:the\s+)?volume|lower\s+(?:the\s+)?volume|quieter)\s*[.!?]*$/i,
];

const scrollSpeedUpIntentPatterns = [
  /^\s*(?:scroll\s+(?:faster|faster|speed|speed\s+up)|faster\s+scroll|speed\s+up|scroll\s+up)\s*[.!?]*$/i,
];

const scrollSpeedDownIntentPatterns = [
  /^\s*(?:scroll\s+(?:slower|slow)|slower\s+scroll|speed\s+down|scroll\s+down)\s*[.!?]*$/i,
];

const playMusicIntentPatterns = [
  /^\s*(?:play|start)(?:\s+the)?\s+(?:music|songs?|mp3s?)\s*[.!?]*$/i,
];

const stopMusicIntentPatterns = [
  /^\s*(?:stop|pause)(?:\s+the)?\s+(?:music|songs?|mp3s?)\s*[.!?]*$/i,
];

const nextSongIntentPatterns = [
  /^\s*(?:next\s+(?:song|track)|skip(?:\s+(?:song|track))?)\s*[.!?]*$/i,
];

const previousSongIntentPatterns = [
  /^\s*(?:previous|last|back)\s+(?:song|track)\s*[.!?]*$/i,
];

const PHOTO_BROWSER_EXIT_HOLD_MS = 1800;
const VOICE_HELP_EXIT_HOLD_MS = 1800;
const VOICE_COMMAND_HELP_PAGES = buildVoiceCommandHelpPages();
const TEXT_AUTO_REPLAY_PAUSE_MS = 900;
const CAMERA_CAPTURE_CUE_BLINK_MS = 220;
const CAMERA_CAPTURE_CONFIRM_MS = 1200;

interface PendingImageEditConfirmation {
  prompt: string;
  imagePath: string;
}

let pendingImageEditConfirmation: PendingImageEditConfirmation | null = null;
let cameraCaptureCueTimer: NodeJS.Timeout | null = null;
let cameraCaptureCueResetTimer: NodeJS.Timeout | null = null;
const HEADER_BADGE_VIEW_CYCLE: GroqHeaderBadgeView[] = [
  "default",
  "time",
  "rpd-model",
  "personality",
  "requests-total",
];

function clearPendingImageEditConfirmation(): void {
  pendingImageEditConfirmation = null;
}

function setPendingImageEditConfirmation(prompt: string, imagePath: string): void {
  pendingImageEditConfirmation = {
    prompt: prompt.trim(),
    imagePath,
  };
}

function getPendingImageEditConfirmation(): PendingImageEditConfirmation | null {
  return pendingImageEditConfirmation;
}

function shouldUseImageEditConfirmMode(): boolean {
  return Boolean(getRuntimeSettings().geminiImageEditConfirmMode);
}

function shouldUseImagePromptHelper(withImageContext: boolean): boolean {
  return withImageContext && Boolean(getRuntimeSettings().geminiImagePromptHelperEnabled);
}

function isConfirmableImageEditRequest(prompt: string): boolean {
  return shouldRouteToImageGeneration(prompt) && shouldUseImageContextForGeneration(prompt);
}

function parseImageEditConfirmCommand(prompt: string): {
  type: "confirm" | "show" | "add" | "replace" | "cancel";
  value?: string;
} | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }
  if (/^\s*confirm(?:ed)?\s*[.!?]*$/i.test(trimmed)) {
    return { type: "confirm" };
  }
  if (/^\s*confirm\s+prompt\s*[.!?]*$/i.test(trimmed)) {
    return { type: "show" };
  }
  if (/^\s*cancel\s*[.!?]*$/i.test(trimmed)) {
    return { type: "cancel" };
  }
  const addMatch = trimmed.match(/^\s*add\s+([\s\S]+?)\s*[.!?]*$/i);
  if (addMatch?.[1]) {
    return { type: "add", value: addMatch[1].trim() };
  }
  const replaceMatch = trimmed.match(/^\s*start\s+over\s+([\s\S]+?)\s*[.!?]*$/i);
  if (replaceMatch?.[1]) {
    return { type: "replace", value: replaceMatch[1].trim() };
  }
  return null;
}

function formatPendingImageEditPrompt(prompt: string): string {
  const compactPrompt = prompt.trim().replace(/\s+/g, " ");
  if (compactPrompt.length <= 140) {
    return compactPrompt;
  }
  return `${compactPrompt.slice(0, 137)}...`;
}

function buildImageEditConfirmMessage(prompt: string): string {
  return (
    `[confirm]Pending edit:\n${formatPendingImageEditPrompt(prompt)}\n\n` +
    "Say confirm or confirmed, add..., start over..., or cancel."
  );
}

function extractImagePromptHelperRequest(prompt: string): string {
  const trimmed = prompt.trim().replace(/\s+/g, " ");
  const stripped = trimmed
    .replace(
      /^(?:edit|restyle|transform|change|make|turn|give|apply)\s+(?:this|the|that|my)?\s*(?:image|photo|picture)\b[:,-]?\s*/i,
      "",
    )
    .replace(/^with\s+/i, "")
    .trim();
  return stripped || trimmed;
}

function normalizeImagePromptHelperOutput(output: string, fallback: string): string {
  const compact = output
    .replace(/^["'`]+|["'`]+$/g, "")
    .replace(/^(?:prompt|image prompt|photo edit prompt)\s*:\s*/i, "")
    .replace(/\s+/g, " ")
    .trim();
  return compact || fallback;
}

async function buildPersonalityImagePrompt(userPrompt: string): Promise<string> {
  const runtimeSettings = getRuntimeSettings();
  const helperRequest = extractImagePromptHelperRequest(userPrompt);
  const helperPrompt = await createPlainTextModelResponse({
    model: runtimeSettings.llmModel,
    maxOutputTokens: runtimeSettings.geminiImagePromptHelperTokenLimit,
    messages: [
      {
        role: "system",
        content:
          `${getSystemPrompt()}\n` +
          "You are an experimental photo-edit prompt helper. " +
          "Rewrite the user's rough photo-edit request into exactly one short, vivid image-edit prompt. " +
          "Output only the final prompt with no explanation, no list, no quotes, and no markdown. " +
          "Keep it concise, concrete, and useful for editing an existing photo. " +
          "Do not include assistant commentary or command phrases like edit photo, confirm, add to, or start over.",
      },
      {
        role: "user",
        content:
          `Rewrite this photo-edit request into one concise prompt for an existing image:\n${helperRequest}`,
      },
    ],
  });
  return normalizeImagePromptHelperOutput(helperPrompt, helperRequest);
}

function stopCameraCaptureCue(resetToDefault = true): void {
  if (cameraCaptureCueTimer) {
    clearInterval(cameraCaptureCueTimer);
    cameraCaptureCueTimer = null;
  }
  if (cameraCaptureCueResetTimer) {
    clearTimeout(cameraCaptureCueResetTimer);
    cameraCaptureCueResetTimer = null;
  }
  if (resetToDefault) {
    display({ RGB: "#000055" });
  }
}

function startCameraCaptureCue(
  text = "[camera]Capturing image...\nStand still.",
): void {
  stopCameraCaptureCue(false);
  let active = true;
  display({
    text,
    RGB: "#ff2a2a",
  });
  cameraCaptureCueTimer = setInterval(() => {
    active = !active;
    display({
      RGB: active ? "#ff2a2a" : "#2a0000",
    });
  }, CAMERA_CAPTURE_CUE_BLINK_MS);
}

function showCameraCapturedCue(
  text = "[camera]Photo captured.",
  resetDelayMs = CAMERA_CAPTURE_CONFIRM_MS,
): void {
  stopCameraCaptureCue(false);
  display({
    text,
    RGB: "#0088ff",
  });
  cameraCaptureCueResetTimer = setTimeout(() => {
    cameraCaptureCueResetTimer = null;
    display({ RGB: "#000055" });
  }, resetDelayMs);
}

function getNextGroqHeaderBadgeView(currentView?: string): GroqHeaderBadgeView {
  const currentIndex = HEADER_BADGE_VIEW_CYCLE.indexOf(
    (currentView as GroqHeaderBadgeView) || "default",
  );
  if (currentIndex === -1) {
    return HEADER_BADGE_VIEW_CYCLE[0];
  }
  return HEADER_BADGE_VIEW_CYCLE[(currentIndex + 1) % HEADER_BADGE_VIEW_CYCLE.length];
}

function getCameraCapturePath(): string {
  return `${cameraDir}/capture-${moment().format("YYYYMMDD-HHmmss-SSS")}.jpg`;
}

function shouldRouteToVision(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed || !hasInteractiveImage()) {
    return false;
  }
  return imageIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldRouteToWeather(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return weatherIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldOpenCameraPreview(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return cameraPreviewIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldRouteToImageGeneration(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return imageGenerationIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldUseImageContextForGeneration(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed || !getLatestShowedImage()) {
    return false;
  }
  return imageGenerationContextPattern.test(trimmed);
}

function shouldShowInteractiveImage(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return showInteractiveImageIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldOpenInteractiveImageEditor(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return editInteractiveImageIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldKeepInteractiveImageFocus(prompt: string): boolean {
  return (
    shouldRouteToVision(prompt) ||
    parseImageEffectCommand(prompt) !== null ||
    shouldUseImageContextForGeneration(prompt)
  );
}

function shouldKeepImageVisibleDuringReply(prompt: string): boolean {
  return (
    parseImageEffectCommand(prompt) !== null ||
    shouldRouteToImageGeneration(prompt)
  );
}

function isEligibleInteractiveImagePath(imagePath: string): boolean {
  if (!imagePath) {
    return false;
  }
  const resolved = path.resolve(imagePath);
  const roots = [path.resolve(cameraDir), path.resolve(imageDir)];
  return (
    fs.existsSync(resolved) &&
    roots.some((root) => resolved === root || resolved.startsWith(`${root}${path.sep}`))
  );
}

function getDefaultInteractiveImageTarget(): {
  path: string;
  source: "manual-selection" | "other";
} | null {
  const cameraRoot = path.resolve(cameraDir);
  const candidates = [
    getInteractiveImage(),
    getLatestShowedImage(),
    getLatestGenImg(),
    getLatestCapturedImg(),
  ];
  for (const candidate of candidates) {
    if (!isEligibleInteractiveImagePath(candidate)) {
      continue;
    }
    const resolved = path.resolve(candidate);
    return {
      path: resolved,
      source:
        resolved === cameraRoot || resolved.startsWith(`${cameraRoot}${path.sep}`)
          ? "manual-selection"
          : "other",
    };
  }
  return null;
}

function replayTextAnswerAndSleep(
  ctx: ChatFlowContext,
  flowName: FlowName,
  answerId: number,
): void {
  setTimeout(() => {
    if (ctx.currentFlowName !== flowName || ctx.answerId !== answerId) {
      return;
    }
    ctx.replayLastAnswer();
    if (ctx.currentFlowName !== flowName || ctx.answerId !== answerId) {
      return;
    }
    ctx.transitionTo("sleep");
  }, TEXT_AUTO_REPLAY_PAUSE_MS);
}

function parseImageEffectCommand(prompt: string): ImageEffectId | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }
  for (const matcher of imageEffectCommandMatchers) {
    if (matcher.patterns.some((pattern) => pattern.test(trimmed))) {
      return matcher.effect;
    }
  }
  return null;
}

function shouldCaptureImage(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return captureIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldSwitchCamera(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return [
    /^\s*(?:switch|swap|change|toggle)\s+camera\s*[.!?]*$/i,
    /^\s*(?:switch|swap|change|toggle)\s+(?:the\s+)?camera\s*(?:source)?\s*[.!?]*$/i,
  ].some((pattern) => pattern.test(trimmed));
}

function getNextCameraSource(current: string): string {
  const currentIndex = CAMERA_SOURCES.findIndex((value) => value === current);
  if (currentIndex === -1) {
    return CAMERA_SOURCES[0];
  }
  return CAMERA_SOURCES[(currentIndex + 1) % CAMERA_SOURCES.length];
}

function shouldAdvanceBotNetModel(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return [
    /^\s*next\s+model\s*[.!?]*$/i,
    /^\s*(?:switch|swap|change)\s+(?:the\s+)?model\s*[.!?]*$/i,
    /^\s*next\s+bot(?:net)?\s+model\s*[.!?]*$/i,
  ].some((pattern) => pattern.test(trimmed));
}

function shouldBrowsePhotos(prompt: string): boolean {
  const trimmed = prompt.trim();
  return browseIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function getAvailableGalleryEntries(): Array<{
  id: "photos" | "generated";
  label: string;
  flow: "photo_browser" | "generated_image_browser";
}> {
  const entries: Array<{
    id: "photos" | "generated";
    label: string;
    flow: "photo_browser" | "generated_image_browser";
  }> = [];
  if (listCapturedImgs().length) {
    entries.push({
      id: "photos",
      label: "Camera Photos",
      flow: "photo_browser",
    });
  }
  if (listGeneratedImgs().length) {
    entries.push({
      id: "generated",
      label: "AI Images",
      flow: "generated_image_browser",
    });
  }
  return entries;
}

function getAvailableManualGalleryEntries(): Array<{
  id: "photos";
  label: string;
  flow: "photo_browser";
}> {
  if (!listCapturedImgs().length) {
    return [];
  }
  return [
    {
      id: "photos",
      label: "Camera Photos",
      flow: "photo_browser",
    },
  ];
}

function shouldShutdown(prompt: string): boolean {
  const trimmed = prompt.trim();
  return shutdownIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldOpenSettings(prompt: string): boolean {
  const trimmed = prompt.trim();
  return settingsIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldCycleHatTextStyle(prompt: string): boolean {
  const trimmed = prompt.trim();
  return styleFontIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldCycleHeaderTheme(prompt: string): boolean {
  const trimmed = prompt.trim();
  return styleThemeIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function getNextHatTextColor(current: string): string {
  const currentIndex = HAT_TEXT_COLORS.findIndex((value) => value === current);
  if (currentIndex === -1) {
    return HAT_TEXT_COLORS[0];
  }
  return HAT_TEXT_COLORS[(currentIndex + 1) % HAT_TEXT_COLORS.length];
}

function getNextHeaderMode(current: string): string {
  const currentIndex = HEADER_MODES.findIndex((value) => value === current);
  if (currentIndex === -1) {
    return HEADER_MODES[0];
  }
  return HEADER_MODES[(currentIndex + 1) % HEADER_MODES.length];
}

function shouldOpenVoiceHelp(prompt: string): boolean {
  const trimmed = prompt.trim();
  return voiceHelpIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldReplayLastAnswerAloud(prompt: string): boolean {
  const trimmed = prompt.trim();
  return replaySpeechIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function shouldClearChat(prompt: string): boolean {
  const trimmed = prompt.trim();
  return clearChatIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function getMusicControlCommand(
  prompt: string,
): "play" | "stop" | "next" | "previous" | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }
  if (playMusicIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return "play";
  }
  if (stopMusicIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return "stop";
  }
  if (nextSongIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return "next";
  }
  if (previousSongIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return "previous";
  }
  return null;
}

function parseVolumeCommand(
  prompt: string,
): { action: "set"; value: number } | { action: "step"; delta: -1 | 1 } | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }

  const setMatch = trimmed.match(
    /^\s*(?:set\s+)?volume(?:\s+(?:to\s+)?)?(10|[1-9])\s*[.!?]*$/i,
  );
  if (setMatch) {
    return {
      action: "set",
      value: parseInt(setMatch[1], 10),
    };
  }

  if (volumeUpIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return { action: "step", delta: 1 };
  }
  if (volumeDownIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return { action: "step", delta: -1 };
  }

  return null;
}

function parseScrollSpeedCommand(
  prompt: string,
): { action: "set"; value: number } | { action: "step"; delta: -1 | 1 } | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }

  const setMatch = trimmed.match(
    /^\s*(?:set\s+)?scroll(?:\s+speed)?(?:\s+(?:to\s+)?)?(10|[1-9])\s*[.!?]*$/i,
  );
  if (setMatch) {
    return {
      action: "set",
      value: parseInt(setMatch[1], 10),
    };
  }

  if (scrollSpeedUpIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return { action: "step", delta: 1 };
  }
  if (scrollSpeedDownIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return { action: "step", delta: -1 };
  }

  return null;
}

function parseScreenTimeoutCommand(prompt: string): number | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }

  const match = trimmed.match(
    /^\s*(?:set\s+)?(?:screen|display)\s+timeout(?:\s+(?:to\s+)?)?(off|10|[1-9])(?:\s*(?:m|min|minute|minutes))?\s*[.!?]*$/i,
  );
  if (!match) {
    return null;
  }

  if (match[1].toLowerCase() === "off") {
    return 0;
  }

  return parseInt(match[1], 10) * 60;
}

function getVoiceModeCommand(prompt: string): "voice-chat" | "text-only" | null {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return null;
  }
  if (voiceOnIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return "voice-chat";
  }
  if (voiceOffIntentPatterns.some((pattern) => pattern.test(trimmed))) {
    return "text-only";
  }
  return null;
}

function shouldForceSpeakOnDemandReply(prompt: string): boolean {
  return speakOnDemandIntentPattern.test(prompt.trim());
}

async function captureAndPrepareLatestImage(): Promise<string> {
  const captureImagePath = `${cameraDir}/capture-${moment().format(
    "YYYYMMDD-HHmmss",
  )}.jpg`;
  startCameraCaptureCue();
  await captureCameraImage(captureImagePath, 8000);
  setLatestCapturedImg(captureImagePath);
  activateInteractiveImage(captureImagePath, "manual-capture");
  queueDisplayImage(captureImagePath);
  clearLatestVisionAnalysis();
  clearPendingImageEditConfirmation();
  return captureImagePath;
}

function stripToolTag(result: string): string {
  return result.replace(/^\[(success|error|response)\]\s*/i, "").trim();
}

async function streamVisionRelayReply(
  userPrompt: string,
  visionAnalysis: string,
  onChunk: (chunk: string) => void,
): Promise<string> {
  try {
    return await streamPlainTextModelResponse({
      model: getRuntimeSettings().llmModel,
      messages: [
        {
          role: "system",
          content:
            `${getSystemPrompt()}\n` +
            "You are answering a question about an already analyzed image. " +
            "Use the supplied vision analysis as your only visual context. " +
            "Reply naturally in your active personality, stay concise, and do not mention Gemini, tools, or backend analysis unless the user asks.",
        },
        {
          role: "user",
          content:
            `User question: ${userPrompt}\n\n` +
            `Vision analysis:\n${visionAnalysis}\n\n` +
            "Answer the user's question naturally. If the analysis seems uncertain, briefly say so.",
        },
      ],
      onChunk,
    });
  } catch (error) {
    console.error("Vision relay reply failed:", error);
    return "";
  }
}

async function streamWeatherRelayReply(
  userPrompt: string,
  weatherSummary: string,
  onChunk: (chunk: string) => void,
): Promise<string> {
  try {
    return await streamPlainTextModelResponse({
      model: getRuntimeSettings().llmModel,
      messages: [
        {
          role: "system",
          content:
            `${getSystemPrompt()}\n` +
            "You are answering a weather question using supplied NOAA/NWS forecast data. " +
            "Stay concise, practical, and in character. Use only the supplied weather data. " +
            "If alerts exist, mention them clearly. Do not invent extra forecast details.",
        },
        {
          role: "user",
          content:
            `Weather data:\n${weatherSummary}\n\n` +
            `User question: ${userPrompt}`,
        },
      ],
      onChunk,
    });
  } catch (error) {
    console.error("Weather relay reply failed:", error);
    return "";
  }
}

export const flowStates: Record<FlowName, FlowStateHandler> = {
  sleep: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(false);
    const currentStatus = getCurrentStatus();
    const preserveLastReply =
      currentStatus.status === "last reply" && Boolean(currentStatus.text);
    onButtonDoubleClick(noop);
    onButtonPressed(() => {
      resetCameraModeControl();
      // Stop any playing music when waking up
      stopMusicPlayback();
      ctx.transitionTo("listening");
    });
    onButtonReleased(() => {
      if (ctx.currentFlowName !== "sleep") {
        return;
      }
      display({
        groq_header_badge_view: getNextGroqHeaderBadgeView(
          getCurrentStatus().groq_header_badge_view,
        ),
      });
    });
    onCameraModeExit(null);
    onTextInput((text: string) => {
      if (ctx.currentFlowName !== "sleep") return;
      ctx.answerId += 1;
      ctx.asrText = text;
      display({ status: "recognizing", text, text_input_enabled: false });
      ctx.transitionTo("answer");
    });
    display({
      status: preserveLastReply ? "last reply" : "idle",
      emoji: preserveLastReply
        ? currentStatus.emoji || STATE_EMOJIS.answering
        : STATE_EMOJIS.idle,
      RGB: preserveLastReply ? "#00c8a3" : "#000055",
      rag_icon_visible: false,
      ...(preserveLastReply
        ? {}
        : currentStatus.text.endsWith("Listening...") || !currentStatus.text
        ? {
          text: "Hold to talk.",
        }
        : {}),
    });
  },
  camera: (ctx: ChatFlowContext) => {
    onButtonDoubleClick(null);
    onButtonPressed(() => {
      handleCameraModePress(() => {
        if (ctx.currentFlowName !== "camera") {
          return;
        }
        resetCameraModeControl();
        display({
          camera_mode: false,
          image: "",
          image_icon_visible: false,
        });
        stopMusicPlayback();
        ctx.transitionTo("listening");
      });
    });
    onButtonReleased(() => {
      handleCameraModeRelease();
    });
    onCameraCapture(() => {
      const captureImagePath = getCurrentStatus().capture_image_path;
      if (!captureImagePath) {
        return;
      }
      setLatestCapturedImg(captureImagePath);
      activateInteractiveImage(captureImagePath, "manual-capture");
      setPendingCapturedImgForChat(captureImagePath);
      clearLatestVisionAnalysis();
      clearPendingImageEditConfirmation();
      showCameraCapturedCue();
      display({
        image: captureImagePath,
        image_icon_visible: false,
        capture_image_path: getCameraCapturePath(),
      });
    });
    onCameraModeExit(() => {
      if (ctx.currentFlowName === "camera") {
        display({ image: "", image_icon_visible: false });
        ctx.transitionTo("sleep");
      }
    });
    display({
      status: "camera",
      emoji: STATE_EMOJIS.camera,
      RGB: "#00ff88",
    });
  },
  gallery_menu: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(true);
    let menuIndex = 0;
    let holdTimer: NodeJS.Timeout | null = null;
    let holdTriggered = false;

    const exitGalleryMenu = () => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      ctx.transitionTo("sleep");
    };

    const renderMenu = () => {
      const entries = getAvailableGalleryEntries();
      if (!entries.length) {
        display({
          status: "gallery",
          emoji: STATE_EMOJIS.camera,
          RGB: "#0088ff",
          text: "No galleries yet.\nDouble press: exit",
          image: "",
          image_icon_visible: false,
        });
        return;
      }
      if (menuIndex >= entries.length) {
        menuIndex = 0;
      }
      const selected = entries[menuIndex];
      display({
        status: "gallery",
        emoji: STATE_EMOJIS.camera,
        RGB: "#0088ff",
        text:
          `Gallery ${menuIndex + 1}/${entries.length}\n` +
          `${selected.label}\n` +
          "Short press: next\nHold: open\nDouble press: exit",
        image: "",
        image_icon_visible: false,
      });
    };

    onButtonDoubleClick(() => {
      exitGalleryMenu();
    });
    onButtonPressed(() => {
      holdTriggered = false;
      if (holdTimer) {
        clearTimeout(holdTimer);
      }
      holdTimer = setTimeout(() => {
        holdTriggered = true;
        const entries = getAvailableGalleryEntries();
        const selected = entries[menuIndex];
        if (!selected) {
          exitGalleryMenu();
          return;
        }
        ctx.transitionTo(selected.flow);
      }, PHOTO_BROWSER_EXIT_HOLD_MS);
    });
    onButtonReleased(() => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      if (holdTriggered) {
        holdTriggered = false;
        return;
      }
      const entries = getAvailableGalleryEntries();
      if (entries.length > 1) {
        menuIndex = (menuIndex + 1) % entries.length;
      }
      renderMenu();
    });
    renderMenu();
  },
  photo_browser: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(true);
    let photoIndex = 0;
    let holdTimer: NodeJS.Timeout | null = null;
    let holdTriggered = false;

    const exitPhotoBrowser = () => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      display({
        image: "",
        image_icon_visible: false,
      });
      ctx.transitionTo("sleep");
    };

    const renderCurrentPhoto = () => {
      const photos = listCapturedImgs();
      if (!photos.length) {
        display({
          status: "photos",
          emoji: STATE_EMOJIS.camera,
          RGB: "#0088ff",
          text: "No saved photos.\nHold to exit.",
          image: "",
          image_icon_visible: false,
        });
        return;
      }
      if (photoIndex >= photos.length) {
        photoIndex = 0;
      }
      const imagePath = photos[photoIndex] || "";
      display({
        status: "photos",
        emoji: STATE_EMOJIS.camera,
        RGB: "#0088ff",
        text:
          `Photo ${photoIndex + 1}/${photos.length}\n` +
          "Short press: next\nHold: select\nDouble press: exit",
        image: imagePath,
        image_icon_visible: false,
      });
    };

    onButtonDoubleClick(() => {
      exitPhotoBrowser();
    });
    onButtonPressed(() => {
      holdTriggered = false;
      if (holdTimer) {
        clearTimeout(holdTimer);
      }
      holdTimer = setTimeout(() => {
        holdTriggered = true;
        const photos = listCapturedImgs();
        const imagePath = photos[photoIndex] || "";
        if (!imagePath) {
          exitPhotoBrowser();
          return;
        }
        activateInteractiveImage(imagePath, "manual-selection");
        queueDisplayImage(imagePath);
        clearPendingImageEditConfirmation();
        display({
          status: "photo ready",
          image: imagePath,
          image_icon_visible: false,
          text: "[gallery]Photo selected.",
          text_input_enabled: true,
        });
        ctx.transitionTo("image");
      }, PHOTO_BROWSER_EXIT_HOLD_MS);
    });
    onButtonReleased(() => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      if (holdTriggered) {
        holdTriggered = false;
        return;
      }
      const photos = listCapturedImgs();
      if (photos.length > 1) {
        photoIndex = (photoIndex + 1) % photos.length;
      }
      renderCurrentPhoto();
    });
    renderCurrentPhoto();
  },
  generated_image_browser: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(true);
    let imageIndex = 0;
    let holdTimer: NodeJS.Timeout | null = null;
    let holdTriggered = false;

    const exitGeneratedImageBrowser = () => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      display({
        image: "",
        image_icon_visible: false,
      });
      ctx.transitionTo("sleep");
    };

    const renderCurrentImage = () => {
      const images = listGeneratedImgs();
      if (!images.length) {
        display({
          status: "ai images",
          emoji: STATE_EMOJIS.camera,
          RGB: "#8a4dff",
          text: "No AI images yet.\nDouble press: exit",
          image: "",
          image_icon_visible: false,
        });
        return;
      }
      if (imageIndex >= images.length) {
        imageIndex = 0;
      }
      const imagePath = images[imageIndex] || "";
      display({
        status: "ai images",
        emoji: STATE_EMOJIS.camera,
        RGB: "#8a4dff",
        text:
          `AI Image ${imageIndex + 1}/${images.length}\n` +
          "Short press: next\nHold: select\nDouble press: exit",
        image: imagePath,
        image_icon_visible: false,
      });
    };

    onButtonDoubleClick(() => {
      exitGeneratedImageBrowser();
    });
    onButtonPressed(() => {
      holdTriggered = false;
      if (holdTimer) {
        clearTimeout(holdTimer);
      }
      holdTimer = setTimeout(() => {
        holdTriggered = true;
        const images = listGeneratedImgs();
        const imagePath = images[imageIndex] || "";
        if (!imagePath) {
          exitGeneratedImageBrowser();
          return;
        }
        activateInteractiveImage(imagePath, "other");
        queueDisplayImage(imagePath);
        clearPendingImageEditConfirmation();
        display({
          status: "photo ready",
          image: imagePath,
          image_icon_visible: false,
          text: "[gallery]AI image selected.",
          text_input_enabled: true,
        });
        ctx.transitionTo("image");
      }, PHOTO_BROWSER_EXIT_HOLD_MS);
    });
    onButtonReleased(() => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      if (holdTriggered) {
        holdTriggered = false;
        return;
      }
      const images = listGeneratedImgs();
      if (images.length > 1) {
        imageIndex = (imageIndex + 1) % images.length;
      }
      renderCurrentImage();
    });
    renderCurrentImage();
  },
  voice_command_help: (ctx: ChatFlowContext) => {
    let pageIndex = 0;
    let holdTimer: NodeJS.Timeout | null = null;
    let holdTriggered = false;

    const exitVoiceHelp = () => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      ctx.transitionTo("sleep");
    };

    const renderCurrentPage = () => {
      const page = VOICE_COMMAND_HELP_PAGES[pageIndex] || VOICE_COMMAND_HELP_PAGES[0];
      display({
        status: "help",
        emoji: STATE_EMOJIS.idle,
        RGB: "#3388ff",
        text: page,
        image: "",
        image_icon_visible: false,
        rag_icon_visible: false,
      });
    };

    onButtonDoubleClick(null);
    onButtonPressed(() => {
      holdTriggered = false;
      if (holdTimer) {
        clearTimeout(holdTimer);
      }
      holdTimer = setTimeout(() => {
        holdTriggered = true;
        exitVoiceHelp();
      }, VOICE_HELP_EXIT_HOLD_MS);
    });
    onButtonReleased(() => {
      if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
      }
      if (holdTriggered) {
        holdTriggered = false;
        return;
      }
      pageIndex = (pageIndex + 1) % VOICE_COMMAND_HELP_PAGES.length;
      renderCurrentPage();
    });
    renderCurrentPage();
  },
  music: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(false);
    // Start deferred music playback when entering music state
    startPendingMusicPlayback();

    // Update display when track changes during continuous playback
    onMusicTrackChange((title) => {
      if (ctx.currentFlowName === "music") {
        display({ text: `Now playing: ${title}` });
      }
    });

    // Return to sleep when non-continuous playback finishes
    onMusicPlaybackEnd(() => {
      if (ctx.currentFlowName === "music") {
        onMusicTrackChange(null);
        onMusicPlaybackEnd(null);
        ctx.transitionTo("sleep");
      }
    });

    onButtonDoubleClick(() => {
      onMusicTrackChange(null);
      onMusicPlaybackEnd(null);
      stopMusicPlayback();
      ctx.transitionTo("sleep");
    });
    onButtonPressed(() => {
      // Stop music immediately when button is pressed
      onMusicTrackChange(null);
      onMusicPlaybackEnd(null);
      stopMusicPlayback();
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);

    const trackTitle = getCurrentTrackTitle();
    display({
      status: "music",
      emoji: STATE_EMOJIS.music,
      RGB: "#0066aa",
      text:
        ctx.musicDisplayText ||
        (isMusicPlaying() && trackTitle
          ? `Now playing: ${trackTitle}`
          : "Music mode. Press the button to talk."),
      rag_icon_visible: false,
    });
  },
  listening: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(false);
    ctx.enterMusicAfterAnswer = false;
    ctx.musicDisplayText = "";
    ctx.isFromWakeListening = false;
    ctx.answerId += 1;
    ctx.wakeSessionActive = false;
    ctx.endAfterAnswer = false;
    onButtonDoubleClick(null);
    ctx.currentRecordFilePath = `${ctx.recordingsDir
      }/user-${Date.now()}.${recordFileFormat}`;
    onButtonPressed(noop);
    const listeningStartedAt = Date.now();
    const manualRecordMaxMs = ctx.getManualRecordMaxSec() * 1000;
    let recordLimitReached = false;
    let waitingForReleaseAfterLimit = false;
    let recordingCompleted = false;
    let released = false;
    let settingsTriggered = false;
    let recordLimitTimer: NodeJS.Timeout | null = null;
    let settingsTimer: NodeJS.Timeout | null = null;
    // If button was already released before we entered this state, go back to sleep
    if (!isButtonDown()) {
      console.log("[listening] Button already released, returning to sleep");
      ctx.transitionTo("sleep");
      return;
    }
    const { result, stop } = recordAudioManually(ctx.currentRecordFilePath);
    recordLimitTimer = setTimeout(() => {
      if (ctx.currentFlowName !== "listening") {
        return;
      }
      recordLimitReached = true;
      waitingForReleaseAfterLimit = true;
      stop();
      display({
        status: "listening",
        emoji: STATE_EMOJIS.listening,
        RGB: "#6048ff",
        text: `Release to send.\nKeep holding ${SETTINGS_OPEN_GRACE_MS / 1000}s for settings...`,
        rag_icon_visible: false,
      });
      settingsTimer = setTimeout(() => {
        if (ctx.currentFlowName !== "listening" || released) {
          return;
        }
        settingsTriggered = true;
        ctx.openSettingsMenu(true);
      }, SETTINGS_OPEN_GRACE_MS);
    }, manualRecordMaxMs);

    const handleRelease = () => {
      released = true;
      if (recordLimitTimer) {
        clearTimeout(recordLimitTimer);
        recordLimitTimer = null;
      }
      if (settingsTimer) {
        clearTimeout(settingsTimer);
        settingsTimer = null;
      }
      if (Date.now() - listeningStartedAt < 500) {
        // Too short to be meaningful — stop recording and return to sleep
        console.log("[listening] Button released too quickly, returning to sleep");
        stop();
        ctx.transitionTo("sleep");
        return;
      }
      if (settingsTriggered) {
        return;
      }
      if (recordLimitReached) {
        waitingForReleaseAfterLimit = false;
        if (recordingCompleted && ctx.currentFlowName === "listening") {
          ctx.transitionTo("asr");
        }
        return;
      }
      stop();
      display({
        RGB: "#ff6800",
        image: "",
      });
    };
    onButtonReleased(handleRelease);
    result
      .then(() => {
        recordingCompleted = true;
        if (settingsTriggered || ctx.currentFlowName !== "listening") {
          return;
        }
        if (waitingForReleaseAfterLimit && !released) {
          return;
        }
        ctx.transitionTo("asr");
      })
      .catch((err) => {
        console.error("Error during recording:", err);
        ctx.transitionTo("sleep");
      });
    display({
      status: "listening",
      emoji: STATE_EMOJIS.listening,
      RGB: "#00ff00",
      text: "Listening...",
      rag_icon_visible: false,
    });
  },
  wake_listening: (ctx: ChatFlowContext) => {
    setEmitPressImmediatelyWithDoubleClick(false);
    ctx.enterMusicAfterAnswer = false;
    ctx.musicDisplayText = "";
    ctx.isFromWakeListening = true;
    ctx.answerId += 1;
    ctx.currentRecordFilePath = `${ctx.recordingsDir
      }/user-${Date.now()}.${recordFileFormat}`;
    onButtonPressed(() => {
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    display({
      status: "detecting",
      emoji: STATE_EMOJIS.recognizing,
      RGB: "#00ff00",
      text: "Detecting voice level...",
      rag_icon_visible: false,
    });
    getDynamicVoiceDetectLevel().then((level) => {
      display({
        status: "listening",
        emoji: STATE_EMOJIS.listening,
        RGB: "#00ff00",
        text: `(Detect level: ${level}%) Listening...`,
        rag_icon_visible: false,
      });
      recordAudio(ctx.currentRecordFilePath, ctx.wakeRecordMaxSec, level)
        .then(() => {
          ctx.transitionTo("asr");
        })
        .catch((err) => {
          console.error("Error during auto recording:", err);
          ctx.endWakeSession();
          ctx.transitionTo("sleep");
        });
    });
  },
  asr: (ctx: ChatFlowContext) => {
    display({
      status: "recognizing",
      emoji: STATE_EMOJIS.recognizing,
    });
    onButtonDoubleClick(null);
    Promise.race([
      ctx.recognizeAudio(ctx.currentRecordFilePath, ctx.isFromWakeListening),
      new Promise<string>((resolve) => {
        onButtonPressed(() => {
          resolve("[UserPress]");
        });
        onButtonReleased(noop);
      }),
    ]).then((result) => {
      if (ctx.currentFlowName !== "asr") return;
      if (result === "[UserPress]") {
        ctx.transitionTo("listening");
        return;
      }
      if (result) {
        console.log("Audio recognized result:", result);
        if (ctx.shouldOpenSettingsMenu(result)) {
          ctx.openSettingsMenu();
          return;
        }
        ctx.asrText = result;
        ctx.endAfterAnswer = ctx.shouldEndAfterAnswer(result);
        if (ctx.wakeSessionActive) {
          ctx.wakeSessionLastSpeechAt = Date.now();
        }
        display({ status: "recognizing", text: result });
        ctx.transitionTo("answer");
        return;
      }
      if (ctx.wakeSessionActive) {
        if (ctx.shouldContinueWakeSession()) {
          ctx.transitionTo("wake_listening");
        } else {
          ctx.endWakeSession();
          ctx.transitionTo("sleep");
        }
        return;
      }
      ctx.transitionTo("sleep");
    });
  },
  settings: (ctx: ChatFlowContext) => {
    onButtonDoubleClick(null);
    let selectTimer: NodeJS.Timeout | null = null;
    let holdTriggered = false;
    onButtonPressed(() => {
      holdTriggered = false;
      selectTimer = setTimeout(() => {
        if (ctx.currentFlowName !== "settings") {
          return;
        }
        holdTriggered = true;
        ctx.activateSettingsSelection();
      }, SETTINGS_SELECT_HOLD_MS);
    });
    onButtonReleased(() => {
      if (selectTimer) {
        clearTimeout(selectTimer);
        selectTimer = null;
      }
      if (ctx.consumeSettingsReleaseGuard()) {
        return;
      }
      if (ctx.currentFlowName !== "settings") {
        return;
      }
      if (holdTriggered) {
        holdTriggered = false;
        return;
      }
      ctx.moveSettingsSelection();
    });
    ctx.renderSettingsMenu();
  },
  answer: (ctx: ChatFlowContext) => {
    ctx.enterMusicAfterAnswer = false;
    ctx.musicDisplayText = "";
    if (ctx.asrText) {
      recordConversationTurn("user", ctx.asrText);
      if (getTextLlmProvider(getRuntimeSettings().llmModel) !== "gemini") {
        recordRpdMessage(1);
      }
    }
    display({
      status: "answering...",
      emoji: STATE_EMOJIS.answering,
      RGB: "#00c8a3",
    });
    const currentAnswerId = ctx.answerId;
    if (getCurrentStatus().image && !shouldKeepImageVisibleDuringReply(ctx.asrText)) {
      display({ image: "", image_icon_visible: false });
    }
    if (hasInteractiveImage() && !shouldKeepInteractiveImageFocus(ctx.asrText)) {
      clearInteractiveImage();
    }
    if (isImMode) {
      const prompt: {
        role: "system" | "user";
        content: string;
      }[] = [
          {
            role: "user",
            content: ctx.asrText,
          },
        ];
      sendWhisplayIMMessage(prompt)
        .then((ok) => {
            if (ok) {
            ctx.rememberLastAnswer({
              text: ctx.pendingExternalReply || ctx.asrText,
              emoji: STATE_EMOJIS.externalIdle,
            });
            display({
              status: "idle",
              emoji: STATE_EMOJIS.externalIdle,
              RGB: "#000055",
              image_icon_visible: false,
            });
          } else {
            display({
              status: "error",
              emoji: STATE_EMOJIS.error,
              text: "OpenClaw send failed",
              image_icon_visible: false,
            });
          }
        })
        .finally(() => {
          clearPendingCapturedImgForChat();
          ctx.transitionTo("sleep");
        });
      return;
    }
    onButtonPressed(() => {
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    const {
      partial,
      endPartial,
      getPlayEndPromise,
      stop: stopPlaying,
    } = ctx.streamResponser;
    let llmResponseText = "";
    const shouldForceReplySpeech =
      getRuntimeSettings().voiceMode === "speak-on-demand" &&
      shouldForceSpeakOnDemandReply(ctx.asrText);
    const runReplyFlow = async (callback: () => Promise<void>): Promise<void> => {
      if (shouldForceReplySpeech) {
        await ctx.streamResponser.withForcedSpeech(callback);
        return;
      }
      await callback();
    };
    const trackingPartial = (text: string): void => {
      llmResponseText += text;
      if (currentAnswerId === ctx.answerId) partial(text);
    };
    const finishDirectAnswer = (showImageAfter = false): void => {
      getPlayEndPromise().then(() => {
        if (ctx.currentFlowName !== "answer" || currentAnswerId !== ctx.answerId) {
          return;
        }
        const img = showImageAfter ? getLatestDisplayImg() : "";
        ctx.rememberLastAnswer({
          text: llmResponseText,
          emoji: STATE_EMOJIS.answering,
          image: img || "",
        });
        clearPendingCapturedImgForChat();
        display({ image_icon_visible: false });
        if (img) {
          ctx.transitionTo("image");
          return;
        }
        replayTextAnswerAndSleep(ctx, "answer", currentAnswerId);
      });
    };
    ctx.partialThinking = "";
    ctx.thinkingSentences = [];
    const finishDirectMessage = (message: string, showImageAfter = false): void => {
      trackingPartial(message);
      endPartial();
      finishDirectAnswer(showImageAfter);
    };
    const transitionDirectlyToSleep = (message: string, emoji = STATE_EMOJIS.answering): void => {
      llmResponseText = message;
      stopPlaying();
      ctx.rememberLastAnswer({
        text: message,
        emoji,
        image: "",
      });
      clearPendingCapturedImgForChat();
      display({ image_icon_visible: false });
      ctx.transitionTo("sleep");
    };
    const transitionDirectlyToMusic = (message: string): void => {
      llmResponseText = message;
      stopPlaying();
      ctx.rememberLastAnswer({
        text: message,
        emoji: STATE_EMOJIS.music,
        image: "",
      });
      clearPendingCapturedImgForChat();
      display({ image_icon_visible: false });
      ctx.transitionTo("music");
    };
    const transitionDirectlyToImage = (
      message: string,
      imagePath: string,
      status = "photo ready",
    ): void => {
      llmResponseText = message;
      stopPlaying();
      ctx.rememberLastAnswer({
        text: message,
        emoji: STATE_EMOJIS.answering,
        image: imagePath,
      });
      clearPendingCapturedImgForChat();
      display({
        status,
        image: imagePath,
        image_icon_visible: false,
        text: `[camera]${message}`,
        text_input_enabled: true,
      });
      ctx.transitionTo("image");
    };
    const transitionToPromptStage = (
      message: string,
      status = "photo edit",
    ): void => {
      llmResponseText = message;
      stopPlaying();
      ctx.rememberLastAnswer({
        text: message,
        emoji: STATE_EMOJIS.answering,
        image: "",
      });
      clearPendingCapturedImgForChat();
      display({
        status,
        image: "",
        image_icon_visible: false,
        text: message,
        text_input_enabled: true,
        RGB: "#8a4dff",
      });
      ctx.transitionTo("image");
    };
    const runImageGenerationRequest = (
      prompt: string,
      withImageContext: boolean,
      baseImagePath = "",
      showSourceImageDuringGeneration = true,
    ): void => {
      display({
        status: "thinking",
        emoji: STATE_EMOJIS.answering,
        RGB: "#8a4dff",
        ...(showSourceImageDuringGeneration ? {} : { image: "" }),
        image_icon_visible: false,
        text: "[generateImage]Creating image...",
      });
      void runReplyFlow(async () => {
        if (baseImagePath && showSourceImageDuringGeneration) {
          activateInteractiveImage(baseImagePath, "other");
          queueDisplayImage(baseImagePath);
          display({
            image: baseImagePath,
            image_icon_visible: false,
          });
        }
        await llmFuncMap.generateImage({
          prompt,
          withImageContext,
        })
          .then((result) => {
            if (currentAnswerId !== ctx.answerId) {
              return;
            }
            const cleaned = stripToolTag(result);
            if (result.startsWith(ToolReturnTag.Error)) {
              trackingPartial(
                cleaned || "I couldn't create that image right now.",
              );
              endPartial();
              return;
            }
            const generatedImagePath = getLatestGenImg();
            if (generatedImagePath) {
              activateInteractiveImage(generatedImagePath, "other");
              display({ image: generatedImagePath });
            }
            trackingPartial(cleaned || "Image created. Take a look.");
            endPartial();
          })
          .catch((error) => {
            console.error("Image generation routing failed:", error);
            if (currentAnswerId !== ctx.answerId) {
              return;
            }
            trackingPartial("I couldn't create that image right now.");
            endPartial();
          });
      });
    };
    const pendingImageEdit = getPendingImageEditConfirmation();
    const imageEditCommand = parseImageEditConfirmCommand(ctx.asrText);
    if (pendingImageEdit && imageEditCommand) {
      if (imageEditCommand.type === "cancel") {
        clearPendingImageEditConfirmation();
        transitionToPromptStage("Canceled the pending image edit.");
        return;
      }
      if (imageEditCommand.type === "show") {
        transitionToPromptStage(
          buildImageEditConfirmMessage(pendingImageEdit.prompt),
        );
        return;
      }
      if (imageEditCommand.type === "add" && imageEditCommand.value) {
        const nextPrompt = `${pendingImageEdit.prompt}. ${imageEditCommand.value}`;
        setPendingImageEditConfirmation(nextPrompt, pendingImageEdit.imagePath);
        transitionToPromptStage(buildImageEditConfirmMessage(nextPrompt));
        return;
      }
      if (imageEditCommand.type === "replace" && imageEditCommand.value) {
        setPendingImageEditConfirmation(
          imageEditCommand.value,
          pendingImageEdit.imagePath,
        );
        transitionToPromptStage(
          buildImageEditConfirmMessage(imageEditCommand.value),
        );
        return;
      }
      if (imageEditCommand.type === "confirm") {
        clearPendingImageEditConfirmation();
        runImageGenerationRequest(
          pendingImageEdit.prompt,
          true,
          pendingImageEdit.imagePath,
          false,
        );
        return;
      }
    }
    if (pendingImageEdit && !isConfirmableImageEditRequest(ctx.asrText)) {
      clearPendingImageEditConfirmation();
    }
    if (shouldOpenSettings(ctx.asrText)) {
      ctx.openSettingsMenu();
      return;
    }
    if (shouldCycleHatTextStyle(ctx.asrText)) {
      const nextColor = getNextHatTextColor(getRuntimeSettings().hatTextColor);
      saveRuntimeSettings({ hatTextColor: nextColor });
      display({
        hat_text_color: nextColor,
        text: `[style]Text ${getHatTextColorLabel(nextColor)}.`,
      });
      finishDirectMessage(`Text style ${getHatTextColorLabel(nextColor)}.`);
      return;
    }
    if (shouldCycleHeaderTheme(ctx.asrText)) {
      const nextHeader = getNextHeaderMode(getRuntimeSettings().headerMode);
      saveRuntimeSettings({ headerMode: nextHeader });
      display({
        header_mode: nextHeader,
        text: `[style]Header ${getHeaderModeLabel(nextHeader)}.`,
      });
      finishDirectMessage(`Theme ${getHeaderModeLabel(nextHeader)}.`);
      return;
    }
    if (shouldOpenVoiceHelp(ctx.asrText)) {
      ctx.endWakeSession();
      ctx.transitionTo("voice_command_help");
      return;
    }
    if (shouldReplayLastAnswerAloud(ctx.asrText)) {
      if (!ctx.lastAnswerText) {
        finishDirectMessage("I don't have anything to read back yet.");
        return;
      }
      ctx.repeatLastAnswerAloud();
      return;
    }
    if (shouldClearChat(ctx.asrText)) {
      resetChatHistory();
      ctx.knowledgePrompts = [];
      ctx.clearLastAnswer();
      clearPendingCapturedImgForChat();
      finishDirectMessage("Started a new chat.");
      return;
    }
    if (shouldOpenCameraPreview(ctx.asrText)) {
      stopPlaying();
      clearPendingCapturedImgForChat();
      display({ image_icon_visible: false });
      const result = ctx.openCameraPreview();
      if (!result.ok) {
        finishDirectMessage(result.message);
      }
      return;
    }
    const musicCommand = getMusicControlCommand(ctx.asrText);
    if (musicCommand) {
      const player = getManagedMusicPlayer(process.env);
      if (musicCommand === "stop") {
        stopMusicPlayback();
        ctx.musicDisplayText = "";
        transitionDirectlyToSleep("Stopped music.");
        return;
      }

      const actionPromise =
        musicCommand === "play"
          ? player.prepareManagedLibraryPlayback(getRuntimeSettings().musicShuffle)
          : musicCommand === "next"
            ? player.prepareNextTrack()
            : player.preparePreviousTrack();

      void actionPromise
        .then((result) => {
          if (currentAnswerId !== ctx.answerId) {
            return;
          }
          if (!result.ok) {
            finishDirectMessage(result.message);
            return;
          }
          ctx.musicDisplayText = result.message;
          transitionDirectlyToMusic(result.message);
        })
        .catch((error) => {
          console.error("Music command failed:", error);
          if (currentAnswerId !== ctx.answerId) {
            return;
          }
          const message =
            error instanceof Error && error.message
              ? error.message
              : "I couldn't control the music right now.";
          finishDirectMessage(message);
        });
      return;
    }
    const volumeCommand = parseVolumeCommand(ctx.asrText);
    if (volumeCommand) {
      let nextLevel = getRuntimeSettings().volumeLevel;
      if (volumeCommand.action === "set") {
        nextLevel = volumeCommand.value;
      } else {
        nextLevel = Math.max(
          VOLUME_LEVEL_OPTIONS[0],
          Math.min(
            VOLUME_LEVEL_OPTIONS[VOLUME_LEVEL_OPTIONS.length - 1],
            nextLevel + volumeCommand.delta,
          ),
        );
      }
      const appliedLevel = setVolumeByLevel(nextLevel);
      saveRuntimeSettings({ volumeLevel: appliedLevel });
      finishDirectMessage(`Volume ${appliedLevel} out of 10.`);
      return;
    }
    const scrollSpeedCommand = parseScrollSpeedCommand(ctx.asrText);
    if (scrollSpeedCommand) {
      let nextLevel = getRuntimeSettings().hatScrollSpeedLevel;
      if (scrollSpeedCommand.action === "set") {
        nextLevel = scrollSpeedCommand.value;
      } else {
        nextLevel = Math.max(
          HAT_SCROLL_SPEED_OPTIONS[0],
          Math.min(
            HAT_SCROLL_SPEED_OPTIONS[HAT_SCROLL_SPEED_OPTIONS.length - 1],
            nextLevel + scrollSpeedCommand.delta,
          ),
        );
      }
      saveRuntimeSettings({ hatScrollSpeedLevel: nextLevel });
      display({ hat_scroll_speed_factor: nextLevel });
      finishDirectMessage(`Scroll speed ${nextLevel} out of 10.`);
      return;
    }
    const screenTimeoutCommand = parseScreenTimeoutCommand(ctx.asrText);
    if (screenTimeoutCommand !== null) {
      saveRuntimeSettings({ idleTimeoutSec: screenTimeoutCommand });
      display({ idle_timeout_sec: screenTimeoutCommand });
      finishDirectMessage(
        screenTimeoutCommand <= 0
          ? "Screen timeout off."
          : `Screen timeout ${getIdleTimeoutLabel(screenTimeoutCommand)}.`,
      );
      return;
    }
    if (shouldSwitchCamera(ctx.asrText)) {
      const currentSource = getRuntimeSettings().cameraSource;
      const nextSource = getNextCameraSource(currentSource);
      saveRuntimeSettings({ cameraSource: nextSource });
      const label = getCameraSourceLabel(nextSource);
      display({
        text: `[camera]Camera source ${label}.`,
      });
      finishDirectMessage(`Camera source ${label}.`);
      return;
    }
    if (shouldAdvanceBotNetModel(ctx.asrText)) {
      const nextModel = getBotNetManager().selectNextModel();
      const label = getBotNetModelLabel(nextModel.id);
      finishDirectMessage(`BotNet model ${label}.`);
      return;
    }
    if (shouldShutdown(ctx.asrText)) {
      void ctx.shutdownDevice();
      return;
    }
    const voiceModeCommand = getVoiceModeCommand(ctx.asrText);
    if (voiceModeCommand) {
      saveRuntimeSettings({ voiceMode: voiceModeCommand });
      const modeLabel = getVoiceModeLabel(voiceModeCommand);
      const message =
        voiceModeCommand === "voice-chat"
          ? `Voice mode ${modeLabel}. I'll talk to you now.`
          : `Voice mode ${modeLabel}. I'll stay quiet.`;
      finishDirectMessage(message);
      return;
    }
    if (shouldBrowsePhotos(ctx.asrText)) {
      const galleries = getAvailableManualGalleryEntries();
      if (!galleries.length) {
        display({ text: "[gallery]No saved photos yet." });
        finishDirectMessage("No saved photos yet.");
        return;
      }
      ctx.endWakeSession();
      ctx.transitionTo(galleries[0].flow);
      return;
    }
    if (shouldCaptureImage(ctx.asrText)) {
      void captureAndPrepareLatestImage()
        .then((captureImagePath) => {
          if (currentAnswerId !== ctx.answerId) {
            return;
          }
          showCameraCapturedCue();
          display({
            image: captureImagePath,
            image_icon_visible: false,
            text: "[camera]Photo captured.",
          });
          transitionDirectlyToImage("Photo captured.", captureImagePath);
        })
        .catch((error) => {
          console.error("Voice capture failed:", error);
          stopCameraCaptureCue();
          if (currentAnswerId !== ctx.answerId) {
            return;
          }
          const message =
            error instanceof Error && error.message
              ? error.message
              : "I couldn't capture a photo right now.";
          display({
            text: `[camera]${message}`,
          });
          finishDirectMessage(message);
        });
      return;
    }
    if (
      shouldShowInteractiveImage(ctx.asrText) ||
      shouldOpenInteractiveImageEditor(ctx.asrText)
    ) {
      const target = getDefaultInteractiveImageTarget();
      if (!target) {
        finishDirectMessage("Show or capture a photo first.");
        return;
      }
      activateInteractiveImage(target.path, target.source);
      transitionDirectlyToImage(
        shouldOpenInteractiveImageEditor(ctx.asrText)
          ? "Ready to edit this photo."
          : "Showing photo.",
        target.path,
      );
      return;
    }
    if (shouldRouteToWeather(ctx.asrText)) {
      if (!isWeatherConfigured()) {
        finishDirectMessage("Set weather latitude and longitude in Settings first.");
        return;
      }
      display({
        text: "[weather]Checking NWS forecast...",
      });
      void runReplyFlow(async () => {
        await fetchWeatherSnapshot()
          .then(async (snapshot) => {
            if (currentAnswerId !== ctx.answerId) {
              return;
            }
            const relayReply = await streamWeatherRelayReply(
              ctx.asrText,
              snapshot.combinedText,
              (chunk) => {
                if (currentAnswerId === ctx.answerId) {
                  trackingPartial(chunk);
                }
              },
            );
            const finalReply =
              relayReply || snapshot.combinedText || "I couldn't fetch the weather right now.";
            if (!relayReply && currentAnswerId === ctx.answerId) {
              trackingPartial(finalReply);
            }
            endPartial();
            finishDirectAnswer();
          })
          .catch((error) => {
            console.error("Weather routing failed:", error);
            if (currentAnswerId !== ctx.answerId) {
              return;
            }
            const message =
              error instanceof Error && error.message
                ? error.message
                : "I couldn't fetch the weather right now.";
            trackingPartial(message);
            endPartial();
            finishDirectAnswer();
          });
      });
      return;
    }
    const imageEffectCommand = parseImageEffectCommand(ctx.asrText);
    if (imageEffectCommand) {
      const currentImagePath = getLatestShowedImage();
      if (!currentImagePath) {
        finishDirectMessage("Show or capture a photo first.");
        return;
      }
      display({
        text: `[effects]Applying ${getImageEffectLabel(imageEffectCommand)} effect...`,
      });
      void applyImageEffect(currentImagePath, imageEffectCommand)
        .then((editedImagePath) => {
          if (currentAnswerId !== ctx.answerId) {
            return;
          }
          setLatestCapturedImg(editedImagePath);
          clearLatestVisionAnalysis();
          display({
            image: editedImagePath,
            text: `[effects]Applied ${getImageEffectLabel(imageEffectCommand)} effect.`,
          });
          finishDirectMessage(
            `Applied ${getImageEffectLabel(imageEffectCommand)} effect.`,
            true,
          );
        })
        .catch((error) => {
          console.error("Image effect failed:", error);
          if (currentAnswerId !== ctx.answerId) {
            return;
          }
          const message =
            error instanceof Error && error.message
              ? error.message
              : "I couldn't edit that image right now.";
          display({
            text: `[effects]${message}`,
          });
          finishDirectMessage(message);
        });
      return;
    }
    [() => Promise.resolve().then(() => ""), getSystemPromptWithKnowledge]
    [enableRAG ? 1 : 0](ctx.asrText)
      .then((res: string) => {
        if (
          shouldRouteToImageGeneration(ctx.asrText) &&
          typeof llmFuncMap.generateImage === "function"
        ) {
          const withImageContext = shouldUseImageContextForGeneration(ctx.asrText);
          if (shouldUseImagePromptHelper(withImageContext)) {
            const currentImagePath = getLatestShowedImage();
            if (currentImagePath) {
              display({
                status: "photo edit",
                emoji: STATE_EMOJIS.answering,
                RGB: "#8a4dff",
                image_icon_visible: false,
                text: "[promptHelper]Refining your photo edit prompt...",
              });
              void runReplyFlow(async () => {
                try {
                  const helperPrompt = await buildPersonalityImagePrompt(ctx.asrText);
                  if (currentAnswerId !== ctx.answerId) {
                    return;
                  }
                  if (shouldUseImageEditConfirmMode()) {
                    setPendingImageEditConfirmation(helperPrompt, currentImagePath);
                    transitionToPromptStage(
                      buildImageEditConfirmMessage(helperPrompt),
                    );
                    return;
                  }
                  clearPendingImageEditConfirmation();
                  runImageGenerationRequest(helperPrompt, true, currentImagePath);
                } catch (error) {
                  console.error("Image prompt helper failed:", error);
                  if (currentAnswerId !== ctx.answerId) {
                    return;
                  }
                  transitionDirectlyToImage(
                    "Prompt helper failed, so I did not run the edit.",
                    currentImagePath,
                  );
                }
              });
              return;
            }
          }
          if (shouldUseImageEditConfirmMode() && withImageContext) {
            const currentImagePath = getLatestShowedImage();
            if (currentImagePath) {
              setPendingImageEditConfirmation(ctx.asrText, currentImagePath);
              transitionToPromptStage(
                buildImageEditConfirmMessage(ctx.asrText),
              );
              return;
            }
          }
          clearPendingImageEditConfirmation();
          runImageGenerationRequest(ctx.asrText, withImageContext);
          return;
        }
        if (shouldRouteToVision(ctx.asrText) && typeof llmFuncMap.describeImage === "function") {
          display({
            text: "[describeImage]Analyzing uploaded image...",
          });
          void runReplyFlow(async () => {
            await llmFuncMap.describeImage({ prompt: ctx.asrText })
              .then(async (result) => {
                if (currentAnswerId !== ctx.answerId) {
                  return;
                }
              const cleaned = stripToolTag(result);
              if (!cleaned) {
                trackingPartial("I couldn't analyze that image.");
                endPartial();
                return;
              }
              if (result.startsWith(ToolReturnTag.Error)) {
                setLatestVisionAnalysis({
                  question: ctx.asrText,
                  rawResponse: cleaned,
                  relayResponse: cleaned,
                  updatedAt: Date.now(),
                  ok: false,
                });
                trackingPartial(cleaned);
                endPartial();
                return;
              }
              const relayReply = await streamVisionRelayReply(
                ctx.asrText,
                cleaned,
                (chunk) => {
                  if (currentAnswerId === ctx.answerId) {
                    trackingPartial(chunk);
                  }
                },
              );
              const finalReply = relayReply || cleaned || "I couldn't analyze that image.";
              if (!relayReply && currentAnswerId === ctx.answerId) {
                trackingPartial(finalReply);
              }
              setLatestVisionAnalysis({
                question: ctx.asrText,
                rawResponse: cleaned,
                relayResponse: finalReply,
                updatedAt: Date.now(),
                  ok: true,
                });
                endPartial();
              })
              .catch((error) => {
              console.error("Vision routing failed:", error);
              if (currentAnswerId !== ctx.answerId) {
                return;
              }
              const message = "I couldn't analyze that image right now.";
                setLatestVisionAnalysis({
                  question: ctx.asrText,
                  rawResponse: message,
                  relayResponse: message,
                  updatedAt: Date.now(),
                  ok: false,
                });
                trackingPartial(message);
                endPartial();
              });
          });
          return;
        }
        let knowledgePrompt = res;
        if (res) {
          console.log("Retrieved knowledge for RAG:\n", res);
        }
        if (ctx.knowledgePrompts.includes(res)) {
          console.log(
            "[RAG] Knowledge prompt already used in this session, skipping to avoid repetition.",
          );
          knowledgePrompt = "";
        }
        if (knowledgePrompt) {
          ctx.knowledgePrompts.push(knowledgePrompt);
        }
        display({
          rag_icon_visible: Boolean(enableRAG && knowledgePrompt),
        });
        const prompt: {
          role: "system" | "user";
          content: string;
        }[] = compact([
          knowledgePrompt
            ? {
              role: "system",
              content: knowledgePrompt,
            }
            : null,
          {
            role: "user",
            content: ctx.asrText,
          },
        ]);
        void runReplyFlow(async () => {
          await chatWithLLMStream(
            prompt,
            (text) => { if (currentAnswerId === ctx.answerId) trackingPartial(text); },
            () => currentAnswerId === ctx.answerId && endPartial(),
            (partialThinking) =>
              currentAnswerId === ctx.answerId &&
              ctx.partialThinkingCallback(partialThinking),
            (functionName: string, result?: string) => {
              if (
                functionName === "endConversation" &&
                result?.startsWith("[success]")
              ) {
                ctx.endAfterAnswer = true;
              }
              if (
                functionName === "generateImage" &&
                result?.startsWith("[success]")
              ) {
                const img = getLatestGenImg();
                if (img) {
                  display({ image: img });
                }
              }
              if (
                functionName.startsWith("playMusic") &&
                result?.startsWith("[success]")
              ) {
                ctx.enterMusicAfterAnswer = true;
                ctx.musicDisplayText = result.replace(/^\[success\]/, "").trim();
              }
              if (result) {
                display({
                  text: `[${functionName}]${result}`,
                });
              } else {
                display({
                  text: `Invoking [${functionName}]... {count}s`,
                });
              }
            },
          );
        });
      });
    getPlayEndPromise().then(() => {
      if (ctx.currentFlowName === "answer") {
        const img = getLatestDisplayImg();
        ctx.rememberLastAnswer({
          text: llmResponseText,
          emoji: STATE_EMOJIS.answering,
          image: img || "",
        });
        autoSaveExchange(ctx.asrText, llmResponseText);
        clearPendingCapturedImgForChat();
        display({ image_icon_visible: false });
        if (ctx.wakeSessionActive || ctx.endAfterAnswer) {
          if (ctx.endAfterAnswer) {
            ctx.endWakeSession();
            ctx.transitionTo("sleep");
          } else {
            ctx.transitionTo("wake_listening");
          }
          return;
        }
        if (ctx.enterMusicAfterAnswer) {
          ctx.transitionTo("music");
          return;
        }
        if (img) {
          ctx.transitionTo("image");
        } else {
          replayTextAnswerAndSleep(ctx, "answer", currentAnswerId);
        }
      }
    });
    onButtonPressed(() => {
      stopPlaying();
      clearPendingCapturedImgForChat();
      display({ image_icon_visible: false });
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
  },
  image: (ctx: ChatFlowContext) => {
    display({ text_input_enabled: true });
    onTextInput((text: string) => {
      ctx.answerId += 1;
      ctx.asrText = text;
      display({ status: "recognizing", text, text_input_enabled: true });
      ctx.transitionTo("answer");
    });
    onButtonPressed(() => {
      display({ image: "" });
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
  },
  external_answer: (ctx: ChatFlowContext) => {
    if (!ctx.pendingExternalReply && !ctx.pendingExternalImageUrl) {
      ctx.transitionTo("sleep");
      return;
    }
    display({
      status: "answering...",
      RGB: "#00c8a3",
      emoji: ctx.pendingExternalEmoji || STATE_EMOJIS.answering,
    });
    onButtonPressed(() => {
      ctx.streamResponser.stop();
      display({ image: "" });
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    const replyText = ctx.pendingExternalReply;
    const replyEmoji = ctx.pendingExternalEmoji;
    const replyImageUrl = ctx.pendingExternalImageUrl;
    const replyForceSpeech = ctx.pendingExternalForceSpeech;
    ctx.currentExternalEmoji = replyEmoji;
    ctx.pendingExternalReply = "";
    ctx.pendingExternalEmoji = "";
    ctx.pendingExternalImageUrl = "";
    ctx.pendingExternalForceSpeech = false;

    // Display the image if one was provided
    if (replyImageUrl) {
      display({ image: replyImageUrl });
    }

    if (replyText) {
      void ctx.streamExternalReply(replyText, replyEmoji, replyForceSpeech);
      ctx.streamResponser.getPlayEndPromise().then(() => {
        if (ctx.currentFlowName !== "external_answer") return;
        ctx.rememberLastAnswer({
          text: replyText,
          emoji: replyEmoji || STATE_EMOJIS.answering,
          image: replyImageUrl || "",
        });
        if (ctx.wakeSessionActive || ctx.endAfterAnswer) {
          if (ctx.endAfterAnswer) {
            ctx.endWakeSession();
            ctx.transitionTo("sleep");
          } else {
            ctx.transitionTo("wake_listening");
          }
        } else if (replyImageUrl) {
          // Stay in image display mode after TTS finishes
          ctx.transitionTo("image");
        } else {
          replayTextAnswerAndSleep(ctx, "external_answer", ctx.answerId);
        }
      });
    } else {
      // Image only, no text to speak — go to image display mode
      ctx.transitionTo("image");
    }
  },
};
