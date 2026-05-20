import fs from "fs";
import path from "path";
import { captureCameraImage } from "./camera-daemon";
import { getRuntimeSettings, type RuntimeSettings } from "../config/runtime-settings";
import { roomMonitorDir, roomMonitorSavedDir } from "../utils/dir";

const ROOM_MONITOR_FREE_SPACE_RESERVE_BYTES = 8 * 1024 * 1024 * 1024;
const ROOM_MONITOR_DAY_KEY_PATTERN = /^\d{4}-\d{2}-\d{2}$/;

let captureTimer: NodeJS.Timeout | null = null;
let captureInProgress = false;
let captureIntervalSec = 0;
let lastCaptureAt: number | null = null;
let lastError = "";

export interface RoomMonitorCapture {
  fileName: string;
  imagePath: string;
  updatedAt: number;
  sizeBytes: number;
}

export interface RoomMonitorCaptureDay {
  dayKey: string;
  label: string;
  count: number;
  updatedAt: number;
  totalSizeBytes: number;
  coverFileName: string;
}

function readCapturesFromDirOldestFirst(dirPath: string): RoomMonitorCapture[] {
  if (!fs.existsSync(dirPath)) {
    return [];
  }

  return fs.readdirSync(dirPath)
    .filter((file) => /\.(jpg|jpeg|png|webp|gif)$/i.test(file))
    .map((fileName) => {
      const imagePath = path.join(dirPath, fileName);
      const stats = fs.statSync(imagePath);
      return {
        fileName,
        imagePath,
        updatedAt: stats.mtimeMs,
        sizeBytes: stats.size,
      };
    })
    .sort((a, b) => a.updatedAt - b.updatedAt);
}

