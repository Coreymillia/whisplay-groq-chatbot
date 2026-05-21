import { imageDir, cameraDir } from "./dir";
import fs from "fs";
import path from "path";
import {
  deleteGalleryImages,
  deleteGalleryImagesForDay,
  ensureGalleryStorageReserve,
  getGalleryImagePath,
  getGalleryImageStatus,
  listGalleryImageDays,
  listGalleryImages,
  listGalleryImagesForDay,
} from "./image-gallery";

export const genImgList: string[] = [];
export const capturedImgList: string[] = [];

export type InteractiveImageSource =
  | "manual-capture"
  | "manual-selection"
  | "browser-upload"
  | "other";

let latestDisplayImg = "";
let latestShowedImg = "";
let pendingCapturedImgForChat = "";
let pendingCapturedImgConsumed = false;
let interactiveImagePath = "";
let interactiveImageSource: InteractiveImageSource = "other";

const setLatestShowedImage = (imagePath: string) => {
  latestShowedImg = imagePath ? path.resolve(imagePath) : "";
};

export const activateInteractiveImage = (
  imagePath: string,
  source: InteractiveImageSource,
) => {
  const normalizedPath = path.resolve(imagePath);
  interactiveImagePath = normalizedPath;
  interactiveImageSource = source;
  setLatestShowedImage(normalizedPath);
};

export const clearInteractiveImage = () => {
  interactiveImagePath = "";
  interactiveImageSource = "other";
};

export const hasInteractiveImage = (): boolean => {
  if (!interactiveImagePath || !latestShowedImg) {
    return false;
  }
  const normalizedShown = path.resolve(latestShowedImg);
  return normalizedShown === interactiveImagePath && fs.existsSync(interactiveImagePath);
};

export const getInteractiveImage = (): string => {
  return hasInteractiveImage() ? interactiveImagePath : "";
};

export const getInteractiveImageSource = (): InteractiveImageSource | "" => {
  return hasInteractiveImage() ? interactiveImageSource : "";
};

const readImagesFromDir = (dirPath: string): string[] =>
  listGalleryImages(dirPath)
    .slice()
    .reverse()
    .map((entry) => entry.imagePath);

// 加载最新生成的图片路径到list中
const loadLatestGenImg = () => {
  const images = readImagesFromDir(imageDir);
  genImgList.push(...images);
};

loadLatestGenImg();

// 加载最新拍摄的图片路径到list中
const loadLatestCapturedImg = () => {
  const images = readImagesFromDir(cameraDir);
  capturedImgList.push(...images);
};

loadLatestCapturedImg();

const clearTrackedImagePath = (imagePath: string) => {
  if (latestDisplayImg === imagePath) {
    latestDisplayImg = "";
  }
  if (latestShowedImg === imagePath) {
    latestShowedImg = "";
  }
  if (interactiveImagePath === imagePath) {
    clearInteractiveImage();
  }
  if (pendingCapturedImgForChat === imagePath) {
    clearPendingCapturedImgForChat();
  }
};

const enforceCapturedGalleryReserve = () => {
  ensureGalleryStorageReserve(cameraDir, clearTrackedImagePath);
  const images = readImagesFromDir(cameraDir);
  capturedImgList.splice(0, capturedImgList.length, ...images);
};

const enforceGeneratedGalleryReserve = () => {
  ensureGalleryStorageReserve(imageDir, clearTrackedImagePath);
  const images = readImagesFromDir(imageDir);
  genImgList.splice(0, genImgList.length, ...images);
};

export const setLatestGenImg = (imgPath: string) => {
  const normalizedPath = path.resolve(imgPath);
  const existingIndex = genImgList.indexOf(normalizedPath);
  if (existingIndex >= 0) {
    genImgList.splice(existingIndex, 1);
  }
  genImgList.push(normalizedPath);
  latestDisplayImg = imgPath;
  setLatestShowedImage(normalizedPath);
  enforceGeneratedGalleryReserve();
};

export const getLatestDisplayImg = () => {
  const img = latestDisplayImg;
  latestDisplayImg = "";
  return img;
};

export const showLatestGenImg = () => {
  if (genImgList.length !== 0) {
    latestDisplayImg = genImgList[genImgList.length - 1] || "";
    if (latestDisplayImg) {
      setLatestShowedImage(latestDisplayImg);
    }
    return !!latestDisplayImg;
  } else {
    return false;
  }
};

export const getLatestGenImg = () => {
  return genImgList.length !== 0 ? genImgList[genImgList.length - 1] : "";
};

export const listGeneratedImgs = (): string[] => {
  const images = readImagesFromDir(imageDir);
  genImgList.splice(0, genImgList.length, ...images);
  return [...genImgList].reverse();
};

export const getGeneratedImgByIndex = (index: number): string => {
  const images = listGeneratedImgs();
  return images[index] || "";
};

export const setLatestCapturedImg = (imgPath: string) => {
  const normalizedPath = path.resolve(imgPath);
  const existingIndex = capturedImgList.indexOf(normalizedPath);
  if (existingIndex >= 0) {
    capturedImgList.splice(existingIndex, 1);
  }
  capturedImgList.push(normalizedPath);
  setLatestShowedImage(imgPath);
  enforceCapturedGalleryReserve();
};

export const setPendingCapturedImgForChat = (imgPath: string) => {
  pendingCapturedImgForChat = imgPath || "";
  pendingCapturedImgConsumed = false;
};

