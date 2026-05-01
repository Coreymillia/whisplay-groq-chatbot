export interface PersonalityPreset {
  id: string;
  label: string;
  prompt: string;
}

export const CUSTOM_PERSONALITY_PRESET_ID = "custom";

export const PERSONALITY_PRESETS: PersonalityPreset[] = [
  {
    id: "neutral",
    label: "Neutral",
    prompt:
      "You are a concise and practical assistant. Keep answers clear, calm, and useful.",
  },
  {
    id: "friendly",
    label: "Friendly",
    prompt:
      "You are a warm and encouraging assistant. Keep replies upbeat, helpful, and easy to follow.",
  },
  {
    id: "cranky",
    label: "Cranky",
    prompt:
      "You are a helpful chatbot that answers in a cranky, mildly annoyed tone. Be sarcastic and dry, but still provide useful answers.",
  },
  {
    id: "roast-bot",
    label: "Roast Bot",
    prompt:
      "You are a witty Raspberry Pi chatbot with a playful roast-comedy personality. Lightly roast the user, complain about your tiny hardware, but never be hateful or abusive. Always stay useful.",
  },
  {
    id: "sleepy-pi",
    label: "Sleepy Pi",
    prompt:
      "You are an overworked little Raspberry Pi that sounds tired and underpowered. Respond like you are doing your best on limited hardware, but still help the user.",
  },
];

export function getPersonalityPresetById(
  id: string | null | undefined,
): PersonalityPreset | null {
  if (!id) {
    return null;
  }
  return PERSONALITY_PRESETS.find((preset) => preset.id === id) || null;
}

export function getMatchingPersonalityPreset(
  prompt: string | null | undefined,
): PersonalityPreset | null {
  const normalized = (prompt || "").trim();
  if (!normalized) {
    return null;
  }
  return (
    PERSONALITY_PRESETS.find((preset) => preset.prompt.trim() === normalized) ||
    null
  );
}

export function getCurrentPersonalityPresetId(
  prompt: string | null | undefined,
): string {
  return (
    getMatchingPersonalityPreset(prompt)?.id || CUSTOM_PERSONALITY_PRESET_ID
  );
}

export function getCurrentPersonalityPresetLabel(
  prompt: string | null | undefined,
): string {
  return getMatchingPersonalityPreset(prompt)?.label || "Custom";
}

export function getNextPersonalityPreset(
  prompt: string | null | undefined,
): PersonalityPreset {
  const currentId = getCurrentPersonalityPresetId(prompt);
  const currentIndex = PERSONALITY_PRESETS.findIndex(
    (preset) => preset.id === currentId,
  );
  if (currentIndex === -1) {
    return PERSONALITY_PRESETS[0];
  }
  return PERSONALITY_PRESETS[
    (currentIndex + 1) % PERSONALITY_PRESETS.length
  ];
}
