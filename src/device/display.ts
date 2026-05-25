import { exec, spawn, type ChildProcess } from "child_process";
import { resolve } from "path";
import { Socket } from "net";
import { getCurrentTimeTag } from "../utils";
import {
  formatGeminiLowTierImageBalanceText,
  getRuntimeSettings,
  getScrollSpeedFactor,
  type GroqHeaderBadgeMode,
} from "../config/runtime-settings";
import { formatGroqHeaderBadgeText } from "../status/groq-header-badge";
import { WebDisplayServer } from "./web-display";
import { webAudioBridge } from "./web-audio-bridge";
import { setVolumeByLevel } from "../utils/volume";
import { getCurrentPersonalityPresetLabel } from "../config/personality-presets";
import dotEnv from "dotenv";

dotEnv.config();

export type GroqHeaderBadgeView =
  | "default"
  | "time"
  | "rpd-model"
  | "personality"
  | "requests-total";

export interface Status {
  status: string;
  emoji: string;
  text: string;
  transaction_id?: string;
  text_input_enabled?: boolean;
  scroll_speed: number;
  scroll_speed_factor: number;
  hat_scroll_speed_factor: number;
  hat_font_size: string;
  hat_font_family: string;
  scroll_sync?: {
    char_end: number;
    duration_ms: number;
  };
  brightness: number;
  RGB: string;
  battery_color: string;
  battery_level: number | undefined;
  image: string;
  camera_mode: boolean;
  camera_capture?: boolean;
  capture_image_path: string;
  wifi_signal_level: number;
  groq_requests_today: number;
  gemini_low_tier_image_balance_usd: number;
  gemini_low_tier_image_balance_text: string;
  llm_model: string;
  groq_header_badge_mode: string;
  groq_header_badge_text: string;
  groq_header_badge_view: GroqHeaderBadgeView;
  vpn_connected: boolean;
  rag_icon_visible: boolean;
  image_icon_visible: boolean;
  music_progress: number | undefined;
  music_duration_ms: number | undefined;
  audio_level: number;
  header_mode: string;
  screensaver_mode: string;
  idle_timeout_sec: number;
  screen_blank_timeout_sec: number;
  hat_text_color: string;
}

function getInitialStatus(): Status {
  const settings = getRuntimeSettings();
  return {
    status: "starting",
    emoji: "😊",
    text: "",
    text_input_enabled: false,
    scroll_speed: 3,
    scroll_speed_factor: getScrollSpeedFactor(settings.scrollSpeedLevel),
    hat_scroll_speed_factor: settings.hatScrollSpeedLevel,
    hat_font_size: settings.hatFontSize,
    hat_font_family: settings.hatFontFamily,
    scroll_sync: undefined,
    brightness: 100,
    RGB: "#00FF30",
    battery_color: "#000000",
    battery_level: undefined,
    image: "",
    camera_mode: false,
    capture_image_path: "",
    wifi_signal_level: 0,
    groq_requests_today: 0,
    gemini_low_tier_image_balance_usd: settings.geminiLowTierImageBalanceUsd,
    gemini_low_tier_image_balance_text: formatGeminiLowTierImageBalanceText(
      settings.geminiLowTierImageBalanceUsd,
    ),
    llm_model: settings.llmModel,
    groq_header_badge_mode: settings.groqHeaderBadgeMode,
    groq_header_badge_text: formatGroqHeaderBadgeText(
      settings.llmModel,
      settings.groqHeaderBadgeMode,
      0,
    ),
    groq_header_badge_view: "default",
    vpn_connected: false,
    rag_icon_visible: false,
    image_icon_visible: false,
    music_progress: undefined,
    music_duration_ms: undefined,
    audio_level: 0,
    header_mode: settings.headerMode,
    screensaver_mode: settings.screensaverMode,
    idle_timeout_sec: settings.idleTimeoutSec,
    screen_blank_timeout_sec: settings.screenBlankTimeoutSec,
    hat_text_color: settings.hatTextColor,
  };
}

export class WhisplayDisplay {
  private currentStatus: Status = getInitialStatus();

