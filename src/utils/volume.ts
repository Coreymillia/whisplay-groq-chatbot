import { execSync } from "child_process";

// amixer -c 1 get Speaker
// Capabilities: volume
// Playback channels: Front Left - Front Right
// Limits: Playback 0 - 127
// Mono:
// Front Left: Playback 121 [95%] [0.00dB]
// Front Right: Playback 121 [95%] [0.00dB]

const soundCardIndex = process.env.SOUND_CARD_INDEX || "1";
console.log(`Using sound card index: ${soundCardIndex}`);
const DEFAULT_MIXER_CONTROL = process.env.SOUND_MIXER_CONTROL || "Speaker";

// curve
const percentToAmixerValueMap = [
  [0, 0],
  [10, 67],
  [20, 85],
  [30, 96],
  [40, 103],
  [50, 109],
  [60, 114],
  [70, 118],
  [80, 121],
  [90, 124],
  [100, 127],
];

const volumeLevelToPercentMap = [5, 10, 20, 30, 40, 50, 60, 75, 90, 100];

function runAmixer(command: string): string {
  const candidates = [
    `amixer -c ${soundCardIndex} ${command}`,
    `amixer ${command}`,
  ];

  let lastError: unknown = null;
  for (const candidate of candidates) {
    try {
      return execSync(candidate, { stdio: ["ignore", "pipe", "pipe"] }).toString();
    } catch (error) {
      lastError = error;
    }
  }
  throw lastError instanceof Error
    ? lastError
    : new Error(`amixer command failed: ${command}`);
}

const getVolumeValueFromAmixer = (): number => {
  const output = runAmixer(`get '${DEFAULT_MIXER_CONTROL}'`);
  const regex = /Front Left: Playback (\d+) \[(\d+)%\] \[([-\d.]+)dB\]/;
  const match = output.match(regex);
  if (match && match[1]) {
    const volume = parseFloat(match[1]);
    return volume;
  }
  return 0; // Default to min if not found
};

function logPercentToAmixerValue(logPercent: number): number {
  if (logPercent < 0 || logPercent > 100) {
    throw new Error("logPercent must be between 0 and 100");
  }
  // 根据percentToAmixerValueMap获得amixerValue，曲线中间的值则根据线性插值
  for (let i = 0; i < percentToAmixerValueMap.length - 1; i++) {
    const [percent1, amixerValue1] = percentToAmixerValueMap[i];
    const [percent2, amixerValue2] = percentToAmixerValueMap[i + 1];
    if (logPercent >= percent1 && logPercent <= percent2) {
      // 线性插值
      return (
        amixerValue1 +
        (amixerValue2 - amixerValue1) *
          ((logPercent - percent1) / (percent2 - percent1))
      );
    }
  }
  return 0; // Default to min if not found
}

export const getCurrentLogPercent = (): number => {
  const value = getVolumeValueFromAmixer();
  // 根据percentToAmixerValueMap获得logPercent，曲线中间的值则根据线性插值
  for (let i = 0; i < percentToAmixerValueMap.length - 1; i++) {
    const [percent1, amixerValue1] = percentToAmixerValueMap[i];
    const [percent2, amixerValue2] = percentToAmixerValueMap[i + 1];
    if (value >= amixerValue1 && value <= amixerValue2) {
      // 线性插值
      return (
        percent1 +
        (percent2 - percent1) *
          ((value - amixerValue1) / (amixerValue2 - amixerValue1))
      );
    }
  }
  return 0;
};

export const setVolumeByAmixer = (logPercent: number): void => {
  const value = logPercentToAmixerValue(logPercent);
  runAmixer(`set '${DEFAULT_MIXER_CONTROL}' ${value}`);
};

export const getPercentFromVolumeLevel = (volumeLevel: number): number => {
  const normalized = Math.max(1, Math.min(10, Math.round(volumeLevel)));
  return volumeLevelToPercentMap[normalized - 1];
};

export const getVolumeLevelFromPercent = (logPercent: number): number => {
  const percent = Math.max(0, Math.min(100, Math.round(logPercent)));
  let bestLevel = 1;
  let bestDistance = Number.POSITIVE_INFINITY;

  volumeLevelToPercentMap.forEach((value, index) => {
    const distance = Math.abs(value - percent);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestLevel = index + 1;
    }
  });

  return bestLevel;
};

export const getCurrentVolumeLevel = (): number => {
  return getVolumeLevelFromPercent(getCurrentLogPercent());
};

export const setVolumeByLevel = (volumeLevel: number): number => {
  const normalized = Math.max(1, Math.min(10, Math.round(volumeLevel)));
  const percent = getPercentFromVolumeLevel(normalized);
  setVolumeByAmixer(percent);
  return normalized;
};
