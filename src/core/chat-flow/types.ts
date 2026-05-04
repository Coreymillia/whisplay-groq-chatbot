import { StreamResponser } from "../StreamResponsor";

export type FlowName =
  | "sleep"
  | "camera"
  | "photo_browser"
  | "voice_command_help"
  | "music"
  | "listening"
  | "settings"
  | "wake_listening"
  | "asr"
  | "answer"
  | "image"
  | "external_answer";

export type FlowStateHandler = (ctx: ChatFlowContext) => void;

export interface ChatFlowContext {
  currentFlowName: FlowName;
  recordingsDir: string;
  currentRecordFilePath: string;
  asrText: string;
  streamResponser: StreamResponser;
  partialThinking: string;
  thinkingSentences: string[];
  answerId: number;
  enableCamera: boolean;
  knowledgePrompts: string[];
  wakeSessionActive: boolean;
  wakeSessionStartAt: number;
  wakeSessionLastSpeechAt: number;
  wakeSessionIdleTimeoutMs: number;
  wakeRecordMaxSec: number;
  wakeEndKeywords: string[];
  endAfterAnswer: boolean;
  pendingExternalReply: string;
  pendingExternalEmoji: string;
  pendingExternalImageUrl: string;
  pendingExternalForceSpeech: boolean;
  currentExternalEmoji: string;
  isFromWakeListening: boolean;
  enterMusicAfterAnswer: boolean;
  musicDisplayText: string;
  settingsMenuIndex: number;
  lastAnswerText: string;
  lastAnswerEmoji: string;
  lastAnswerImage: string;

  transitionTo: (flowName: FlowName) => void;
  recognizeAudio: (path: string, isFromAutoListening?: boolean) => Promise<string>;
  partialThinkingCallback: (partialThinking: string) => void;
  startWakeSession: () => void;
  endWakeSession: () => void;
  shouldContinueWakeSession: () => boolean;
  shouldEndAfterAnswer: (text: string) => boolean;
  shouldOpenSettingsMenu: (text: string) => boolean;
  shutdownDevice: () => Promise<void>;
  streamExternalReply: (
    text: string,
    emoji?: string,
    forceSpeech?: boolean,
  ) => Promise<void>;
  openSettingsMenu: (ignoreNextRelease?: boolean) => void;
  closeSettingsMenu: () => void;
  renderSettingsMenu: (message?: string) => void;
  moveSettingsSelection: () => void;
  activateSettingsSelection: () => void;
  consumeSettingsReleaseGuard: () => boolean;
  getManualRecordMaxSec: () => number;
  rememberLastAnswer: (payload: {
    text: string;
    emoji?: string;
    image?: string;
  }) => void;
  hasLastAnswer: () => boolean;
  replayLastAnswer: () => void;
  repeatLastAnswerAloud: () => void;
}