  private client = null as Socket | null;
  private buttonPressedCallback: () => void = () => {};
  private buttonReleasedCallback: () => void = () => {};
  private buttonDoubleClickCallback: (() => void) | null = null;
  private buttonDown = false;
  private onCameraCaptureCallback: () => void = () => {};
  private cameraPreviewRequestCallback: () => { ok: boolean; message: string } = () => ({
    ok: false,
    message: "Camera preview is not available.",
  });
  private textInputCallback: (text: string) => void = () => {};
  private isReady: Promise<void>;
  private pythonProcess: any; // Placeholder for Python process if needed
  private buttonPressTimeArray: number[] = [];
  private buttonReleaseTimeArray: number[] = [];
  private buttonDetectInterval: NodeJS.Timeout | null = null;
  private pendingSingleReleaseCallback: (() => void) | null = null;
  private emitPressImmediatelyWithDoubleClick = false;
  private webDisplay: WebDisplayServer | null = null;
  private deviceEnabled: boolean;
  private cameraEnabled: boolean;
  private receiveBuffer = "";
  private textCounterTimer: NodeJS.Timeout | null = null;
  private textCounterTemplate: string | null = null;
  private textCounterStartAt = 0;
  private audioLevelTimer: NodeJS.Timeout | null = null;
  private audioLevelSampleProcess: ChildProcess | null = null;
  private audioLevelSamplePending = false;
  private audioLevelMonitorFailed = false;

  private formatGroqHeaderBadgeText(
    llmModel: string,
    badgeMode: GroqHeaderBadgeMode,
    requestsToday: number,
    badgeView: GroqHeaderBadgeView,
  ): string {
    if (badgeView === "time") {
      return getCurrentTimeTag().slice(11, 16);
    }
    if (badgeView === "rpd-model") {
      const rpdText = formatGroqHeaderBadgeText(llmModel, "rpd-remaining", requestsToday);
      const modelText = formatGroqHeaderBadgeText(llmModel, "model", requestsToday);
      return `${rpdText}/${modelText}`;
    }
    if (badgeView === "personality") {
      const settings = getRuntimeSettings();
      return getCurrentPersonalityPresetLabel(
        settings.personalityPrompt,
        settings.savedPersonalityPresets,
      );
    }
    if (badgeView === "requests-total") {
      return `${Math.max(0, Math.round(requestsToday))}`;
    }
    return formatGroqHeaderBadgeText(llmModel, badgeMode, requestsToday);
  }

  constructor() {
    this.deviceEnabled = parseBoolEnv("WHISPLAY_DEVICE_ENABLED", true);
    this.cameraEnabled = parseBoolEnv("ENABLE_CAMERA", false);
    try {
      setVolumeByLevel(getRuntimeSettings().volumeLevel);
    } catch (error) {
      console.warn("[volume] Failed to apply startup volume:", error);
    }
    const webCameraEnabled = parseBoolEnv("WEB_CAMERA_ENABLED", false);
    if (this.cameraEnabled && !webCameraEnabled) {
      this.ensureCameraDaemon();
    }
    const webEnabled = parseBoolEnv("WHISPLAY_WEB_ENABLED", false);
    if (webEnabled) {
      const port = parseInt(process.env.WHISPLAY_WEB_PORT || "17880", 10);
      const host = process.env.WHISPLAY_WEB_HOST || "0.0.0.0";
      this.webDisplay = new WebDisplayServer({
        host,
        port,
        onButtonPress: () => this.handleButtonPressedEvent(),
        onButtonRelease: () => this.handleButtonReleasedEvent(),
        onButtonDoubleClick: () => this.handleButtonDoubleClickEvent(),
        onTextInput: (text: string) => this.handleTextInputEvent(text),
        onSettingsSaved: (settings) => {
          try {
            setVolumeByLevel(settings.volumeLevel);
          } catch (error) {
            console.warn("[volume] Failed to apply saved volume:", error);
          }
          void this.display({
            header_mode: settings.headerMode,
            screensaver_mode: settings.screensaverMode,
            idle_timeout_sec: settings.idleTimeoutSec,
            screen_blank_timeout_sec: settings.screenBlankTimeoutSec,
            hat_text_color: settings.hatTextColor,
            gemini_low_tier_image_balance_usd: settings.geminiLowTierImageBalanceUsd,
            gemini_low_tier_image_balance_text: formatGeminiLowTierImageBalanceText(
              settings.geminiLowTierImageBalanceUsd,
            ),
            llm_model: settings.llmModel,
            groq_header_badge_mode: settings.groqHeaderBadgeMode,
            scroll_speed: this.currentStatus.scroll_speed,
            scroll_speed_factor: getScrollSpeedFactor(settings.scrollSpeedLevel),
            hat_scroll_speed_factor: settings.hatScrollSpeedLevel,
            hat_font_size: settings.hatFontSize,
            hat_font_family: settings.hatFontFamily,
          });
        },
        onImageUploaded: (imagePath) => {
          void this.display({
            status: "photo ready",
            image: imagePath,
            image_icon_visible: false,
            text: "[camera]Photo captured.",
            text_input_enabled: true,
          });
        },
        onCameraPreviewRequested: () => this.handleCameraPreviewRequest(),
      });
      this.webDisplay.updateStatus(this.currentStatus);
    }

    if (this.deviceEnabled) {
      this.startPythonProcess();
      this.isReady = new Promise<void>((resolve) => {
        this.connectWithRetry(15, resolve);
      });
    } else {
      this.isReady = Promise.resolve();
    }
  }

