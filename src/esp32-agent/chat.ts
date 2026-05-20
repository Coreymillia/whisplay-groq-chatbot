import {
  appendEsp32AgentProjectChatMessages,
  createEsp32AgentProjectCheckpoint,
  deleteEsp32AgentProjectFile,
  getEsp32AgentProject,
  listEsp32AgentProjectChatMessages,
  listEsp32AgentProjectWorkspaceTextFiles,
  readEsp32AgentProjectErrorLog,
  writeEsp32AgentProjectFile,
} from "./workspace";
import { getOpenAIClient, getOpenAILLMModel } from "../cloud-api/openai/openai";
import { getRuntimeSettings } from "../config/runtime-settings";

export interface Esp32AgentProposedOperation {
  type: "write_file" | "delete_file";
  path: string;
  content?: string;
  summary: string;
}

export interface Esp32AgentProposalResponse {
  reply: string;
  operations: Esp32AgentProposedOperation[];
}

function sanitizeOperation(
  operation: Partial<Esp32AgentProposedOperation>,
): Esp32AgentProposedOperation | null {
  const type =
    operation.type === "delete_file" ? "delete_file" : operation.type === "write_file"
      ? "write_file"
      : null;
  const relativePath =
    typeof operation.path === "string" ? operation.path.trim().replace(/\\/g, "/") : "";
  if (!type || !relativePath) {
    return null;
  }
  const normalizedPath = relativePath.replace(/^\/+/, "");
  if (!normalizedPath || normalizedPath.startsWith("../")) {
    return null;
  }
  const summary =
    typeof operation.summary === "string" && operation.summary.trim()
      ? operation.summary.trim().slice(0, 240)
      : type === "delete_file"
        ? "Delete file"
        : "Write file";
  return {
    type,
    path: normalizedPath,
    content:
      type === "write_file" ? String(operation.content || "") : undefined,
    summary,
  };
}

function buildWorkspaceContext(projectId: string, userPrompt: string): string {
  const project = getEsp32AgentProject(projectId);
  const files = listEsp32AgentProjectWorkspaceTextFiles(projectId);
  const normalizedPrompt = userPrompt.toLowerCase();
  const prioritizedFiles = [...files]
    .sort((left, right) => {
      const score = (file: { path: string }) => {
        const lowerPath = file.path.toLowerCase();
        const baseName = lowerPath.split("/").pop() || lowerPath;
        let total = 0;
        if (normalizedPrompt.includes(lowerPath)) total += 100;
        if (normalizedPrompt.includes(baseName)) total += 60;
        if (lowerPath === "platformio.ini") total += 40;
        if (lowerPath === "src/main.cpp") total += 35;
        if (lowerPath.startsWith("include/")) total += 10;
        if (lowerPath.startsWith("src/")) total += 8;
        return total;
      };
      return score(right) - score(left);
    })
    .slice(0, 8);

  const selectedFileChunks = [];
  let totalChars = 0;
  for (const file of prioritizedFiles) {
    const trimmedContent =
      file.content.length > 6000
        ? `${file.content.slice(0, 6000)}\n[truncated]`
        : file.content;
    const nextChunk = `FILE: ${file.path}\n${trimmedContent}\n`;
    if (totalChars + nextChunk.length > 30000) {
      break;
    }
    selectedFileChunks.push(nextChunk);
    totalChars += nextChunk.length;
  }

  const errorLog = readEsp32AgentProjectErrorLog(projectId);
  const chatHistory = listEsp32AgentProjectChatMessages(projectId).slice(-12);

  return [
    `Project name: ${project.name}`,
    `Preset: ${project.presetId}`,
    `Board family: ${project.boardFamily}`,
    `Display profile: ${project.displayProfile}`,
    `Build command: ${project.buildCommand}`,
    `Upload command: ${project.uploadCommand}`,
    `Saved upload port: ${project.uploadPort || "(none)"}`,
    "",
    "Recent agent conversation:",
    chatHistory.length
      ? chatHistory.map((message) => `${message.role.toUpperCase()}: ${message.content}`).join("\n")
      : "(none)",
    "",
    "Saved PlatformIO error log:",
    errorLog.trim() || "(none)",
    "",
    "Workspace file tree:",
    files.map((file) => file.path).join("\n"),
    "",
    "Selected file contents:",
    selectedFileChunks.length ? selectedFileChunks.join("\n---\n") : "(none selected)",
  ].join("\n");
}

