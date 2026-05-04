import { imageDir, cameraDir } from "./dir";
import fs from "fs";
import path from "path";

export const genImgList: string[] = [];
export const capturedImgList: string[] = [];
export const MAX_CAPTURED_IMGS = 100;

let latestDisplayImg = "";
let latestShowedImg = "";
let pendingCapturedImgForChat = "";
let pendingCapturedImgConsumed = false;

const setLatestShowedImage = (imagePath: string) => {
  latestShowedImg = imagePath;
};

const readImagesFromDir = (dirPath: string): string[] => {
  if (!fs.existsSync(dirPath)) {
    return [];
  }
  return fs.readdirSync(dirPath)
    .filter((file) => /\.(jpg|png|jpeg|webp|gif)$/i.test(file))
    .sort((a, b) => {
      const aTime = fs.statSync(path.join(dirPath, a)).mtime.getTime();
      const bTime = fs.statSync(path.join(dirPath, b)).mtime.getTime();
      return aTime - bTime;
    })
    .map((file) => path.join(dirPath, file));
};

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
  if (pendingCapturedImgForChat === imagePath) {
    clearPendingCapturedImgForChat();
  }
};

const enforceCapturedImgLimit = (maxCount = MAX_CAPTURED_IMGS) => {
  if (maxCount <= 0) {
    return;
  }
  const images = readImagesFromDir(cameraDir);
  const overflowCount = Math.max(0, images.length - maxCount);
  if (overflowCount <= 0) {
    capturedImgList.splice(0, capturedImgList.length, ...images);
    return;
  }
  const removed = images.slice(0, overflowCount);
  removed.forEach((imagePath) => {
    if (fs.existsSync(imagePath)) {
      fs.unlinkSync(imagePath);
    }
    clearTrackedImagePath(imagePath);
  });
  const remainingImages = images.slice(overflowCount);
  capturedImgList.splice(0, capturedImgList.length, ...remainingImages);
};

export const setLatestGenImg = (imgPath: string) => {
  genImgList.push(imgPath);
  latestDisplayImg = imgPath;
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

export const setLatestCapturedImg = (imgPath: string) => {
  const normalizedPath = path.resolve(imgPath);
  const existingIndex = capturedImgList.indexOf(normalizedPath);
  if (existingIndex >= 0) {
    capturedImgList.splice(existingIndex, 1);
  }
  capturedImgList.push(normalizedPath);
  setLatestShowedImage(imgPath);
  enforceCapturedImgLimit();
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
  setLatestShowedImage(imagePath);
  return imagePath;
};

export const deleteCapturedImg = (fileName: string): string => {
  const safeFileName = path.basename(fileName || "");
  if (!safeFileName) {
    return "";
  }
  const imagePath = path.resolve(cameraDir, safeFileName);
  if (!imagePath.startsWith(path.resolve(cameraDir) + path.sep)) {
    return "";
  }
  if (!fs.existsSync(imagePath)) {
    return "";
  }
  fs.unlinkSync(imagePath);
  const existingIndex = capturedImgList.indexOf(imagePath);
  if (existingIndex >= 0) {
    capturedImgList.splice(existingIndex, 1);
  }
  clearTrackedImagePath(imagePath);
  return imagePath;
};

export const showLatestCapturedImg = () => {
  if (capturedImgList.length !== 0) {
    latestDisplayImg = capturedImgList[capturedImgList.length - 1] || "";
    if (latestDisplayImg) {
      setLatestShowedImage(latestDisplayImg);
    }
    return !!latestDisplayImg;
  } else {
    return false;
  }
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
