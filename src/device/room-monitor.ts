import fs from "fs";
import path from "path";
import { captureCameraImage } from "./camera-daemon";
import { getRuntimeSettings, type RuntimeSettings } from "../config/runtime-settings";
import { roomMonitorDir, roomMonitorSavedDir } from "../utils/dir";
import {
  deleteGalleryImages,
  deleteGalleryImagesForDay,
  ensureGalleryStorageReserve,
  type GalleryImageDay as RoomMonitorCaptureDay,
  type GalleryImageEntry as RoomMonitorCapture,
  formatGalleryDayKey,
  getGalleryImagePath,
  getGalleryImageStatus,
  listGalleryImageDays,
  listGalleryImages,
  listGalleryImagesForDay,
  readGalleryImagesOldestFirst,
} from "../utils/image-gallery";

let captureTimer: NodeJS.Timeout | null = null;
let captureInProgress = false;
let captureIntervalSec = 0;
let lastCaptureAt: number | null = null;
let lastError = "";

function clearRoomMonitorTimer(): void {
  if (captureTimer) {
    clearTimeout(captureTimer);
    captureTimer = null;
  }
}

function ensureStorageReserve(): void {
  ensureGalleryStorageReserve(roomMonitorDir);
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
  return listGalleryImages(roomMonitorDir);
}

export function listRoomMonitorCaptureDays(): RoomMonitorCaptureDay[] {
  return listGalleryImageDays(roomMonitorDir);
}

export function listRoomMonitorCapturesForDay(dayKey: string): RoomMonitorCapture[] {
  return listGalleryImagesForDay(roomMonitorDir, dayKey);
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
  return listGalleryImages(roomMonitorSavedDir);
}

export function getRoomMonitorCapturePath(fileName: string): string {
  return getGalleryImagePath(roomMonitorDir, fileName);
}

export function getSavedRoomMonitorCapturePath(fileName: string): string {
  return getSafePathFromDir(roomMonitorSavedDir, fileName);
}

export function deleteRoomMonitorCaptures(fileNames: string[]): {
  deleted: string[];
  skipped: string[];
} {
  return deleteGalleryImages(roomMonitorDir, fileNames);
}

export function deleteRoomMonitorCapturesForDay(dayKey: string): {
  deleted: string[];
  skipped: string[];
} {
  return deleteGalleryImagesForDay(roomMonitorDir, dayKey);
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
  const galleryStatus = getGalleryImageStatus(roomMonitorDir);
  return {
    enabled: captureIntervalSec > 0,
    intervalSec: captureIntervalSec,
    captureInProgress,
    lastCaptureAt,
    lastError,
    totalCount: galleryStatus.totalCount,
    totalSizeBytes: galleryStatus.totalSizeBytes,
    freeSpaceBytes: galleryStatus.freeSpaceBytes,
    freeSpaceReserveBytes: galleryStatus.freeSpaceReserveBytes,
    dayCount: galleryStatus.dayCount,
    savedCount: listSavedRoomMonitorCaptures().length,
  };
}