export async function generateEsp32AgentProposal(input: {
  projectId: string;
  prompt: string;
}): Promise<Esp32AgentProposalResponse> {
  const prompt = input.prompt.trim();
  if (!prompt) {
    throw new Error("Agent prompt is required.");
  }

  const openai = getOpenAIClient();
  if (!openai) {
    throw new Error("Configure an OpenAI-compatible API key before using Agent chat.");
  }

  const runtime = getRuntimeSettings();
  const systemPrompt = [
    runtime.esp32AgentPersonalityPrompt,
    "Return JSON only.",
    "Schema:",
    '{ "reply": string, "operations": [{ "type": "write_file" | "delete_file", "path": string, "content"?: string, "summary": string }] }',
    "Use write_file for both create and update operations.",
    "Only propose changes inside the sandbox workspace.",
    "If no file changes are needed, return an empty operations array.",
  ].join(" ");

  const completion = await openai.chat.completions.create({
    model: getOpenAILLMModel(),
    stream: false,
    response_format: { type: "json_object" },
    messages: [
      {
        role: "system",
        content: systemPrompt,
      },
      {
        role: "system",
        content: buildWorkspaceContext(input.projectId, prompt),
      },
      {
        role: "user",
        content: prompt,
      },
    ],
  } as any);

  const content = completion.choices?.[0]?.message?.content || "{}";
  let parsed: any = {};
  try {
    parsed = JSON.parse(content);
  } catch {
    throw new Error("Agent response was not valid JSON.");
  }

  const reply =
    typeof parsed.reply === "string" && parsed.reply.trim()
      ? parsed.reply.trim()
      : "I reviewed the project and prepared the proposed changes below.";
  const operations = Array.isArray(parsed.operations)
    ? parsed.operations
        .map((operation: Partial<Esp32AgentProposedOperation>) =>
          sanitizeOperation(operation),
        )
        .filter(
          (
            operation: Esp32AgentProposedOperation | null,
          ): operation is Esp32AgentProposedOperation => operation !== null,
        )
    : [];

  appendEsp32AgentProjectChatMessages({
    projectId: input.projectId,
    messages: [
      { role: "user", content: prompt },
      { role: "assistant", content: reply },
    ],
  });

  return {
    reply,
    operations,
  };
}

export function applyEsp32AgentProposal(input: {
  projectId: string;
  operations: Esp32AgentProposedOperation[];
}): {
  checkpoint: { id: string; label: string; note: string; createdAt: string; fileCount: number };
  appliedCount: number;
} {
  const operations = input.operations
    .map((operation) => sanitizeOperation(operation))
    .filter(
      (operation): operation is Esp32AgentProposedOperation => operation !== null,
    );
  if (!operations.length) {
    throw new Error("No valid file operations to apply.");
  }

  const checkpoint = createEsp32AgentProjectCheckpoint({
    projectId: input.projectId,
    label: "Before agent apply",
    note: "Automatic savepoint created before applying Agent changes.",
  }).checkpoint;

  for (const operation of operations) {
    if (operation.type === "delete_file") {
      deleteEsp32AgentProjectFile({
        projectId: input.projectId,
        relativePath: operation.path,
      });
      continue;
    }
    writeEsp32AgentProjectFile({
      projectId: input.projectId,
      relativePath: operation.path,
      content: operation.content || "",
    });
  }

  return {
    checkpoint,
    appliedCount: operations.length,
  };
}