  startMonitoringDoubleClick(): void {
    if (this.buttonDetectInterval || !this.buttonDoubleClickCallback) return;
    // check if there are two presses and two releases
    this.buttonDetectInterval = setTimeout(() => {
      // clean old click arrays >= 1500ms
      const now = Date.now();
      this.buttonPressTimeArray = this.buttonPressTimeArray.filter(
        (time) => now - time <= 1000,
      );
      this.buttonReleaseTimeArray = this.buttonReleaseTimeArray.filter(
        (time) => now - time <= 1000,
      );
      const doubleClickDetected =
        this.buttonPressTimeArray.length >= 2 &&
        this.buttonReleaseTimeArray.length >= 2;

      if (doubleClickDetected) {
        this.pendingSingleReleaseCallback = null;
        this.buttonDoubleClickCallback?.();
      } else if (this.pendingSingleReleaseCallback) {
        this.pendingSingleReleaseCallback();
        this.pendingSingleReleaseCallback = null;
      } else {
        const lastReleaseTime =
          this.buttonReleaseTimeArray[this.buttonReleaseTimeArray.length - 1] || 0;
        const lastPressTime =
          this.buttonPressTimeArray[this.buttonPressTimeArray.length - 1] || 0;
        if (
          !this.emitPressImmediatelyWithDoubleClick &&
          (!lastReleaseTime || lastReleaseTime < lastPressTime)
        ) {
          console.log("emit pressed");
          this.buttonPressedCallback();
        }
      }

      // reset arrays and interval
      this.buttonPressTimeArray = [];
      this.buttonReleaseTimeArray = [];
      this.buttonDetectInterval = null;
    }, 800);
  }

  startPythonProcess(): void {
    if (!this.deviceEnabled) {
      return;
    }
    const command = `cd ${resolve(
      __dirname,
      "../../python",
    )} && python3 chatbot-ui.py`;
    console.log("Starting Python process...");
    this.pythonProcess = exec(command, (error, stdout, stderr) => {
      if (error) {
        console.error("Error starting Python process:", error);
        return;
      }
      console.log("Python process stdout:", stdout);
      console.error("Python process stderr:", stderr);
    });
    this.pythonProcess.stdout.on("data", (data: any) =>
      console.log(data.toString()),
    );
    this.pythonProcess.stderr.on("data", (data: any) =>
      console.error(data.toString()),
    );
  }