function formatLocalDayKey(timestamp: number): string {
  const date = new Date(timestamp);
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, "0");
  const day = String(date.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

function formatDayLabel(dayKey: string): string {
  const [year, month, day] = dayKey.split("-").map((value) => parseInt(value, 10));
  const date = new Date(year, month - 1, day);
  if (Number.isNaN(date.getTime())) {
    return dayKey;
  }
  return date.toLocaleDateString(undefined, {
    weekday: "short",
    month: "short",
    day: "numeric",
    year: "numeric",
  });
}

function groupCapturesByDay(captures: RoomMonitorCapture[]): RoomMonitorCaptureDay[] {
  const dayMap = new Map<string, RoomMonitorCaptureDay>();
  for (const capture of captures) {
    const dayKey = formatLocalDayKey(capture.updatedAt);
    const existing = dayMap.get(dayKey);
    if (existing) {
      existing.count += 1;
      existing.totalSizeBytes += capture.sizeBytes;
      if (capture.updatedAt > existing.updatedAt) {
        existing.updatedAt = capture.updatedAt;
        existing.coverFileName = capture.fileName;
      }
      continue;
    }
    dayMap.set(dayKey, {
      dayKey,
      label: formatDayLabel(dayKey),
      count: 1,
      updatedAt: capture.updatedAt,
      totalSizeBytes: capture.sizeBytes,
      coverFileName: capture.fileName,
    });
  }
  return [...dayMap.values()].sort((left, right) => right.updatedAt - left.updatedAt);
}

function clearRoomMonitorTimer(): void {
  if (captureTimer) {
    clearTimeout(captureTimer);
    captureTimer = null;
  }
}

function getAvailableFreeSpaceBytes(): number {
  try {
    const stats = fs.statfsSync(roomMonitorDir);
    return stats.bavail * stats.bsize;
  } catch (error) {
    console.warn("[RoomMonitor] Failed to read filesystem free space:", error);
    return 0;
  }
}

function ensureStorageReserve(): void {
  const captures = readCapturesFromDirOldestFirst(roomMonitorDir);
  let freeSpaceBytes = getAvailableFreeSpaceBytes();
  for (const capture of captures) {
    if (freeSpaceBytes >= ROOM_MONITOR_FREE_SPACE_RESERVE_BYTES) {
      break;
    }
    try {
      if (fs.existsSync(capture.imagePath)) {
        fs.unlinkSync(capture.imagePath);
        freeSpaceBytes += capture.sizeBytes;
      }
    } catch (error) {
      console.warn("[RoomMonitor] Failed to remove old capture:", error);
      break;
    }
  }
}

function scheduleNextCapture(): void {
  clearRoomMonitorTimer();
  if (captureIntervalSec <= 0) {
    return;
  }
  captureTimer = setTimeout(() => {
    void captureRoomMonitorImage();
  }, captureIntervalSec * 1000);
}

function getSafePathFromDir(dirPath: string, fileName: string): string {
  const safeFileName = path.basename(fileName || "");
  if (!safeFileName) {
    return "";
  }
  const imagePath = path.resolve(dirPath, safeFileName);
  if (!imagePath.startsWith(path.resolve(dirPath) + path.sep)) {
    return "";
  }
  if (!fs.existsSync(imagePath)) {
    return "";
  }
  return imagePath;
}

function createUniqueSavedPath(fileName: string): string {
  const parsed = path.parse(path.basename(fileName));
  let candidateName = `${parsed.name}${parsed.ext}`;
  let counter = 1;
  while (fs.existsSync(path.join(roomMonitorSavedDir, candidateName))) {
    candidateName = `${parsed.name}-${counter}${parsed.ext}`;
    counter += 1;
  }
  return path.join(roomMonitorSavedDir, candidateName);
}

export async function captureRoomMonitorImage(): Promise<void> {
  if (captureInProgress) {
    return;
  }

  captureInProgress = true;
  const savedPath = path.join(roomMonitorDir, `room-monitor-${Date.now()}.jpg`);

  try {
    await captureCameraImage(savedPath, 15000);
    lastCaptureAt = Date.now();
    lastError = "";
    ensureStorageReserve();
  } catch (error) {
    lastError = error instanceof Error ? error.message : String(error);
    console.error("[RoomMonitor] Capture failed:", error);
    if (fs.existsSync(savedPath)) {
      fs.unlinkSync(savedPath);
    }
  } finally {
    captureInProgress = false;
    scheduleNextCapture();
  }
}

export function applyRoomMonitorSettings(settings: RuntimeSettings): void {
  captureIntervalSec = settings.roomMonitorIntervalSec;
  scheduleNextCapture();
}

export function startRoomMonitor(): void {
  ensureStorageReserve();
  applyRoomMonitorSettings(getRuntimeSettings());
}

export function listRoomMonitorCaptures(): RoomMonitorCapture[] {
  return readCapturesFromDirOldestFirst(roomMonitorDir).reverse();
}

export function listRoomMonitorCaptureDays(): RoomMonitorCaptureDay[] {
  return groupCapturesByDay(listRoomMonitorCaptures());
}

export function listRoomMonitorCapturesForDay(dayKey: string): RoomMonitorCapture[] {
  const normalizedDayKey = dayKey.trim();
  if (!ROOM_MONITOR_DAY_KEY_PATTERN.test(normalizedDayKey)) {
    throw new Error("Invalid room monitor day.");
  }
  return listRoomMonitorCaptures().filter(
    (capture) => formatLocalDayKey(capture.updatedAt) === normalizedDayKey,
  );
}

export function moveRoomMonitorCapturesToSaved(fileNames: string[]): {
  moved: Array<{ fromFileName: string; savedFileName: string }>;
  skipped: string[];
} {
  const moved: Array<{ fromFileName: string; savedFileName: string }> = [];
  const skipped: string[] = [];
  for (const fileName of fileNames) {
    const sourcePath = getRoomMonitorCapturePath(fileName);
    if (!sourcePath) {
      skipped.push(fileName);
      continue;
    }
    const destinationPath = createUniqueSavedPath(fileName);
    fs.renameSync(sourcePath, destinationPath);
    moved.push({
      fromFileName: path.basename(fileName),
      savedFileName: path.basename(destinationPath),
    });
  }
  return { moved, skipped };
}

export function listSavedRoomMonitorCaptures(): RoomMonitorCapture[] {
  return readCapturesFromDirOldestFirst(roomMonitorSavedDir).reverse();
}

export function getRoomMonitorCapturePath(fileName: string): string {
  return getSafePathFromDir(roomMonitorDir, fileName);
}

export function getSavedRoomMonitorCapturePath(fileName: string): string {
  return getSafePathFromDir(roomMonitorSavedDir, fileName);
}

export function getRoomMonitorStatus(): {
  enabled: boolean;
  intervalSec: number;
  captureInProgress: boolean;
  lastCaptureAt: number | null;
  lastError: string;
  totalCount: number;
  totalSizeBytes: number;
  freeSpaceBytes: number;
  freeSpaceReserveBytes: number;
  dayCount: number;
  savedCount: number;
} {
  const captures = readCapturesFromDirOldestFirst(roomMonitorDir);
  const totalSizeBytes = captures.reduce((sum, capture) => sum + capture.sizeBytes, 0);
  return {
    enabled: captureIntervalSec > 0,
    intervalSec: captureIntervalSec,
    captureInProgress,
    lastCaptureAt,
    lastError,
    totalCount: captures.length,
    totalSizeBytes,
    freeSpaceBytes: getAvailableFreeSpaceBytes(),
    freeSpaceReserveBytes: ROOM_MONITOR_FREE_SPACE_RESERVE_BYTES,
    dayCount: groupCapturesByDay(captures.slice().reverse()).length,
    savedCount: listSavedRoomMonitorCaptures().length,
  };
}
