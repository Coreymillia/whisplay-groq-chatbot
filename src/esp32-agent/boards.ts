export type Esp32AgentChipFamily = "esp32" | "esp32c3" | "esp32s3" | "unknown";

export interface Esp32AgentBoardDefinition {
  id: string;
  label: string;
  chipFamily: Esp32AgentChipFamily;
  description: string;
}

export interface Esp32AgentBoardSummary extends Esp32AgentBoardDefinition {
  isKnown: true;
}

export interface Esp32AgentResolvedBoard extends Esp32AgentBoardDefinition {
  isKnown: boolean;
}

export const DEFAULT_ESP32_AGENT_BOARD_ID = "esp32dev";

export const ESP32_AGENT_BOARD_DEFINITIONS: Esp32AgentBoardDefinition[] = [
  {
    id: "esp32dev",
    label: "ESP32 DevKit / WROOM / CYD base board",
    chipFamily: "esp32",
    description:
      "Generic classic ESP32 DevKitC / WROOM boards, including CYD-style display boards.",
  },
  {
    id: "esp32cam",
    label: "ESP32-CAM (AI Thinker style)",
    chipFamily: "esp32",
    description:
      "Camera-focused ESP32 boards that usually use AI Thinker style pin mappings.",
  },
  {
    id: "esp32c3dev",
    label: "ESP32-C3 DevKitM-1 / generic C3 dev board",
    chipFamily: "esp32c3",
    description:
      "Common RISC-V ESP32-C3 development boards and close generic equivalents.",
  },
  {
    id: "esp32-s3-devkitc-1",
    label: "ESP32-S3 DevKitC-1",
    chipFamily: "esp32s3",
    description:
      "Standard Espressif ESP32-S3 DevKitC-1 style boards for USB-native S3 projects.",
  },
  {
    id: "m5stack-core2",
    label: "M5Stack Core2",
    chipFamily: "esp32",
    description:
      "M5Stack Core2 touch display hardware. Use with a Core2-specific starter, not CYD display code.",
  },
  {
    id: "adafruit_feather_esp32s3_tft",
    label: "Adafruit Feather ESP32-S3 TFT",
    chipFamily: "esp32s3",
    description:
      "Adafruit Feather ESP32-S3 TFT boards with a built-in display and USB-native flashing.",
  },
];

const BOARD_BY_ID = new Map(
  ESP32_AGENT_BOARD_DEFINITIONS.map((entry) => [entry.id, entry]),
);

const CUSTOM_BOARD_ID_PATTERN = /^[A-Za-z0-9._-]+$/;

export function listEsp32AgentBoards(): Esp32AgentBoardSummary[] {
  return ESP32_AGENT_BOARD_DEFINITIONS.map((entry) => ({
    ...entry,
    isKnown: true as const,
  }));
}

export function getEsp32AgentBoardById(
  boardId: string,
): Esp32AgentBoardDefinition | null {
  return BOARD_BY_ID.get(boardId) || null;
}

export function isKnownEsp32AgentBoardId(boardId: string): boolean {
  return BOARD_BY_ID.has(boardId);
}

export function resolveEsp32AgentBoardSelection(
  value: unknown,
  fallbackBoardId = DEFAULT_ESP32_AGENT_BOARD_ID,
): Esp32AgentResolvedBoard {
  const rawValue = typeof value === "string" ? value.trim() : "";
  const knownBoard = rawValue ? getEsp32AgentBoardById(rawValue) : null;
  if (knownBoard) {
    return {
      ...knownBoard,
      isKnown: true,
    };
  }

  if (rawValue && CUSTOM_BOARD_ID_PATTERN.test(rawValue)) {
    return {
      id: rawValue,
      label: `Custom PlatformIO board (${rawValue})`,
      chipFamily: "unknown",
      description:
        "Custom PlatformIO board ID entered manually for a board outside the curated Whisplay list.",
      isKnown: false,
    };
  }

  const fallbackBoard =
    getEsp32AgentBoardById(fallbackBoardId) ||
    getEsp32AgentBoardById(DEFAULT_ESP32_AGENT_BOARD_ID) ||
    ESP32_AGENT_BOARD_DEFINITIONS[0];
  return {
    ...fallbackBoard,
    isKnown: true,
  };
}