  killPythonProcess(): void {
    if (!this.deviceEnabled) {
      return;
    }
    if (this.pythonProcess) {
      console.log("Killing Python process...", this.pythonProcess.pid);
      this.pythonProcess.kill();
      try {
        process.kill(this.pythonProcess.pid, "SIGKILL");
      } catch (error) {
        const nodeError = error as NodeJS.ErrnoException;
        if (nodeError.code !== "ESRCH") {
          throw error;
        }
      }
      this.pythonProcess = null;
    }
  }

  async connectWithRetry(
    retries: number = 10,
    outerResolve: () => void,
  ): Promise<void> {
    if (!this.deviceEnabled) {
      outerResolve();
      return;
    }
    await new Promise((resolve, reject) => {
      const attemptConnection = (attempt: number) => {
        this.connect()
          .then(() => {
            resolve(true);
          })
          .catch((err) => {
            if (attempt < retries) {
              console.log(`Connection attempt ${attempt} failed, retrying...`);
              setTimeout(() => attemptConnection(attempt + 1), 5000);
            } else {
              console.error("Failed to connect after multiple attempts:", err);
              reject(err);
            }
          });
      };
      attemptConnection(1);
    });
    outerResolve();
  }

  async connect(): Promise<void> {
    console.log("Connecting to local display socket...");
    return new Promise<void>((resolve, reject) => {
      // 销毁原来的this.client
      if (this.client) {
        this.client.destroy();
      }
      this.client = new Socket();
      this.client.connect(12345, "0.0.0.0", () => {
        console.log("Connected to local display socket");
        this.receiveBuffer = "";
        this.sendToDisplay(JSON.stringify(this.currentStatus));
        resolve();
      });
      this.client.on("data", (data: Buffer) => {
        this.receiveBuffer += data.toString();
        while (this.receiveBuffer.includes("\n")) {
          const newlineIndex = this.receiveBuffer.indexOf("\n");
          const line = this.receiveBuffer.slice(0, newlineIndex).trim();
          this.receiveBuffer = this.receiveBuffer.slice(newlineIndex + 1);
          if (!line || line === "OK") {
            continue;
          }
          console.log(
            `[${getCurrentTimeTag()}] Received data from Whisplay hat:`,
            line,
          );
          try {
            const json = JSON.parse(line);
            if (json.event === "button_pressed") {
              this.handleButtonPressedEvent();
            }
            if (json.event === "button_released") {
              this.handleButtonReleasedEvent();
            }
            if (json.event === "camera_capture") {
              this.handleCameraCaptureEvent();
            }
            if (json.event === "exit_camera_mode") {
              this.display({ camera_mode: false });
            }
          } catch {
            // ignore invalid non-json lines
          }
        }
      });
      this.client.on("error", (err: any) => {
        // 如果是ECONNREFUSED
        if (err.code === "ECONNREFUSED") {
          reject(err);
        }
      });
    });
  }

  onButtonPressed(callback: () => void): void {
    this.buttonPressedCallback = callback;
  }

  onButtonReleased(callback: () => void): void {
    this.buttonReleasedCallback = callback;
  }

  onButtonDoubleClick(callback: (() => void) | null): void {
    if (this.buttonDetectInterval) {
      clearTimeout(this.buttonDetectInterval);
      this.buttonDetectInterval = null;
    }
    this.pendingSingleReleaseCallback = null;
    this.buttonPressTimeArray = [];
    this.buttonReleaseTimeArray = [];
    this.buttonDoubleClickCallback = callback || null;
  }

  setEmitPressImmediatelyWithDoubleClick(enabled: boolean): void {
    this.emitPressImmediatelyWithDoubleClick = enabled;
  }

  onCameraCapture(callback: () => void): void {
    this.onCameraCaptureCallback = callback;
  }

  onCameraPreviewRequested(
    callback: () => { ok: boolean; message: string },
  ): void {
    this.cameraPreviewRequestCallback = callback;
  }

  onTextInput(callback: (text: string) => void): void {
    this.textInputCallback = callback;
  }

  private async sendToDisplay(data: string): Promise<void> {
    if (!this.deviceEnabled) {
      return;
    }
    await this.isReady;
    try {
      this.client?.write(`${data}\n`, "utf8", () => {
        // console.log("send", data);
      });
    } catch (error) {
      console.error("Failed to update display.");
    }
  }

