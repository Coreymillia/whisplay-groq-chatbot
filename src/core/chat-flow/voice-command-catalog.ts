export interface VoiceCommandHelpEntry {
  id: string;
  summary: string;
}

export const VOICE_COMMAND_HELP_ENTRIES: VoiceCommandHelpEntry[] = [
  {
    id: "help",
    summary: "help / voice commands",
  },
  {
    id: "settings",
    summary: "open settings",
  },
  {
    id: "voice-on",
    summary: "talk to me / voice on",
  },
  {
    id: "voice-off",
    summary: "stop speaking / voice off",
  },
  {
    id: "manual-speech",
    summary: "read that aloud / say that again",
  },
  {
    id: "new-chat",
    summary: "new chat / clear chat",
  },
  {
    id: "volume-set",
    summary: "set volume to 1-10",
  },
  {
    id: "volume-step",
    summary: "volume up / volume down",
  },
  {
    id: "screen-timeout-set",
    summary: "screen timeout 1-10 min",
  },
  {
    id: "screen-timeout-off",
    summary: "screen timeout off",
  },
  {
    id: "photo-capture",
    summary: "take photo / capture image",
  },
  {
    id: "camera-switch",
    summary: "switch camera / swap camera",
  },
  {
    id: "photo-browser",
    summary: "browse photos / images",
  },
  {
    id: "vision",
    summary: "what do you see? / read text",
  },
  {
    id: "weather",
    summary: "what's the weather? / weather alerts",
  },
  {
    id: "image-effects",
    summary: "make it retro / spooky / comic",
  },
  {
    id: "image-effects-2",
    summary: "sketch it / pixelate / halftone",
  },
  {
    id: "image-effects-3",
    summary: "cyberpunk / glitch it / VHS",
  },
  {
    id: "image-effects-4",
    summary: "auto contrast / colors pop",
  },
  {
    id: "music-play-stop",
    summary: "play music / stop music",
  },
  {
    id: "music-skip",
    summary: "next song / previous song",
  },
  {
    id: "shutdown",
    summary: "shutdown / shutdown pi",
  },
];

export function buildVoiceCommandHelpPages(linesPerPage = 4): string[] {
  if (linesPerPage <= 0) {
    return [];
  }

  const pages: string[] = [];
  const totalPages = Math.ceil(VOICE_COMMAND_HELP_ENTRIES.length / linesPerPage);

  for (let pageIndex = 0; pageIndex < totalPages; pageIndex += 1) {
    const entries = VOICE_COMMAND_HELP_ENTRIES.slice(
      pageIndex * linesPerPage,
      (pageIndex + 1) * linesPerPage,
    );
    const header = `VOICE CMDS ${pageIndex + 1}/${totalPages}`;
    const footer =
      pageIndex === 0 ? "\n\nShort: next\nHold: exit" : "";
    pages.push(
      `${header}\n${entries.map((entry) => entry.summary).join("\n")}${footer}`,
    );
  }

  return pages;
}
