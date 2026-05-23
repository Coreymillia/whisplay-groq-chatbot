import {
  BOTNET_MODEL_OPTIONS,
  type BotNetModelOption,
} from "./botnet-models";

export type TextLlmProvider = "openai-compatible" | "gemini";

export interface TextLlmModelOption extends BotNetModelOption {
  provider: TextLlmProvider;
}

export const GEMINI_TEXT_MODEL_OPTIONS: TextLlmModelOption[] = [
  {
    id: "gemini-2.5-flash",
    label: "Gemini 2.5 Flash",
    shortLabel: "G2.5",
    provider: "gemini",
  },
  {
    id: "gemini-2.5-flash-lite",
    label: "Gemini 2.5 Flash-Lite",
    shortLabel: "G-Lite",
    provider: "gemini",
  },
  {
    id: "gemini-2.5-pro",
    label: "Gemini 2.5 Pro",
    shortLabel: "G-Pro",
    provider: "gemini",
  },
];

export const TEXT_LLM_MODEL_OPTIONS: TextLlmModelOption[] = [
  ...BOTNET_MODEL_OPTIONS.map((option) => ({
    ...option,
    provider: "openai-compatible" as const,
  })),
  ...GEMINI_TEXT_MODEL_OPTIONS,
];

export const DEFAULT_TEXT_LLM_MODEL = TEXT_LLM_MODEL_OPTIONS[0].id;

export function normalizeTextLlmModel(value: unknown): string {
  if (typeof value !== "string") {
    return DEFAULT_TEXT_LLM_MODEL;
  }
  const trimmed = value.trim();
  if (!trimmed) {
    return DEFAULT_TEXT_LLM_MODEL;
  }
  return (
    TEXT_LLM_MODEL_OPTIONS.find((option) => option.id === trimmed)?.id ||
    DEFAULT_TEXT_LLM_MODEL
  );
}

export function getTextLlmModelOption(value: string): TextLlmModelOption | null {
  return (
    TEXT_LLM_MODEL_OPTIONS.find(
      (option) => option.id === normalizeTextLlmModel(value),
    ) || null
  );
}

export function getTextLlmModelLabel(value: string): string {
  return getTextLlmModelOption(value)?.label || DEFAULT_TEXT_LLM_MODEL;
}

export function getTextLlmProvider(value: string): TextLlmProvider {
  return getTextLlmModelOption(value)?.provider || "openai-compatible";
}

export function isGeminiTextLlmModel(value: string): boolean {
  return getTextLlmProvider(value) === "gemini";
}
