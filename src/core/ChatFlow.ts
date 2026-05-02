import {
  getCurrentTimeTag,
  getRecordFileDurationMs,
  splitSentences,
} from "./../utils/index";
import { display } from "../device/display";
import { recognizeAudio, ttsProcessor } from "../cloud-api/server";
import { isImMode } from "../cloud-api/llm";
import { DEFAULT_EMOJI, extractEmojis } from "../utils";
import { StreamResponser } from "./StreamResponsor";
import { recordingsDir } from "../utils/dir";
import dotEnv from "dotenv";
import { WakeWordListener } from "../device/wakeword";
import { WhisplayIMBridgeServer } from "../device/im-bridge";
import { FlowStateMachine } from "./chat-flow/stateMachine";
import { flowStates } from "./chat-flow/states";
import { ChatFlowContext, FlowName } from "./chat-flow/types";
import { playWakeupChime } from "../device/audio";
import { stopMusicPlayback, isMusicPlaying } from "../device/music-player";
import type { Status } from "../device/display";
import { getRuntimeSettings } from "../config/runtime-settings";
import { STATE_EMOJIS } from "../config/state-emojis";
import { requestSystemShutdown } from "../device/system-control";
import {
  applySettingsMenuAction,
  buildSettingsMenuItems,
  renderSettingsMenu as renderSettingsMenuText,
} from "./chat-flow/settings-menu";

dotEnv.config();

class ChatFlow implements ChatFlowContext {
  currentFlowName: FlowName = "sleep";
  recordingsDir: string = "";
  currentRecordFilePath: string = "";
  asrText: string = "";
  streamResponser: StreamResponser;
  partialThinking: string = "";
  thinkingSentences: string[] = [];
  answerId: number = 0;
  enableCamera: boolean = false;
  knowledgePrompts: string[] = [];
  wakeWordListener: WakeWordListener | null = null;
  wakeSessionActive: boolean = false;
  wakeSessionStartAt: number = 0;
  wakeSessionLastSpeechAt: number = 0;
  wakeSessionIdleTimeoutMs: number =
    parseInt(process.env.WAKE_WORD_IDLE_TIMEOUT_SEC || "60") * 1000;
  wakeRecordMaxSec: number = parseInt(
    process.env.WAKE_WORD_RECORD_MAX_SEC || "60",
  );
  wakeEndKeywords: string[] = (process.env.WAKE_WORD_END_KEYWORDS || "byebye,goodbye,stop,byebye").toLowerCase()
    .split(",")
    .map((item) => item.trim().toLowerCase())
    .filter((item) => item.length > 0);
  endAfterAnswer: boolean = false;
  whisplayIMBridge: WhisplayIMBridgeServer | null = null;
  pendingExternalReply: string = "";
  pendingExternalEmoji: string = "";
  pendingExternalImageUrl: string = "";
  currentExternalEmoji: string = "";
  stateMachine: FlowStateMachine;
  isFromWakeListening: boolean = false;
  enterMusicAfterAnswer: boolean = false;
  musicDisplayText: string = "";
  settingsMenuIndex: number = 0;
  lastAnswerText: string = "";
  lastAnswerEmoji: string = STATE_EMOJIS.answering;
  lastAnswerImage: string = "";
  private ignoreNextSettingsRelease = false;
  private shutdownConfirmArmed = false;