  getCurrentStatus(): Status {
    return this.currentStatus;
  }

  private stopTextCounter(): void {
    if (this.textCounterTimer) {
      clearInterval(this.textCounterTimer);
      this.textCounterTimer = null;
    }
    this.textCounterTemplate = null;
    this.textCounterStartAt = 0;
  }

  private startTextCounter(template: string): void {
    this.stopTextCounter();
    this.textCounterTemplate = template;
    this.textCounterStartAt = Date.now();
    this.textCounterTimer = setInterval(() => {
      if (!this.textCounterTemplate) {
        this.stopTextCounter();
        return;
      }
      const elapsedSec = Math.floor((Date.now() - this.textCounterStartAt) / 1000);
      const renderedText = this.textCounterTemplate.replace(
        /\{count\}/g,
        `${elapsedSec}`,
      );
      if (this.currentStatus.text === renderedText) {
        return;
      }
      this.currentStatus.text = renderedText;
      const data = JSON.stringify({ text: renderedText, brightness: 100 });
      this.sendToDisplay(data);
      this.webDisplay?.updateStatus(this.currentStatus);
    }, 1000);
  }

  async display(newStatus: Partial<Status> = {}): Promise<void> {
    const hasTextOverride = Object.prototype.hasOwnProperty.call(
      newStatus,
      "text",
    );
    const normalizedStatus: Partial<Status> = { ...newStatus };
    if (hasTextOverride) {
      const incomingText = `${newStatus.text ?? ""}`;
      if (incomingText.includes("{count}")) {
        this.startTextCounter(incomingText);
        const initialText = incomingText.replace(/\{count\}/g, "0");
        normalizedStatus.text = initialText;
      } else {
        this.stopTextCounter();
      }
    }

    const {
      status,
      emoji,
      text,
      text_input_enabled,
      scroll_speed,
      scroll_speed_factor,
      hat_scroll_speed_factor,
      hat_font_size,
      hat_font_family,
      RGB,
      brightness,
      scroll_sync,
      battery_level,
      battery_color,
      image,
      camera_mode,
      camera_capture,
      capture_image_path,
      wifi_signal_level,
      groq_requests_today,
      gemini_low_tier_image_balance_usd,
      gemini_low_tier_image_balance_text,
      llm_model,
      groq_header_badge_mode,
      groq_header_badge_view,
      vpn_connected,
      rag_icon_visible,
      image_icon_visible,
      music_progress,
      music_duration_ms,
      audio_level,
      header_mode,
      screensaver_mode,
      idle_timeout_sec,
      screen_blank_timeout_sec,
      hat_text_color,
    } = {
      ...this.currentStatus,
      ...normalizedStatus,
    };
    const groqHeaderBadgeText = this.formatGroqHeaderBadgeText(
      llm_model,
      groq_header_badge_mode as GroqHeaderBadgeMode,
      groq_requests_today,
      groq_header_badge_view,
    );

    const changedValues = Object.entries(normalizedStatus).filter(
      ([key, value]) => (this.currentStatus as any)[key] !== value,
    );

    const isTextChanged = changedValues.some(([key]) => key === "text");
    const isGroqHeaderBadgeTextChanged =
      this.currentStatus.groq_header_badge_text !== groqHeaderBadgeText;

    this.currentStatus.status = status;
    this.currentStatus.emoji = emoji;
    this.currentStatus.text = text;
    this.currentStatus.text_input_enabled = text_input_enabled;
    this.currentStatus.scroll_speed = scroll_speed;
    this.currentStatus.scroll_speed_factor = scroll_speed_factor;
    this.currentStatus.hat_scroll_speed_factor = hat_scroll_speed_factor;
    this.currentStatus.hat_font_size = hat_font_size;
    this.currentStatus.hat_font_family = hat_font_family;
    this.currentStatus.RGB = RGB;
    this.currentStatus.brightness = brightness;
    this.currentStatus.scroll_sync = scroll_sync;
    this.currentStatus.battery_level = battery_level;
    this.currentStatus.battery_color = battery_color;
    this.currentStatus.image = image;
    this.currentStatus.camera_mode = camera_mode;
    this.currentStatus.capture_image_path = capture_image_path;
    this.currentStatus.wifi_signal_level = wifi_signal_level;
    this.currentStatus.groq_requests_today = groq_requests_today;
    this.currentStatus.gemini_low_tier_image_balance_usd =
      gemini_low_tier_image_balance_usd;
    this.currentStatus.gemini_low_tier_image_balance_text =
      gemini_low_tier_image_balance_text;
    this.currentStatus.llm_model = llm_model;
    this.currentStatus.groq_header_badge_mode = groq_header_badge_mode;
    this.currentStatus.groq_header_badge_text = groqHeaderBadgeText;
    this.currentStatus.groq_header_badge_view = groq_header_badge_view;
    this.currentStatus.vpn_connected = vpn_connected;
    this.currentStatus.rag_icon_visible = rag_icon_visible;
    this.currentStatus.image_icon_visible = image_icon_visible;
    this.currentStatus.music_progress = music_progress;
    this.currentStatus.music_duration_ms = music_duration_ms;
    this.currentStatus.audio_level = audio_level;
    this.currentStatus.header_mode = header_mode;
    this.currentStatus.screensaver_mode = screensaver_mode;
    this.currentStatus.idle_timeout_sec = idle_timeout_sec;
    this.currentStatus.screen_blank_timeout_sec = screen_blank_timeout_sec;
    this.currentStatus.hat_text_color = hat_text_color;
    this.syncAudioLevelMonitor(status, header_mode);
    
    const changedValuesObj = Object.fromEntries(changedValues);
    if (isGroqHeaderBadgeTextChanged) {
      changedValuesObj.groq_header_badge_text = groqHeaderBadgeText;
    }
    changedValuesObj.brightness = 100;
    const data = JSON.stringify(changedValuesObj);
    if (isTextChanged) console.log("send data:", data);

    if (normalizedStatus.camera_capture) {
      const capturePath = normalizedStatus.capture_image_path || this.currentStatus.capture_image_path;
      if (capturePath) {
        const webCamEnabled = parseBoolEnv("WEB_CAMERA_ENABLED", false);
        if (webCamEnabled && webAudioBridge.isCameraAvailable()) {
          // Request capture from browser camera regardless of physical device state.
          webAudioBridge
            .requestCameraCapture(capturePath)
            .then(() => this.handleCameraCaptureEvent())
            .catch((e) =>
              console.error("[WebCamera] Capture failed:", e),
            );
        } else if (!this.deviceEnabled) {
          // No physical hardware and no web camera: use the Pi camera daemon.
          this.sendCameraDaemonCommand("capture", { path: capturePath });
          this.handleCameraCaptureEvent();
        }
        // When deviceEnabled=true and no web camera: chatbot-ui.py handles the capture.
      }
    }

    this.sendToDisplay(data);
    this.webDisplay?.updateStatus(this.currentStatus);
  }

