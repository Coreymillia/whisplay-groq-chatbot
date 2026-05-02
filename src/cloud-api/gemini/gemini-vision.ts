import { GoogleGenAI } from "@google/genai";
import { LLMTool, ToolReturnTag } from "../../type";
import dotenv from "dotenv";
import { getLatestShowedImage, getImageMimeType } from "../../utils/image";
import { readFileSync } from "fs";
import { getRuntimeSettings } from "../../config/runtime-settings";

dotenv.config();

const getGeminiVisionClient = (): GoogleGenAI | null => {
  const runtimeKey = getRuntimeSettings().geminiApiKey;
  const apiKey = runtimeKey || process.env.GEMINI_API_KEY || "";
  if (!apiKey) {
    return null;
  }
  return new GoogleGenAI({ apiKey });
};

const getGeminiVisionModel = (): string => {
  return process.env.GEMINI_VISION_MODEL || "gemini-2.5-flash";
};

export const addGeminiVisionTool = (visionTools: LLMTool[]) => {
  visionTools.push({
    type: "function",
    function: {
      name: "describeImage",
      description:
        "Use this tool when the user wants to analyze and interpret the latest uploaded, shown, or captured image. Call it for prompts like 'what do you see', 'describe this image', or questions about the current photo.",
      parameters: {
        type: "object",
        properties: {
          prompt: {
            type: "string",
            description:
              "The query or prompt to help with interpreting the image, e.g., 'What is in this image?'",
          },
        },
        required: ["prompt"],
      },
    },
    func: async (params) => {
      const { prompt } = params;
      const gemini = getGeminiVisionClient();
      if (!gemini) {
        return `${ToolReturnTag.Error} Gemini vision is not configured yet.`;
      }
      const imgPath = getLatestShowedImage();
      if (!imgPath) {
        return `${ToolReturnTag.Error} No image is found.`;
      }
      const base64ImageFile = readFileSync(imgPath, { encoding: "base64" });
      const contents = [
        {
          inlineData: {
            mimeType: getImageMimeType(imgPath) || "image/jpeg",
            data: base64ImageFile,
          },
        },
        { text: prompt },
      ];
      try {
        const response = await gemini.models.generateContent({
          model: getGeminiVisionModel(),
          contents,
        });
        const content = response.text;
        return (
          `${ToolReturnTag.Success}${content}` ||
          `${ToolReturnTag.Error} No content received from Gemini.`
        );
      } catch (error) {
        console.error("Error during Gemini vision request:", error);
        return `${ToolReturnTag.Error} Failed to analyze the image.`;
      }
    },
  });
};