  constructor(options: { enableCamera?: boolean } = {}) {
    console.log(`[${getCurrentTimeTag()}] ChatBot started.`);
    this.recordingsDir = recordingsDir;
    this.stateMachine = new FlowStateMachine(this, flowStates);
    this.streamResponser = new StreamResponser(
      ttsProcessor,
      (sentences: string[]) => {
        if (!this.isAnswerFlow()) return;
        const fullText = sentences.join(" ");
        const emoji =
          this.currentFlowName === "external_answer"
            ? this.currentExternalEmoji || STATE_EMOJIS.answering
            : STATE_EMOJIS.answering;
        display({
          status: "answering",
          emoji,
          text: fullText,
          RGB: "#0000ff",
          scroll_speed: 3,
        });
      },
      (text: string) => {
        if (!this.isAnswerFlow()) return;
        display({
          status: "answering",
          text: text || undefined,
          scroll_speed: 3,
        });
      },
      ({ charEnd, durationMs }) => {
        if (!this.isAnswerFlow()) return;
        if (!durationMs || durationMs <= 0) return;
        display({
          scroll_sync: {
            char_end: charEnd,
            duration_ms: durationMs,
          },
        });
      }
    );
    if (options?.enableCamera) {
      this.enableCamera = true;
    }

    this.transitionTo("sleep");

    const wakeEnabled = (process.env.WAKE_WORD_ENABLED || "").toLowerCase();
    if (wakeEnabled === "true") {
      this.wakeWordListener = new WakeWordListener();
      this.wakeWordListener.on("wake", () => {
        if (this.currentFlowName === "sleep") {
          this.startWakeSession();
        }
      });
      this.wakeWordListener.start();
    }

    if (isImMode) {
      this.whisplayIMBridge = new WhisplayIMBridgeServer();
      this.whisplayIMBridge.on(
        "reply",
        (payload: { reply: string; emoji?: string; imagePath?: string }) => {
          this.pendingExternalReply = payload.reply;
          this.pendingExternalEmoji = payload.emoji || "";
          this.pendingExternalImageUrl = payload.imagePath || "";
          this.transitionTo("external_answer");
        },
      );
      this.whisplayIMBridge.on(
        "status",
        (payload: { status: string; emoji?: string; text?: string; tool?: string }) => {
          const statusText = payload.tool
            ? `[${payload.tool}] ${payload.text || ""}`
            : payload.text || "";
          const textInputEnabled =
            payload.status === "idle" && this.currentFlowName === "sleep";
          const statusMap: Record<string, Partial<Status>> = {
            thinking: {
              status: "Thinking",
              emoji: payload.emoji || STATE_EMOJIS.thinking,
              text: statusText,
              RGB: "#ff6800",
              scroll_speed: 6,
              text_input_enabled: false,
            },
            tool_calling: {
              status: "Tool calling",
              emoji: payload.emoji || STATE_EMOJIS.tool,
              text: statusText,
              RGB: "#ff6800",
              scroll_speed: 4,
              text_input_enabled: false,
            },
            answering: {
              status: "answering...",
              emoji: payload.emoji || STATE_EMOJIS.answering,
              RGB: "#00c8a3",
              text_input_enabled: false,
            },
            idle: {
              status: "idle",
              emoji: payload.emoji || STATE_EMOJIS.externalIdle,
              RGB: "#000055",
              text_input_enabled: textInputEnabled,
            },
          };
          const displayPayload = statusMap[payload.status] || {
            status: payload.status,
            emoji: payload.emoji || STATE_EMOJIS.thinking,
            text: statusText,
            RGB: "#ff6800",
            text_input_enabled: false,
          };
          display(displayPayload);
        },
      );
      this.whisplayIMBridge.start();
    }
  }

  async recognizeAudio(path: string, isFromAutoListening?: boolean): Promise<string> {
    if (!isFromAutoListening && (await getRecordFileDurationMs(path)) < 500) {
      console.log("Record audio too short, skipping recognition.");
      return Promise.resolve("");
    }
    console.time(`[ASR time]`);
    const result = await recognizeAudio(path);
    console.timeEnd(`[ASR time]`);
    return result;
  }

  partialThinkingCallback = (partialThinking: string): void => {
    this.partialThinking += partialThinking;
    const { sentences, remaining } = splitSentences(this.partialThinking);
    if (sentences.length > 0) {
      this.thinkingSentences.push(...sentences);
      const displayText = this.thinkingSentences.join(" ");
      display({
        status: "Thinking",
        emoji: STATE_EMOJIS.thinking,
        text: displayText,
        RGB: "#ff6800", // yellow
        scroll_speed: 6,
      });
    }
    this.partialThinking = remaining;
  };

  transitionTo = (flowName: FlowName): void => {
    if (flowName !== "music" && isMusicPlaying()) {
      stopMusicPlayback();
    }
    console.log(`[${getCurrentTimeTag()}] switch to:`, flowName);
    this.stateMachine.transitionTo(flowName);
    display({ text_input_enabled: flowName === "sleep" });
  };

  isAnswerFlow = (): boolean => {
    return (
      this.currentFlowName === "answer" ||
      this.currentFlowName === "external_answer"
    );
  };

  streamExternalReply = async (text: string, emoji?: string): Promise<void> => {
    if (!text) {
      this.streamResponser.endPartial();
      return;
    }
    if (emoji) {
      display({
        status: "answering",
        emoji,
        scroll_speed: 3,
      });
    }
    const { sentences, remaining } = splitSentences(text);
    const parts = [...sentences];
    if (remaining.trim()) {
      parts.push(remaining);
    }
    for (const part of parts) {
      this.streamResponser.partial(part);
      await new Promise((resolve) => setTimeout(resolve, 120));
    }
    this.streamResponser.endPartial();
  };

  startWakeSession = (): void => {
    this.wakeSessionActive = true;
    this.wakeSessionStartAt = Date.now();
    this.wakeSessionLastSpeechAt = this.wakeSessionStartAt;
    this.endAfterAnswer = false;
    playWakeupChime();
    this.transitionTo("wake_listening");
  };

  endWakeSession = (): void => {
    this.wakeSessionActive = false;
    this.endAfterAnswer = false;
  };

