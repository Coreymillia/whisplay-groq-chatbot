import fs from "fs";
import path from "path";

export const GALLERY_FREE_SPACE_RESERVE_BYTES = 8 * 1024 * 1024 * 1024;
const GALLERY_DAY_KEY_PATTERN = /^\d{4}-\d{2}-\d{2}$/;

export interface GalleryImageEntry {
  fileName: string;
  imagePath: string;
  updatedAt: number;
  sizeBytes: number;
}

export interface GalleryImageDay {
  dayKey: string;
  label: string;
  count: number;
  updatedAt: number;
  totalSizeBytes: number;
  coverFileName: string;
}

export interface GalleryImageStatus {
  totalCount: number;
  totalSizeBytes: number;
  freeSpaceBytes: number;
  freeSpaceReserveBytes: number;
  dayCount: number;
}

function isImageFile(fileName: string): boolean {
  return /\.(jpg|jpeg|png|webp|gif)$/i.test(fileName);
}

export function formatGalleryDayKey(timestamp: number): string {
  const date = new Date(timestamp);
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, "0");
  const day = String(date.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

export function formatGalleryDayLabel(dayKey: string): string {
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

export function readGalleryImagesOldestFirst(dirPath: string): GalleryImageEntry[] {
  if (!fs.existsSync(dirPath)) {
    return [];
  }

  return fs.readdirSync(dirPath)
    .filter((fileName) => isImageFile(fileName))
    .map((fileName) => {
      const imagePath = path.join(dirPath, fileName);
      const stats = fs.statSync(imagePath);
      if (!stats.isFile()) {
        return null;
      }
      return {
        fileName,
        imagePath,
        updatedAt: stats.mtimeMs,
        sizeBytes: stats.size,
      };
    })
    .filter((entry): entry is GalleryImageEntry => Boolean(entry))
    .sort((left, right) => left.updatedAt - right.updatedAt);
}

export function listGalleryImages(dirPath: string): GalleryImageEntry[] {
  return readGalleryImagesOldestFirst(dirPath).reverse();
}

export function listGalleryImageDays(dirPath: string): GalleryImageDay[] {
  const dayMap = new Map<string, GalleryImageDay>();
  for (const entry of listGalleryImages(dirPath)) {
    const dayKey = formatGalleryDayKey(entry.updatedAt);
    const existing = dayMap.get(dayKey);
    if (existing) {
      existing.count += 1;
      existing.totalSizeBytes += entry.sizeBytes;
      if (entry.updatedAt > existing.updatedAt) {
        existing.updatedAt = entry.updatedAt;
        existing.coverFileName = entry.fileName;
      }
      continue;
    }
    dayMap.set(dayKey, {
      dayKey,
      label: formatGalleryDayLabel(dayKey),
      count: 1,
      updatedAt: entry.updatedAt,
      totalSizeBytes: entry.sizeBytes,
      coverFileName: entry.fileName,
    });
  }
  return [...dayMap.values()].sort((left, right) => right.updatedAt - left.updatedAt);
}

export function listGalleryImagesForDay(
  dirPath: string,
  dayKey: string,
): GalleryImageEntry[] {
  const normalizedDayKey = dayKey.trim();
  if (!GALLERY_DAY_KEY_PATTERN.test(normalizedDayKey)) {
    throw new Error("Invalid gallery day.");
  }
  return listGalleryImages(dirPath).filter(
    (entry) => formatGalleryDayKey(entry.updatedAt) === normalizedDayKey,
  );
}

export function getGalleryImagePath(dirPath: string, fileName: string): string {
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

function getAvailableFreeSpaceBytes(dirPath: string): number {
  try {
    const stats = fs.statfsSync(dirPath);
    return stats.bavail * stats.bsize;
  } catch (error) {
    console.warn("[Gallery] Failed to read filesystem free space:", error);
    return 0;
  }
}

export function ensureGalleryStorageReserve(
  dirPath: string,
  onDelete?: (imagePath: string) => void,
): void {
  const images = readGalleryImagesOldestFirst(dirPath);
  let freeSpaceBytes = getAvailableFreeSpaceBytes(dirPath);
  for (const image of images) {
    if (freeSpaceBytes >= GALLERY_FREE_SPACE_RESERVE_BYTES) {
      break;
    }
    try {
      if (!fs.existsSync(image.imagePath)) {
        continue;
      }
      fs.unlinkSync(image.imagePath);
      onDelete?.(image.imagePath);
      freeSpaceBytes += image.sizeBytes;
    } catch (error) {
      console.warn("[Gallery] Failed to remove old image:", error);
      break;
    }
  }
}

export function deleteGalleryImages(
  dirPath: string,
  fileNames: string[],
  onDelete?: (imagePath: string) => void,
): { deleted: string[]; skipped: string[] } {
  const deleted: string[] = [];
  const skipped: string[] = [];
  for (const fileName of fileNames) {
    const imagePath = getGalleryImagePath(dirPath, fileName);
    if (!imagePath) {
      skipped.push(fileName);
      continue;
    }
    fs.unlinkSync(imagePath);
    onDelete?.(imagePath);
    deleted.push(path.basename(imagePath));
  }
  return { deleted, skipped };
}

export function deleteGalleryImagesForDay(
  dirPath: string,
  dayKey: string,
  onDelete?: (imagePath: string) => void,
): { deleted: string[]; skipped: string[] } {
  const fileNames = listGalleryImagesForDay(dirPath, dayKey).map((entry) => entry.fileName);
  return deleteGalleryImages(dirPath, fileNames, onDelete);
}

export function getGalleryImageStatus(dirPath: string): GalleryImageStatus {
  const images = readGalleryImagesOldestFirst(dirPath);
  const totalSizeBytes = images.reduce((sum, image) => sum + image.sizeBytes, 0);
  return {
    totalCount: images.length,
    totalSizeBytes,
    freeSpaceBytes: getAvailableFreeSpaceBytes(dirPath),
    freeSpaceReserveBytes: GALLERY_FREE_SPACE_RESERVE_BYTES,
    dayCount: listGalleryImageDays(dirPath).length,
  };
}
