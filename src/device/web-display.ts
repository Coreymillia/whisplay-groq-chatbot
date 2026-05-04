import fs from "fs";
import path from "path";
import http from "http";
import { Socket } from "net";
import Koa from "koa";
import Router from "@koa/router";
import bodyParser from "koa-bodyparser";
import serve from "koa-static";
import { WebSocketServer, WebSocket, RawData } from "ws";
import { dataDir, cameraFeedDir } from "../utils/dir";
import {
  deleteCapturedImg,
  getImageMimeType,
  listCapturedImgs,
  getLatestShowedImage,
  setLatestCapturedImg,
} from "../utils/image";
import {
  clearLatestVisionAnalysis,
  getLatestVisionAnalysis,
} from "../utils/vision-analysis";
import {
  listSavedChatHistories,
  loadSavedChatHistory,
  resetChatHistory,
} from "../cloud-api/server";
import {
  captureCameraImage,
  sendCameraDaemonCommand,
} from "./camera-daemon";
import {
  getPublicRuntimeSettings,
  IDLE_TIMEOUT_OPTIONS,
  RECORD_TIMEOUT_OPTIONS,
  SCROLL_SPEED_OPTIONS,
  VOLUME_LEVEL_OPTIONS,
  saveRuntimeSettings,
} from "../config/runtime-settings";
import { PERSONALITY_PRESETS } from "../config/personality-presets";
import type { RuntimeSettings } from "../config/runtime-settings";
import {
  webAudioBridge,
  FRAME_AUDIO_CHUNK,
  FRAME_LIVE_CAMERA,
  FRAME_CAMERA_CAPTURE,
  type WebAudioBridgeServer,
} from "./web-audio-bridge";
import type { Status } from "./display";
import { requestSystemShutdown } from "./system-control";

type ButtonHandler = () => void;

type TextInputHandler = (text: string) => void;

interface WebDisplayOptions {
  host: string;
  port: number;
  onButtonPress: ButtonHandler;
  onButtonRelease: ButtonHandler;
  onTextInput?: TextInputHandler;
  onSettingsSaved?: (settings: RuntimeSettings) => void;
  onImageUploaded?: (imagePath: string) => void;
}

function normalizeBodyKey(key: string): string {
  return key.replace(/[^a-z0-9]/gi, "").toLowerCase();
}

function normalizeRequestBody(body: unknown): Record<string, unknown> {
  if (!body || typeof body !== "object" || Array.isArray(body)) {
    return {};
  }
  return Object.entries(body as Record<string, unknown>).reduce<Record<string, unknown>>(
    (result, [key, value]) => {
      result[normalizeBodyKey(key)] = value;
      return result;
    },
    {},
  );
}

function getBodyString(
  body: Record<string, unknown>,
  key: string,
): string | undefined {
  const value = body[normalizeBodyKey(key)];
  return typeof value === "string" ? value : undefined;
}

function getBodyBoolean(body: Record<string, unknown>, key: string): boolean {
  return body[normalizeBodyKey(key)] === true;
}

function getBodyNumber(
  body: Record<string, unknown>,
  key: string,
): number | undefined {
  const value = body[normalizeBodyKey(key)];
  if (typeof value === "number" && Number.isFinite(value)) {
    return value;
  }
  if (typeof value === "string" && value.trim().length > 0) {
    const numeric = Number(value);
    if (Number.isFinite(numeric)) {
      return numeric;
    }
  }
  return undefined;
}

export class WebDisplayServer implements WebAudioBridgeServer {
  private app: Koa;
  private router: Router;
  private currentStatus: Status | null = null;
  private imageRevision = 0;
  private cameraFramePath: string | null = null;
  private host: string;
  private port: number;
  private onButtonPress: ButtonHandler;
  private onButtonRelease: ButtonHandler;
  private onTextInput: TextInputHandler;
  private server: http.Server | null = null;
  private wsServer: WebSocketServer | null = null;
  private wsClients = new Set<WebSocket>();
  private onSettingsSaved: (settings: RuntimeSettings) => void;
  private onImageUploaded: (imagePath: string) => void;

