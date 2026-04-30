require("dotenv").config();
import { getRuntimeSettings } from "./runtime-settings";

export const DEFAULT_SYSTEM_PROMPT =
  "You are WhisplayGroqHat, a concise and helpful voice assistant for Raspberry Pi Zero 2 W. Speak naturally, stay practical, and keep replies short enough to feel fast on a handheld device. Use emoji sparingly and only when they add clarity.";

const wakeWordEnabled =
  (process.env.WAKE_WORD_ENABLED || "").toLowerCase() === "true";

const wakeWordConversationToolPrompt = wakeWordEnabled
  ? " If the endConversation tool is available and the user clearly wants to end the current conversation, call that tool before giving your brief final reply."
  : "";

// default 5 minutes
export const CHAT_HISTORY_RESET_TIME = parseInt(process.env.CHAT_HISTORY_RESET_TIME || "300" , 10) * 1000; // convert to milliseconds

export let lastMessageTime = 0;

export const updateLastMessageTime = (): void => {
  lastMessageTime = Date.now();
}

export const shouldResetChatHistory = (): boolean => {
  return Date.now() - lastMessageTime > CHAT_HISTORY_RESET_TIME;
}

export const getSystemPrompt = (): string => {
  const runtimePrompt = getRuntimeSettings().personalityPrompt;
  const baseSystemPrompt =
    runtimePrompt || process.env.SYSTEM_PROMPT || DEFAULT_SYSTEM_PROMPT;
  return `${baseSystemPrompt}${wakeWordConversationToolPrompt}`;
};

export const systemPrompt = getSystemPrompt();
