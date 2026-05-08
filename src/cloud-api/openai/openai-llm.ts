import dotenv from "dotenv";
import * as fs from "fs";
import * as path from "path";
import { isEmpty } from "lodash";
import moment from "moment";
import {
  shouldResetChatHistory,
  getSystemPrompt,
  updateLastMessageTime,
} from "../../config/llm-config";
import { FunctionCall, Message, ToolReturnTag } from "../../type";
import { combineFunction } from "../../utils";
import { getOpenAIClient, getOpenAILLMModel } from "./openai";
import { llmFuncMap, llmTools } from "../../config/llm-tools";
import {
  ChatWithLLMStreamFunction,
  SavedChatHistorySummary,
  SummaryTextWithLLMFunction,
} from "../interface";
import { chatHistoryDir } from "../../utils/dir";
import {
  consumePendingCapturedImgForChat,
  hasPendingCapturedImgForChat,
  getImageMimeType,
} from "../../utils/image";

dotenv.config();
// OpenAI LLM
const openaiEnableTools =
  (process.env.OPENAI_ENABLE_TOOLS || "true").toLowerCase() === "true";
const shouldIncludeTools = openaiEnableTools;
const useCapturedImageInChat =
  (process.env.USE_CAPTURED_IMAGE_IN_CHAT || "false").toLowerCase() === "true";
const openaiUseStream =
  (process.env.OPENAI_USE_STREAM || "true").toLowerCase() === "true";
const openaiUseImagePath =
  (process.env.OPENAI_USE_IMAGE_PATH || "false").toLowerCase() === "true";
const openaiMaxMessagesLength = parseInt(
  process.env.OPENAI_MAX_MESSAGES_LENGTH || "0",
  10,
);

const buildChatHistoryFileName = (): string =>
  `openai_chat_history_${moment().format("YYYY-MM-DD_HH-mm-ss_SSS")}.json`;

const buildImageDataUrl = (imagePath: string): string => {
  const mimeType = getImageMimeType(imagePath) || "image/jpeg";
  const base64 = fs.readFileSync(imagePath).toString("base64");
  return `data:${mimeType};base64,${base64}`;
};

const chatHistoryFileName = buildChatHistoryFileName();
const CHAT_HISTORY_FILE_PATTERN = /^openai_chat_history_.*\.json$/;

const messages: Message[] = [
  {
    role: "system",
    content: getSystemPrompt(),
  },
];

const syncSystemPrompt = (): void => {
  const currentSystemPrompt = getSystemPrompt();
  if (messages[0]?.role === "system") {
    messages[0].content = currentSystemPrompt;
    return;
  }
  messages.unshift({
    role: "system",
    content: currentSystemPrompt,
  });
};

const resetChatHistory = (): void => {
  messages.length = 0;
  messages.push({
    role: "system",
    content: getSystemPrompt(),
  });
};

const writeChatHistory = (fileName: string, history: Message[]): void => {
  fs.mkdirSync(chatHistoryDir, { recursive: true });
  fs.writeFileSync(path.join(chatHistoryDir, fileName), JSON.stringify(history, null, 2));
};

const archiveCurrentChatHistory = (): string | null => {
  const hasConversation = messages.some(
    (message, index) => index > 0 && message.content.trim().length > 0,
  );
  if (!hasConversation) {
    return null;
  }
  const fileName = buildChatHistoryFileName();
  writeChatHistory(fileName, messages);
  return fileName;
};

const getSafeChatHistoryPath = (fileName: string): string => {
  return path.resolve(chatHistoryDir, path.basename(fileName || ""));
};

const readSavedChatHistory = (fileName: string): Message[] | null => {
  const historyPath = getSafeChatHistoryPath(fileName);
  if (!historyPath.startsWith(path.resolve(chatHistoryDir) + path.sep)) {
    return null;
  }
  if (!fs.existsSync(historyPath) || !CHAT_HISTORY_FILE_PATTERN.test(path.basename(historyPath))) {
    return null;
  }
  try {
    const parsed = JSON.parse(fs.readFileSync(historyPath, "utf8"));
    if (!Array.isArray(parsed)) {
      return null;
    }
    return parsed.filter((message) => {
      return (
        message &&
        typeof message === "object" &&
        typeof message.role === "string" &&
        "content" in message
      );
    }) as Message[];
  } catch (error) {
    console.error("Failed to read saved chat history:", error);
    return null;
  }
};

const getHistoryPreview = (history: Message[]): string => {
  const lastUserMessage = [...history]
    .reverse()
    .find((message) => message.role === "user" && typeof message.content === "string");
  const fallbackMessage = [...history]
    .reverse()
    .find((message) => typeof message.content === "string" && message.content.trim().length > 0);
  const previewSource = (lastUserMessage?.content || fallbackMessage?.content || "").trim();
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
  messages.length = 0;
  messages.push(...history);
  syncSystemPrompt();
  updateLastMessageTime();
  return true;
};

