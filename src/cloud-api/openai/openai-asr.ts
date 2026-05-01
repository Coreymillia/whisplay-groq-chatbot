import fs from "fs";
import { getOpenAIASRModel, getOpenAIClient } from "./openai";

export const recognizeAudio = async (
  audioFilePath: string
): Promise<string> => {
  const openai = getOpenAIClient();
  if (!openai) {
    console.error("OpenAI API key is not set.");
    return "";
  }
  if (!fs.existsSync(audioFilePath)) {
    console.error("Audio file does not exist:", audioFilePath);
    return "";
  }

  try {
    const transcription = await openai.audio.transcriptions.create({
      file: fs.createReadStream(audioFilePath),
      model: getOpenAIASRModel(),
    });
    console.log("Transcription result:", transcription.text);
    return transcription.text;
  } catch (error) {
    console.error("Audio recognition failed:", error);
    return "";
  }
};
