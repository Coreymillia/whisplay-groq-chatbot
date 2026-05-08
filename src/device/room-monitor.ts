import fs from "fs";
import path from "path";
import { captureCameraImage } from "./camera-daemon";
import { getRuntimeSettings, type RuntimeSettings } from "../config/runtime-settings";
import { roomMonitorDir } from "../utils/dir";

const ROOM_MONITOR_FREE_SPACE_RESERVE_BYTES = 8 * 1024 * 1024 * 1024;

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

function readRoomMonitorImagesOldestFirst(): RoomMonitorCapture[] {
  if (!fs.existsSync(roomMonitorDir)) {
    return [];
  }

  return fs.readdirSync(roomMonitorDir)
    .filter((file) => /\.(jpg|jpeg|png|webp|gif)$/i.test(file))
    .map((fileName) => {
      const imagePath = path.join(roomMonitorDir, fileName);
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
  const captures = readRoomMonitorImagesOldestFirst();
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
  return readRoomMonitorImagesOldestFirst().reverse();
}

export function getRoomMonitorCapturePath(fileName: string): string {
  const safeFileName = path.basename(fileName || "");
  if (!safeFileName) {
    return "";
  }
  const imagePath = path.resolve(roomMonitorDir, safeFileName);
  if (!imagePath.startsWith(path.resolve(roomMonitorDir) + path.sep)) {
    return "";
  }
  if (!fs.existsSync(imagePath)) {
    return "";
  }
  return imagePath;
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
} {
  const captures = readRoomMonitorImagesOldestFirst();
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
  };
}