  shouldContinueWakeSession = (): boolean => {
    if (!this.wakeSessionActive) return false;
    const last = this.wakeSessionLastSpeechAt || this.wakeSessionStartAt;
    return Date.now() - last < this.wakeSessionIdleTimeoutMs;
  };

  shouldEndAfterAnswer = (text: string): boolean => {
    const lower = text.toLowerCase();
    return this.wakeEndKeywords.some(
      (keyword) => keyword && lower.includes(keyword),
    );
  };

  shouldOpenSettingsMenu = (text: string): boolean => {
    const lower = text.trim().toLowerCase();
    return (
      lower === "settings" ||
      lower.includes("open settings") ||
      lower.includes("settings menu")
    );
  };

  getManualRecordMaxSec = (): number => {
    return getRuntimeSettings().manualRecordMaxSec;
  };

  openSettingsMenu = (ignoreNextRelease: boolean = false): void => {
    this.answerId += 1;
    this.streamResponser.stop();
    stopMusicPlayback();
    this.endWakeSession();
    this.settingsMenuIndex = 0;
    this.shutdownConfirmArmed = false;
    this.ignoreNextSettingsRelease = ignoreNextRelease;
    display({
      image: "",
      image_icon_visible: false,
      rag_icon_visible: false,
    });
    this.transitionTo("settings");
  };

  closeSettingsMenu = (): void => {
    this.shutdownConfirmArmed = false;
    this.transitionTo("sleep");
  };

  renderSettingsMenu = (message: string = ""): void => {
    const runtimeSettings = getRuntimeSettings();
    display({
      status: "settings",
      emoji: STATE_EMOJIS.settings,
      RGB: "#6048ff",
      text: renderSettingsMenuText(this.settingsMenuIndex, message),
      text_input_enabled: false,
      rag_icon_visible: false,
      image_icon_visible: false,
      image: "",
      header_mode: runtimeSettings.headerMode,
      screensaver_mode: runtimeSettings.screensaverMode,
      idle_timeout_sec: runtimeSettings.idleTimeoutSec,
    });
  };

  moveSettingsSelection = (): void => {
    const items = buildSettingsMenuItems();
    this.settingsMenuIndex = (this.settingsMenuIndex + 1) % items.length;
    this.shutdownConfirmArmed = false;
    this.renderSettingsMenu();
  };

  activateSettingsSelection = (): void => {
    const items = buildSettingsMenuItems();
    const selected = items[this.settingsMenuIndex];
    if (selected.id === "shutdown") {
      if (!this.shutdownConfirmArmed) {
        this.shutdownConfirmArmed = true;
        this.renderSettingsMenu("Hold again to shut down");
        return;
      }
      this.shutdownConfirmArmed = false;
      void this.shutdownDevice();
      return;
    }
    this.shutdownConfirmArmed = false;
    const result = applySettingsMenuAction(selected.id);
    if (result.shouldExit) {
      this.closeSettingsMenu();
      return;
    }
    this.renderSettingsMenu(result.message);
  };

  shutdownDevice = async (): Promise<void> => {
    const runtimeSettings = getRuntimeSettings();
    display({
      status: "shutdown",
      emoji: "⏻",
      RGB: "#ff6b00",
      text: "Powering off...",
      text_input_enabled: false,
      rag_icon_visible: false,
      image_icon_visible: false,
      image: "",
      header_mode: runtimeSettings.headerMode,
      screensaver_mode: runtimeSettings.screensaverMode,
      idle_timeout_sec: runtimeSettings.idleTimeoutSec,
    });

    try {
      await requestSystemShutdown();
    } catch (error) {
      console.error("Shutdown request failed:", error);
      this.renderSettingsMenu("Shutdown failed");
    }
  };

  consumeSettingsReleaseGuard = (): boolean => {
    const shouldIgnore = this.ignoreNextSettingsRelease;
    this.ignoreNextSettingsRelease = false;
    return shouldIgnore;
  };

  rememberLastAnswer = ({
    text,
    emoji,
    image,
  }: {
    text: string;
    emoji?: string;
    image?: string;
  }): void => {
    this.lastAnswerText = text.trim();
    this.lastAnswerEmoji = emoji || STATE_EMOJIS.answering;
    this.lastAnswerImage = image || "";
  };

  hasLastAnswer = (): boolean => {
    return Boolean(this.lastAnswerText || this.lastAnswerImage);
  };

  replayLastAnswer = (): void => {
    if (!this.hasLastAnswer()) {
      return;
    }
    display({
      status: "last reply",
      emoji: this.lastAnswerEmoji || STATE_EMOJIS.answering,
      text: this.lastAnswerText || undefined,
      image: this.lastAnswerImage || "",
      image_icon_visible: Boolean(this.lastAnswerImage),
      RGB: "#00c8a3",
      scroll_speed: 3,
      text_input_enabled: true,
    });
  };
}

export default ChatFlow;