  constructor(options: WebDisplayOptions) {
    this.host = options.host;
    this.port = options.port;
    this.onButtonPress = options.onButtonPress;
    this.onButtonRelease = options.onButtonRelease;
    this.onTextInput = options.onTextInput || (() => {});
    this.onSettingsSaved = options.onSettingsSaved || (() => {});
    this.onImageUploaded = options.onImageUploaded || (() => {});
    this.app = new Koa();
    this.router = new Router();
    this.cameraFramePath = this.resolveCameraFramePath();

    const staticRoot = this.resolveWebRoot();
    this.registerRoutes(staticRoot);
    this.app.use(bodyParser({ jsonLimit: "15mb", formLimit: "15mb", textLimit: "15mb" }));
    this.app.use(this.router.routes());
    this.app.use(this.router.allowedMethods());
    this.app.use(serve(staticRoot));

    this.server = http.createServer(this.app.callback());
    this.wsServer = new WebSocketServer({ server: this.server, path: "/ws" });
    this.wsServer.on("connection", (socket) => {
      this.wsClients.add(socket);
      if (this.currentStatus) {
        socket.send(JSON.stringify({ type: "state", payload: this.buildStatePayload() }));
      }
      socket.on("message", (message, isBinary) =>
        this.handleWsMessage(socket, message, isBinary),
      );
      socket.on("close", () => this.wsClients.delete(socket));
      socket.on("error", () => this.wsClients.delete(socket));
    });

    // Register this server with the web-audio bridge so it can send commands
    // to connected browser clients.
    webAudioBridge.setServer(this);

    this.server.listen(this.port, this.host, () => {
      console.log(
        `[WebDisplay] Simulator running at http://${this.host}:${this.port}`,
      );
    });
  }

  updateStatus(status: Status): void {
    const prevCameraMode = this.currentStatus?.camera_mode || false;
    const nextImage = status.image || "";
    const prevImage = this.currentStatus?.image || "";
    if (nextImage !== prevImage) {
      this.imageRevision += 1;
    }
    this.currentStatus = { ...status };
    const nextCameraMode = this.currentStatus.camera_mode;
    if (!prevCameraMode && nextCameraMode) {
      if (webAudioBridge.isCameraEnabled()) {
        // Use browser camera: tell the web client to start streaming frames.
        webAudioBridge.notifyCameraStreamState(true);
      } else {
        this.sendCameraDaemonCommand("start_stream");
      }
    } else if (prevCameraMode && !nextCameraMode) {
      if (webAudioBridge.isCameraEnabled()) {
        webAudioBridge.notifyCameraStreamState(false);
      } else {
        this.sendCameraDaemonCommand("stop_stream");
      }
    }
    this.broadcastState();
  }

  close(): void {
    webAudioBridge.setServer(null);
    this.wsServer?.close();
    this.wsServer = null;
    this.wsClients.clear();
    this.server?.close();
    this.server = null;
  }

  /** Broadcast a text or binary message to every connected browser client. */
  broadcastToWebClients(message: string | Buffer): void {
    for (const client of this.wsClients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    }
  }

  /** Return the number of currently connected browser clients. */
  getWebClientCount(): number {
    return this.wsClients.size;
  }

  private resolveWebRoot(): string {
    return path.resolve(__dirname, "../..", "web", "whisplay-display");
  }

  private registerRoutes(staticRoot: string): void {
    this.router.get("/", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      ctx.type = "text/html";
      ctx.body = fs.createReadStream(path.join(staticRoot, "index.html"));
    });