export const hasPendingCapturedImgForChat = (): boolean => {
  return Boolean(
    pendingCapturedImgForChat &&
      !pendingCapturedImgConsumed &&
      fs.existsSync(pendingCapturedImgForChat),
  );
};

export const consumePendingCapturedImgForChat = (): string => {
  if (!hasPendingCapturedImgForChat()) {
    return "";
  }
  pendingCapturedImgConsumed = true;
  return pendingCapturedImgForChat;
};

export const clearPendingCapturedImgForChat = () => {
  pendingCapturedImgForChat = "";
  pendingCapturedImgConsumed = false;
};

export const getLatestCapturedImg = () => {
  return capturedImgList.length !== 0
    ? capturedImgList[capturedImgList.length - 1]
    : "";
};

export const listCapturedImgs = (): string[] => {
  const images = readImagesFromDir(cameraDir);
  capturedImgList.splice(0, capturedImgList.length, ...images);
  return [...capturedImgList].reverse();
};

export const showCapturedImgByIndex = (index: number): string => {
  const images = listCapturedImgs();
  const imagePath = images[index] || "";
  if (!imagePath) {
    return "";
  }
  latestDisplayImg = imagePath;
  activateInteractiveImage(imagePath, "manual-selection");
  return imagePath;
};

export const deleteCapturedImg = (fileName: string): string => {
  const imagePath = getGalleryImagePath(cameraDir, fileName);
  if (!imagePath) {
    return "";
  }
  const result = deleteGalleryImages(cameraDir, [fileName], clearTrackedImagePath);
  if (!result.deleted.length) {
    return "";
  }
  const images = readImagesFromDir(cameraDir);
  capturedImgList.splice(0, capturedImgList.length, ...images);
  return imagePath;
};

export const deleteCapturedImgs = (fileNames: string[]): { deleted: string[]; skipped: string[] } => {
  const result = deleteGalleryImages(cameraDir, fileNames, clearTrackedImagePath);
  const images = readImagesFromDir(cameraDir);
  capturedImgList.splice(0, capturedImgList.length, ...images);
  return result;
};

export const deleteCapturedImgsForDay = (dayKey: string): { deleted: string[]; skipped: string[] } => {
  const result = deleteGalleryImagesForDay(cameraDir, dayKey, clearTrackedImagePath);
  const images = readImagesFromDir(cameraDir);
  capturedImgList.splice(0, capturedImgList.length, ...images);
  return result;
};

export const showLatestCapturedImg = () => {
  if (capturedImgList.length !== 0) {
    latestDisplayImg = capturedImgList[capturedImgList.length - 1] || "";
    if (latestDisplayImg) {
      activateInteractiveImage(latestDisplayImg, "manual-selection");
    }
    return !!latestDisplayImg;
  } else {
    return false;
  }
};

export const getCapturedImgPath = (fileName: string): string => {
  return getGalleryImagePath(cameraDir, fileName);
};

export const getGeneratedImgPath = (fileName: string): string => {
  return getGalleryImagePath(imageDir, fileName);
};

export const deleteGeneratedImg = (fileName: string): string => {
  const imagePath = getGalleryImagePath(imageDir, fileName);
  if (!imagePath) {
    return "";
  }
  const result = deleteGalleryImages(imageDir, [fileName], clearTrackedImagePath);
  if (!result.deleted.length) {
    return "";
  }
  const images = readImagesFromDir(imageDir);
  genImgList.splice(0, genImgList.length, ...images);
  return imagePath;
};

export const deleteGeneratedImgs = (fileNames: string[]): { deleted: string[]; skipped: string[] } => {
  const result = deleteGalleryImages(imageDir, fileNames, clearTrackedImagePath);
  const images = readImagesFromDir(imageDir);
  genImgList.splice(0, genImgList.length, ...images);
  return result;
};

export const deleteGeneratedImgsForDay = (dayKey: string): { deleted: string[]; skipped: string[] } => {
  const result = deleteGalleryImagesForDay(imageDir, dayKey, clearTrackedImagePath);
  const images = readImagesFromDir(imageDir);
  genImgList.splice(0, genImgList.length, ...images);
  return result;
};

export const listCapturedImgDays = () => {
  return listGalleryImageDays(cameraDir);
};

export const listGeneratedImgDays = () => {
  return listGalleryImageDays(imageDir);
};

export const listCapturedImgsForDay = (dayKey: string) => {
  return listGalleryImagesForDay(cameraDir, dayKey);
};

export const listGeneratedImgsForDay = (dayKey: string) => {
  return listGalleryImagesForDay(imageDir, dayKey);
};

export const getCapturedImgStatus = () => {
  return getGalleryImageStatus(cameraDir);
};

export const getGeneratedImgStatus = () => {
  return getGalleryImageStatus(imageDir);
};

export const queueDisplayImage = (imagePath: string) => {
  const normalizedPath = path.resolve(imagePath);
  latestDisplayImg = normalizedPath;
  setLatestShowedImage(normalizedPath);
  return normalizedPath;
};

export const getLatestShowedImage = () => {
  return latestShowedImg;
};

export const getImageMimeType = (imagePath: string): string => {
  const ext = path.extname(imagePath).toLowerCase();
  switch (ext) {
    case ".jpg":
    case ".jpeg":
      return "image/jpeg";
    case ".png":
      return "image/png";
    case ".gif":
      return "image/gif";
    case ".bmp":
      return "image/bmp";
    case ".webp":
      return "image/webp";
    default:
      return "application/octet-stream";
  }
};
