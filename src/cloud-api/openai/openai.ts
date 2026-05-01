import { OpenAI, ClientOptions } from "openai";
import { proxyFetch } from "../proxy-fetch";
import dotenv from "dotenv";
import { getRuntimeSettings } from "../../config/runtime-settings";

dotenv.config();

const openAiBaseURL = process.env.OPENAI_API_BASE_URL;

export const getOpenAILLMModel = (): string =>
  process.env.OPENAI_LLM_MODEL || "gpt-4o";

export const getOpenAIVisionModel = (): string =>
  process.env.OPENAI_VISION_MODEL || process.env.OPENAI_LLM_MODEL || "gpt-4o";

export const getOpenAIImageModel = (): string =>
  process.env.OPENAI_IMAGE_MODEL || "dall-e-3";

export const getOpenAIVoiceModel = (): string =>
  process.env.OPENAI_VOICE_MODEL || "tts-1";

export const getOpenAIVoiceType = (): string =>
  process.env.OPENAI_VOICE_TYPE || "nova";

export const getOpenAIASRModel = (): string => {
  if (process.env.OPENAI_ASR_MODEL) {
    return process.env.OPENAI_ASR_MODEL;
  }
  if ((openAiBaseURL || "").includes("api.groq.com")) {
    return "whisper-large-v3-turbo";
  }
  return "whisper-1";
};

export const getOpenAIApiKey = (): string => {
  const runtimeSettings = getRuntimeSettings();
  return runtimeSettings.groqApiKey || process.env.OPENAI_API_KEY || "";
};

export const getOpenAIClient = (): OpenAI | null => {
  const apiKey = getOpenAIApiKey();
  if (!apiKey) {
    return null;
  }

  const openAiOptions: ClientOptions = {
    apiKey,
    fetch: proxyFetch as any,
  };

  if (openAiBaseURL) {
    Object.assign(openAiOptions, { baseURL: openAiBaseURL });
  }

  return new OpenAI(openAiOptions);
};

export const openaiLLMModel = getOpenAILLMModel();
export const openaiVisionModel = getOpenAIVisionModel();
export const openaiImageModel = getOpenAIImageModel();
export const openai = getOpenAIClient();
