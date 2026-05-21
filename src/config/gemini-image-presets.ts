export type GeminiImagePresetId =
  | "none"
  | "dali-dream"
  | "melting-psychedelic"
  | "neon-hallucination"
  | "glitch-trip"
  | "retro-cosmic-poster"
  | "surreal-collage";

export interface GeminiImagePresetDefinition {
  id: GeminiImagePresetId;
  label: string;
  stylePrompt: string;
  fallbackPrompt: string;
}

export const GEMINI_IMAGE_PRESET_OPTIONS: GeminiImagePresetDefinition[] = [
  {
    id: "none",
    label: "None",
    stylePrompt: "",
    fallbackPrompt: "",
  },
  {
    id: "dali-dream",
    label: "Dali Dream",
    stylePrompt:
      "Apply a bold surrealist treatment with dreamlike symbolism, warped perspective, impossible spatial logic, elongated forms, melting details, painterly texture, and dramatic cinematic lighting. Keep the subject recognizable while pushing it into a strange, imaginative dream world.",
    fallbackPrompt:
      "Transform the image into a surreal dreamscape with melting forms, symbolic details, and painterly surrealist atmosphere while preserving the core subject.",
  },
  {
    id: "melting-psychedelic",
    label: "Melting Psychedelic",
    stylePrompt:
      "Push the scene into a vivid psychedelic transformation with liquid distortions, melting edges, organic color waves, hallucinogenic texture, high saturation, strange glow, and a warped but readable subject.",
    fallbackPrompt:
      "Turn the image into a melting psychedelic vision with flowing color, soft liquid distortion, and an intense trippy atmosphere.",
  },
  {
    id: "neon-hallucination",
    label: "Neon Hallucination",
    stylePrompt:
      "Create an electric neon hallucination look using luminous blues, magentas, violets, glowing outlines, dream-club lighting, reflective highlights, and a hyper-stylized synthetic atmosphere.",
    fallbackPrompt:
      "Restyle the image into a neon hallucination with glowing electric colors, dramatic contrast, and futuristic dream lighting.",
  },
  {
    id: "glitch-trip",
    label: "Glitch Trip",
    stylePrompt:
      "Restyle the image with a heavy glitch-art aesthetic: RGB separation, digital smearing, scan-line texture, corrupted fragments, surreal signal noise, and intentional visual instability without completely losing the subject.",
    fallbackPrompt:
      "Transform the image into a wild glitch-art trip with corrupted color channels, digital distortion, and surreal electronic chaos.",
  },
  {
    id: "retro-cosmic-poster",
    label: "Retro Cosmic Poster",
    stylePrompt:
      "Turn the image into a bold retro cosmic poster with stylized composition, vintage sci-fi poster flair, rich gradients, strong silhouette readability, celestial accents, and a punchy graphic finish.",
    fallbackPrompt:
      "Reimagine the image as a retro cosmic poster with graphic composition, space-age mood, vintage print energy, and bold color treatment.",
  },
  {
    id: "surreal-collage",
    label: "Surreal Collage",
    stylePrompt:
      "Create a surreal collage treatment with layered textures, cut-and-paste dream logic, unexpected symbolic elements, mixed media feel, and visually striking composition while keeping the original subject legible.",
    fallbackPrompt:
      "Transform the image into a surreal collage with layered mixed-media textures, unexpected symbolic additions, and dreamlike composition.",
  },
];

const PRESET_ONLY_PROMPT_PATTERNS = [
  /\bfavorite\s+(?:personal\s+)?style\b/i,
  /\byour\s+(?:favorite|personal)\s+style\b/i,
  /\bsurprise\s+me\b/i,
  /\bdo\s+your\s+thing\b/i,
  /\bwork\s+your\s+magic\b/i,
  /\bmake\s+(?:this|the|my)?\s*(?:image|photo|picture)\s+artistic\b/i,
  /^\s*edit\s+(?:this|the|that|my)?\s*(?:image|photo|picture)\s*[.!?]*$/i,
];

export function normalizeGeminiImagePreset(
  value: unknown,
): GeminiImagePresetId {
  if (
    typeof value === "string" &&
    GEMINI_IMAGE_PRESET_OPTIONS.some((option) => option.id === value)
  ) {
    return value as GeminiImagePresetId;
  }
  return "none";
}

export function getGeminiImagePresetDefinition(
  presetId: GeminiImagePresetId,
): GeminiImagePresetDefinition {
  return (
    GEMINI_IMAGE_PRESET_OPTIONS.find((option) => option.id === presetId) ||
    GEMINI_IMAGE_PRESET_OPTIONS[0]
  );
}

export function isPresetOnlyImagePrompt(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return false;
  }
  return PRESET_ONLY_PROMPT_PATTERNS.some((pattern) => pattern.test(trimmed));
}

export function buildGeminiImagePrompt(
  userPrompt: string,
  presetId: GeminiImagePresetId,
): string {
  const trimmedPrompt = userPrompt.trim();
  const preset = getGeminiImagePresetDefinition(presetId);
  if (!trimmedPrompt || preset.id === "none") {
    return trimmedPrompt;
  }

  if (isPresetOnlyImagePrompt(trimmedPrompt)) {
    return [
      `Selected preset: ${preset.label}.`,
      `Preset style directions: ${preset.stylePrompt}`,
      `Default creative edit request: ${preset.fallbackPrompt}`,
      "Apply the preset strongly while preserving the main subject and keeping the result visually coherent.",
    ].join("\n\n");
  }

  return [
    `Selected preset: ${preset.label}.`,
    `Preset style directions: ${preset.stylePrompt}`,
    `User edit request: ${trimmedPrompt}`,
    "Honor the user's direct request while expressing it through the selected preset style. Preserve the main subject unless the user explicitly asks for a major transformation.",
  ].join("\n\n");
}
