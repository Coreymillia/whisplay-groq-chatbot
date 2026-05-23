import { isEmpty } from "lodash";
import * as fs from "fs";
import * as path from "path";
import { LLMTool } from "../../type";
import {
  shouldResetChatHistory,
  systemPrompt,
  updateLastMessageTime,
} from "../../config/llm-config";
import { getGeminiClient } from "./gemini";
import { llmFuncMap, llmToolsForGemini } from "../../config/llm-tools";
import dotenv from "dotenv";
import { FunctionCall, Message } from "../../type";
import {
  ChatWithLLMStreamFunction,
  SavedChatHistorySummary,
  SummaryTextWithLLMFunction,
} from "../interface";
import { ToolListUnion, ToolUnion, Part, Content } from "@google/genai";
import moment from "moment";
import { chatHistoryDir } from "../../utils/dir";
import {
  consumePendingCapturedImgForChat,
  hasPendingCapturedImgForChat,
  getImageMimeType,
} from "../../utils/image";
import { getRuntimeSettings } from "../../config/runtime-settings";
import { isGeminiTextLlmModel } from "../../config/text-llm-models";

dotenv.config();

const useCapturedImageInChat =
  (process.env.USE_CAPTURED_IMAGE_IN_CHAT || "false").toLowerCase() ===
  "true";

const CHAT_HISTORY_FILE_PATTERN = /^gemini_chat_history_.*\.json$/;

function buildChatHistoryFileName(): string {
  return `gemini_chat_history_${moment().format("YYYY-MM-DD_HH-mm-ss_SSS")}.json`;
}

function getActiveGeminiLlmModel(): string {
  const runtimeModel = getRuntimeSettings().llmModel;
  if (isGeminiTextLlmModel(runtimeModel)) {
    return runtimeModel;
  }
  return process.env.GEMINI_MODEL || "gemini-2.5-flash";
}

const convertToolsToGeminiFormat = (tools: LLMTool[]): ToolListUnion => {
  return [
    {
      functionDeclarations: tools.map((tool) => ({
        name: tool.function.name,
        description: tool.function.description,
        parameters: tool.function.parameters,
      })),
    } as ToolUnion,
  ];
};

function createGeminiChatInstance(
  history?: Content[],
  customSystemPrompt?: string,
  model = getActiveGeminiLlmModel(),
) {
  const gemini = getGeminiClient();
  return gemini?.chats.create({
    model,
    config: {
      tools: convertToolsToGeminiFormat(llmToolsForGemini),
      systemInstruction: {
        parts: [{ text: customSystemPrompt || systemPrompt }],
        role: "system",
      },
    },
    history,
  })!;
}

let activeModel = getActiveGeminiLlmModel();
let chat = createGeminiChatInstance(undefined, undefined, activeModel);

const resetChatHistory = (): void => {
  activeModel = getActiveGeminiLlmModel();
  chat = createGeminiChatInstance(undefined, undefined, activeModel);
};

const writeChatHistory = (fileName: string, history: Content[]): void => {
  fs.mkdirSync(chatHistoryDir, { recursive: true });
  fs.writeFileSync(path.join(chatHistoryDir, fileName), JSON.stringify(history, null, 2));
};

const archiveCurrentChatHistory = (): string | null => {
  if (!chat) {
    return null;
  }
  const history = chat.getHistory();
  const hasConversation = history.some((entry) =>
    (entry.parts || []).some((part) => Boolean(part.text?.trim())),
  );
  if (!hasConversation) {
    return null;
  }
  const fileName = buildChatHistoryFileName();
  writeChatHistory(fileName, history);
  return fileName;
};

const getSafeChatHistoryPath = (fileName: string): string => {
  return path.resolve(chatHistoryDir, path.basename(fileName || ""));
};

