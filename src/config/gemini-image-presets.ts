export type GeminiImagePresetId =
  | "none"
  | "dali-dream"
  | "melting-psychedelic"
  | "neon-hallucination"
  | "glitch-trip"
  | "retro-cosmic-poster"
  | "surreal-collage"
  | "biomechanical-growth"
  | "cyberpunk-noir-1980s"
  | "tech-blueprint"
  | "haunted-daguerreotype"
  | "bas-relief-stone-carving"
  | "van-gogh"
  | "picasso"
  | "stencil-street-art"
  | "visionary-psychedelic";

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
  {
    id: "biomechanical-growth",
    label: "Biomechanical Growth",
    stylePrompt:
      "Edit the photo into a dark biomechanical horror scene. Preserve the original composition and recognizable subject shapes, but merge them with intricate organic-mechanical textures: ribbed tubing, tendon-like cables, obsidian bone structures, metallic joints, and wet industrial surfaces. Use moody low-key lighting, subtle reflections, and a heavy atmospheric shadowed tone.",
    fallbackPrompt:
      "Transform the image into a dark biomechanical world with organic machinery, tendon-like cables, bone-like structures, and moody cinematic shadows while keeping the subject recognizable.",
  },
  {
    id: "cyberpunk-noir-1980s",
    label: "Cyberpunk Noir 1980s",
    stylePrompt:
      "Reimagine the photo as 1980s cyberpunk noir. Keep the original scene recognizable, but add deep cobalt-blue shadows, crimson neon rim lighting, glossy rain-slick reflections, soft atmospheric haze, and dramatic high-contrast night lighting like a retro futuristic city thriller.",
    fallbackPrompt:
      "Restyle the image into 1980s cyberpunk noir with neon reflections, deep blue and red lighting, glossy surfaces, and moody futuristic night atmosphere.",
  },
  {
    id: "tech-blueprint",
    label: "Tech Blueprint",
    stylePrompt:
      "Convert the image into a clean technical blueprint. Preserve the exact layout and major shapes from the original photo, but render everything as crisp white schematic line-art on a deep cyan-blue drafting background with subtle grid lines, cross-hatching, and diagram-style detail.",
    fallbackPrompt:
      "Turn the image into a technical blueprint with clean white schematic lines, blueprint-blue background, grid detail, and precise diagram styling.",
  },
  {
    id: "haunted-daguerreotype",
    label: "Haunted Daguerreotype",
    stylePrompt:
      "Edit the photo into a haunted 19th-century daguerreotype. Use monochrome sepia tones, strong contrast, soft dark edge vignetting, antique photographic texture, fine scratches, silver-like tarnish, and faint ghostly motion blur. Keep the original subject recognizable but make the image feel eerie, aged, and chemically distressed.",
    fallbackPrompt:
      "Transform the image into a haunted antique daguerreotype with sepia tones, scratches, tarnish, ghostly blur, and eerie historical atmosphere.",
  },
  {
    id: "bas-relief-stone-carving",
    label: "Bas-Relief Stone Carving",
    stylePrompt:
      "Transform the scene into an ancient stone bas-relief carving. Preserve the original composition, but make all subjects and objects appear sculpted directly from weathered gray limestone with chiseled edges, worn texture, and deep side lighting that emphasizes carved depth and shadow.",
    fallbackPrompt:
      "Restyle the image as an ancient stone bas-relief with carved limestone texture, chiselled depth, and dramatic side lighting.",
  },
  {
    id: "van-gogh",
    label: "Van Gogh",
    stylePrompt:
      "Restyle the photo with a dramatic post-impressionist oil painting treatment inspired by expressive brushwork, swirling motion, luminous moonlit color energy, vivid cobalt and golden tones, textured paint buildup, and emotionally charged lighting. Preserve the original subject and composition while translating it into a richly painted scene.",
    fallbackPrompt:
      "Transform the image into a vivid post-impressionist oil painting with swirling brushwork, bold texture, glowing night-sky color energy, and expressive painterly motion while keeping the subject recognizable.",
  },
  {
    id: "picasso",
    label: "Picasso",
    stylePrompt:
      "Reimagine the photo as a bold cubist painting with fractured planes, geometric simplification, angular forms, flattened perspective, and a confident modernist palette. Preserve the core subject and main composition, but break the scene into stylized overlapping shapes and strong painterly structure.",
    fallbackPrompt:
      "Turn the image into a striking cubist artwork with geometric planes, abstracted facial or object structure, flattened depth, and expressive modernist color.",
  },
  {
    id: "stencil-street-art",
    label: "Stencil Street Art",
    stylePrompt:
      "Edit the photo into gritty stencil-driven street art. Keep the original subject readable, but simplify it into bold cutout silhouettes, sharp sprayed edges, limited high-contrast color blocks, weathered wall texture, urban grime, layered poster remnants, and rebellious mural energy.",
    fallbackPrompt:
      "Transform the image into high-contrast stencil street art with spray-paint texture, distressed wall surfaces, and bold mural-like urban attitude.",
  },
  {
    id: "visionary-psychedelic",
    label: "Visionary Psychedelic",
    stylePrompt:
      "Transform the image into a visionary psychedelic scene with luminous anatomy-like linework, sacred geometry, radiant energy fields, spiritual symmetry, translucent layered forms, prismatic color gradients, and intricate consciousness-themed detail. Keep the original subject recognizable while elevating it into a vivid metaphysical artwork.",
    fallbackPrompt:
      "Restyle the image into visionary psychedelic art with glowing sacred geometry, radiant inner light, intricate spiritual detail, and layered cosmic color.",
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
