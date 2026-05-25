const http = require("http");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { execFile, spawnSync } = require("child_process");
const { BotNetHubTransport, DEFAULT_SESSION, sanitizeSession, normalizeUrl } = require("./online-transport");

const PORT = Number.parseInt(process.env.PORT || "18990", 10);
const HOST = process.env.HOST || "0.0.0.0";
const ROOT_DIR = __dirname;
const PUBLIC_DIR = path.join(ROOT_DIR, "public");
const DATA_DIR = path.resolve(process.env.GROQBOTNET_DATA_DIR || path.join(ROOT_DIR, "data"));
const SETTINGS_PATH = path.join(DATA_DIR, "settings.json");
const CONVERSATIONS_PATH = path.join(DATA_DIR, "conversations.json");
const STATE_PATH = path.join(DATA_DIR, "state.json");
const HUB_SESSION_PATH = path.join(DATA_DIR, "botnet-hub-session.json");
const ROOM_MONITOR_DIR = path.join(DATA_DIR, "room-monitor");
const AI_IMAGE_IMPORT_DIR = path.join(DATA_DIR, "ai-imports");
const AI_IMAGE_IMPORT_INDEX_PATH = path.join(AI_IMAGE_IMPORT_DIR, "index.json");
const ROOM_MONITOR_AUTO_BRIGHTNESS_SCRIPT = path.join(ROOT_DIR, "display", "auto_brightness.py");

fs.mkdirSync(DATA_DIR, { recursive: true });
fs.mkdirSync(ROOM_MONITOR_DIR, { recursive: true });
fs.mkdirSync(AI_IMAGE_IMPORT_DIR, { recursive: true });

const DEFAULT_SETTINGS = {
  enabled: true,
  botName: "GroqBotNet Bot",
  botnetMode: "persona-relay",
  model: "llama-3.1-8b-instant",
  transportMode: "lan-direct",
  publicBaseUrl: "",
  peerUrl: "",
  hubUrl: "",
  nodeHandle: "GroqBotNet Bot",
  personalityPrompt:
    "You are a concise, imaginative chatbot talking to another chatbot. Stay in character, be conversational, and keep replies reasonably short.",
  memoryTurns: 12,
  replyDelaySec: 6,
  maxBotReplies: 8,
  maxRequestsPerHour: 30,
  roomMonitorIntervalSec: 0,
  roomMonitorStartTime: "",
  roomMonitorStopTime: "",
  roomMonitorFreeReserveGb: 8,
  roomMonitorAutoBrightness: true,
  aiImageSyncEnabled: true,
  aiImageSyncIntervalSec: 30,
  tftDisplayMode: "auto",
  companionIdleTimeoutSec: 20,
  companionIdleMode: "slideshow",
  companionPhotoHoldSec: 20,
  companionOledIdleMode: "rain",
  companionTextColor: "multicolor",
  companionScrollSpeedSec: 0.25,
  groqApiKey: "",
};

const DEFAULT_STATE = {
  requestTimestamps: [],
};

const timers = new Map();
let roomMonitorTimer = null;
let roomMonitorCaptureInProgress = false;
let roomMonitorLastCaptureAt = null;
let roomMonitorLastError = "";
let roomMonitorDetectedCamera = "";
let roomMonitorCameraCommand = "";
let roomMonitorLastBrightnessSummary = "";
let aiImageSyncTimer = null;
let aiImageSyncInProgress = false;
let aiImageSyncLastError = "";
let aiImageSyncLastSyncAt = 0;
let companionPollTimer = null;

const DEFAULT_AI_IMAGE_ARCHIVE = {
  latestLocalFileName: "",
  lastImportedAt: 0,
  entries: [],
};

const DEFAULT_COMPANION_SNAPSHOT = {
  configured: false,
  ready: false,
  reachable: false,
  sourceUrl: "",
  status: "idle",
  replyMessage: "",
  editHelperText: "",
  modelTag: "BOT",
  modelLabel: "BOT",
  badgeText: "",
  requestsToday: 0,
  remainingRequests: null,
  balanceText: "",
  imageAvailable: false,
  imageUrl: "",
  imageRevision: 0,
  lastSuccessAt: 0,
  lastError: "",
};

const BOTNET_MODEL_META = {
  "llama-3.1-8b-instant": { shortLabel: "L3.1-8B", rpd: 14400 },
  "llama-3.3-70b-versatile": { shortLabel: "L3.3-70B", rpd: 1000 },
  "meta-llama/llama-4-scout-17b-16e-instruct": { shortLabel: "L4-Scout", rpd: 1000 },
  "qwen/qwen3-32b": { shortLabel: "Qwen3-32B", rpd: 1000 },
  "compound-beta": { shortLabel: "Compound", rpd: 250 },
  "compound-beta-mini": { shortLabel: "Cmpd Mini", rpd: 250 },
  "openai/gpt-oss-20b": { shortLabel: "GPT-20B", rpd: 1000 },
  "openai/gpt-oss-120b": { shortLabel: "GPT-120B", rpd: 1000 },
};

function readJson(filePath, fallback) {
  try {
    return JSON.parse(fs.readFileSync(filePath, "utf8"));
  } catch {
    return fallback;
  }
}

function writeJson(filePath, value) {
  fs.writeFileSync(filePath, JSON.stringify(value, null, 2));
}

function normalizePositiveInt(value, fallback, minimum = 1, maximum = 500) {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  return Math.min(maximum, Math.max(minimum, parsed));
}

function normalizeRoomMonitorIntervalSec(value, fallback) {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  if (parsed <= 0) {
    return 0;
  }
  return Math.min(86400, Math.max(10, parsed));
}

function normalizeAiImageSyncIntervalSec(value, fallback) {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  if (parsed <= 0) {
    return 0;
  }
  return Math.min(3600, Math.max(10, parsed));
}

function normalizeTftDisplayMode(value) {
  const normalized = String(value || "").trim().toLowerCase();
  if (normalized === "local" || normalized === "companion") {
    return normalized;
  }
  return "auto";
}

function normalizeCompanionIdleTimeoutSec(value, fallback) {
  return normalizePositiveInt(value, fallback, 5, 600);
}

function normalizeCompanionPhotoHoldSec(value, fallback) {
  return normalizePositiveInt(value, fallback, 5, 600);
}

function normalizeCompanionIdleMode(value) {
  const normalized = String(value || "").trim().toLowerCase();
  if (normalized === "matrix") {
    return "matrix";
  }
  return "slideshow";
}

function normalizeCompanionTextColor(value) {
  const normalized = String(value || "").trim().toLowerCase();
  if (["white", "green", "cyan", "yellow", "multicolor"].includes(normalized)) {
    return normalized;
  }
  return "multicolor";
}

function normalizeCompanionScrollSpeedSec(value, fallback) {
  const parsed = Number.parseFloat(String(value ?? ""));
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  return Math.min(2, Math.max(0.08, parsed));
}

function normalizeCompanionOledIdleMode(value) {
  const normalized = String(value || "").trim().toLowerCase();
  if (normalized === "header") {
    return "header";
  }
  return "rain";
}

function normalizeFileName(value) {
  return path.basename(String(value || "").trim());
}

