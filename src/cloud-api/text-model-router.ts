import type { Content } from "@google/genai";
import { getGeminiClient } from "./gemini/gemini";
import { getOpenAIClient } from "./openai/openai";
import { getTextLlmProvider } from "../config/text-llm-models";

export interface PlainTextModelMessage {
  role: "system" | "user" | "assistant";
  content: string;
}

function splitGeminiMessages(messages: PlainTextModelMessage[]): {
  systemInstruction?: string;
  history: Content[];
  currentUserMessage: string;
} {
  const systemParts = messages
    .filter((message) => message.role === "system")
    .map((message) => message.content.trim())
    .filter(Boolean);
  const conversationalMessages = messages.filter(
    (message) => message.role !== "system" && message.content.trim(),
  );
  const currentUserMessage = conversationalMessages
    .slice()
    .reverse()
    .find((message) => message.role === "user")?.content.trim() || "";
  const historyMessages = currentUserMessage
    ? conversationalMessages.slice(0, conversationalMessages.length - 1)
    : conversationalMessages;

  return {
    systemInstruction: systemParts.length ? systemParts.join("\n\n") : undefined,
    history: historyMessages.map((message) => ({
      role: message.role === "assistant" ? "model" : "user",
      parts: [{ text: message.content }],
    })),
    currentUserMessage,
  };
}

export async function streamPlainTextModelResponse(input: {
  model: string;
  messages: PlainTextModelMessage[];
  onChunk: (chunk: string) => void;
}): Promise<string> {
  const provider = getTextLlmProvider(input.model);
  if (provider === "gemini") {
    const gemini = getGeminiClient();
    if (!gemini) {
      throw new Error("Configure a Gemini API key before using Gemini text models.");
    }
    const { systemInstruction, history, currentUserMessage } = splitGeminiMessages(
      input.messages,
    );
    if (!currentUserMessage) {
      return "";
    }
    const chat = gemini.chats.create({
      model: input.model,
      config: systemInstruction
        ? {
            systemInstruction: {
              role: "system",
              parts: [{ text: systemInstruction }],
            },
          }
        : undefined,
      history,
    });
    const stream = await chat.sendMessageStream({
      message: [{ text: currentUserMessage }],
    });
    let answer = "";
    for await (const chunk of stream) {
      const text = chunk.text || "";
      if (!text) {
        continue;
      }
      answer += text;
      input.onChunk(text);
    }
    return answer.trim();
  }

  const openai = getOpenAIClient();
  if (!openai) {
    throw new Error(
      "Configure a Groq/OpenAI-compatible API key before using this text model.",
    );
  }
  const stream = await openai.chat.completions.create({
    model: input.model,
    stream: true,
    messages: input.messages.map((message) => ({
      role: message.role,
      content: message.content,
    })),
  });
  let answer = "";
  for await (const chunk of stream) {
    const text = chunk.choices[0]?.delta?.content || "";
    if (!text) {
      continue;
    }
    answer += text;
    input.onChunk(text);
  }
  return answer.trim();
}

export async function createJsonTextModelResponse(input: {
  model: string;
  messages: PlainTextModelMessage[];
}): Promise<string> {
  const provider = getTextLlmProvider(input.model);
  if (provider === "gemini") {
    const gemini = getGeminiClient();
    if (!gemini) {
      throw new Error("Configure a Gemini API key before using Gemini text models.");
    }
    const prompt = input.messages
      .map((message) => `${message.role.toUpperCase()}:\n${message.content}`)
      .join("\n\n");
    const response = await gemini.models.generateContent({
      model: input.model,
      contents: prompt,
      config: {
        responseMimeType: "application/json",
      },
    });
    return response.text || "{}";
  }

  const openai = getOpenAIClient();
  if (!openai) {
    throw new Error(
      "Configure a Groq/OpenAI-compatible API key before using this text model.",
    );
  }
  const completion = await openai.chat.completions.create({
    model: input.model,
    stream: false,
    response_format: { type: "json_object" },
    messages: input.messages.map((message) => ({
      role: message.role,
      content: message.content,
    })),
  } as any);
  return completion.choices?.[0]?.message?.content || "{}";
}

export async function createPlainTextModelResponse(input: {
  model: string;
  messages: PlainTextModelMessage[];
  maxOutputTokens?: number;
}): Promise<string> {
  const provider = getTextLlmProvider(input.model);
  if (provider === "gemini") {
    const gemini = getGeminiClient();
    if (!gemini) {
      throw new Error("Configure a Gemini API key before using Gemini text models.");
    }
    const prompt = input.messages
      .map((message) => `${message.role.toUpperCase()}:\n${message.content}`)
      .join("\n\n");
    const response = await gemini.models.generateContent({
      model: input.model,
      contents: prompt,
      config:
        typeof input.maxOutputTokens === "number"
          ? { maxOutputTokens: input.maxOutputTokens }
          : undefined,
    });
    return (response.text || "").trim();
  }

  const openai = getOpenAIClient();
  if (!openai) {
    throw new Error(
      "Configure a Groq/OpenAI-compatible API key before using this text model.",
    );
  }
  const completion = await openai.chat.completions.create({
    model: input.model,
    stream: false,
    max_tokens: input.maxOutputTokens,
    messages: input.messages.map((message) => ({
      role: message.role,
      content: message.content,
    })),
  });
  return completion.choices?.[0]?.message?.content?.trim() || "";
}