const readSavedChatHistory = (fileName: string): Content[] | null => {
  const historyPath = getSafeChatHistoryPath(fileName);
  if (!historyPath.startsWith(path.resolve(chatHistoryDir) + path.sep)) {
    return null;
  }
  if (
    !fs.existsSync(historyPath) ||
    !CHAT_HISTORY_FILE_PATTERN.test(path.basename(historyPath))
  ) {
    return null;
  }
  try {
    const parsed = JSON.parse(fs.readFileSync(historyPath, "utf8"));
    return Array.isArray(parsed) ? (parsed as Content[]) : null;
  } catch (error) {
    console.error("Failed to read saved Gemini chat history:", error);
    return null;
  }
};

const getHistoryPreview = (history: Content[]): string => {
  const previewSource = [...history]
    .reverse()
    .flatMap((entry) => entry.parts || [])
    .map((part) => part.text || "")
    .find((text) => text.trim().length > 0) || "";
  return previewSource.length > 72
    ? `${previewSource.slice(0, 69).trimEnd()}...`
    : previewSource;
};

const listSavedChatHistories = (): SavedChatHistorySummary[] => {
  if (!fs.existsSync(chatHistoryDir)) {
    return [];
  }
  return fs.readdirSync(chatHistoryDir)
    .filter((fileName) => CHAT_HISTORY_FILE_PATTERN.test(fileName))
    .map((fileName) => {
      const historyPath = path.join(chatHistoryDir, fileName);
      const stats = fs.statSync(historyPath);
      const history = readSavedChatHistory(fileName) || [];
      return {
        fileName,
        updatedAt: stats.mtimeMs,
        messageCount: history.length,
        preview: getHistoryPreview(history),
      };
    })
    .sort((a, b) => b.updatedAt - a.updatedAt);
};

const loadSavedChatHistory = (fileName: string): boolean => {
  const history = readSavedChatHistory(fileName);
  if (!history || history.length === 0) {
    return false;
  }
  activeModel = getActiveGeminiLlmModel();
  chat = createGeminiChatInstance(history, undefined, activeModel);
  updateLastMessageTime();
  return true;
};

