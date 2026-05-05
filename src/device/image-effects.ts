import { execFile } from "child_process";
import path from "path";
import moment from "moment";
import { cameraDir } from "../utils/dir";

export type ImageEffectId =
  | "retro"
  | "comic"
  | "sketch"
  | "pixelate"
  | "halftone"
  | "edge"
  | "spooky"
  | "dreamy"
  | "warm"
  | "cyberpunk"
  | "glitch"
  | "vhs"
  | "auto-contrast"
  | "colors-pop";

interface ImageEffectResult {
  ok: boolean;
  output?: string;
  effect?: ImageEffectId;
  error?: string;
}

const IMAGE_EFFECT_LABELS: Record<ImageEffectId, string> = {
  retro: "retro",
  comic: "comic book",
  sketch: "sketch",
  pixelate: "pixelate",
  halftone: "halftone",
  edge: "edge",
  spooky: "spooky",
  dreamy: "dreamy",
  warm: "warm",
  cyberpunk: "cyberpunk",
  glitch: "glitch",
  vhs: "VHS",
  "auto-contrast": "auto contrast",
  "colors-pop": "colors pop",
};

function effectScriptPath(): string {
  return path.resolve(__dirname, "../../python/image_effects.py");
}

function buildOutputPath(effect: ImageEffectId): string {
  return path.join(
    cameraDir,
    `effect-${effect}-${moment().format("YYYYMMDD-HHmmss-SSS")}.png`,
  );
}

export function getImageEffectLabel(effect: ImageEffectId): string {
  return IMAGE_EFFECT_LABELS[effect];
}

export async function applyImageEffect(
  inputPath: string,
  effect: ImageEffectId,
): Promise<string> {
  const outputPath = buildOutputPath(effect);
  const scriptPath = effectScriptPath();

  const result = await new Promise<ImageEffectResult>((resolve, reject) => {
    execFile(
      "python3",
      [
        scriptPath,
        "--input",
        inputPath,
        "--output",
        outputPath,
        "--effect",
        effect,
      ],
      {
        cwd: path.resolve(__dirname, "../../python"),
        timeout: 45000,
        maxBuffer: 1024 * 1024,
      },
      (error, stdout, stderr) => {
        const raw = `${stdout || ""}${stderr || ""}`.trim();
        let parsed: ImageEffectResult | null = null;
        try {
          const lines = raw.split(/\r?\n/).filter(Boolean);
          parsed = JSON.parse(lines[lines.length - 1] || "{}") as ImageEffectResult;
        } catch {
          parsed = null;
        }

        if (error) {
          if (parsed?.error) {
            reject(new Error(parsed.error));
            return;
          }
          reject(
            new Error(
              raw || error.message || "Image effect processing failed.",
            ),
          );
          return;
        }

        if (!parsed?.ok || !parsed.output) {
          reject(new Error(parsed?.error || "Image effect processing failed."));
          return;
        }

        resolve(parsed);
      },
    );
  });

  return result.output!;
}