  private handleButtonPressedEvent(): void {
    this.buttonDown = true;
    this.buttonPressTimeArray.push(Date.now());
    if (!this.buttonDoubleClickCallback || this.emitPressImmediatelyWithDoubleClick) {
      console.log("emit pressed");
      this.buttonPressedCallback();
    }
    this.startMonitoringDoubleClick();
  }

  private handleButtonReleasedEvent(): void {
    this.buttonDown = false;
    this.buttonReleaseTimeArray.push(Date.now());
    if (!this.buttonDoubleClickCallback) {
      console.log("emit released");
      this.buttonReleasedCallback();
      return;
    }
    this.pendingSingleReleaseCallback = () => {
      console.log("emit released");
      this.buttonReleasedCallback();
    };
  }

  isButtonDown(): boolean {
    return this.buttonDown;
  }

  private handleCameraCaptureEvent(): void {
    this.onCameraCaptureCallback();
  }

  private handleCameraPreviewRequest(): { ok: boolean; message: string } {
    return this.cameraPreviewRequestCallback();
  }

  private handleButtonDoubleClickEvent(): void {
    this.buttonDoubleClickCallback?.();
  }

  private handleTextInputEvent(text: string): void {
    this.textInputCallback(text);
  }

  stopWebDisplay(): void {
    this.webDisplay?.close();
    this.webDisplay = null;
  }

