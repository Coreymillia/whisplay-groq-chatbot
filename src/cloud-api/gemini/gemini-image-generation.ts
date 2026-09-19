import { GenerateContentResponse, GenerateImagesResponse, GoogleGenAI } from "@google/genai";
import { readFileSync, writeFileSync } from "fs";
import path from "path";
import { imageDir } from "../../utils/dir";
import {
  getImageMimeType,
  getPreferredContextImage,
  setLatestGenImg,
} from "../../utils/image";
import { LLMTool, ToolReturnTag } from "../../type";
import {
  GEMINI_LOW_TIER_IMAGE_COST_USD,
  getRuntimeSettings,
  isImagenImageModel,
  normalizeGeminiImageModel,
} from "../../config/runtime-settings";
import { buildGeminiImagePrompt } from "../../config/gemini-image-presets";
import { undiciProxyFetch } from "../proxy-fetch";

const getGeminiImageClient = (): GoogleGenAI | null => {
  const runtimeKey = getRuntimeSettings().geminiApiKey;
  const apiKey = runtimeKey || process.env.GEMINI_API_KEY || "";
  if (!apiKey) {
    return null;
  }
  return new GoogleGenAI({
    apiKey,
    fetch: undiciProxyFetch as any,
  });
};

const getGeminiImageModel = (): string => {
  return normalizeGeminiImageModel(
    getRuntimeSettings().geminiImageModel ||
      process.env.GEMINI_IMAGE_MODEL ||
      "gemini-2.5-flash-image",
  );
};

const shouldUseResponseModalities = (model: string): boolean => {
  return /preview/i.test(model) || /^gemini-3/i.test(model);
};

const createImageGenerationConfigs = (
  model: string,
): Array<Record<string, unknown>> => {
  const baseConfig: Record<string, unknown> = {
    imageConfig: {
      aspectRatio: "1:1",
    },
  };
  const modalityConfig: Record<string, unknown> = {
    ...baseConfig,
    responseModalities: ["IMAGE", "TEXT"],
  };
  const mimeConfig: Record<string, unknown> = {
    ...baseConfig,
    responseMimeType: "image/png",
  };
  return shouldUseResponseModalities(model)
    ? [modalityConfig, mimeConfig]
    : [mimeConfig, modalityConfig];
};

const isResponseConfigMismatch = (message: string): boolean => {
  return /response[_ ]mime[_ ]type|responsemodalities/i.test(message);
};

function saveGeneratedImage(imageBytes: string, prefix: string): string {
  const buffer = Buffer.from(imageBytes, "base64");
  const fileName = `${prefix}-${Date.now()}.png`;
  const imagePath = path.join(imageDir, fileName);
  writeFileSync(imagePath, buffer);
  setLatestGenImg(imagePath);
  return imagePath;
}

function getFirstGeneratedImageBytes(
  response: GenerateImagesResponse,
): string | null {
  for (const generatedImage of response.generatedImages || []) {
    const imageBytes = generatedImage.image?.imageBytes;
    if (imageBytes) {
      return imageBytes;
    }
  }
  return null;
}

function recordLowTierGeminiImageCharge(model: string): void {
  if (model !== "gemini-2.5-flash-image") {
    return;
  }
  try {
    const { recordGeminiLowTierImageCharge } = require("../../status/gemini-image-cost") as {
      recordGeminiLowTierImageCharge: () => number;
    };
    const remainingBalance = recordGeminiLowTierImageCharge();
    console.log(
      `[Gemini Image Cost] Charged $${GEMINI_LOW_TIER_IMAGE_COST_USD.toFixed(2)} for ${model}. Remaining balance: $${remainingBalance.toFixed(2)}`,
    );
  } catch (costError) {
    console.warn(
      "[Gemini Image Cost] Failed to record low-tier image charge:",
      costError,
    );
  }
}