const chatWithLLMStream: ChatWithLLMStreamFunction = async (
  inputMessages: Message[] = [],
  partialCallback: (partial: string) => void,
  endCallback: () => void,
  partialThinkingCallback?: (partialThinking: string) => void,
  invokeFunctionCallback?: (functionName: string, result?: string) => void,
): Promise<void> => {
  const openai = getOpenAIClient();
  if (!openai) {
    console.error("OpenAI API key is not set.");
    return;
  }
  if (shouldResetChatHistory()) {
    resetChatHistory();
  } else {
    syncSystemPrompt();
  }
  const openaiLLMModel = getOpenAILLMModel();
  updateLastMessageTime();
  let endResolve: () => void = () => {};
  const promise = new Promise<void>((resolve) => {
    endResolve = resolve;
  }).finally(() => {
    writeChatHistory(chatHistoryFileName, messages);
  });
  messages.push(...inputMessages);
  // Trim messages to MAX_MESSAGES_LENGTH (keep first system prompt + last N messages)
  if (openaiMaxMessagesLength > 0 && messages.length > openaiMaxMessagesLength + 1) {
    const firstSystemMessage = messages[0];
    const restMessages = messages.slice(1);
    const trimmed = restMessages.slice(-openaiMaxMessagesLength);
    messages.length = 0;
    messages.push(firstSystemMessage, ...trimmed);
  }
  const lastUserMessage = [...inputMessages]
    .reverse()
    .find((msg) => msg.role === "user");
  const capturedImagePath =
    useCapturedImageInChat && lastUserMessage && hasPendingCapturedImgForChat()
      ? consumePendingCapturedImgForChat()
      : "";
  const multimodalLastUserContent = capturedImagePath
    ? [
        {
          type: "text",
          text: lastUserMessage?.content || "",
        },
        {
          type: "image_url",
          image_url: {
            url: openaiUseImagePath ? capturedImagePath : buildImageDataUrl(capturedImagePath),
          },
        },
      ]
    : [
        {
          type: "text",
          text: lastUserMessage?.content || "",
        },
      ];

  const lastUserMessageIndex = messages
    .map((msg, index) => ({ msg, index }))
    .filter(({ msg }) => msg.role === "user")
    .map(({ index }) => index)
    .pop();

  const requestMessages = messages.map((msg, index) => {
    if (
      capturedImagePath &&
      msg.role === "user" &&
      lastUserMessageIndex !== undefined &&
      index === lastUserMessageIndex
    ) {
      return {
        role: "user",
        content: multimodalLastUserContent,
      };
    }
    return {
      role: msg.role,
      content: msg.content,
      ...(msg.tool_call_id ? { tool_call_id: msg.tool_call_id } : {}),
      ...(msg.tool_calls ? { tool_calls: msg.tool_calls } : {}),
    };
  });
  let answer = "";
  let functionCalls: FunctionCall[] = [];
  if (openaiUseStream) {
    const chatCompletion = await openai.chat.completions.create({
      model: openaiLLMModel,
      messages: requestMessages as any,
      stream: true,
      tools: shouldIncludeTools ? llmTools : undefined,
    }).catch((error) => {
      console.log("Error during OpenAI chat completion request:", error.message);
      endResolve();
      endCallback();
      return [];
    });
    let partialAnswer = "";
    const functionCallsPackages: any[] = [];
    for await (const chunk of chatCompletion) {
      if (chunk.choices[0].delta.content) {
        partialCallback(chunk.choices[0].delta.content);
        partialAnswer += chunk.choices[0].delta.content;
      }
      if (chunk.choices[0].delta.tool_calls) {
        functionCallsPackages.push(...chunk.choices[0].delta.tool_calls);
      }
    }
    answer = partialAnswer;
    functionCalls = combineFunction(functionCallsPackages);
  } else {
    const chatCompletion = await openai.chat.completions.create({
      model: openaiLLMModel,
      messages: requestMessages as any,
      stream: false,
      tools: shouldIncludeTools ? llmTools : undefined,
    }).catch((error) => {
      console.log("Error during OpenAI chat completion request:", error.message);
      endResolve();
      endCallback();
      return null;
    });
    if (chatCompletion && chatCompletion.choices && chatCompletion.choices.length > 0) {
      const msg = chatCompletion.choices[0].message;
      answer = msg?.content || "";
      partialCallback(answer);
      functionCalls = combineFunction((msg?.tool_calls as any) || []);
    }
  }
  messages.push({
    role: "assistant",
    content: answer,
    tool_calls: isEmpty(functionCalls) ? undefined : functionCalls,
  });
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
        invokeFunctionCallback?.(name! as string);
        if (func) {
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
        } else {
          console.error(`Function ${name} not found`);
          return [id, `Function ${name} not found`];
        }
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
  } else {
    endResolve();
    endCallback();
  }
  return promise;
};

const summaryTextWithLLM: SummaryTextWithLLMFunction = async (
  text: string,
  promptPrefix: string,
): Promise<string> => {
  const openai = getOpenAIClient();
  if (!openai) {
    console.error("OpenAI API key is not set. Using original text.");
    return text;
  }
  const openaiLLMModel = getOpenAILLMModel();
  const chatCompletion = await openai.chat.completions
    .create({
      model: openaiLLMModel,
      messages: [
        {
          role: "system",
          content: promptPrefix,
        },
        {
          role: "user",
          content: text,
        },
      ],
      stream: false,
    })
    .catch((error) => {
      console.log("Error during OpenAI summary request:", error.message);
      return null;
    });
  if (!chatCompletion) {
    return text;
  }
  if (chatCompletion.choices && chatCompletion.choices.length > 0) {
    const summary = chatCompletion.choices[0].message?.content || "";
    console.log("OpenAI summary:", summary);
    return summary;
  } else {
    console.log("No summary returned from OpenAI. Using original text.");
    return text;
  }
};

export default {
  chatWithLLMStream,
  resetChatHistory,
  summaryTextWithLLM,
  listSavedChatHistories,
  loadSavedChatHistory,
  archiveCurrentChatHistory,
};