  recordConversationTurn(role: "user" | "bot", text: string): void {
    this.webDisplay?.addConversationTurn(role, text);
  }

  private isVuHeaderMode(headerMode: string): boolean {
    return (
      headerMode === "vu-bars" ||
      headerMode === "vu-scope" ||
      headerMode === "vu-wave"
    );
  }

  private shouldRunAudioLevelMonitor(status: string, headerMode: string): boolean {
    return this.deviceEnabled && this.isVuHeaderMode(headerMode) && status === "listening";
  }

  private parseAudioLevel(soxOutput: string): number | null {
    const match = soxOutput.match(/RMS\s+amplitude:\s+([0-9.eE+-]+)/i);
    if (!match) {
      return null;
    }
    const rms = parseFloat(match[1]);
    if (!Number.isFinite(rms) || rms < 0) {
      return null;
    }
    return Math.max(0, Math.min(100, Math.round(Math.sqrt(rms) * 140)));
  }

  private pushAudioLevel(level: number): void {
    const normalizedLevel = Math.max(0, Math.min(100, Math.round(level)));
    if (this.currentStatus.audio_level === normalizedLevel) {
      return;
    }
    this.currentStatus.audio_level = normalizedLevel;
    const data = JSON.stringify({ audio_level: normalizedLevel, brightness: 100 });
    this.sendToDisplay(data);
    this.webDisplay?.updateStatus(this.currentStatus);
  }

  private sampleAudioLevel(): void {
    if (this.audioLevelSamplePending || this.audioLevelMonitorFailed) {
      return;
    }
    this.audioLevelSamplePending = true;
    let output = "";
    const sampleProcess = spawn("sox", [
      "-q",
      "-t",
      "alsa",
      "default",
      "-n",
      "trim",
      "0",
      "0.10",
      "stat",
    ]);
    this.audioLevelSampleProcess = sampleProcess;

    sampleProcess.stdout?.on("data", (data: Buffer) => {
      output += data.toString();
    });
    sampleProcess.stderr?.on("data", (data: Buffer) => {
      output += data.toString();
    });

    sampleProcess.on("error", (error) => {
      if (!this.audioLevelMonitorFailed) {
        console.warn("[audio-level] monitor failed:", error.message);
      }
      this.audioLevelMonitorFailed = true;
      this.audioLevelSamplePending = false;
      this.audioLevelSampleProcess = null;
      this.stopAudioLevelMonitor();
    });

    sampleProcess.on("close", (code) => {
      this.audioLevelSamplePending = false;
      this.audioLevelSampleProcess = null;
      if (code && code !== 0) {
        if (!this.audioLevelMonitorFailed) {
          console.warn(`[audio-level] sample exited with code ${code}`);
        }
        this.audioLevelMonitorFailed = true;
        this.stopAudioLevelMonitor();
        return;
      }

      const detectedLevel = this.parseAudioLevel(output) ?? 0;
      const currentLevel = this.currentStatus.audio_level;
      const smoothedLevel =
        detectedLevel >= currentLevel
          ? Math.round(currentLevel * 0.35 + detectedLevel * 0.65)
          : Math.round(currentLevel * 0.8 + detectedLevel * 0.2);
      this.pushAudioLevel(smoothedLevel);
    });
  }

  private startAudioLevelMonitor(): void {
    if (this.audioLevelTimer) {
      return;
    }
    this.audioLevelMonitorFailed = false;
    this.sampleAudioLevel();
    this.audioLevelTimer = setInterval(() => {
      this.sampleAudioLevel();
    }, 140);
  }