function normalizeTimeOfDay(value) {
  const trimmed = String(value || "").trim();
  if (!trimmed) {
    return "";
  }
  const match = trimmed.match(/^(\d{1,2}):(\d{2})$/);
  if (!match) {
    return "";
  }
  const hours = Number.parseInt(match[1], 10);
  const minutes = Number.parseInt(match[2], 10);
  if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
    return "";
  }
  return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}`;
}

function normalizeRoomMonitorFreeReserveGb(value, fallback) {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  return Math.min(8, Math.max(5, parsed));
}

function normalizeBoolean(value, fallback) {
  if (typeof value === "boolean") {
    return value;
  }
  if (value === undefined || value === null || value === "") {
    return fallback;
  }
  const normalized = String(value).trim().toLowerCase();
  if (["1", "true", "yes", "on"].includes(normalized)) {
    return true;
  }
  if (["0", "false", "no", "off"].includes(normalized)) {
    return false;
  }
  return fallback;
}

function sanitizeAiImageArchive(archive) {
  const entries = Array.isArray(archive?.entries)
    ? archive.entries
        .filter((entry) => entry && typeof entry === "object")
        .map((entry) => ({
          sourceUrl: normalizeUrl(entry.sourceUrl || ""),
          remoteFileName: normalizeFileName(entry.remoteFileName || ""),
          localFileName: normalizeFileName(entry.localFileName || ""),
          importedAt: Number.parseInt(String(entry.importedAt || "0"), 10) || 0,
          updatedAt: Number.parseInt(String(entry.updatedAt || "0"), 10) || 0,
          sizeBytes: Number.parseInt(String(entry.sizeBytes || "0"), 10) || 0,
        }))
        .filter((entry) => entry.sourceUrl && entry.remoteFileName && entry.localFileName)
    : [];

  const seen = new Set();
  const filtered = [];
  for (const entry of entries) {
    const key = `${entry.sourceUrl}|${entry.remoteFileName}`;
    const filePath = path.join(AI_IMAGE_IMPORT_DIR, entry.localFileName);
    if (seen.has(key) || !fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
      continue;
    }
    seen.add(key);
    filtered.push(entry);
  }

  const latestLocalFileName = normalizeFileName(archive?.latestLocalFileName || "");
  return {
    latestLocalFileName:
      latestLocalFileName &&
      filtered.some((entry) => entry.localFileName === latestLocalFileName)
        ? latestLocalFileName
        : filtered[0]?.localFileName || "",
    lastImportedAt: Number.parseInt(String(archive?.lastImportedAt || "0"), 10) || 0,
    entries: filtered.sort((a, b) => b.importedAt - a.importedAt),
  };
}

function normalizeBotnetMode(value) {
  return value === "auto-bot" ? "auto-bot" : "persona-relay";
}

function normalizeTransportMode(value) {
  return value === "online-hub" ? "online-hub" : "lan-direct";
}

function loadSettings() {
  const loaded = readJson(SETTINGS_PATH, {});
  return {
    ...DEFAULT_SETTINGS,
    ...loaded,
    enabled: loaded.enabled !== false,
    botnetMode: normalizeBotnetMode(loaded.botnetMode || DEFAULT_SETTINGS.botnetMode),
    transportMode: normalizeTransportMode(loaded.transportMode || DEFAULT_SETTINGS.transportMode),
    publicBaseUrl: normalizeUrl(loaded.publicBaseUrl || DEFAULT_SETTINGS.publicBaseUrl),
    peerUrl: normalizeUrl(loaded.peerUrl || DEFAULT_SETTINGS.peerUrl),
    hubUrl: normalizeUrl(loaded.hubUrl || DEFAULT_SETTINGS.hubUrl),
    nodeHandle: String(loaded.nodeHandle || loaded.botName || DEFAULT_SETTINGS.nodeHandle).trim() || DEFAULT_SETTINGS.nodeHandle,
    memoryTurns: normalizePositiveInt(loaded.memoryTurns, DEFAULT_SETTINGS.memoryTurns, 1, 50),
    replyDelaySec: normalizePositiveInt(loaded.replyDelaySec, DEFAULT_SETTINGS.replyDelaySec, 0, 3600),
    maxBotReplies: normalizePositiveInt(loaded.maxBotReplies, DEFAULT_SETTINGS.maxBotReplies, 0, 200),
    maxRequestsPerHour: normalizePositiveInt(
      loaded.maxRequestsPerHour,
      DEFAULT_SETTINGS.maxRequestsPerHour,
      1,
      500,
    ),
    roomMonitorIntervalSec: normalizeRoomMonitorIntervalSec(
      loaded.roomMonitorIntervalSec,
      DEFAULT_SETTINGS.roomMonitorIntervalSec,
    ),
    roomMonitorStartTime: normalizeTimeOfDay(
      loaded.roomMonitorStartTime || DEFAULT_SETTINGS.roomMonitorStartTime,
    ),
    roomMonitorStopTime: normalizeTimeOfDay(
      loaded.roomMonitorStopTime || DEFAULT_SETTINGS.roomMonitorStopTime,
    ),
    roomMonitorFreeReserveGb: normalizeRoomMonitorFreeReserveGb(
      loaded.roomMonitorFreeReserveGb,
      DEFAULT_SETTINGS.roomMonitorFreeReserveGb,
    ),
    roomMonitorAutoBrightness: normalizeBoolean(
      loaded.roomMonitorAutoBrightness,
      DEFAULT_SETTINGS.roomMonitorAutoBrightness,
    ),
    aiImageSyncEnabled: normalizeBoolean(
      loaded.aiImageSyncEnabled,
      DEFAULT_SETTINGS.aiImageSyncEnabled,
    ),
    aiImageSyncIntervalSec: normalizeAiImageSyncIntervalSec(
      loaded.aiImageSyncIntervalSec,
      DEFAULT_SETTINGS.aiImageSyncIntervalSec,
    ),
    tftDisplayMode: normalizeTftDisplayMode(
      loaded.tftDisplayMode || DEFAULT_SETTINGS.tftDisplayMode,
    ),
    companionIdleTimeoutSec: normalizeCompanionIdleTimeoutSec(
      loaded.companionIdleTimeoutSec,
      DEFAULT_SETTINGS.companionIdleTimeoutSec,
    ),
    companionIdleMode: normalizeCompanionIdleMode(
      loaded.companionIdleMode || DEFAULT_SETTINGS.companionIdleMode,
    ),
    companionPhotoHoldSec: normalizeCompanionPhotoHoldSec(
      loaded.companionPhotoHoldSec,
      DEFAULT_SETTINGS.companionPhotoHoldSec,
    ),
    companionOledIdleMode: normalizeCompanionOledIdleMode(
      loaded.companionOledIdleMode || DEFAULT_SETTINGS.companionOledIdleMode,
    ),
    companionTextColor: normalizeCompanionTextColor(
      loaded.companionTextColor || DEFAULT_SETTINGS.companionTextColor,
    ),
    companionScrollSpeedSec: normalizeCompanionScrollSpeedSec(
      loaded.companionScrollSpeedSec,
      DEFAULT_SETTINGS.companionScrollSpeedSec,
    ),
  };
}

function sanitizeSettingsUpdate(body, currentSettings) {
  return {
    ...currentSettings,
    enabled: body.enabled !== false && body.enabled !== "false",
    botName: String(body.botName || currentSettings.botName).trim() || currentSettings.botName,
    botnetMode: normalizeBotnetMode(body.botnetMode || currentSettings.botnetMode),
    model: String(body.model || currentSettings.model).trim() || currentSettings.model,
    transportMode: normalizeTransportMode(body.transportMode || currentSettings.transportMode),
    publicBaseUrl: normalizeUrl(body.publicBaseUrl ?? currentSettings.publicBaseUrl),
    peerUrl: normalizeUrl(body.peerUrl ?? currentSettings.peerUrl),
    hubUrl: normalizeUrl(body.hubUrl ?? currentSettings.hubUrl),
    nodeHandle: String(body.nodeHandle || currentSettings.nodeHandle).trim() || currentSettings.nodeHandle,
    personalityPrompt:
      String(body.personalityPrompt || currentSettings.personalityPrompt).trim() ||
      currentSettings.personalityPrompt,
    memoryTurns: normalizePositiveInt(body.memoryTurns, currentSettings.memoryTurns, 1, 50),
    replyDelaySec: normalizePositiveInt(body.replyDelaySec, currentSettings.replyDelaySec, 0, 3600),
    maxBotReplies: normalizePositiveInt(body.maxBotReplies, currentSettings.maxBotReplies, 0, 200),
    maxRequestsPerHour: normalizePositiveInt(
      body.maxRequestsPerHour,
      currentSettings.maxRequestsPerHour,
      1,
      500,
    ),
    roomMonitorIntervalSec: normalizeRoomMonitorIntervalSec(
      body.roomMonitorIntervalSec,
      currentSettings.roomMonitorIntervalSec,
    ),
    roomMonitorStartTime: normalizeTimeOfDay(
      body.roomMonitorStartTime ?? currentSettings.roomMonitorStartTime,
    ),
    roomMonitorStopTime: normalizeTimeOfDay(
      body.roomMonitorStopTime ?? currentSettings.roomMonitorStopTime,
    ),
    roomMonitorFreeReserveGb: normalizeRoomMonitorFreeReserveGb(
      body.roomMonitorFreeReserveGb,
      currentSettings.roomMonitorFreeReserveGb,
    ),
    roomMonitorAutoBrightness: normalizeBoolean(
      body.roomMonitorAutoBrightness,
      currentSettings.roomMonitorAutoBrightness,
    ),
    aiImageSyncEnabled: normalizeBoolean(
      body.aiImageSyncEnabled,
      currentSettings.aiImageSyncEnabled,
    ),
    aiImageSyncIntervalSec: normalizeAiImageSyncIntervalSec(
      body.aiImageSyncIntervalSec,
      currentSettings.aiImageSyncIntervalSec,
    ),
    tftDisplayMode: normalizeTftDisplayMode(
      body.tftDisplayMode ?? currentSettings.tftDisplayMode,
    ),
    companionIdleTimeoutSec: normalizeCompanionIdleTimeoutSec(
      body.companionIdleTimeoutSec,
      currentSettings.companionIdleTimeoutSec,
    ),
    companionIdleMode: normalizeCompanionIdleMode(
      body.companionIdleMode ?? currentSettings.companionIdleMode,
    ),
    companionPhotoHoldSec: normalizeCompanionPhotoHoldSec(
      body.companionPhotoHoldSec,
      currentSettings.companionPhotoHoldSec,
    ),
    companionOledIdleMode: normalizeCompanionOledIdleMode(
      body.companionOledIdleMode ?? currentSettings.companionOledIdleMode,
    ),
    companionTextColor: normalizeCompanionTextColor(
      body.companionTextColor ?? currentSettings.companionTextColor,
    ),
    companionScrollSpeedSec: normalizeCompanionScrollSpeedSec(
      body.companionScrollSpeedSec,
      currentSettings.companionScrollSpeedSec,
    ),
    groqApiKey:
      typeof body.groqApiKey === "string" && body.groqApiKey.trim().length > 0
        ? body.groqApiKey.trim()
        : currentSettings.groqApiKey,
  };
}

let settings = loadSettings();
let conversations = readJson(CONVERSATIONS_PATH, []);
let runtimeState = {
  ...DEFAULT_STATE,
  ...readJson(STATE_PATH, DEFAULT_STATE),
};
let hubSession = sanitizeSession(readJson(HUB_SESSION_PATH, DEFAULT_SESSION));
let companionSnapshot = {
  ...DEFAULT_COMPANION_SNAPSHOT,
};
let aiImageArchive = sanitizeAiImageArchive(
  readJson(AI_IMAGE_IMPORT_INDEX_PATH, DEFAULT_AI_IMAGE_ARCHIVE),
);

function saveSettings() {
  writeJson(SETTINGS_PATH, settings);
}

function saveConversations() {
  writeJson(CONVERSATIONS_PATH, conversations);
}

function saveRuntimeState() {
  writeJson(STATE_PATH, runtimeState);
}

function saveHubSession() {
  writeJson(HUB_SESSION_PATH, hubSession);
}

function saveAiImageArchive() {
  aiImageArchive = sanitizeAiImageArchive(aiImageArchive);
  writeJson(AI_IMAGE_IMPORT_INDEX_PATH, aiImageArchive);
}

function getPublicSettings() {
  return {
    ...settings,
    groqApiKeyConfigured: Boolean(settings.groqApiKey),
    groqApiKey: "",
  };
}

function resetCompanionSnapshot(errorMessage = "") {
  companionSnapshot = {
    ...DEFAULT_COMPANION_SNAPSHOT,
    configured: Boolean(normalizeUrl(settings.peerUrl || "")),
    sourceUrl: normalizeUrl(settings.peerUrl || ""),
    lastError: errorMessage,
  };
}

function getCompanionSnapshotStatus() {
  return {
    ...companionSnapshot,
  };
}

function getCompanionModelMeta(modelTag) {
  return BOTNET_MODEL_META[String(modelTag || "").trim()] || null;
}

function isCompanionEditStatus(statusText, replyMessage) {
  const status = String(statusText || "").trim().toLowerCase();
  const reply = String(replyMessage || "").trim().toLowerCase();
  return (
    status.includes("photo") ||
    status.includes("camera") ||
    status.includes("image") ||
    status.includes("answering") ||
    reply.startsWith("[camera]") ||
    reply.includes("edit this photo") ||
    isGenericImageSavedMessage(reply)
  );
}

function isGenericImageSavedMessage(replyMessage) {
  const normalized = String(replyMessage || "").trim().toLowerCase();
  return normalized === "image file saved." || normalized.endsWith("image file saved.");
}

function getAiImportFilePath(fileName) {
  const safeFileName = normalizeFileName(fileName);
  if (!safeFileName) {
    return "";
  }
  const filePath = path.resolve(AI_IMAGE_IMPORT_DIR, safeFileName);
  if (!filePath.startsWith(`${path.resolve(AI_IMAGE_IMPORT_DIR)}${path.sep}`)) {
    return "";
  }
  if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    return "";
  }
  return filePath;
}

function getAiImageSourceBaseUrl() {
  return normalizeUrl(settings.peerUrl || "");
}

function buildAiImageArchiveKey(sourceUrl, remoteFileName) {
  return `${normalizeUrl(sourceUrl)}|${normalizeFileName(remoteFileName)}`;
}

function listAiImportedImages(limit = 60) {
  aiImageArchive = sanitizeAiImageArchive(aiImageArchive);
  const entries = aiImageArchive.entries.map((entry) => {
    const filePath = getAiImportFilePath(entry.localFileName);
    if (!filePath) {
      return null;
    }
    const stats = fs.statSync(filePath);
    return {
      sourceUrl: entry.sourceUrl,
      remoteFileName: entry.remoteFileName,
      fileName: entry.localFileName,
      importedAt: entry.importedAt,
      updatedAt: entry.updatedAt || stats.mtimeMs,
      sizeBytes: stats.size,
      imageUrl: `/api/ai-images/image?file=${encodeURIComponent(entry.localFileName)}`,
      isLatest: entry.localFileName === aiImageArchive.latestLocalFileName,
    };
  }).filter(Boolean);
  return entries.slice(0, limit);
}

async function fetchRemoteAiImageList() {
  const sourceUrl = getAiImageSourceBaseUrl();
  if (!sourceUrl) {
    throw new Error("Connect to a Whisplay peer before syncing AI images.");
  }
  const response = await fetch(`${sourceUrl}/api/generated-images`, {
    headers: { "Cache-Control": "no-store" },
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `Failed to load remote AI images (HTTP ${response.status}).`);
  }

  const importedKeys = new Set(
    aiImageArchive.entries.map((entry) => buildAiImageArchiveKey(entry.sourceUrl, entry.remoteFileName)),
  );
  return {
    sourceUrl,
    photos: Array.isArray(payload.photos)
      ? payload.photos
          .filter((photo) => photo && typeof photo.fileName === "string" && typeof photo.imageUrl === "string")
          .map((photo) => ({
            fileName: normalizeFileName(photo.fileName),
            imageUrl: String(photo.imageUrl),
            updatedAt: Number.parseInt(String(photo.updatedAt || "0"), 10) || 0,
            sizeBytes: Number.parseInt(String(photo.sizeBytes || "0"), 10) || 0,
            imported: importedKeys.has(buildAiImageArchiveKey(sourceUrl, photo.fileName)),
          }))
          .filter((photo) => photo.fileName && photo.imageUrl)
      : [],
  };
}

async function pollCompanionSnapshot() {
  const sourceUrl = normalizeUrl(settings.peerUrl || "");
  if (!sourceUrl) {
    resetCompanionSnapshot("");
    return companionSnapshot;
  }

  const nextSnapshot = {
    ...DEFAULT_COMPANION_SNAPSHOT,
    configured: true,
    sourceUrl,
  };

  try {
    const response = await fetch(`${sourceUrl}/api/state`, {
      headers: { "Cache-Control": "no-store" },
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(payload.error || `Companion poll failed with HTTP ${response.status}.`);
    }
    nextSnapshot.ready = Boolean(payload.ready ?? true);
    nextSnapshot.reachable = true;
    nextSnapshot.status = String(payload.status || "idle").trim() || "idle";
    nextSnapshot.replyMessage = String(payload.text || "").trim();
    const inEditMode = isCompanionEditStatus(
      nextSnapshot.status,
      nextSnapshot.replyMessage,
    );
    nextSnapshot.editHelperText = inEditMode
      ? isGenericImageSavedMessage(nextSnapshot.replyMessage)
        ? companionSnapshot.editHelperText || "Edited photo ready."
        : nextSnapshot.replyMessage
      : "";
    nextSnapshot.modelTag = String(payload.llm_model || "BOT").trim() || "BOT";
    const modelMeta = getCompanionModelMeta(nextSnapshot.modelTag);
    nextSnapshot.modelLabel = modelMeta?.shortLabel || nextSnapshot.modelTag;
    nextSnapshot.badgeText = String(payload.groq_header_badge_text || "").trim();
    nextSnapshot.requestsToday =
      Number.parseInt(String(payload.groq_requests_today || "0"), 10) || 0;
    nextSnapshot.remainingRequests =
      typeof modelMeta?.rpd === "number"
        ? Math.max(0, modelMeta.rpd - nextSnapshot.requestsToday)
        : null;
    nextSnapshot.balanceText = String(payload.gemini_low_tier_image_balance_text || "").trim();
    nextSnapshot.imageAvailable = Boolean(payload.image);
    nextSnapshot.imageRevision = Number.parseInt(String(payload.image_revision || "0"), 10) || 0;
    nextSnapshot.imageUrl = nextSnapshot.imageAvailable
      ? `${sourceUrl}/image?rev=${nextSnapshot.imageRevision || Date.now()}`
      : "";
    nextSnapshot.lastSuccessAt = Date.now();
    companionSnapshot = nextSnapshot;
    return companionSnapshot;
  } catch (error) {
    companionSnapshot = {
      ...companionSnapshot,
      configured: true,
      sourceUrl,
      reachable: false,
      lastError: error instanceof Error ? error.message : String(error),
    };
    return companionSnapshot;
  }
}

function clearCompanionPollTimer() {
  if (companionPollTimer) {
    clearTimeout(companionPollTimer);
    companionPollTimer = null;
  }
}

function scheduleCompanionPoll() {
  clearCompanionPollTimer();
  if (!normalizeUrl(settings.peerUrl || "")) {
    resetCompanionSnapshot("");
    return;
  }
  companionPollTimer = setTimeout(() => {
    pollCompanionSnapshot()
      .catch((error) => {
        companionSnapshot = {
          ...companionSnapshot,
          lastError: error instanceof Error ? error.message : String(error),
        };
      })
      .finally(() => {
        scheduleCompanionPoll();
      });
  }, 3000);
}

function getPiCameraCommand() {
  if (roomMonitorCameraCommand) {
    return roomMonitorCameraCommand;
  }
  for (const candidate of ["rpicam-still", "libcamera-still"]) {
    const result = spawnSync("which", [candidate], { encoding: "utf8" });
    if (result.status === 0 && result.stdout.trim()) {
      roomMonitorCameraCommand = result.stdout.trim();
      return roomMonitorCameraCommand;
    }
  }
  return "";
}

function execFileAsync(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    execFile(command, args, options, (error, stdout, stderr) => {
      if (error) {
        const detail = String(stderr || stdout || error.message || "").trim();
        reject(new Error(detail || `Command failed: ${path.basename(command)}`));
        return;
      }
      resolve({ stdout: String(stdout || ""), stderr: String(stderr || "") });
    });
  });
}

async function refreshRoomMonitorCameraInfo() {
  const command = getPiCameraCommand();
  if (!command) {
    roomMonitorDetectedCamera = "";
    return [];
  }
  try {
    const { stdout, stderr } = await execFileAsync(command, ["--list-cameras"], {
      timeout: 10000,
      maxBuffer: 1024 * 1024,
    });
    const text = `${stdout}\n${stderr}`;
    const lines = text
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter(
        (line) =>
          line &&
          !/^available cameras/i.test(line) &&
          !/^options:/i.test(line) &&
          !/^-+$/.test(line),
      );
    roomMonitorDetectedCamera =
      lines.find((line) => /^\d+\s*:/.test(line)) ||
      lines[0] ||
      "Raspberry Pi camera detected";
    return lines;
  } catch (error) {
    roomMonitorDetectedCamera = "";
    throw error;
  }
}

function clearRoomMonitorTimer() {
  if (roomMonitorTimer) {
    clearTimeout(roomMonitorTimer);
    roomMonitorTimer = null;
  }
}

function parseRoomMonitorTimeParts(value) {
  const normalized = normalizeTimeOfDay(value);
  if (!normalized) {
    return null;
  }
  const [hours, minutes] = normalized.split(":").map((part) => Number.parseInt(part, 10));
  return { hours, minutes, totalMinutes: hours * 60 + minutes };
}

function isWithinRoomMonitorActiveWindow(now = new Date()) {
  const start = parseRoomMonitorTimeParts(settings.roomMonitorStartTime);
  const stop = parseRoomMonitorTimeParts(settings.roomMonitorStopTime);
  if (!start || !stop) {
    return true;
  }
  if (start.totalMinutes === stop.totalMinutes) {
    return true;
  }
  const currentMinutes = now.getHours() * 60 + now.getMinutes();
  if (start.totalMinutes < stop.totalMinutes) {
    return currentMinutes >= start.totalMinutes && currentMinutes < stop.totalMinutes;
  }
  return currentMinutes >= start.totalMinutes || currentMinutes < stop.totalMinutes;
}

function getNextRoomMonitorWindowStartDelayMs(now = new Date()) {
  const start = parseRoomMonitorTimeParts(settings.roomMonitorStartTime);
  const stop = parseRoomMonitorTimeParts(settings.roomMonitorStopTime);
  if (!start || !stop || start.totalMinutes === stop.totalMinutes) {
    return settings.roomMonitorIntervalSec * 1000;
  }

  const next = new Date(now);
  next.setSeconds(0, 0);
  next.setHours(start.hours, start.minutes, 0, 0);
  if (next <= now && isWithinRoomMonitorActiveWindow(now)) {
    next.setDate(next.getDate() + 1);
  } else if (next <= now && !isWithinRoomMonitorActiveWindow(now)) {
    next.setDate(next.getDate() + 1);
  }
  return Math.max(1000, next.getTime() - now.getTime());
}

function scheduleRoomMonitorCapture() {
  clearRoomMonitorTimer();
  if (settings.roomMonitorIntervalSec <= 0) {
    return;
  }
  const delayMs = isWithinRoomMonitorActiveWindow()
    ? settings.roomMonitorIntervalSec * 1000
    : getNextRoomMonitorWindowStartDelayMs();
  roomMonitorTimer = setTimeout(() => {
    captureRoomMonitorImage().catch((error) => {
      roomMonitorLastError = error instanceof Error ? error.message : String(error);
      scheduleRoomMonitorCapture();
    });
  }, delayMs);
}

function applyRoomMonitorSettings() {
  try {
    enforceRoomMonitorStorageReserve();
  } catch (error) {
    roomMonitorLastError = error instanceof Error ? error.message : String(error);
  }
  scheduleRoomMonitorCapture();
}

function getRoomMonitorFilePath(fileName) {
  const safeFileName = normalizeFileName(fileName);
  if (!safeFileName) {
    return "";
  }
  const filePath = path.resolve(ROOM_MONITOR_DIR, safeFileName);
  if (!filePath.startsWith(`${path.resolve(ROOM_MONITOR_DIR)}${path.sep}`)) {
    return "";
  }
  if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    return "";
  }
  return filePath;
}

function deleteRoomMonitorCaptures(fileNames) {
  const deleted = [];
  const skipped = [];
  for (const fileName of Array.isArray(fileNames) ? fileNames : []) {
    const filePath = getRoomMonitorFilePath(fileName);
    if (!filePath) {
      skipped.push(String(fileName || ""));
      continue;
    }
    fs.unlinkSync(filePath);
    deleted.push(path.basename(filePath));
  }
  return { deleted, skipped };
}

function getRoomMonitorFreeBytes() {
  const result = spawnSync("df", ["-B1", ROOM_MONITOR_DIR], { encoding: "utf8" });
  if (result.status !== 0) {
    throw new Error(result.stderr?.trim() || "Failed to read free disk space.");
  }
  const lines = result.stdout
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean);
  const dataLine = lines[lines.length - 1] || "";
  const columns = dataLine.split(/\s+/);
  const available = Number.parseInt(columns[3] || "", 10);
  if (!Number.isFinite(available)) {
    throw new Error("Failed to parse free disk space.");
  }
  return available;
}

function enforceRoomMonitorStorageReserve() {
  const reserveBytes = settings.roomMonitorFreeReserveGb * 1024 * 1024 * 1024;
  if (reserveBytes <= 0) {
    return [];
  }

  let freeBytes = getRoomMonitorFreeBytes();
  const deleted = [];
  if (freeBytes >= reserveBytes) {
    return deleted;
  }

  const oldestFirst = listRoomMonitorCaptures(5000).slice().sort((a, b) => a.capturedAt - b.capturedAt);
  for (const capture of oldestFirst) {
    const filePath = getRoomMonitorFilePath(capture.fileName);
    if (!filePath) {
      continue;
    }
    fs.unlinkSync(filePath);
    deleted.push(capture.fileName);
    freeBytes += capture.sizeBytes || 0;
    if (freeBytes >= reserveBytes) {
      break;
    }
  }
  return deleted;
}

function listRoomMonitorCaptures(limit = 60) {
  const entries = fs
    .readdirSync(ROOM_MONITOR_DIR, { withFileTypes: true })
    .filter((entry) => entry.isFile() && /\.(?:jpe?g|png)$/i.test(entry.name))
    .map((entry) => {
      const filePath = path.join(ROOM_MONITOR_DIR, entry.name);
      const stats = fs.statSync(filePath);
      return {
        fileName: entry.name,
        capturedAt: stats.mtimeMs,
        sizeBytes: stats.size,
        url: `/api/room-monitor/image?file=${encodeURIComponent(entry.name)}`,
        downloadUrl: `/api/room-monitor/image?file=${encodeURIComponent(entry.name)}&download=1`,
      };
    })
    .sort((a, b) => b.capturedAt - a.capturedAt);
  return entries.slice(0, limit);
}

function getRoomMonitorCaptureArgs(outputPath) {
  return [
    "--nopreview",
    "--timeout",
    "2500",
    "--metering",
    "average",
    "--awb",
    "auto",
    "--output",
    outputPath,
  ];
}

async function applyRoomMonitorAutoBrightness(filePath) {
  const { stdout } = await execFileAsync(
    "python3",
    [ROOM_MONITOR_AUTO_BRIGHTNESS_SCRIPT, filePath],
    {
      timeout: 45000,
      maxBuffer: 1024 * 1024,
    },
  );
  const payload = JSON.parse(String(stdout || "").trim() || "{}");
  roomMonitorLastBrightnessSummary = payload.summary || "";
  return payload;
}

function getRoomMonitorStatus() {
  const captures = listRoomMonitorCaptures(2000);
  const totalSizeBytes = captures.reduce((sum, entry) => sum + entry.sizeBytes, 0);
  let freeSpaceBytes = 0;
  try {
    freeSpaceBytes = getRoomMonitorFreeBytes();
  } catch (error) {
    roomMonitorLastError =
      roomMonitorLastError ||
      (error instanceof Error ? error.message : String(error));
  }
  return {
    enabled: settings.roomMonitorIntervalSec > 0,
    intervalSec: settings.roomMonitorIntervalSec,
    activeNow: isWithinRoomMonitorActiveWindow(),
    startTime: settings.roomMonitorStartTime,
    stopTime: settings.roomMonitorStopTime,
    freeReserveGb: settings.roomMonitorFreeReserveGb,
    captureInProgress: roomMonitorCaptureInProgress,
    lastCaptureAt: roomMonitorLastCaptureAt,
    lastError: roomMonitorLastError,
    detectedCamera: roomMonitorDetectedCamera,
    cameraCommand: path.basename(getPiCameraCommand() || ""),
    autoBrightnessEnabled: settings.roomMonitorAutoBrightness,
    lastBrightnessSummary: roomMonitorLastBrightnessSummary,
    totalCount: captures.length,
    totalSizeBytes,
    freeSpaceBytes,
    captures: captures.slice(0, 40),
  };
}

function getAiImageArchiveStatus(limit = 8) {
  const allPhotos = listAiImportedImages(5000);
  const photos = allPhotos.slice(0, Math.max(1, limit));
  const totalSizeBytes = allPhotos.reduce((sum, photo) => sum + photo.sizeBytes, 0);
  let freeSpaceBytes = 0;
  try {
    freeSpaceBytes = getRoomMonitorFreeBytes();
  } catch (error) {
    aiImageSyncLastError =
      aiImageSyncLastError ||
      (error instanceof Error ? error.message : String(error));
  }
  return {
    enabled: settings.aiImageSyncEnabled,
    intervalSec: settings.aiImageSyncIntervalSec,
    syncInProgress: aiImageSyncInProgress,
    sourceUrl: getAiImageSourceBaseUrl(),
    reserveGb: settings.roomMonitorFreeReserveGb,
    lastSyncAt: aiImageSyncLastSyncAt,
    lastImportedAt: aiImageArchive.lastImportedAt || 0,
    lastError: aiImageSyncLastError,
    totalCount: aiImageArchive.entries.length,
    totalSizeBytes,
    freeSpaceBytes,
    latestFileName: aiImageArchive.latestLocalFileName || "",
    latestImageUrl: photos[0]?.imageUrl || "",
    photos,
  };
}

function clearAiImageSyncTimer() {
  if (aiImageSyncTimer) {
    clearTimeout(aiImageSyncTimer);
    aiImageSyncTimer = null;
  }
}

function buildAiImportLocalFileName(remoteFileName) {
  const extension = path.extname(remoteFileName).toLowerCase() || ".png";
  const stem = path
    .basename(remoteFileName, extension)
    .replace(/[^a-z0-9._-]+/gi, "-")
    .replace(/-+/g, "-")
    .replace(/^-|-$/g, "") || "image";
  return `${Date.now()}-${stem}${extension}`;
}

async function importRemoteAiImage(photo, sourceUrl) {
  const normalizedSourceUrl = normalizeUrl(sourceUrl || getAiImageSourceBaseUrl());
  if (!normalizedSourceUrl) {
    throw new Error("No Whisplay source URL is configured for AI image import.");
  }
  const remoteFileName = normalizeFileName(photo?.fileName || "");
  if (!remoteFileName) {
    throw new Error("Missing remote AI image file name.");
  }
  const archiveKey = buildAiImageArchiveKey(normalizedSourceUrl, remoteFileName);
  if (
    aiImageArchive.entries.some(
      (entry) => buildAiImageArchiveKey(entry.sourceUrl, entry.remoteFileName) === archiveKey,
    )
  ) {
    return { imported: false, skipped: remoteFileName, reason: "already-imported" };
  }

  enforceRoomMonitorStorageReserve();
  const response = await fetch(`${normalizedSourceUrl}/api/generated-images/image/${encodeURIComponent(remoteFileName)}`, {
    headers: { "Cache-Control": "no-store" },
  });
  if (!response.ok) {
    throw new Error(`Failed to download AI image ${remoteFileName} (HTTP ${response.status}).`);
  }
  const buffer = Buffer.from(await response.arrayBuffer());
  const reserveBytes = settings.roomMonitorFreeReserveGb * 1024 * 1024 * 1024;
  const freeBytes = getRoomMonitorFreeBytes();
  if (freeBytes - buffer.length < reserveBytes) {
    throw new Error(
      `Skipping ${remoteFileName}: not enough free space to keep the ${settings.roomMonitorFreeReserveGb} GB reserve without deleting imported AI images.`,
    );
  }

  let localFileName = buildAiImportLocalFileName(remoteFileName);
  let filePath = path.join(AI_IMAGE_IMPORT_DIR, localFileName);
  while (fs.existsSync(filePath)) {
    localFileName = buildAiImportLocalFileName(remoteFileName);
    filePath = path.join(AI_IMAGE_IMPORT_DIR, localFileName);
  }
  fs.writeFileSync(filePath, buffer);

  const importedAt = Date.now();
  aiImageArchive.entries.unshift({
    sourceUrl: normalizedSourceUrl,
    remoteFileName,
    localFileName,
    importedAt,
    updatedAt: Number.parseInt(String(photo?.updatedAt || "0"), 10) || importedAt,
    sizeBytes: buffer.length,
  });
  aiImageArchive.latestLocalFileName = localFileName;
  aiImageArchive.lastImportedAt = importedAt;
  saveAiImageArchive();
  return { imported: true, fileName: localFileName, remoteFileName };
}

async function syncRemoteAiImages(options = {}) {
  if (aiImageSyncInProgress) {
    return { imported: [], skipped: [], sourceUrl: getAiImageSourceBaseUrl() };
  }
  aiImageSyncInProgress = true;
  try {
    const remote = await fetchRemoteAiImageList();
    const imported = [];
    const skipped = [];
    const orderedPhotos = remote.photos.slice().sort((a, b) => a.updatedAt - b.updatedAt);
    for (const photo of orderedPhotos) {
      if (photo.imported && !options.forceSingleFile) {
        continue;
      }
      if (options.fileName && photo.fileName !== options.fileName) {
        continue;
      }
      const result = await importRemoteAiImage(photo, remote.sourceUrl);
      if (result.imported) {
        imported.push(result.remoteFileName || photo.fileName);
      } else {
        skipped.push(result.skipped || photo.fileName);
      }
    }
    aiImageSyncLastError = "";
    aiImageSyncLastSyncAt = Date.now();
    saveAiImageArchive();
    return { imported, skipped, sourceUrl: remote.sourceUrl };
  } catch (error) {
    aiImageSyncLastError = error instanceof Error ? error.message : String(error);
    throw error;
  } finally {
    aiImageSyncInProgress = false;
    scheduleAiImageSync();
  }
}

function scheduleAiImageSync() {
  clearAiImageSyncTimer();
  if (!settings.aiImageSyncEnabled || settings.aiImageSyncIntervalSec <= 0) {
    return;
  }
  const delayMs = aiImageSyncLastSyncAt ? settings.aiImageSyncIntervalSec * 1000 : 1500;
  aiImageSyncTimer = setTimeout(() => {
    syncRemoteAiImages().catch((error) => {
      aiImageSyncLastError = error instanceof Error ? error.message : String(error);
    });
  }, delayMs);
}

function applyAiImageSyncSettings() {
  aiImageSyncLastError = "";
  scheduleAiImageSync();
}

function applyCompanionSettings() {
  pollCompanionSnapshot()
    .catch((error) => {
      companionSnapshot = {
        ...companionSnapshot,
        lastError: error instanceof Error ? error.message : String(error),
      };
    })
    .finally(() => {
      scheduleCompanionPoll();
    });
}

async function captureRoomMonitorImage(force = false) {
  if (roomMonitorCaptureInProgress) {
    return false;
  }
  if (!force && !isWithinRoomMonitorActiveWindow()) {
    scheduleRoomMonitorCapture();
    return false;
  }

  const command = getPiCameraCommand();
  if (!command) {
    roomMonitorLastError =
      "No Raspberry Pi camera capture command found. Install rpicam-still or libcamera-still.";
    return false;
  }

  roomMonitorCaptureInProgress = true;
  const outputPath = path.join(ROOM_MONITOR_DIR, `room-monitor-${Date.now()}.jpg`);
  let captureCompleted = false;

  try {
    enforceRoomMonitorStorageReserve();
    await refreshRoomMonitorCameraInfo();
    await execFileAsync(
      command,
      getRoomMonitorCaptureArgs(outputPath),
      {
        timeout: 20000,
        maxBuffer: 1024 * 1024,
      },
    );
    roomMonitorLastCaptureAt = Date.now();
    captureCompleted = true;
    if (settings.roomMonitorAutoBrightness) {
      await applyRoomMonitorAutoBrightness(outputPath);
    } else {
      roomMonitorLastBrightnessSummary = "";
    }
    roomMonitorLastError = "";
    enforceRoomMonitorStorageReserve();
    return true;
  } catch (error) {
    roomMonitorLastError = error instanceof Error ? error.message : String(error);
    if (!captureCompleted && fs.existsSync(outputPath)) {
      fs.unlinkSync(outputPath);
    }
    throw error;
  } finally {
    roomMonitorCaptureInProgress = false;
    scheduleRoomMonitorCapture();
  }
}

const hubTransport = new BotNetHubTransport({
  getSettings: () => settings,
  getSession: () => hubSession,
  setSession: (nextSession) => {
    hubSession = sanitizeSession(nextSession);
    saveHubSession();
  },
  onPeerStart: async (payload) => handlePeerStart(payload),
  onPeerMessage: async (payload) => handlePeerMessage(payload),
});

function nowIso() {
  return new Date().toISOString();
}

function makeMessage({ speakerType, speakerName, text, kind = "message" }) {
  return {
    id: crypto.randomUUID(),
    createdAt: nowIso(),
    speakerType,
    speakerName,
    kind,
    text: String(text || "").trim(),
  };
}

function makeConversation({
  topic,
  peerUrl,
  starter,
  mode = "botnet",
  botnetMode = settings.botnetMode,
  transportMode = settings.transportMode,
  linkId = "",
  peerNodeId = "",
}) {
  const createdAt = nowIso();
  return {
    id: crypto.randomUUID(),
    topic: String(topic || "").trim(),
    createdAt,
    updatedAt: createdAt,
    status: "active",
    starter,
    mode,
    botnetMode: mode === "botnet" ? normalizeBotnetMode(botnetMode) : undefined,
    transportMode: mode === "botnet" ? normalizeTransportMode(transportMode) : undefined,
    peerUrl: normalizeUrl(peerUrl || settings.peerUrl),
    linkId: String(linkId || "").trim(),
    peerNodeId: String(peerNodeId || "").trim(),
    maxBotReplies: settings.maxBotReplies,
    replyCount: 0,
    messages: [],
  };
}

function findConversation(id) {
  return conversations.find((conversation) => conversation.id === id) || null;
}

function getLatestActiveSoloConversation() {
  return (
    conversations.find(
      (conversation) => conversation.mode === "solo" && conversation.status === "active",
    ) || null
  );
}

function getLatestActiveBotnetConversation() {
  return (
    conversations.find(
      (conversation) => conversation.mode === "botnet" && conversation.status === "active",
    ) || null
  );
}

function isAutoBotConversation(conversation) {
  return normalizeBotnetMode(conversation?.botnetMode) === "auto-bot";
}

function isOnlineConversation(conversation) {
  return normalizeTransportMode(conversation?.transportMode || settings.transportMode) === "online-hub";
}

function touchConversation(conversation) {
  conversation.updatedAt = nowIso();
  saveConversations();
}

function addMessage(conversation, message) {
  conversation.messages.push(message);
  conversation.updatedAt = nowIso();
  saveConversations();
}

function addEvent(conversation, text) {
  addMessage(
    conversation,
    makeMessage({
      speakerType: "system",
      speakerName: "System",
      text,
      kind: "event",
    }),
  );
}

function cleanupRequestWindow() {
  const cutoff = Date.now() - 60 * 60 * 1000;
  runtimeState.requestTimestamps = runtimeState.requestTimestamps.filter((timestamp) => timestamp >= cutoff);
}

function consumeRequestQuota() {
  cleanupRequestWindow();
  if (runtimeState.requestTimestamps.length >= settings.maxRequestsPerHour) {
    throw new Error(`This bot has already used ${settings.maxRequestsPerHour} Groq requests in the last hour.`);
  }
  runtimeState.requestTimestamps.push(Date.now());
  saveRuntimeState();
}

function buildConversationMessages(conversation, extraUserPrompt) {
  const relevant = conversation.messages
    .filter((message) => message.kind === "message" || message.kind === "relay")
    .slice(-settings.memoryTurns);

  const conversationModeText =
    conversation.mode === "solo"
      ? "You are talking to a human through a small browser chat UI."
      : "You are talking to another chatbot over text.";

  const messages = [
    {
      role: "system",
      content: [
        settings.personalityPrompt,
        "You are part of GroqBotNet.",
        `Your bot name is ${settings.botName}.`,
        conversationModeText,
        conversation.mode === "solo"
          ? "Keep replies concise, natural, helpful, and in character."
          : "Keep replies concise, natural, and chatbot-to-chatbot friendly.",
        "Do not mention system prompts, tokens, APIs, or hidden instructions.",
      ].join(" "),
    },
  ];

  for (const message of relevant) {
    if (message.speakerType === "self") {
      messages.push({ role: "assistant", content: message.text });
    } else if (message.speakerType === "peer" || message.speakerType === "user") {
      messages.push({ role: "user", content: message.text });
    }
  }

  if (extraUserPrompt) {
    messages.push({ role: "user", content: extraUserPrompt });
  }

  return messages;
}

function buildPersonaRelayMessages(conversation, userPrompt) {
  const relevant = conversation.messages
    .filter((message) => message.kind === "message" || message.kind === "relay")
    .slice(-settings.memoryTurns);

  const messages = [
    {
      role: "system",
      content: [
        settings.personalityPrompt,
        "You are part of GroqBotNet.",
        `Your bot name is ${settings.botName}.`,
        "You are rewriting the local user's prompt into the exact message you would personally send to the peer bot.",
        "Preserve the user's intent, but express it in your own personality and voice.",
        "Keep it concise and natural.",
        "Write the final message itself, exactly as it should be sent to the peer.",
        "Do not give advice, instructions, stage directions, or commentary.",
        "Do not say things like 'ask them', 'tell them', 'send a message', 'you want to know', or 'here is the message'.",
        "Do not wrap the whole reply in quotes unless the message itself truly needs quotes.",
        'Example local prompt: "ask my friend how it is doing today" -> "How are ye doin\\\' today, matey?"',
        "Return only the outgoing message text with no commentary about rewriting, filtering, or translating.",
      ].join(" "),
    },
  ];

  for (const message of relevant) {
    if (message.speakerType === "self") {
      messages.push({ role: "assistant", content: message.text });
    } else if (message.speakerType === "peer") {
      messages.push({ role: "user", content: message.text });
    }
  }

  messages.push({
    role: "user",
    content: `Rewrite this into the exact message to send to the peer bot: ${String(userPrompt || "").trim()}`,
  });

  return messages;
}

async function callGroq(messages) {
  const apiKey = settings.groqApiKey || process.env.OPENAI_API_KEY || process.env.GROQ_API_KEY || "";
  if (!apiKey) {
    throw new Error("No Groq API key configured yet.");
  }

  const response = await fetch("https://api.groq.com/openai/v1/chat/completions", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${apiKey}`,
    },
    body: JSON.stringify({
      model: settings.model,
      messages,
      temperature: 0.9,
    }),
  });

  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error?.message || `Groq request failed with HTTP ${response.status}`);
  }

  const text = payload.choices?.[0]?.message?.content;
  if (!text || typeof text !== "string") {
    throw new Error("Groq returned an empty reply.");
  }
  return text.trim();
}

