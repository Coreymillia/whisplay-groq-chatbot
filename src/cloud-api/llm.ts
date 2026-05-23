import { noop } from "lodash";
import dotenv from "dotenv";
import { LLMServer } from "../type";
import { getRuntimeSettings } from "../config/runtime-settings";
import { getTextLlmProvider } from "../config/text-llm-models";
import {
  ArchiveCurrentChatHistoryFunction,
  ChatWithLLMStreamFunction,
  ListSavedChatHistoriesFunction,
  LoadSavedChatHistoryFunction,
  ResetChatHistoryFunction,
  SummaryTextWithLLMFunction,
} from "./interface";
import { pluginRegistry, LLMProvider } from "../plugin";
import openaiLlm from "./openai/openai-llm";
import geminiLlm from "./gemini/gemini-llm";

dotenv.config();

let _chatWithLLMStream: ChatWithLLMStreamFunction = noop as any;
let _resetChatHistory: ResetChatHistoryFunction = noop as any;
let _summaryTextWithLLM: SummaryTextWithLLMFunction = async (text, _) => text;
let _listSavedChatHistories: ListSavedChatHistoriesFunction = () => [];
let _loadSavedChatHistory: LoadSavedChatHistoryFunction = () => false;
let _archiveCurrentChatHistory: ArchiveCurrentChatHistoryFunction = () => null;

const MAX_FUNCTION_CALL_DEPTH = 5;
let functionCallDepth = 0;

function getDynamicTextLlmProvider(): Partial<LLMProvider> | null {
  if (llmServer !== LLMServer.openai && llmServer !== LLMServer.gemini) {
    return null;
  }
  return getTextLlmProvider(getRuntimeSettings().llmModel) === "gemini"
    ? geminiLlm
    : openaiLlm;
}

const chatWithLLMStream: ChatWithLLMStreamFunction = async (
  inputMessages,
  partialCallback,
  endCallBack,
  partialThinkingCallback?,
  invokeFunctionCallback?,
) => {
  const isTopLevel = functionCallDepth === 0;
  functionCallDepth++;
  if (functionCallDepth > MAX_FUNCTION_CALL_DEPTH) {
    console.warn(`[LLM] Function call depth exceeded ${MAX_FUNCTION_CALL_DEPTH}, stopping.`);
    functionCallDepth = 0;
    endCallBack();
    return;
  }
  try {
    const activeProvider = getDynamicTextLlmProvider();
    const chatHandler = activeProvider?.chatWithLLMStream || _chatWithLLMStream;
    return await chatHandler(
      inputMessages,
      partialCallback,
      endCallBack,
      partialThinkingCallback,
      invokeFunctionCallback,
    );
  } finally {
    if (isTopLevel) {
      functionCallDepth = 0;
    }
  }
};

const llmServer: LLMServer = (
  process.env.LLM_SERVER || LLMServer.test
).toLowerCase() as LLMServer;

console.log(`Current LLM Server: ${llmServer}`);

// Activate LLM plugin
try {
  const llmProvider = pluginRegistry.activatePluginSync<"llm">("llm", llmServer);
  _chatWithLLMStream = llmProvider.chatWithLLMStream;
  _resetChatHistory = llmProvider.resetChatHistory;
  if (llmProvider.summaryTextWithLLM) {
    _summaryTextWithLLM = llmProvider.summaryTextWithLLM;
  }
  if (llmProvider.listSavedChatHistories) {
    _listSavedChatHistories = llmProvider.listSavedChatHistories;
  }
  if (llmProvider.loadSavedChatHistory) {
    _loadSavedChatHistory = llmProvider.loadSavedChatHistory;
  }
  if (llmProvider.archiveCurrentChatHistory) {
    _archiveCurrentChatHistory = llmProvider.archiveCurrentChatHistory;
  }
} catch (e: any) {
  console.warn(e.message);
}

const isImMode = llmServer === LLMServer.whisplayim;

const resetChatHistory: ResetChatHistoryFunction = () => {
  const activeProvider = getDynamicTextLlmProvider();
  return (activeProvider?.resetChatHistory || _resetChatHistory)();
};

const summaryTextWithLLM: SummaryTextWithLLMFunction = async (text, promptPrefix) => {
  const activeProvider = getDynamicTextLlmProvider();
  const summaryHandler =
    activeProvider?.summaryTextWithLLM || _summaryTextWithLLM;
  return summaryHandler(text, promptPrefix);
};

const listSavedChatHistories: ListSavedChatHistoriesFunction = () => {
  const activeProvider = getDynamicTextLlmProvider();
  return (activeProvider?.listSavedChatHistories || _listSavedChatHistories)();
};

const loadSavedChatHistory: LoadSavedChatHistoryFunction = (fileName) => {
  const activeProvider = getDynamicTextLlmProvider();
  return (activeProvider?.loadSavedChatHistory || _loadSavedChatHistory)(fileName);
};

const archiveCurrentChatHistory: ArchiveCurrentChatHistoryFunction = () => {
  const activeProvider = getDynamicTextLlmProvider();
  return (activeProvider?.archiveCurrentChatHistory || _archiveCurrentChatHistory)();
};

export {
  chatWithLLMStream,
  resetChatHistory,
  summaryTextWithLLM,
  listSavedChatHistories,
  loadSavedChatHistory,
  archiveCurrentChatHistory,
  isImMode,
};