    this.router.get("/image", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      if (!this.currentStatus?.image) {
        ctx.status = 404;
        ctx.body = "No image";
        return;
      }

      const safePath = this.resolveSafeImagePath(this.currentStatus.image);
      if (!safePath || !fs.existsSync(safePath)) {
        ctx.status = 404;
        ctx.body = "Image not found";
        return;
      }

      ctx.type = getImageMimeType(safePath);
      ctx.body = fs.createReadStream(safePath);
    });

    this.router.get("/camera", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      if (!this.cameraFramePath) {
        ctx.status = 404;
        ctx.body = "Camera frame not configured";
        return;
      }
      if (!fs.existsSync(this.cameraFramePath)) {
        ctx.status = 404;
        ctx.body = "Camera frame not found";
        return;
      }
      ctx.type = getImageMimeType(this.cameraFramePath);
      ctx.body = fs.createReadStream(this.cameraFramePath);
    });

    this.router.get("/api/vision/image", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      const latestImagePath = getLatestShowedImage();
      if (!latestImagePath) {
        ctx.status = 404;
        ctx.body = "No image";
        return;
      }
      const safePath = this.resolveSafeImagePath(latestImagePath);
      if (!safePath || !fs.existsSync(safePath)) {
        ctx.status = 404;
        ctx.body = "Image not found";
        return;
      }
      ctx.type = getImageMimeType(safePath);
      ctx.body = fs.createReadStream(safePath);
    });

    this.router.post("/api/vision/upload", (ctx) => {
      const body = normalizeRequestBody((ctx.request as any).body);
      const fileName = getBodyString(body, "fileName") || "upload.jpg";
      const contentType = getBodyString(body, "contentType") || "image/jpeg";
      const dataUrl = getBodyString(body, "dataUrl") || "";
      const match = /^data:(image\/[a-z0-9.+-]+);base64,(.+)$/i.exec(dataUrl);
      if (!match) {
        ctx.status = 400;
        ctx.body = { ok: false, error: "Invalid image payload." };
        return;
      }
      const mimeType = match[1].toLowerCase();
      const supportedMimeTypes: Record<string, string> = {
        "image/jpeg": ".jpg",
        "image/jpg": ".jpg",
        "image/png": ".png",
        "image/webp": ".webp",
        "image/gif": ".gif",
      };
      const extension =
        supportedMimeTypes[mimeType] ||
        supportedMimeTypes[contentType.toLowerCase()] ||
        path.extname(fileName).toLowerCase();
      if (!extension || !Object.values(supportedMimeTypes).includes(extension)) {
        ctx.status = 400;
        ctx.body = { ok: false, error: "Unsupported image type." };
        return;
      }
      const base64Payload = match[2];
      const buffer = Buffer.from(base64Payload, "base64");
      if (!buffer.length) {
        ctx.status = 400;
        ctx.body = { ok: false, error: "Empty image payload." };
        return;
      }
      const uploadDir = path.resolve(dataDir, "camera");
      fs.mkdirSync(uploadDir, { recursive: true });
      const savedPath = path.join(uploadDir, `vision-upload-${Date.now()}${extension}`);
      fs.writeFileSync(savedPath, buffer);
      setLatestCapturedImg(savedPath);
      clearLatestVisionAnalysis();
      this.onImageUploaded(savedPath);
      ctx.body = {
        ok: true,
        imageUrl: `/api/vision/image?ts=${Date.now()}`,
        fileName: path.basename(savedPath),
      };
    });

    this.router.post("/api/vision/capture", async (ctx) => {
      const uploadDir = path.resolve(dataDir, "camera");
      fs.mkdirSync(uploadDir, { recursive: true });
      const savedPath = path.join(uploadDir, `vision-capture-${Date.now()}.jpg`);
      try {
        await captureCameraImage(savedPath, 8000);
      } catch (error) {
        ctx.status = 500;
        ctx.body = {
          ok: false,
          error: error instanceof Error ? error.message : "Capture failed.",
        };
        return;
      }
      setLatestCapturedImg(savedPath);
      clearLatestVisionAnalysis();
      this.onImageUploaded(savedPath);
      ctx.body = {
        ok: true,
        imageUrl: `/api/vision/image?ts=${Date.now()}`,
        fileName: path.basename(savedPath),
      };
    });

    this.router.get("/api/vision/analysis", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      ctx.body = {
        analysis: getLatestVisionAnalysis(),
      };
    });

    this.router.get("/api/photos", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      const photos = listCapturedImgs().map((photoPath) => {
        const stats = fs.statSync(photoPath);
        const fileName = path.basename(photoPath);
        return {
          fileName,
          imageUrl: `/api/photos/image/${encodeURIComponent(fileName)}`,
          updatedAt: stats.mtimeMs,
          sizeBytes: stats.size,
        };
      });
      ctx.body = { photos };
    });

    this.router.get("/api/photos/image/:fileName", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      const fileName = decodeURIComponent(String(ctx.params.fileName || ""));
      const photoPath = path.resolve(dataDir, "camera", path.basename(fileName));
      if (!photoPath.startsWith(path.resolve(dataDir, "camera") + path.sep)) {
        ctx.status = 400;
        ctx.body = "Invalid photo path";
        return;
      }
      if (!fs.existsSync(photoPath)) {
        ctx.status = 404;
        ctx.body = "Photo not found";
        return;
      }
      ctx.type = getImageMimeType(photoPath);
      ctx.body = fs.createReadStream(photoPath);
    });

    this.router.delete("/api/photos", (ctx) => {
      const body = normalizeRequestBody((ctx.request as any).body);
      const fileName = getBodyString(body, "fileName") || "";
      const deletedPath = deleteCapturedImg(fileName);
      if (!deletedPath) {
        ctx.status = 404;
        ctx.body = { ok: false, error: "Photo not found." };
        return;
      }
      if (this.currentStatus?.image === deletedPath) {
        this.updateStatus({
          ...this.currentStatus,
          image: "",
          image_icon_visible: false,
        });
      }
      ctx.body = { ok: true };
    });

    this.router.get("/api/chat/histories", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      ctx.body = {
        histories: listSavedChatHistories(),
      };
    });

    this.router.post("/api/chat/reset", (ctx) => {
      resetChatHistory();
      ctx.body = { ok: true };
    });

    this.router.post("/api/chat/load", (ctx) => {
      const body = normalizeRequestBody((ctx.request as any).body);
      const fileName = getBodyString(body, "fileName") || "";
      if (!fileName) {
        ctx.status = 400;
        ctx.body = { ok: false, error: "Missing chat history file." };
        return;
      }
      const loaded = loadSavedChatHistory(fileName);
      if (!loaded) {
        ctx.status = 404;
        ctx.body = { ok: false, error: "Chat history not found." };
        return;
      }
      ctx.body = { ok: true };
    });

    this.router.get("/api/settings", (ctx) => {
      ctx.set("Cache-Control", "no-store");
      ctx.body = {
        settings: getPublicRuntimeSettings(),
        presets: PERSONALITY_PRESETS,
        volumeLevelOptions: VOLUME_LEVEL_OPTIONS,
        scrollSpeedOptions: SCROLL_SPEED_OPTIONS,
        recordTimeoutOptions: RECORD_TIMEOUT_OPTIONS,
        idleTimeoutOptions: IDLE_TIMEOUT_OPTIONS,
      };
    });

    this.router.post("/api/settings", (ctx) => {
      const body = normalizeRequestBody((ctx.request as any).body);
      const settings = saveRuntimeSettings({
        groqApiKey: getBodyString(body, "groqApiKey"),
        clearGroqApiKey: getBodyBoolean(body, "clearGroqApiKey"),
        geminiApiKey: getBodyString(body, "geminiApiKey"),
        personalityPrompt: getBodyString(body, "personalityPrompt"),
        volumeLevel: getBodyNumber(body, "volumeLevel"),
        scrollSpeedLevel: getBodyNumber(body, "scrollSpeedLevel"),
        voiceMode: getBodyString(body, "voiceMode"),
        uiTheme: getBodyString(body, "uiTheme"),
        cameraSource: getBodyString(body, "cameraSource"),
        esp32CamUrl: getBodyString(body, "esp32CamUrl"),
        manualRecordMaxSec: getBodyNumber(body, "manualRecordMaxSec"),
        headerMode: getBodyString(body, "headerMode"),
        screensaverMode: getBodyString(body, "screensaverMode"),
        idleTimeoutSec: getBodyNumber(body, "idleTimeoutSec"),
      });
      this.onSettingsSaved(settings);

      ctx.body = {
        ok: true,
        settings: {
          groqApiKeyConfigured: Boolean(settings.groqApiKey),
          geminiApiKeyConfigured: Boolean(settings.geminiApiKey),
          personalityPrompt: settings.personalityPrompt,
          personalityPresetId: getPublicRuntimeSettings().personalityPresetId,
          volumeLevel: settings.volumeLevel,
          scrollSpeedLevel: settings.scrollSpeedLevel,
          voiceMode: settings.voiceMode,
          uiTheme: settings.uiTheme,
          cameraSource: settings.cameraSource,
          esp32CamUrl: settings.esp32CamUrl,
          manualRecordMaxSec: settings.manualRecordMaxSec,
          headerMode: settings.headerMode,
          screensaverMode: settings.screensaverMode,
          idleTimeoutSec: settings.idleTimeoutSec,
        },
        presets: PERSONALITY_PRESETS,
        volumeLevelOptions: VOLUME_LEVEL_OPTIONS,
        scrollSpeedOptions: SCROLL_SPEED_OPTIONS,
        recordTimeoutOptions: RECORD_TIMEOUT_OPTIONS,
        idleTimeoutOptions: IDLE_TIMEOUT_OPTIONS,
      };
    });

    this.router.post("/api/system/shutdown", async (ctx) => {
      try {
        await requestSystemShutdown();
        ctx.body = { ok: true };
      } catch (error) {
        ctx.status = 500;
        ctx.body = {
          ok: false,
          error:
            error instanceof Error ? error.message : "Shutdown request failed",
        };
      }
    });

  }

  private buildStatePayload(): any {
    if (!this.currentStatus) {
      return { ready: false };
    }

    return {
      ready: true,
      status: this.currentStatus.status,
      emoji: this.currentStatus.emoji,
      text: this.currentStatus.text,
      text_input_enabled: this.currentStatus.text_input_enabled,
      scroll_speed: this.currentStatus.scroll_speed,
      scroll_speed_factor: this.currentStatus.scroll_speed_factor,
      scroll_sync: this.currentStatus.scroll_sync,
      brightness: this.currentStatus.brightness,
      RGB: this.currentStatus.RGB,
      battery_color: this.currentStatus.battery_color,
      battery_level: this.currentStatus.battery_level,
      image: this.currentStatus.image,
      camera_mode: this.currentStatus.camera_mode,
      capture_image_path: this.currentStatus.capture_image_path,
      wifi_signal_level: this.currentStatus.wifi_signal_level,
      vpn_connected: this.currentStatus.vpn_connected,
      rag_icon_visible: this.currentStatus.rag_icon_visible,
      image_icon_visible: this.currentStatus.image_icon_visible,
      image_revision: this.imageRevision,
      music_progress: this.currentStatus.music_progress,
      music_duration_ms: this.currentStatus.music_duration_ms,
      header_mode: this.currentStatus.header_mode,
      screensaver_mode: this.currentStatus.screensaver_mode,
      idle_timeout_sec: this.currentStatus.idle_timeout_sec,
    };
  }

  private broadcastState(): void {
    if (!this.currentStatus || this.wsClients.size === 0) {
      return;
    }
    const payload = JSON.stringify({ type: "state", payload: this.buildStatePayload() });
    for (const client of this.wsClients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(payload);
      }
    }
  }

  private handleWsMessage(
    socket: WebSocket,
    message: RawData,
    isBinary: boolean,
  ): void {
    // ── Binary frames: audio / camera data from browser ─────────────────────
    if (isBinary) {
      const buf = Buffer.isBuffer(message)
        ? message
        : Buffer.from(message as ArrayBuffer);
      if (buf.length < 2) return;
      const frameType = buf[0];
      const payload = buf.slice(1);
      if (frameType === FRAME_AUDIO_CHUNK) {
        webAudioBridge.handleAudioChunk(payload);
      } else if (frameType === FRAME_LIVE_CAMERA) {
        webAudioBridge.handleLiveCameraFrame(payload);
      } else if (frameType === FRAME_CAMERA_CAPTURE) {
        webAudioBridge.handleCameraCaptureResult(payload);
      }
      return;
    }

    // ── Text / JSON frames ────────────────────────────────────────────────────
    let data: any;
    try {
      data = JSON.parse(message.toString());
    } catch {
      return;
    }

    if (data?.type === "button") {
      const action = String(data.action || "");
      if (action === "press") {
        this.onButtonPress();
      } else if (action === "release") {
        this.onButtonRelease();
      }
      return;
    }
    if (data?.type === "record_complete") {
      webAudioBridge.handleRecordComplete();
      return;
    }
    if (data?.type === "play_complete") {
      webAudioBridge.handlePlayComplete(data.playId);
      return;
    }
    if (data?.type === "text_input") {
      const text = typeof data.text === "string" ? data.text.trim() : "";
      if (text) {
        this.onTextInput(text);
      }
      return;
    }
    if (data?.type === "ping") {
      socket.send(JSON.stringify({ type: "pong" }));
    }
  }

  private resolveCameraFramePath(): string | null {
    const candidate = path.resolve(cameraFeedDir, "web_live.jpg");
    // Camera frames are produced by the Python camera module (camera.py/CameraThread)
    // and consumed here by web-display. This avoids direct camera device ownership in web-display.
    const safe = this.resolveSafeImagePath(candidate);
    return safe || null;
  }

  private sendCameraDaemonCommand(cmd: string): void {
    const port = parseInt(process.env.WHISPLAY_CAMERA_DAEMON_PORT || "18765", 10);
    const socket = new Socket();
    socket.setTimeout(600);
    socket.connect(port, "127.0.0.1", () => {
      socket.write(`${JSON.stringify({ cmd })}\n`);
      socket.end();
    });
    socket.on("error", () => {
      socket.destroy();
    });
    socket.on("timeout", () => {
      socket.destroy();
    });
  }

  private resolveSafeImagePath(imagePath: string): string | null {
    const resolved = path.resolve(imagePath);
    const base = path.resolve(dataDir);
    if (!resolved.startsWith(base + path.sep) && resolved !== base) {
      return null;
    }
    return resolved;
  }
}