  stopAudioLevelMonitor(): void {
    if (this.audioLevelTimer) {
      clearInterval(this.audioLevelTimer);
      this.audioLevelTimer = null;
    }
    if (this.audioLevelSampleProcess) {
      try {
        this.audioLevelSampleProcess.kill("SIGINT");
      } catch {
        // Process already exited.
      }
      this.audioLevelSampleProcess = null;
    }
    this.audioLevelSamplePending = false;
    this.pushAudioLevel(0);
  }

  private syncAudioLevelMonitor(status: string, headerMode: string): void {
    if (this.shouldRunAudioLevelMonitor(status, headerMode)) {
      this.startAudioLevelMonitor();
      return;
    }
    this.stopAudioLevelMonitor();
  }

  private ensureCameraDaemon(): void {
    const command = `cd ${resolve(
      __dirname,
      "../../python",
    )} && python3 camera.py --ensure-daemon`;
    exec(command, (error, stdout, stderr) => {
      if (error) {
        console.warn("[CameraDaemon] ensure failed:", error.message);
        return;
      }
      if (stdout?.trim()) {
        console.log(stdout.trim());
      }
      if (stderr?.trim()) {
        console.warn(stderr.trim());
      }
    });
  }

  private sendCameraDaemonCommand(
    cmd: string,
    payload: Record<string, unknown> = {},
  ): void {
    const port = parseInt(process.env.WHISPLAY_CAMERA_DAEMON_PORT || "18765", 10);
    const socket = new Socket();
    socket.setTimeout(1000);
    socket.connect(port, "127.0.0.1", () => {
      socket.write(`${JSON.stringify({ cmd, ...payload })}\n`);
      socket.end();
    });
    socket.on("error", () => {
      socket.destroy();
    });
    socket.on("timeout", () => {
      socket.destroy();
    });
  }
}

// Create a singleton instance to maintain backward compatibility
const displayInstance = new WhisplayDisplay();

export const display = displayInstance.display.bind(displayInstance);
export const getCurrentStatus =
  displayInstance.getCurrentStatus.bind(displayInstance);
export const onButtonPressed =
  displayInstance.onButtonPressed.bind(displayInstance);
export const onButtonReleased =
  displayInstance.onButtonReleased.bind(displayInstance);
export const onButtonDoubleClick =
  displayInstance.onButtonDoubleClick.bind(displayInstance);
export const setEmitPressImmediatelyWithDoubleClick =
  displayInstance.setEmitPressImmediatelyWithDoubleClick.bind(displayInstance);
export const onCameraCapture =
  displayInstance.onCameraCapture.bind(displayInstance);
export const onCameraPreviewRequested =
  displayInstance.onCameraPreviewRequested.bind(displayInstance);
export const onTextInput =
  displayInstance.onTextInput.bind(displayInstance);
export const isButtonDown =
  displayInstance.isButtonDown.bind(displayInstance);
export const recordConversationTurn =
  displayInstance.recordConversationTurn.bind(displayInstance);

function cleanup() {
  console.log("Cleaning up display process before exit...");
  displayInstance.stopAudioLevelMonitor();
  displayInstance.killPythonProcess();
  displayInstance.stopWebDisplay();
}

// kill the Python process on exit signals
process.on("exit", cleanup);
["SIGINT", "SIGTERM"].forEach((signal) => {
  process.on(signal, () => {
    console.log(`Received ${signal}, exiting...`);
    cleanup();
    process.exit(0);
  });
});
process.on("uncaughtException", (err) => {
  console.error("Uncaught Exception:", err);
  cleanup();
  process.exit(1);
});
process.on("unhandledRejection", (reason, promise) => {
  console.error("Unhandled Rejection at:", promise, "reason:", reason);
  cleanup();
  process.exit(1);
});
process.on("keyboardInterrupt", () => {
  console.log("Keyboard Interrupt received, killing Python process...");
  cleanup();
  process.exit(0);
});

function parseBoolEnv(key: string, defaultValue: boolean): boolean {
  const raw = process.env[key];
  if (!raw) {
    return defaultValue;
  }
  return raw.toLowerCase() === "true" || raw === "1";
}
