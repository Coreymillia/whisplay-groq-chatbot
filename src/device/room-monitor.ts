import fs from "fs";
import path from "path";
import { captureCameraImage } from "./camera-daemon";
import { getRuntimeSettings, type RuntimeSettings } from "../config/runtime-settings";
import { roomMonitorDir, roomMonitorSavedDir } from "../utils/dir";
import { getBotNetManager } from "./botnet";
import {
  deleteGalleryImages,
  deleteGalleryImagesForDay,
  ensureGalleryStorageReserve,
  type GalleryImageDay as RoomMonitorCaptureDay,
  formatGalleryHourKey,
  formatGalleryHourLabel,
  type GalleryImageEntry as RoomMonitorCapture,
  formatGalleryDayLabel,
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

export interface RemoteRoomMonitorCapture {
  fileName: string;
  updatedAt: number;
  sizeBytes: number;
  imageUrl: string;
}

export interface RemoteRoomMonitorCaptureDay {
  dayKey: string;
  label: string;
  count: number;
  updatedAt: number;
  totalSizeBytes: number;
  coverImageUrl: string;
}

export interface RemoteRoomMonitorCaptureHour {
  hourKey: string;
  label: string;
  count: number;
  updatedAt: number;
  totalSizeBytes: number;
  coverImageUrl: string;
}

export interface RemoteRoomMonitorStatus {
  connected: boolean;
  peerUrl: string;
  enabled: boolean;
  intervalSec: number;
  activeNow: boolean;
  startTime: string;
  stopTime: string;
  captureInProgress: boolean;
  lastCaptureAt: number | null;
  lastError: string;
  detectedCamera: string;
  cameraCommand: string;
  totalCount: number;
  totalSizeBytes: number;
  freeSpaceBytes: number;
  freeSpaceReserveBytes: number;
  dayCount: number;
  savedCount: number;
}

interface RemoteRoomMonitorSourceCapture {
  fileName: string;
  capturedAt: number;
  sizeBytes: number;
}

interface RemoteRoomMonitorSourcePayload {
  status: RemoteRoomMonitorStatus;
  captures: RemoteRoomMonitorSourceCapture[];
}

function normalizeRemotePeerUrl(value: string): string {
  const trimmed = value.trim();
  if (!trimmed) {
    return "";
  }
  const withProtocol = /^https?:\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
  return withProtocol.replace(/\/+$/, "");
}

function getRemoteRoomMonitorPeerUrl(): string {
  const peerUrl = normalizeRemotePeerUrl(getBotNetManager().getState().settings.peerUrl || "");
  if (!peerUrl) {
    throw new Error("Set Connect to Bot in the GroqBotNet card first.");
  }
  return peerUrl;
}

function coerceNumber(value: unknown, fallback = 0): number {
  return typeof value === "number" && Number.isFinite(value) ? value : fallback;
}

function coerceString(value: unknown, fallback = ""): string {
  return typeof value === "string" ? value : fallback;
}

function buildRemoteRoomMonitorStatus(
  peerUrl: string,
  remoteStatus: Record<string, unknown>,
  captures: RemoteRoomMonitorSourceCapture[],
): RemoteRoomMonitorStatus {
  const reserveGb = coerceNumber(remoteStatus.freeReserveGb, 0);
  const totalSizeBytes = captures.reduce((sum, capture) => sum + capture.sizeBytes, 0);
  return {
    connected: true,
    peerUrl,
    enabled: Boolean(remoteStatus.enabled),
    intervalSec: coerceNumber(remoteStatus.intervalSec, 0),
    activeNow: remoteStatus.activeNow !== false,
    startTime: coerceString(remoteStatus.startTime),
    stopTime: coerceString(remoteStatus.stopTime),
    captureInProgress: Boolean(remoteStatus.captureInProgress),
    lastCaptureAt:
      typeof remoteStatus.lastCaptureAt === "number" && Number.isFinite(remoteStatus.lastCaptureAt)
        ? remoteStatus.lastCaptureAt
        : null,
    lastError: coerceString(remoteStatus.lastError),
    detectedCamera: coerceString(remoteStatus.detectedCamera),
    cameraCommand: coerceString(remoteStatus.cameraCommand),
    totalCount: captures.length,
    totalSizeBytes,
    freeSpaceBytes: coerceNumber(remoteStatus.freeSpaceBytes, 0),
    freeSpaceReserveBytes: reserveGb > 0 ? reserveGb * 1024 * 1024 * 1024 : 0,
    dayCount: 0,
    savedCount: listSavedRoomMonitorCaptures().length,
  };
}

function buildRemoteRoomMonitorDayGroups(
  captures: RemoteRoomMonitorCapture[],
): RemoteRoomMonitorCaptureDay[] {
  const dayMap = new Map<string, RemoteRoomMonitorCaptureDay>();
  for (const capture of captures) {
    const dayKey = formatGalleryDayKey(capture.updatedAt);
    const existing = dayMap.get(dayKey);
    if (existing) {
      existing.count += 1;
      existing.totalSizeBytes += capture.sizeBytes;
      if (capture.updatedAt > existing.updatedAt) {
        existing.updatedAt = capture.updatedAt;
        existing.coverImageUrl = capture.imageUrl;
      }
      continue;
    }
    dayMap.set(dayKey, {
      dayKey,
      label: formatGalleryDayLabel(dayKey),
      count: 1,
      updatedAt: capture.updatedAt,
      totalSizeBytes: capture.sizeBytes,
      coverImageUrl: capture.imageUrl,
    });
  }
  return [...dayMap.values()].sort((left, right) => right.updatedAt - left.updatedAt);
}

function buildRemoteRoomMonitorHourGroups(
  captures: RemoteRoomMonitorCapture[],
  dayKey: string,
): RemoteRoomMonitorCaptureHour[] {
  const normalizedDayKey = dayKey.trim();
  const hourMap = new Map<string, RemoteRoomMonitorCaptureHour>();
  for (const capture of captures) {
    if (formatGalleryDayKey(capture.updatedAt) !== normalizedDayKey) {
      continue;
    }
    const hourKey = formatGalleryHourKey(capture.updatedAt);
    const existing = hourMap.get(hourKey);
    if (existing) {
      existing.count += 1;
      existing.totalSizeBytes += capture.sizeBytes;
      if (capture.updatedAt > existing.updatedAt) {
        existing.updatedAt = capture.updatedAt;
        existing.coverImageUrl = capture.imageUrl;
      }
      continue;
    }
    hourMap.set(hourKey, {
      hourKey,
      label: formatGalleryHourLabel(hourKey),
      count: 1,
      updatedAt: capture.updatedAt,
      totalSizeBytes: capture.sizeBytes,
      coverImageUrl: capture.imageUrl,
    });
  }
  return [...hourMap.values()].sort((left, right) => right.updatedAt - left.updatedAt);
}

async function fetchRemoteRoomMonitorPayload(): Promise<RemoteRoomMonitorSourcePayload> {
  const peerUrl = getRemoteRoomMonitorPeerUrl();
  const response = await fetch(`${peerUrl}/api/room-monitor/images?limit=5000`, {
    headers: { Accept: "application/json" },
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok || payload?.ok === false) {
    throw new Error(
      payload?.error || payload?.message || `Remote room monitor request failed with HTTP ${response.status}`,
    );
  }

  const remoteStatus =
    payload?.roomMonitor && typeof payload.roomMonitor === "object"
      ? (payload.roomMonitor as Record<string, unknown>)
      : {};
  const rawCaptures = Array.isArray(remoteStatus.captures) ? remoteStatus.captures : [];
  const captures = rawCaptures
    .map((entry) => {
      if (!entry || typeof entry !== "object") {
        return null;
      }
      const fileName = coerceString((entry as Record<string, unknown>).fileName);
      const capturedAt = coerceNumber((entry as Record<string, unknown>).capturedAt, NaN);
      if (!fileName || !Number.isFinite(capturedAt)) {
        return null;
      }
      return {
        fileName,
        capturedAt,
        sizeBytes: coerceNumber((entry as Record<string, unknown>).sizeBytes, 0),
      };
    })
    .filter((entry): entry is RemoteRoomMonitorSourceCapture => Boolean(entry));

  const mappedCaptures: RemoteRoomMonitorCapture[] = captures.map((capture) => ({
    fileName: capture.fileName,
    updatedAt: capture.capturedAt,
    sizeBytes: capture.sizeBytes,
    imageUrl: `/api/remote-room-monitor/photos/image/${encodeURIComponent(capture.fileName)}`,
  }));
  const status = buildRemoteRoomMonitorStatus(peerUrl, remoteStatus, captures);
  status.dayCount = buildRemoteRoomMonitorDayGroups(mappedCaptures).length;
  return {
    status,
    captures,
  };
}

function listRemoteRoomMonitorCaptureDaysFromCaptures(
  captures: RemoteRoomMonitorSourceCapture[],
): RemoteRoomMonitorCaptureDay[] {
  return buildRemoteRoomMonitorDayGroups(
    captures.map((capture) => ({
      fileName: capture.fileName,
      updatedAt: capture.capturedAt,
      sizeBytes: capture.sizeBytes,
      imageUrl: `/api/remote-room-monitor/photos/image/${encodeURIComponent(capture.fileName)}`,
    })),
  );
}

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

export function deleteSavedRoomMonitorCaptures(fileNames: string[]): {
  deleted: string[];
  skipped: string[];
} {
  return deleteGalleryImages(roomMonitorSavedDir, fileNames);
}

export async function listRemoteRoomMonitorCaptureDays(): Promise<{
  days: RemoteRoomMonitorCaptureDay[];
  status: RemoteRoomMonitorStatus;
}> {
  const payload = await fetchRemoteRoomMonitorPayload();
  return {
    days: listRemoteRoomMonitorCaptureDaysFromCaptures(payload.captures),
    status: payload.status,
  };
}

export async function listRemoteRoomMonitorCapturesForDay(
  dayKey: string,
): Promise<{
  photos: RemoteRoomMonitorCapture[];
  status: RemoteRoomMonitorStatus;
}> {
  const normalizedDayKey = dayKey.trim();
  const payload = await fetchRemoteRoomMonitorPayload();
  const photos = payload.captures
    .filter((capture) => formatGalleryDayKey(capture.capturedAt) === normalizedDayKey)
    .map((capture) => ({
      fileName: capture.fileName,
      updatedAt: capture.capturedAt,
      sizeBytes: capture.sizeBytes,
      imageUrl: `/api/remote-room-monitor/photos/image/${encodeURIComponent(capture.fileName)}`,
    }))
    .sort((left, right) => right.updatedAt - left.updatedAt);
  return {
    photos,
    status: payload.status,
  };
}

export async function listRemoteRoomMonitorCaptureHours(
  dayKey: string,
): Promise<{
  hours: RemoteRoomMonitorCaptureHour[];
  status: RemoteRoomMonitorStatus;
}> {
  const normalizedDayKey = dayKey.trim();
  const payload = await fetchRemoteRoomMonitorPayload();
  const captures = payload.captures.map((capture) => ({
    fileName: capture.fileName,
    updatedAt: capture.capturedAt,
    sizeBytes: capture.sizeBytes,
    imageUrl: `/api/remote-room-monitor/photos/image/${encodeURIComponent(capture.fileName)}`,
  }));
  return {
    hours: buildRemoteRoomMonitorHourGroups(captures, normalizedDayKey),
    status: payload.status,
  };
}

export async function listRemoteRoomMonitorCapturesForHour(
  dayKey: string,
  hourKey: string,
): Promise<{
  photos: RemoteRoomMonitorCapture[];
  status: RemoteRoomMonitorStatus;
}> {
  const normalizedDayKey = dayKey.trim();
  const normalizedHourKey = hourKey.trim();
  const payload = await fetchRemoteRoomMonitorPayload();
  const photos = payload.captures
    .filter(
      (capture) =>
        formatGalleryDayKey(capture.capturedAt) === normalizedDayKey &&
        formatGalleryHourKey(capture.capturedAt) === normalizedHourKey,
    )
    .map((capture) => ({
      fileName: capture.fileName,
      updatedAt: capture.capturedAt,
      sizeBytes: capture.sizeBytes,
      imageUrl: `/api/remote-room-monitor/photos/image/${encodeURIComponent(capture.fileName)}`,
    }))
    .sort((left, right) => right.updatedAt - left.updatedAt);
  return {
    photos,
    status: payload.status,
  };
}

export async function getRemoteRoomMonitorImage(
  fileName: string,
): Promise<{ contentType: string; data: Buffer }> {
  const safeFileName = path.basename(fileName || "");
  if (!safeFileName) {
    throw new Error("Remote room monitor image not found.");
  }
  const peerUrl = getRemoteRoomMonitorPeerUrl();
  const response = await fetch(
    `${peerUrl}/api/room-monitor/image?file=${encodeURIComponent(safeFileName)}`,
  );
  if (!response.ok) {
    throw new Error(`Remote image request failed with HTTP ${response.status}`);
  }
  const contentType = response.headers.get("content-type") || "image/jpeg";
  const data = Buffer.from(await response.arrayBuffer());
  if (!data.length) {
    throw new Error("Remote room monitor image was empty.");
  }
  return { contentType, data };
}

async function deleteRemoteRoomMonitorCapture(
  peerUrl: string,
  fileName: string,
): Promise<boolean> {
  const response = await fetch(`${peerUrl}/api/room-monitor/images`, {
    method: "DELETE",
    headers: {
      "Content-Type": "application/json",
      Accept: "application/json",
    },
    body: JSON.stringify({ fileNames: [fileName] }),
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok || payload?.ok === false) {
    return false;
  }
  const deleted = Array.isArray(payload?.deleted) ? payload.deleted : [];
  return deleted.includes(fileName);
}

export async function importRemoteRoomMonitorCaptures(fileNames: string[]): Promise<{
  moved: Array<{ fromFileName: string; savedFileName: string }>;
  skipped: string[];
}> {
  const peerUrl = getRemoteRoomMonitorPeerUrl();
  const moved: Array<{ fromFileName: string; savedFileName: string }> = [];
  const skipped: string[] = [];

  for (const fileName of [...new Set(fileNames.map((entry) => path.basename(entry || "")).filter(Boolean))]) {
    let destinationPath = "";
    try {
      const image = await getRemoteRoomMonitorImage(fileName);
      destinationPath = createUniqueSavedPath(fileName);
      fs.writeFileSync(destinationPath, image.data);
      const deleted = await deleteRemoteRoomMonitorCapture(peerUrl, fileName);
      if (!deleted) {
        fs.unlinkSync(destinationPath);
        skipped.push(fileName);
        continue;
      }
      moved.push({
        fromFileName: fileName,
        savedFileName: path.basename(destinationPath),
      });
    } catch (error) {
      if (destinationPath && fs.existsSync(destinationPath)) {
        fs.unlinkSync(destinationPath);
      }
      console.warn("[RoomMonitor] Failed to import remote room monitor image:", error);
      skipped.push(fileName);
    }
  }

  return { moved, skipped };
}

export async function deleteRemoteRoomMonitorCaptures(fileNames: string[]): Promise<{
  deleted: string[];
  skipped: string[];
  status: RemoteRoomMonitorStatus;
}> {
  const peerUrl = getRemoteRoomMonitorPeerUrl();
  const response = await fetch(`${peerUrl}/api/room-monitor/images`, {
    method: "DELETE",
    headers: {
      "Content-Type": "application/json",
      Accept: "application/json",
    },
    body: JSON.stringify({
      fileNames: [...new Set(fileNames.map((entry) => path.basename(entry || "")).filter(Boolean))],
    }),
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok || payload?.ok === false) {
    throw new Error(payload?.error || `Remote delete failed with HTTP ${response.status}`);
  }
  const latest = await listRemoteRoomMonitorCaptureDays();
  return {
    deleted: Array.isArray(payload?.deleted) ? payload.deleted : [],
    skipped: Array.isArray(payload?.skipped) ? payload.skipped : [],
    status: latest.status,
  };
}

export async function deleteRemoteRoomMonitorCapturesForDay(dayKey: string): Promise<{
  deleted: string[];
  skipped: string[];
  status: RemoteRoomMonitorStatus;
}> {
  const { photos } = await listRemoteRoomMonitorCapturesForDay(dayKey);
  return deleteRemoteRoomMonitorCaptures(photos.map((photo) => photo.fileName));
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
