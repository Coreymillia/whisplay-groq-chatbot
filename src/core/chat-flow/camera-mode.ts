import { display, getCurrentStatus } from "../../device/display";

const CAMERA_TALK_HOLD_MS = 800;

let cameraModePressAt = 0;
let cameraModeLongPressTimer: NodeJS.Timeout | null = null;
let cameraModeLongPressTriggered = false;
let onCameraModeExitCallback: () => void = () => {};

function exitCameraMode(): void {
  if (!getCurrentStatus().camera_mode) {
    return;
  }
  resetCameraModeControl();
  display({ camera_mode: false });
  onCameraModeExitCallback();
}

function clearCameraModeTimers(): void {
  if (cameraModeLongPressTimer) {
    clearTimeout(cameraModeLongPressTimer);
    cameraModeLongPressTimer = null;
  }
}

export function resetCameraModeControl(): void {
  clearCameraModeTimers();
  cameraModePressAt = 0;
  cameraModeLongPressTriggered = false;
}

export function onCameraModeExit(callback: (() => void) | null): void {
  onCameraModeExitCallback = callback || (() => {});
}

export function enterCameraMode(captureImgPath: string): void {
  resetCameraModeControl();
  display({
    camera_mode: true,
    capture_image_path: captureImgPath,
  });
}

export function exitCameraModeNow(): void {
  exitCameraMode();
}

export function handleCameraModePress(onTalkHold?: () => void): void {
  cameraModePressAt = Date.now();
  cameraModeLongPressTriggered = false;
  if (cameraModeLongPressTimer) {
    clearTimeout(cameraModeLongPressTimer);
  }
  cameraModeLongPressTimer = setTimeout(() => {
    cameraModeLongPressTimer = null;
    cameraModeLongPressTriggered = true;
    onTalkHold?.();
  }, CAMERA_TALK_HOLD_MS);
}

export function handleCameraModeRelease(): void {
  const status = getCurrentStatus();
  if (!status.camera_mode) {
    resetCameraModeControl();
    return;
  }

  const duration = Date.now() - cameraModePressAt;
  if (cameraModeLongPressTimer) {
    clearTimeout(cameraModeLongPressTimer);
    cameraModeLongPressTimer = null;
  }

  if (cameraModeLongPressTriggered) {
    cameraModePressAt = 0;
    cameraModeLongPressTriggered = false;
    return;
  }

  if (cameraModePressAt > 0 && duration <= CAMERA_TALK_HOLD_MS) {
    display({
      camera_capture: true,
      text: "[camera]Capturing image...\nStand still.",
      RGB: "#ff2a2a",
    });
  }

  cameraModePressAt = 0;
}
