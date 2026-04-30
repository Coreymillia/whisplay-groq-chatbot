import mp3Duration from "mp3-duration";
import {
  getOpenAIClient,
  getOpenAIVoiceModel,
  getOpenAIVoiceType,
} from "./openai";
import dotenv from "dotenv";
import { TTSResult } from "../../type";

dotenv.config();

const openaiTTS = async (
  text: string
): Promise<TTSResult> => {
  const openai = getOpenAIClient();
  if (!openai) {
    console.error("OpenAI API key is not set.");
    return { duration: 0 };
  }
  const openAiVoiceModel = getOpenAIVoiceModel();
  const openAiVoiceType = getOpenAIVoiceType();
  const mp3 = await openai.audio.speech.create({
    model: openAiVoiceModel,
    voice: openAiVoiceType,
    input: text,
  }).catch((error) => {
    console.log("OpenAI TTS failed:", error);
    return null;
  });
  if (!mp3) {
    return { duration: 0 };
  }
  const buffer = Buffer.from(await mp3.arrayBuffer());
  const duration = await mp3Duration(buffer);
  return { buffer, duration: duration * 1000 };
};

export default openaiTTS;
