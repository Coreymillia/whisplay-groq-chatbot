export interface BotNetModelOption {
  id: string;
  label: string;
  shortLabel?: string;
  rateLimits?: {
    rpm?: number;
    rpd?: number;
    tpm?: number;
    tpd?: number | null;
  };
}

export const BOTNET_MODEL_OPTIONS: BotNetModelOption[] = [
  {
    id: "llama-3.1-8b-instant",
    label: "Llama 3.1 8B Instant",
    shortLabel: "L3.1-8B",
    rateLimits: { rpm: 30, rpd: 14400, tpm: 6000, tpd: 500000 },
  },
  {
    id: "llama-3.3-70b-versatile",
    label: "Llama 3.3 70B Versatile",
    shortLabel: "L3.3-70B",
    rateLimits: { rpm: 30, rpd: 1000, tpm: 12000, tpd: 100000 },
  },
  {
    id: "meta-llama/llama-4-scout-17b-16e-instruct",
    label: "Llama 4 Scout 17B 16E",
    shortLabel: "L4-Scout",
    rateLimits: { rpm: 30, rpd: 1000, tpm: 30000, tpd: 500000 },
  },
  {
    id: "qwen/qwen3-32b",
    label: "Qwen 3 32B",
    shortLabel: "Qwen3-32B",
    rateLimits: { rpm: 60, rpd: 1000, tpm: 6000, tpd: 500000 },
  },
  {
    id: "groq/compound",
    label: "Groq Compound",
    shortLabel: "Compound",
    rateLimits: { rpm: 30, rpd: 250, tpm: 70000, tpd: null },
  },
  {
    id: "groq/compound-mini",
    label: "Groq Compound Mini",
    shortLabel: "Cmpd Mini",
    rateLimits: { rpm: 30, rpd: 250, tpm: 70000, tpd: null },
  },
  {
    id: "openai/gpt-oss-20b",
    label: "GPT-OSS 20B",
    shortLabel: "GPT-20B",
    rateLimits: { rpm: 30, rpd: 1000, tpm: 8000, tpd: 200000 },
  },
  {
    id: "openai/gpt-oss-120b",
    label: "GPT-OSS 120B",
    shortLabel: "GPT-120B",
    rateLimits: { rpm: 30, rpd: 1000, tpm: 8000, tpd: 200000 },
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