async function generateWithGeminiContent(
  gemini: GoogleGenAI,
  geminiImageModel: string,
  finalPrompt: string,
  withImageContext: boolean,
): Promise<{ ok: boolean; error?: string }> {
  let imageContext = undefined;
  if (withImageContext) {
    const latestImgPath = getPreferredContextImage();
    if (latestImgPath) {
      const base64ImageFile = readFileSync(latestImgPath, {
        encoding: "base64",
      });
      imageContext = {
        inlineData: {
          mimeType: getImageMimeType(latestImgPath),
          data: base64ImageFile,
        },
      };
    }
  }

  const requestContents = [
    {
      role: "user" as const,
      parts: [
        {
          text: finalPrompt,
        },
        ...(imageContext ? [imageContext] : []),
      ],
    },
  ];

  let response: GenerateContentResponse | null = null;
  let generationError = "";
  const requestConfigs = createImageGenerationConfigs(geminiImageModel);
  for (let index = 0; index < requestConfigs.length; index += 1) {
    const config = requestConfigs[index];
    try {
      response = (await gemini.models.generateContent({
        model: geminiImageModel,
        contents: requestContents,
        config,
      })) as GenerateContentResponse;
      generationError = "";
      break;
    } catch (err) {
      generationError =
        err instanceof Error && err.message
          ? err.message
          : "Image generation request failed.";
      console.error("Error generating image:", err);
      const shouldRetry =
        index < requestConfigs.length - 1 &&
        isResponseConfigMismatch(generationError);
      if (shouldRetry) {
        console.log(
          `Retrying Gemini image request for model ${geminiImageModel} with alternate response config.`,
        );
        continue;
      }
      break;
    }
  }

  if (!response?.candidates?.[0]?.content?.parts?.length) {
    return {
      ok: false,
      error: generationError || "Image generation failed.",
    };
  }

  if (response.text) {
    console.log("Gemini image response text:", response.text);
  }

  try {
    for (const part of response.candidates[0].content.parts) {
      if (part.text) {
        console.log(part.text);
      } else if (part.inlineData) {
        const imagePath = saveGeneratedImage(
          part.inlineData.data!,
          "gemini-image",
        );
        console.log(`Image saved as ${imagePath}`);
        recordLowTierGeminiImageCharge(geminiImageModel);
        return { ok: true };
      }
    }
  } catch (error) {
    console.error("Error saving image:", error);
    return {
      ok: false,
      error: "Image generation failed.",
    };
  }

  return {
    ok: false,
    error: "Image generation failed.",
  };
}

async function generateWithImagen(
  gemini: GoogleGenAI,
  geminiImageModel: string,
  finalPrompt: string,
): Promise<{ ok: boolean; error?: string }> {
  try {
    const response = (await gemini.models.generateImages({
      model: geminiImageModel,
      prompt: finalPrompt,
      config: {
        numberOfImages: 1,
        aspectRatio: "1:1",
        outputMimeType: "image/png",
      },
    })) as GenerateImagesResponse;

    const imageBytes = getFirstGeneratedImageBytes(response);
    if (!imageBytes) {
      return {
        ok: false,
        error: "Image generation failed.",
      };
    }

    const imagePath = saveGeneratedImage(imageBytes, "imagen-image");
    console.log(`Image saved as ${imagePath}`);
    return { ok: true };
  } catch (error) {
    console.error("Error generating image:", error);
    return {
      ok: false,
      error:
        error instanceof Error && error.message
          ? error.message
          : "Image generation failed.",
    };
  }
}

function getImageContextWarning(model: string): string {
  return `${model} does not support photo edits yet. Switch to a Gemini image model for image-context edits.`;
}

export const addGeminiGenerationTool = (imageGenerationTools: LLMTool[]) => {
  imageGenerationTools.push({
    type: "function",
    function: {
      name: "generateImage",
      description:
        "Generate or draw an image from a text prompt, or edit an image based on a text prompt.",
      parameters: {
        type: "object",
        properties: {
          prompt: {
            type: "string",
            description: "The text prompt to generate the image from",
          },
          withImageContext: {
            type: "boolean",
            description:
              "When user mentions 'this image/picture/photo' or similar, set this to true, the tools will request and provide context from the latest showed image",
          },
        },
        required: ["prompt"],
      },
    },
    func: async (params: { prompt: string; withImageContext: boolean }) => {
      const gemini = getGeminiImageClient();
      if (!gemini) {
        return `${ToolReturnTag.Error} Gemini image generation is not configured yet.`;
      }

      const geminiImageModel = getGeminiImageModel();
      console.log(`Generating image with gemini model: ${geminiImageModel}`);
      const { prompt, withImageContext } = params;
      const runtimeSettings = getRuntimeSettings();
      const finalPrompt =
        buildGeminiImagePrompt(prompt, runtimeSettings.geminiImagePreset) || prompt;
      console.log(
        `Generating image with preset: ${runtimeSettings.geminiImagePreset || "none"}`,
      );

      if (isImagenImageModel(geminiImageModel)) {
        if (withImageContext && getPreferredContextImage()) {
          return `${ToolReturnTag.Error} ${getImageContextWarning(geminiImageModel)}`;
        }

        const result = await generateWithImagen(
          gemini,
          geminiImageModel,
          finalPrompt,
        );
        return result.ok
          ? `${ToolReturnTag.Success}Image file saved.`
          : `${ToolReturnTag.Error}${result.error || "Image generation failed."}`;
      }

      const result = await generateWithGeminiContent(
        gemini,
        geminiImageModel,
        finalPrompt,
        withImageContext,
      );
      return result.ok
        ? `${ToolReturnTag.Success}Image file saved.`
        : `${ToolReturnTag.Error}${result.error || "Image generation failed."}`;
    },
  });
};