const chatWithLLMStream: ChatWithLLMStreamFunction = async (
  inputMessages: Message[] = [],
  partialCallback: (partialAnswer: string) => void,
  endCallback: () => void,
  partialThinkingCallback?: (partialThinking: string) => void,
  invokeFunctionCallback?: (functionName: string, result?: string) => void,
): Promise<void> => {
  const gemini = getGeminiClient();
  const selectedModel = getActiveGeminiLlmModel();
  if (!gemini) {
    console.error("Google Gemini API key is not set.");
    return;
  }
  if (!chat || selectedModel !== activeModel) {
    activeModel = selectedModel;
    chat = createGeminiChatInstance(undefined, undefined, activeModel);
  }

  if (shouldResetChatHistory()) {
    resetChatHistory();
  }
  updateLastMessageTime();

  const chatHistory = chat.getHistory();
  const knowledgePrompt = inputMessages.find((msg) => msg.role === "system");
  if (knowledgePrompt) {
    chatHistory.push({
      parts: [{ text: knowledgePrompt.content }],
      role: "system",
    });
    chat = createGeminiChatInstance(chatHistory, undefined, activeModel);
  }

  let endResolve: () => void = () => {};
  const promise = new Promise<void>((resolve) => {
    endResolve = resolve;
  }).finally(() => {
    if (chat) {
      writeChatHistory(buildChatHistoryFileName(), chat.getHistory());
    }
  });

  let partialAnswer = "";
  const functionCallsPackages: any[] = [];

  try {
    const lastUserMessageIndex = inputMessages
      .map((msg, index) => ({ msg, index }))
      .filter(({ msg }) => msg.role === "user")
      .map(({ index }) => index)
      .pop();
    const capturedImagePath =
      useCapturedImageInChat &&
      lastUserMessageIndex !== undefined &&
      hasPendingCapturedImgForChat()
        ? consumePendingCapturedImgForChat()
        : "";
    const imagePart = capturedImagePath
      ? {
          inlineData: {
            mimeType: getImageMimeType(capturedImagePath),
            data: fs.readFileSync(capturedImagePath).toString("base64"),
          },
        }
      : null;

    const geminiPart: Part[] = inputMessages
      .map((msg, index) => {
        if (msg.role === "user") {
          const parts: any[] = [{ text: msg.content }];
          if (
            imagePart &&
            lastUserMessageIndex !== undefined &&
            index === lastUserMessageIndex
          ) {
            parts.push(imagePart);
          }
          return parts;
        } else if (msg.role === "assistant") {
          return { text: msg.content };
        } else if (msg.role === "tool") {
          return {
            functionResponse: {
              name: msg.tool_call_id!,
              response: { result: msg.content },
            },
          };
        }
        return null;
      })
      .flat()
      .filter((item) => item !== null) as Part[];

    const response = await chat.sendMessageStream({
      message: geminiPart,
    });

    for await (const chunk of response) {
      const chunkText = chunk.text;
      if (chunkText) {
        partialCallback(chunkText);
        partialAnswer += chunkText;
      }

      const functionCalls = chunk.functionCalls;
      if (functionCalls) {
        functionCalls.forEach((call: any) => {
          functionCallsPackages.push({
            id: `call_${Date.now()}_${Math.random()}`,
            type: "function",
            function: {
              name: call.name,
              arguments: JSON.stringify(call.args || {}),
            },
          });
        });
      }
    }

    console.log("Stream ended");
    const functionCalls = functionCallsPackages;
    console.log("functionCalls: ", JSON.stringify(functionCalls));

    if (!isEmpty(functionCalls)) {
      const results = await Promise.all(
        functionCalls.map(async (call: FunctionCall) => {
          const {
            function: { arguments: argString, name },
            id,
          } = call;
          let args: Record<string, any> = {};
          try {
            args = JSON.parse(argString || "{}");
          } catch {
            console.error(
              `Error parsing arguments for function ${name}:`,
              argString,
            );
          }
          const func = llmFuncMap[name! as string];
          if (func) {
            invokeFunctionCallback?.(name! as string);
            return [
              id,
              await func(args)
                .then((res) => {
                  invokeFunctionCallback?.(name! as string, res);
                  return res;
                })
                .catch((err) => {
                  console.error(`Error executing function ${name}:`, err);
                  return `Error executing function ${name}: ${err.message}`;
                }),
            ];
          }
          console.error(`Function ${name} not found`);
          return [id, `Function ${name} not found`];
        }),
      );

      console.log("call results: ", results);
      const newMessages: Message[] = results.map(([id, result]: any) => ({
        role: "tool",
        content: result as string,
        tool_call_id: id as string,
      }));

      await chatWithLLMStream(newMessages, partialCallback, () => {
        endResolve();
        endCallback();
      });
      return;
    }
    endResolve();
    endCallback();
  } catch (error: any) {
    console.error("Error:", error.message);
    endResolve();
  }

  return promise;
};

const summaryTextWithLLM: SummaryTextWithLLMFunction = async (
  text: string,
  promptPrefix: string,
): Promise<string> => {
  const gemini = getGeminiClient();
  if (!gemini) {
    console.error("Gemini API key is not set. Using original text.");
    return text;
  }
  const response = await gemini.models.generateContent({
    model: getActiveGeminiLlmModel(),
    contents: [
      {
        parts: [{ text: `${promptPrefix}\n\n${text}\n\n` }],
        role: "user",
      },
    ],
  }).catch((error) => {
    console.log("Error during Gemini summary request:", error.message);
    return null;
  });
  if (!response) {
    return text;
  }
  if (response.text) {
    const summary = response.text;
    console.log("Gemini summary:", summary);
    return summary;
  }
  console.log("No summary returned from Gemini. Using original text.");
  return text;
};

export default {
  chatWithLLMStream,
  resetChatHistory,
  summaryTextWithLLM,
  listSavedChatHistories,
  loadSavedChatHistory,
  archiveCurrentChatHistory,
};
