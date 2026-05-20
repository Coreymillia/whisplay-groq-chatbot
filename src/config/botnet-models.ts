export interface BotNetModelOption {
  id: string;
  label: string;
}

export const BOTNET_MODEL_OPTIONS: BotNetModelOption[] = [
  {
    id: "llama-3.1-8b-instant",
    label: "Llama 3.1 8B Instant",
  },
  {
    id: "llama-3.3-70b-versatile",
    label: "Llama 3.3 70B Versatile",
  },
  {
    id: "meta-llama/llama-4-scout-17b-16e-instruct",
    label: "Llama 4 Scout 17B 16E",
  },
  {
    id: "qwen/qwen3-32b",
    label: "Qwen 3 32B",
  },
  {
    id: "groq/compound",
    label: "Groq Compound",
  },
  {
    id: "groq/compound-mini",
    label: "Groq Compound Mini",
  },
  {
    id: "openai/gpt-oss-20b",
    label: "GPT-OSS 20B",
  },
  {
    id: "openai/gpt-oss-120b",
    label: "GPT-OSS 120B",
  },
];

export const DEFAULT_BOTNET_MODEL = BOTNET_MODEL_OPTIONS[0].id;

export function normalizeBotNetModel(value: unknown): string {
  if (typeof value !== "string") {
    return DEFAULT_BOTNET_MODEL;
  }
  const trimmed = value.trim();
  if (!trimmed) {
    return DEFAULT_BOTNET_MODEL;
  }
  return (
    BOTNET_MODEL_OPTIONS.find((option) => option.id === trimmed)?.id ||
    DEFAULT_BOTNET_MODEL
  );
}

export function getBotNetModelOption(value: string): BotNetModelOption | null {
  return (
    BOTNET_MODEL_OPTIONS.find((option) => option.id === normalizeBotNetModel(value)) ||
    null
  );
}

export function getBotNetModelLabel(value: string): string {
  return getBotNetModelOption(value)?.label || DEFAULT_BOTNET_MODEL;
}

export function getNextBotNetModel(value: string): string {
  const currentId = normalizeBotNetModel(value);
  const currentIndex = BOTNET_MODEL_OPTIONS.findIndex(
    (option) => option.id === currentId,
  );
  if (currentIndex === -1) {
    return DEFAULT_BOTNET_MODEL;
  }
  return BOTNET_MODEL_OPTIONS[
    (currentIndex + 1) % BOTNET_MODEL_OPTIONS.length
  ].id;
}