async function sendToPeer(conversation, payload) {
  if (isOnlineConversation(conversation)) {
    await hubTransport.sendEvent(
      payload.type === "start" ? "botnet.peer-start" : "botnet.peer-message",
      conversation,
      payload,
    );
    return;
  }
  const peerUrl = normalizeUrl(conversation.peerUrl || settings.peerUrl);
  if (!peerUrl) {
    throw new Error("Peer URL is not configured.");
  }

  const response = await fetch(`${peerUrl}${payload.type === "start" ? "/api/botnet/start" : "/api/botnet/message"}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const body = await response.json().catch(() => ({}));
  if (!response.ok || body.ok === false) {
    throw new Error(body.error || `Peer request failed with HTTP ${response.status}`);
  }
}

function clearConversationTimer(conversationId) {
  const timer = timers.get(conversationId);
  if (timer) {
    clearTimeout(timer);
    timers.delete(conversationId);
  }
}

function markConversationComplete(conversation, reason) {
  conversation.status = "complete";
  if (reason) {
    addEvent(conversation, reason);
  } else {
    touchConversation(conversation);
  }
  clearConversationTimer(conversation.id);
}

async function generateReply(conversation, extraPrompt) {
  consumeRequestQuota();
  const reply = await callGroq(buildConversationMessages(conversation, extraPrompt));
  return reply;
}

async function generatePersonaRelay(conversation, userPrompt) {
  consumeRequestQuota();
  return callGroq(buildPersonaRelayMessages(conversation, userPrompt));
}

async function emitBotReply(
  conversation,
  extraPrompt,
  options = { deliverToPeer: true, enforceReplyLimit: true },
) {
  if (conversation.status !== "active") {
    return;
  }
  if (
    options.enforceReplyLimit &&
    conversation.maxBotReplies > 0 &&
    conversation.replyCount >= conversation.maxBotReplies
  ) {
    markConversationComplete(conversation, "Conversation stopped because the max bot replies limit was reached.");
    return;
  }

  const reply = await generateReply(conversation, extraPrompt);
  conversation.replyCount += 1;
  addMessage(
    conversation,
    makeMessage({
      speakerType: "self",
      speakerName: settings.botName,
      text: reply,
    }),
  );

  if (options.deliverToPeer) {
    const payload = {
      type: "message",
      conversationId: conversation.id,
      topic: conversation.topic,
      senderBotName: settings.botName,
      senderUrl: normalizeUrl(settings.publicBaseUrl),
      botnetMode: normalizeBotnetMode(conversation.botnetMode),
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
      message: reply,
    };

    try {
      await sendToPeer(conversation, payload);
    } catch (error) {
      addEvent(conversation, `Failed to deliver message to peer: ${error.message}`);
    }
  }

  if (
    options.enforceReplyLimit &&
    conversation.maxBotReplies > 0 &&
    conversation.replyCount >= conversation.maxBotReplies
  ) {
    markConversationComplete(conversation, "Conversation reached the configured max bot replies.");
  }
}

function scheduleReply(conversation, extraPrompt) {
  clearConversationTimer(conversation.id);
  const delayMs = settings.replyDelaySec * 1000;
  const timer = setTimeout(async () => {
    timers.delete(conversation.id);
    try {
      await emitBotReply(conversation, extraPrompt);
    } catch (error) {
      addEvent(conversation, `Reply failed: ${error.message}`);
      markConversationComplete(conversation);
    }
  }, delayMs);
  timers.set(conversation.id, timer);
}

async function startLocalConversation(topic) {
  const conversation = makeConversation({
    topic,
    peerUrl: settings.peerUrl,
    starter: "self",
    mode: "botnet",
    botnetMode: "auto-bot",
    transportMode: settings.transportMode,
    linkId: hubSession.linkId,
    peerNodeId: hubSession.peerNodeId,
  });
  conversations.unshift(conversation);
  saveConversations();

  try {
    await emitBotReply(
      conversation,
      `Start a fresh chatbot-to-chatbot conversation about this topic: "${topic}". Send only the first message.`,
    );
  } catch (error) {
    addEvent(conversation, `Could not start conversation: ${error.message}`);
  }

  return conversation;
}

async function requestPeerStart(topic) {
  const conversation = makeConversation({
    topic,
    peerUrl: settings.peerUrl,
    starter: "peer",
    mode: "botnet",
    botnetMode: "auto-bot",
    transportMode: settings.transportMode,
    linkId: hubSession.linkId,
    peerNodeId: hubSession.peerNodeId,
  });
  conversations.unshift(conversation);
  saveConversations();

  try {
    await sendToPeer(conversation, {
      type: "start",
      conversationId: conversation.id,
      topic,
      senderBotName: settings.botName,
      senderUrl: normalizeUrl(settings.publicBaseUrl),
      botnetMode: "auto-bot",
      maxBotReplies: conversation.maxBotReplies,
      replyCount: conversation.replyCount,
    });
    addEvent(conversation, "Asked the peer bot to open the conversation.");
  } catch (error) {
    addEvent(conversation, `Could not reach peer to start conversation: ${error.message}`);
  }

  return conversation;
}

async function relayUserPromptToPeer(conversation, userPrompt) {
  addMessage(
    conversation,
    makeMessage({
      speakerType: "user",
      speakerName: "You",
      text: userPrompt,
      kind: "user-prompt",
    }),
  );

  const relayedMessage = await generatePersonaRelay(conversation, userPrompt);
  addMessage(
    conversation,
    makeMessage({
      speakerType: "self",
      speakerName: `${settings.botName} sent`,
      text: relayedMessage,
      kind: "relay",
    }),
  );

  await sendToPeer(conversation, {
    type: "message",
    conversationId: conversation.id,
    topic: conversation.topic,
    senderBotName: settings.botName,
    senderUrl: normalizeUrl(settings.publicBaseUrl),
    botnetMode: "persona-relay",
    maxBotReplies: conversation.maxBotReplies,
    replyCount: conversation.replyCount,
    message: relayedMessage,
  });
}

async function handlePersonaRelayPrompt(body) {
  const prompt = String(body.topic || body.message || "").trim();
  if (!prompt) {
    throw new Error("Prompt is required.");
  }

  const requestedId = String(body.conversationId || "").trim();
  let conversation =
    (requestedId && findConversation(requestedId)) ||
    (!body.newConversation && getLatestActiveBotnetConversation()) ||
    null;

  if (
    !conversation ||
    conversation.mode !== "botnet" ||
    conversation.status !== "active" ||
    (!isOnlineConversation(conversation) && !conversation.peerUrl) ||
    isAutoBotConversation(conversation)
  ) {
    const topic = prompt.length > 72 ? `${prompt.slice(0, 72)}...` : prompt;
    conversation = makeConversation({
      topic: topic || "Persona relay",
      peerUrl: settings.peerUrl,
      starter: "self",
      mode: "botnet",
      botnetMode: "persona-relay",
      transportMode: settings.transportMode,
      linkId: hubSession.linkId,
      peerNodeId: hubSession.peerNodeId,
    });
    conversations.unshift(conversation);
    saveConversations();
  }

  if (isOnlineConversation(conversation)) {
    if (!hubTransport.getPublicState().connected) {
      throw new Error("Connect to the hub before sending an online message.");
    }
    if (!String(conversation.linkId || hubSession.linkId || "").trim()) {
      throw new Error("No online peer link is active yet.");
    }
  } else if (!normalizeUrl(conversation.peerUrl || settings.peerUrl)) {
    throw new Error("Peer URL is not configured.");
  }

  await relayUserPromptToPeer(conversation, prompt);
  return conversation;
}

async function handlePeerStart(body) {
  const conversationId = String(body.conversationId || "").trim() || crypto.randomUUID();
  const transportMode = normalizeTransportMode(
    body.transportMode || (body.senderNodeId ? "online-hub" : settings.transportMode),
  );
  let conversation = findConversation(conversationId);
  if (!conversation) {
    conversation = {
      id: conversationId,
      topic: String(body.topic || "").trim(),
      createdAt: nowIso(),
      updatedAt: nowIso(),
      status: "active",
      starter: "peer",
      mode: "botnet",
      botnetMode: normalizeBotnetMode(body.botnetMode || "auto-bot"),
      transportMode,
      peerUrl: normalizeUrl(body.senderUrl || settings.peerUrl),
      linkId: String(body.linkId || hubSession.linkId || "").trim(),
      peerNodeId: String(body.senderNodeId || body.peerNodeId || hubSession.peerNodeId || "").trim(),
      maxBotReplies: normalizePositiveInt(body.maxBotReplies, settings.maxBotReplies, 0, 200),
      replyCount: normalizePositiveInt(body.replyCount, 0, 0, 200),
      messages: [],
    };
    conversations.unshift(conversation);
    saveConversations();
  }

  conversation.botnetMode = normalizeBotnetMode(body.botnetMode || conversation.botnetMode);
  conversation.transportMode = transportMode;
  conversation.linkId = String(body.linkId || conversation.linkId || hubSession.linkId || "").trim();
  conversation.peerNodeId = String(
    body.senderNodeId || body.peerNodeId || conversation.peerNodeId || hubSession.peerNodeId || "",
  ).trim();
  if (transportMode === "online-hub") {
    hubSession = sanitizeSession({
      ...hubSession,
      linkId: conversation.linkId,
      peerNodeId: conversation.peerNodeId,
      peerHandle: String(body.senderBotName || hubSession.peerHandle || "").trim(),
      peerOnline: true,
      lastError: "",
    });
    saveHubSession();
  }
  addEvent(conversation, `Peer ${body.senderBotName || "bot"} requested a new conversation.`);
  if (!isAutoBotConversation(conversation)) {
    return;
  }
  scheduleReply(
    conversation,
    `Start a fresh chatbot-to-chatbot conversation about this topic: "${conversation.topic}". Send only the first message.`,
  );
}

async function handlePeerMessage(body) {
  const conversationId = String(body.conversationId || "").trim();
  if (!conversationId) {
    throw new Error("Missing conversationId.");
  }
  const transportMode = normalizeTransportMode(
    body.transportMode || (body.senderNodeId ? "online-hub" : settings.transportMode),
  );

  let conversation = findConversation(conversationId);
  if (!conversation) {
    conversation = {
      id: conversationId,
      topic: String(body.topic || "").trim(),
      createdAt: nowIso(),
      updatedAt: nowIso(),
      status: "active",
      starter: "peer",
      mode: "botnet",
      botnetMode: normalizeBotnetMode(body.botnetMode || settings.botnetMode),
      transportMode,
      peerUrl: normalizeUrl(body.senderUrl || settings.peerUrl),
      linkId: String(body.linkId || hubSession.linkId || "").trim(),
      peerNodeId: String(body.senderNodeId || body.peerNodeId || hubSession.peerNodeId || "").trim(),
      maxBotReplies: normalizePositiveInt(body.maxBotReplies, settings.maxBotReplies, 0, 200),
      replyCount: 0,
      messages: [],
    };
    conversations.unshift(conversation);
  }

  conversation.botnetMode = normalizeBotnetMode(body.botnetMode || conversation.botnetMode);
  conversation.transportMode = transportMode;
  conversation.peerUrl = normalizeUrl(body.senderUrl || conversation.peerUrl || settings.peerUrl);
  conversation.linkId = String(body.linkId || conversation.linkId || hubSession.linkId || "").trim();
  conversation.peerNodeId = String(
    body.senderNodeId || body.peerNodeId || conversation.peerNodeId || hubSession.peerNodeId || "",
  ).trim();
  conversation.maxBotReplies = normalizePositiveInt(
    body.maxBotReplies,
    conversation.maxBotReplies || settings.maxBotReplies,
    0,
    200,
  );
  conversation.replyCount = Math.max(
    normalizePositiveInt(body.replyCount, 0, 0, 200),
    conversation.replyCount || 0,
  );
  if (transportMode === "online-hub") {
    hubSession = sanitizeSession({
      ...hubSession,
      linkId: conversation.linkId,
      peerNodeId: conversation.peerNodeId,
      peerHandle: String(body.senderBotName || hubSession.peerHandle || "").trim(),
      peerOnline: true,
      lastError: "",
    });
    saveHubSession();
  }

  addMessage(
    conversation,
    makeMessage({
      speakerType: "peer",
      speakerName: String(body.senderBotName || "Peer Bot").trim() || "Peer Bot",
      text: body.message,
    }),
  );

  if (conversation.maxBotReplies > 0 && conversation.replyCount >= conversation.maxBotReplies) {
    markConversationComplete(conversation, "Received the final peer message and closed the conversation.");
    return;
  }

  if (!isAutoBotConversation(conversation)) {
    if (conversation.starter === "peer") {
      scheduleReply(conversation);
      return;
    }
    touchConversation(conversation);
    return;
  }
  scheduleReply(conversation);
}

async function startSoloConversation(initialMessage) {
  const topic = initialMessage.length > 72 ? `${initialMessage.slice(0, 72)}...` : initialMessage;
  const conversation = makeConversation({
    topic: topic || "Solo chat",
    peerUrl: "",
    starter: "user",
    mode: "solo",
  });
  conversations.unshift(conversation);
  addMessage(
    conversation,
    makeMessage({
      speakerType: "user",
      speakerName: "You",
      text: initialMessage,
    }),
  );

  try {
    await emitBotReply(conversation, "", {
      deliverToPeer: false,
      enforceReplyLimit: false,
    });
  } catch (error) {
    addEvent(conversation, `Solo reply failed: ${error.message}`);
  }

  return conversation;
}

async function handleSoloMessage(body) {
  const message = String(body.message || "").trim();
  if (!message) {
    throw new Error("Message is required.");
  }

  const requestedId = String(body.conversationId || "").trim();
  let conversation =
    (requestedId && findConversation(requestedId)) ||
    (!body.newConversation && getLatestActiveSoloConversation()) ||
    null;

  if (!conversation || conversation.mode !== "solo" || conversation.status !== "active") {
    return startSoloConversation(message);
  }

  addMessage(
    conversation,
    makeMessage({
      speakerType: "user",
      speakerName: "You",
      text: message,
    }),
  );

  try {
    await emitBotReply(conversation, "", {
      deliverToPeer: false,
      enforceReplyLimit: false,
    });
  } catch (error) {
    addEvent(conversation, `Solo reply failed: ${error.message}`);
  }

  return conversation;
}

function stopConversation(conversationId) {
  const conversation = findConversation(conversationId);
  if (!conversation) {
    return false;
  }
  conversation.status = "stopped";
  addEvent(conversation, "Conversation stopped by user.");
  clearConversationTimer(conversationId);
  return true;
}

function clearAllConversations() {
  conversations = [];
  for (const timer of timers.values()) {
    clearTimeout(timer);
  }
  timers.clear();
  saveConversations();
}

function sendJson(res, statusCode, payload) {
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(JSON.stringify(payload));
}

async function readBody(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  if (chunks.length === 0) {
    return {};
  }
  const raw = Buffer.concat(chunks).toString("utf8");
  return raw ? JSON.parse(raw) : {};
}

function contentTypeFor(filePath) {
  if (filePath.endsWith(".html")) return "text/html; charset=utf-8";
  if (filePath.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (filePath.endsWith(".css")) return "text/css; charset=utf-8";
  if (filePath.endsWith(".jpg") || filePath.endsWith(".jpeg")) return "image/jpeg";
  if (filePath.endsWith(".png")) return "image/png";
  return "text/plain; charset=utf-8";
}

function serveStatic(req, res) {
  const requestPath =
    req.url === "/"
      ? "/index.html"
      : req.url === "/hdmi"
        ? "/hdmi.html"
        : req.url === "/room-monitor"
          ? "/room-monitor.html"
          : req.url;
  const filePath = path.normalize(path.join(PUBLIC_DIR, requestPath));
  if (!filePath.startsWith(PUBLIC_DIR) || !fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    res.writeHead(404);
    res.end("Not found");
    return;
  }
  res.writeHead(200, { "Content-Type": contentTypeFor(filePath) });
  fs.createReadStream(filePath).pipe(res);
}

function getStatePayload() {
  return {
    settings: getPublicSettings(),
    online: hubTransport.getPublicState(),
    roomMonitor: getRoomMonitorStatus(),
    aiImageArchive: getAiImageArchiveStatus(40),
    companionSnapshot: getCompanionSnapshotStatus(),
    conversations,
    stats: {
      requestsUsedThisHour: runtimeState.requestTimestamps.filter(
        (timestamp) => timestamp >= Date.now() - 60 * 60 * 1000,
      ).length,
    },
  };
}

const server = http.createServer(async (req, res) => {
  try {
    if (!req.url) {
      sendJson(res, 400, { ok: false, error: "Missing URL." });
      return;
    }

    const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);

    if (req.method === "GET" && url.pathname === "/api/state") {
      cleanupRequestWindow();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/settings") {
      const body = await readBody(req);
      settings = sanitizeSettingsUpdate(body, settings);
      if (settings.transportMode !== "online-hub") {
        hubTransport.disconnect();
      }
      saveSettings();
      applyRoomMonitorSettings();
      applyAiImageSyncSettings();
      applyCompanionSettings();
      sendJson(res, 200, { ok: true, settings: getPublicSettings() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/room-monitor/capture") {
      await captureRoomMonitorImage(true);
      sendJson(res, 200, { ok: true, roomMonitor: getRoomMonitorStatus() });
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/room-monitor/images") {
      const limitValue = Number.parseInt(url.searchParams.get("limit") || "", 10);
      const limit =
        Number.isFinite(limitValue) && limitValue > 0
          ? Math.min(limitValue, 5000)
          : 40;
      const roomMonitor = getRoomMonitorStatus();
      roomMonitor.captures = listRoomMonitorCaptures(limit);
      sendJson(res, 200, { ok: true, roomMonitor });
      return;
    }

    if (req.method === "DELETE" && url.pathname === "/api/room-monitor/images") {
      const body = await readBody(req);
      const fileNames = Array.isArray(body.fileNames)
        ? body.fileNames.filter((entry) => typeof entry === "string")
        : [];
      const result = deleteRoomMonitorCaptures(fileNames);
      sendJson(res, 200, {
        ok: true,
        deleted: result.deleted,
        skipped: result.skipped,
        roomMonitor: getRoomMonitorStatus(),
      });
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/room-monitor/image") {
      const fileName = url.searchParams.get("file") || "";
      const filePath = getRoomMonitorFilePath(fileName);
      if (!filePath) {
        sendJson(res, 404, { ok: false, error: "Image not found." });
        return;
      }
      const download = url.searchParams.get("download") === "1";
      res.writeHead(200, {
        "Content-Type": contentTypeFor(filePath),
        "Cache-Control": "no-store",
        ...(download
          ? {
              "Content-Disposition": `attachment; filename="${path.basename(filePath)}"`,
            }
          : {}),
      });
      fs.createReadStream(filePath).pipe(res);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/ai-images") {
      const limitValue = Number.parseInt(url.searchParams.get("limit") || "", 10);
      const limit =
        Number.isFinite(limitValue) && limitValue > 0
          ? Math.min(limitValue, 200)
          : 40;
      sendJson(res, 200, { ok: true, aiImageArchive: getAiImageArchiveStatus(limit) });
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/ai-images/image") {
      const fileName = url.searchParams.get("file") || "";
      const filePath = getAiImportFilePath(fileName);
      if (!filePath) {
        sendJson(res, 404, { ok: false, error: "AI image not found." });
        return;
      }
      res.writeHead(200, {
        "Content-Type": contentTypeFor(filePath),
        "Cache-Control": "no-store",
      });
      fs.createReadStream(filePath).pipe(res);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/remote-ai-images") {
      const remote = await fetchRemoteAiImageList();
      sendJson(res, 200, { ok: true, sourceUrl: remote.sourceUrl, photos: remote.photos.slice(0, 40) });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/remote-ai-images/import") {
      const body = await readBody(req);
      const fileName = normalizeFileName(body.fileName || "");
      if (!fileName) {
        sendJson(res, 400, { ok: false, error: "fileName is required." });
        return;
      }
      const result = await syncRemoteAiImages({ fileName, forceSingleFile: true });
      sendJson(res, 200, {
        ok: true,
        imported: result.imported,
        skipped: result.skipped,
        aiImageArchive: getAiImageArchiveStatus(40),
      });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/remote-ai-images/import-all-new") {
      const result = await syncRemoteAiImages();
      sendJson(res, 200, {
        ok: true,
        imported: result.imported,
        skipped: result.skipped,
        aiImageArchive: getAiImageArchiveStatus(40),
      });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/conversations/start") {
      const body = await readBody(req);
      const topic = String(body.topic || "").trim();
      if (!topic) {
        sendJson(res, 400, { ok: false, error: "Topic is required." });
        return;
      }
      const requestedMode = normalizeBotnetMode(body.botnetMode || settings.botnetMode);
      const starter = body.starter === "peer" ? "peer" : "self";
      const conversation =
        requestedMode === "persona-relay"
          ? await handlePersonaRelayPrompt(body)
          : starter === "peer"
            ? await requestPeerStart(topic)
            : await startLocalConversation(topic);
      sendJson(res, 200, { ok: true, conversation, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/solo/send") {
      const body = await readBody(req);
      const conversation = await handleSoloMessage(body);
      sendJson(res, 200, { ok: true, conversation, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/conversations/stop") {
      const body = await readBody(req);
      const id = String(body.conversationId || "").trim();
      if (!id || !stopConversation(id)) {
        sendJson(res, 404, { ok: false, error: "Conversation not found." });
        return;
      }
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/conversations/clear") {
      clearAllConversations();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/start") {
      const body = await readBody(req);
      await handlePeerStart(body);
      sendJson(res, 200, { ok: true });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/message") {
      const body = await readBody(req);
      await handlePeerMessage(body);
      sendJson(res, 200, { ok: true });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/test") {
      const result = await hubTransport.test();
      sendJson(res, 200, { ok: true, result, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/register") {
      await hubTransport.register();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/connect") {
      await hubTransport.connect();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/invite") {
      const invite = await hubTransport.createInvite();
      sendJson(res, 200, { ok: true, invite, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/redeem") {
      const body = await readBody(req);
      await hubTransport.redeemInvite(body.inviteCode);
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/botnet/disconnect") {
      hubTransport.disconnect();
      sendJson(res, 200, { ok: true, ...getStatePayload() });
      return;
    }

    if (req.method === "GET" && !url.pathname.startsWith("/api/")) {
      serveStatic(req, res);
      return;
    }

    sendJson(res, 404, { ok: false, error: "Not found." });
  } catch (error) {
    sendJson(res, 500, { ok: false, error: error instanceof Error ? error.message : "Unknown error." });
  }
});

applyRoomMonitorSettings();
applyAiImageSyncSettings();
applyCompanionSettings();
refreshRoomMonitorCameraInfo().catch((error) => {
  roomMonitorLastError = error instanceof Error ? error.message : String(error);
});

server.listen(PORT, HOST, () => {
  console.log(`[GroqBotNet] Listening on http://${HOST}:${PORT}`);
  if (settings.enabled && settings.transportMode === "online-hub" && hubSession.nodeId && hubSession.authToken) {
    hubTransport.connect().catch((error) => {
      hubSession = sanitizeSession({
        ...hubSession,
        lastError: error instanceof Error ? error.message : "Failed to reconnect to the hub.",
      });
      saveHubSession();
    });
  }
});
