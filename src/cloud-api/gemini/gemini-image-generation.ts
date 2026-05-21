import { LLMTool, ToolReturnTag } from "../../type";
import {
  getImageMimeType,
  getLatestShowedImage,
  setLatestGenImg,
} from "../../utils/image";
import { GenerateContentResponse, GoogleGenAI } from "@google/genai";
import path from "path";
import { imageDir } from "../../utils/dir";
import { readFileSync, writeFileSync } from "fs";
import { getRuntimeSettings } from "../../config/runtime-settings";
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
  return (
    getRuntimeSettings().geminiImageModel ||
    process.env.GEMINI_IMAGE_MODEL ||
    "gemini-2.5-flash-image"
  );
};

const shouldUseResponseModalities = (model: string): boolean => {
  return /preview/i.test(model) || /^gemini-3/i.test(model);
};


export const addGeminiGenerationTool = (imageGenerationTools: LLMTool[]) => {
  imageGenerationTools.push({
    type: "function",
    function: {
      name: "generateImage",
      description: "Generate or draw an image from a text prompt, or edit an image based on a text prompt.",
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
      const finalPrompt = buildGeminiImagePrompt(
        prompt,
        runtimeSettings.geminiImagePreset,
      ) || prompt;
      console.log(
        `Generating image with preset: ${runtimeSettings.geminiImagePreset || "none"}`,
      );
      let imageContext = undefined;
      if (withImageContext) {
        const latestImgPath = getLatestShowedImage();
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
      const config: Record<string, unknown> = {
        imageConfig: {
          aspectRatio: "1:1",
        },
      };
      if (shouldUseResponseModalities(geminiImageModel)) {
        config.responseModalities = ["IMAGE", "TEXT"];
      } else {
        config.responseMimeType = "image/png";
      }
      let response: GenerateContentResponse | null = null;
      let generationError = "";
      try {
        response = (await gemini!.models.generateContent({
          model: geminiImageModel!,
          contents: requestContents,
          config,
        })) as GenerateContentResponse;
      } catch (err) {
        generationError =
          err instanceof Error && err.message
            ? err.message
            : "Image generation request failed.";
        console.error(`Error generating image:`, err);
      }
      if (!response?.candidates?.[0]?.content?.parts?.length) {
        return `${ToolReturnTag.Error}${generationError || "Image generation failed."}`;
      }
      if (response.text) {
        console.log("Gemini image response text:", response.text);
      }
      const fileName = `gemini-image-${Date.now()}.png`;
      const imagePath = path.join(imageDir, fileName);
      let isSuccess = false;
      try {
        for (const part of response.candidates![0].content!.parts!) {
          if (part.text) {
            console.log(part.text);
          } else if (part.inlineData) {
            const imageData = part.inlineData.data!;
            const buffer = Buffer.from(imageData, "base64");
            writeFileSync(imagePath, buffer);
            setLatestGenImg(imagePath);
            isSuccess = true;
            console.log(`Image saved as ${imagePath}`);
          }
        }
      } catch (error) {
        console.error("Error saving image:", error);
      }
      return isSuccess
        ? `${ToolReturnTag.Success}Image file saved.`
        : `${ToolReturnTag.Error}Image generation failed.`;
    },
  });
};
