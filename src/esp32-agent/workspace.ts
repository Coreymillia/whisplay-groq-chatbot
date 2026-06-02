import fs from "fs";
import path from "path";
import { randomUUID } from "crypto";
import { esp32AgentProjectsDir } from "../utils/dir";
import {
  DEFAULT_TEXT_LLM_MODEL,
  normalizeTextLlmModel,
} from "../config/text-llm-models";
import {
  DEFAULT_ESP32_AGENT_BOARD_ID,
  type Esp32AgentChipFamily,
  type Esp32AgentResolvedBoard,
  listEsp32AgentBoards,
  resolveEsp32AgentBoardSelection,
} from "./boards";

type Esp32AgentBoardFamily = "cyd" | "generic";
type Esp32AgentDisplayProfile = "standard" | "inverted" | "none";

interface Esp32AgentPresetDefinition {
  id: string;
  name: string;
  description: string;
  boardFamily: Esp32AgentBoardFamily;
  displayProfile: Esp32AgentDisplayProfile;
  templateSourcePath: string;
  buildCommandTemplate: string;
  uploadCommandTemplate: string;
  notes: string[];
  supportedBoardIds: string[];
  supportsCustomBoard?: boolean;
  excludedEntries?: string[];
}

export interface Esp32AgentPresetSummary {
  id: string;
  name: string;
  description: string;
  boardFamily: Esp32AgentBoardFamily;
  displayProfile: Esp32AgentDisplayProfile;
  templateSourcePath: string;
  buildCommandTemplate: string;
  uploadCommandTemplate: string;
  notes: string[];
  supportedBoardIds: string[];
  supportsCustomBoard: boolean;
}

export interface Esp32AgentProjectManifest {
  id: string;
  name: string;
  slug: string;
  agentModel: string;
  boardId: string;
  boardLabel: string;
  chipFamily: Esp32AgentChipFamily;
  uploadPort: string;
  presetId: string;
  boardFamily: Esp32AgentBoardFamily;
  displayProfile: Esp32AgentDisplayProfile;
  templateSourcePath: string;
  projectRoot: string;
  workspacePath: string;
  checkpointsPath: string;
  buildCommand: string;
  uploadCommand: string;
  createdAt: string;
  updatedAt: string;
}

export interface Esp32AgentFileNode {
  name: string;
  path: string;
  type: "file" | "directory";
  children?: Esp32AgentFileNode[];
}

export interface Esp32AgentWorkspaceSnapshotFile {
  path: string;
  contentBase64: string;
}

export interface Esp32AgentCheckpointSummary {
  id: string;
  label: string;
  note: string;
  createdAt: string;
  fileCount: number;
}

export interface Esp32AgentChatMessage {
  id: string;
  role: "user" | "assistant";
  content: string;
  createdAt: string;
}

export interface Esp32AgentWorkspaceTextFile {
  path: string;
  content: string;
}

interface Esp32AgentCheckpointRecord extends Esp32AgentCheckpointSummary {
  errorLog: string;
  files: Esp32AgentWorkspaceSnapshotFile[];
}

interface Esp32AgentProjectBundleProject {
  name: string;
  presetId: string;
  boardFamily: Esp32AgentBoardFamily;
  displayProfile: Esp32AgentDisplayProfile;
  templateSourcePath: string;
  agentModel: string;
  boardId: string;
  boardLabel: string;
  chipFamily: Esp32AgentChipFamily;
  uploadPort: string;
}

interface Esp32AgentProjectBundle {
  format: "whisplay-esp32-agent-project";
  version: 1;
  exportedAt: string;
  project: Esp32AgentProjectBundleProject;
  files: Esp32AgentWorkspaceSnapshotFile[];
  errorLog: string;
  checkpoints: Esp32AgentCheckpointRecord[];
}

const REPO_ROOT = path.resolve(__dirname, "../..");
const METADATA_FILE_NAME = "project.json";
const INTERNAL_DIR_NAME = ".whisplay-agent";
const CHECKPOINTS_DIR_NAME = "checkpoints";
const LOGS_DIR_NAME = "logs";
const ERROR_LOG_FILE_NAME = "latest-error.txt";
const CHECKPOINT_FILE_PREFIX = "checkpoint-";
const CHAT_HISTORY_FILE_NAME = "agent-chat.json";
const DEFAULT_EXCLUDED_ENTRIES = new Set([
  ".git",
  ".pio",
  "node_modules",
  "dist",
  "build",
]);

const PRESET_DEFINITIONS: Esp32AgentPresetDefinition[] = [
  {
    id: "generic-starter",
    name: "ESP32 Starter (Serial)",
    description:
      "Minimal serial-first starter for common ESP32 boards, with no CYD-specific display wiring.",
    boardFamily: "generic",
    displayProfile: "none",
    templateSourcePath: "ESP32AgentTemplates/GenericStarter",
    buildCommandTemplate: 'pio run -d "{{workspacePath}}"',
    uploadCommandTemplate: 'pio run -d "{{workspacePath}}" -t upload',
    notes: [
      "Best for bring-up on generic dev boards, ESP32-CAM, ESP32-C3, and ESP32-S3 targets.",
      "Safe starting point for unknown boards because the code only relies on Arduino + Serial.",
    ],
    supportedBoardIds: listEsp32AgentBoards().map((entry) => entry.id),
    supportsCustomBoard: true,
  },
  {
    id: "cyd-basic-inverted",
    name: "CYD Starter (Inverted Display)",
    description:
      "Minimal CYD starter for display experiments such as fill screen, text, and simple shapes.",
    boardFamily: "cyd",
    displayProfile: "inverted",
    templateSourcePath: "ESP32AgentTemplates/CYDStarter/INVERTEDdisplay",
    buildCommandTemplate: 'pio run -d "{{workspacePath}}"',
    uploadCommandTemplate: 'pio run -d "{{workspacePath}}" -t upload',
    notes: [
      "Best for simple chatbot edits and beginner experiments.",
      "Keeps only the display setup, starter text, and a few drawing primitives.",
    ],
    supportedBoardIds: ["esp32dev"],
  },
  {
    id: "cyd-basic-standard",
    name: "CYD Starter (Standard Display)",
    description:
      "Minimal CYD starter for display experiments without the full Companion app logic.",
    boardFamily: "cyd",
    displayProfile: "standard",
    templateSourcePath: "ESP32AgentTemplates/CYDStarter",
    buildCommandTemplate: 'pio run -d "{{workspacePath}}"',
    uploadCommandTemplate: 'pio run -d "{{workspacePath}}" -t upload',
    notes: [
      "Best for simple chatbot edits and beginner experiments.",
      "Starts with fill screen, starter text, and simple line/rectangle drawing.",
    ],
    supportedBoardIds: ["esp32dev"],
    excludedEntries: ["INVERTEDdisplay"],
  },
  {
    id: "cyd-inverted",
    name: "Companion CYD (Inverted Display)",
    description:
      "Starter sandbox preset based on the working CompanionCYD inverted-display firmware.",
    boardFamily: "cyd",
    displayProfile: "inverted",
    templateSourcePath: "CompanionCYD/INVERTEDdisplay",
    buildCommandTemplate: 'pio run -d "{{workspacePath}}"',
    uploadCommandTemplate: 'pio run -d "{{workspacePath}}" -t upload',
    notes: [
      "Uses the same PlatformIO env as the standard CYD build.",
      "Display inversion is enabled in src/main.cpp via gfx->invertDisplay(true).",
    ],
    supportedBoardIds: ["esp32dev"],
  },
  {
    id: "cyd-standard",
    name: "Companion CYD (Standard Display)",
    description:
      "Starter sandbox preset based on the working CompanionCYD standard-display firmware.",
    boardFamily: "cyd",
    displayProfile: "standard",
    templateSourcePath: "CompanionCYD",
    buildCommandTemplate: 'pio run -d "{{workspacePath}}"',
    uploadCommandTemplate: 'pio run -d "{{workspacePath}}" -t upload',
    notes: [
      "Uses the same PlatformIO env as the inverted CYD build.",
      "Keeps the standard display initialization without gfx->invertDisplay(true).",
    ],
    supportedBoardIds: ["esp32dev"],
    excludedEntries: ["INVERTEDdisplay"],
  },
];

function getProjectMetadataPath(projectRoot: string): string {
  return path.join(projectRoot, INTERNAL_DIR_NAME, METADATA_FILE_NAME);
}

function getProjectErrorLogPath(projectRoot: string): string {
  return path.join(projectRoot, LOGS_DIR_NAME, ERROR_LOG_FILE_NAME);
}

function getProjectCheckpointPath(projectRoot: string, checkpointId: string): string {
  return path.join(
    projectRoot,
    CHECKPOINTS_DIR_NAME,
    `${CHECKPOINT_FILE_PREFIX}${checkpointId}.json`,
  );
}

function getProjectChatHistoryPath(projectRoot: string): string {
  return path.join(projectRoot, INTERNAL_DIR_NAME, CHAT_HISTORY_FILE_NAME);
}

function sanitizeProjectName(name: string): string {
  return name
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48);
}

function sanitizeCheckpointLabel(label: string): string {
  return label
    .trim()
    .replace(/\s+/g, " ")
    .slice(0, 80);
}

function resolveTemplatePath(templateSourcePath: string): string {
  return path.resolve(REPO_ROOT, templateSourcePath);
}

function toPresetSummary(
  preset: Esp32AgentPresetDefinition,
): Esp32AgentPresetSummary {
  return {
    id: preset.id,
    name: preset.name,
    description: preset.description,
    boardFamily: preset.boardFamily,
    displayProfile: preset.displayProfile,
    templateSourcePath: preset.templateSourcePath,
    buildCommandTemplate: preset.buildCommandTemplate,
    uploadCommandTemplate: preset.uploadCommandTemplate,
    notes: [...preset.notes],
    supportedBoardIds: [...preset.supportedBoardIds],
    supportsCustomBoard: preset.supportsCustomBoard === true,
  };
}

function getPresetDefinitionOrThrow(
  presetId: string,
): Esp32AgentPresetDefinition {
  const preset = PRESET_DEFINITIONS.find((entry) => entry.id === presetId);
  if (!preset) {
    throw new Error(`Unknown ESP32 agent preset: ${presetId}`);
  }
  return preset;
}

function renderCommand(template: string, workspacePath: string): string {
  return template.split("{{workspacePath}}").join(workspacePath);
}

function normalizeUploadPort(value: unknown): string {
  return typeof value === "string" ? value.trim() : "";
}

function buildUploadCommand(
  template: string,
  workspacePath: string,
  uploadPort: string,
): string {
  const baseCommand = renderCommand(template, workspacePath);
  const normalizedPort = normalizeUploadPort(uploadPort);
  if (!normalizedPort) {
    return baseCommand;
  }
  return `${baseCommand} --upload-port ${JSON.stringify(normalizedPort)}`;
}

function resolveBoardForPresetOrThrow(
  preset: Esp32AgentPresetDefinition,
  boardId: unknown,
): Esp32AgentResolvedBoard {
  const fallbackBoardId = preset.supportedBoardIds[0] || DEFAULT_ESP32_AGENT_BOARD_ID;
  const board = resolveEsp32AgentBoardSelection(boardId, fallbackBoardId);
  if (board.isKnown && preset.supportedBoardIds.includes(board.id)) {
    return board;
  }
  if (!board.isKnown && preset.supportsCustomBoard) {
    return board;
  }
  throw new Error(
    `${preset.name} does not support the board target "${board.id}".`,
  );
}

function updateWorkspacePlatformIoBoard(
  workspacePath: string,
  boardId: string,
): void {
  const platformIoPath = path.join(workspacePath, "platformio.ini");
  if (!fs.existsSync(platformIoPath)) {
    return;
  }
  const current = fs.readFileSync(platformIoPath, "utf8");
  const next = /^\s*board\s*=.*$/m.test(current)
    ? current.replace(/^\s*board\s*=.*$/m, `board = ${boardId}`)
    : `${current.trimEnd()}\nboard = ${boardId}\n`;
  if (next !== current) {
    fs.writeFileSync(platformIoPath, next, "utf8");
  }
}

function hydrateProjectManifest(
  manifest: Esp32AgentProjectManifest,
): Esp32AgentProjectManifest {
  const preset = getPresetDefinitionOrThrow(manifest.presetId);
  const normalizedUploadPort = normalizeUploadPort(manifest.uploadPort);
  const board = resolveBoardForPresetOrThrow(
    preset,
    manifest.boardId || DEFAULT_ESP32_AGENT_BOARD_ID,
  );
  return {
    ...manifest,
    agentModel: normalizeTextLlmModel(
      manifest.agentModel || DEFAULT_TEXT_LLM_MODEL,
    ),
    boardId: board.id,
    boardLabel: board.label,
    chipFamily: board.chipFamily,
    uploadPort: normalizedUploadPort,
    buildCommand: renderCommand(preset.buildCommandTemplate, manifest.workspacePath),
    uploadCommand: buildUploadCommand(
      preset.uploadCommandTemplate,
      manifest.workspacePath,
      normalizedUploadPort,
    ),
  };
}

function ensureTemplateExists(sourcePath: string): void {
  if (!fs.existsSync(sourcePath)) {
    throw new Error(`Preset template path not found: ${sourcePath}`);
  }
}

function ensureProjectsRootExists(): void {
  fs.mkdirSync(esp32AgentProjectsDir, { recursive: true });
}

function copyTemplateTree(
  sourceDir: string,
  destinationDir: string,
  excludedEntries: Set<string>,
): void {
  fs.mkdirSync(destinationDir, { recursive: true });
  const entries = fs.readdirSync(sourceDir, { withFileTypes: true });
  for (const entry of entries) {
    if (excludedEntries.has(entry.name)) {
      continue;
    }
    const sourcePath = path.join(sourceDir, entry.name);
    const destinationPath = path.join(destinationDir, entry.name);
    if (entry.isDirectory()) {
      copyTemplateTree(sourcePath, destinationPath, excludedEntries);
      continue;
    }
    if (entry.isFile()) {
      fs.mkdirSync(path.dirname(destinationPath), { recursive: true });
      fs.copyFileSync(sourcePath, destinationPath);
    }
  }
}

function writeProjectManifest(
  projectRoot: string,
  manifest: Esp32AgentProjectManifest,
): void {
  const metadataPath = getProjectMetadataPath(projectRoot);
  fs.mkdirSync(path.dirname(metadataPath), { recursive: true });
  fs.writeFileSync(
    metadataPath,
    `${JSON.stringify(manifest, null, 2)}\n`,
    "utf8",
  );
}

function readProjectManifest(
  projectRoot: string,
): Esp32AgentProjectManifest | null {
  const metadataPath = getProjectMetadataPath(projectRoot);
  if (!fs.existsSync(metadataPath)) {
    return null;
  }
  const raw = fs.readFileSync(metadataPath, "utf8");
  return hydrateProjectManifest(JSON.parse(raw) as Esp32AgentProjectManifest);
}

function createProjectManifest(input: {
  name: string;
  preset: Esp32AgentPresetDefinition;
  agentModel?: string;
  board: Esp32AgentResolvedBoard;
}): Esp32AgentProjectManifest {
  ensureProjectsRootExists();
  const trimmedName = input.name.trim();
  if (!trimmedName) {
    throw new Error("Project name is required.");
  }

  const slugBase = sanitizeProjectName(trimmedName) || "esp32-project";
  const projectId = randomUUID();
  const projectRoot = path.join(
    esp32AgentProjectsDir,
    `${slugBase}-${projectId.slice(0, 8)}`,
  );
  const workspacePath = path.join(projectRoot, "workspace");
  const checkpointsPath = path.join(projectRoot, CHECKPOINTS_DIR_NAME);
  fs.mkdirSync(workspacePath, { recursive: true });
  fs.mkdirSync(checkpointsPath, { recursive: true });

  const now = new Date().toISOString();
  const manifest: Esp32AgentProjectManifest = {
    id: projectId,
    name: trimmedName,
    slug: slugBase,
    agentModel: normalizeTextLlmModel(
      input.agentModel || DEFAULT_TEXT_LLM_MODEL,
    ),
    boardId: input.board.id,
    boardLabel: input.board.label,
    chipFamily: input.board.chipFamily,
    uploadPort: "",
    presetId: input.preset.id,
    boardFamily: input.preset.boardFamily,
    displayProfile: input.preset.displayProfile,
    templateSourcePath: input.preset.templateSourcePath,
    projectRoot,
    workspacePath,
    checkpointsPath,
    buildCommand: renderCommand(input.preset.buildCommandTemplate, workspacePath),
    uploadCommand: buildUploadCommand(
      input.preset.uploadCommandTemplate,
      workspacePath,
      "",
    ),
    createdAt: now,
    updatedAt: now,
  };
  writeProjectManifest(projectRoot, manifest);
  return manifest;
}

function collectWorkspaceSnapshotFiles(
  dirPath: string,
  basePath = "",
): Esp32AgentWorkspaceSnapshotFile[] {
  if (!fs.existsSync(dirPath)) {
    return [];
  }
  const entries = fs
    .readdirSync(dirPath, { withFileTypes: true })
    .sort((left, right) => left.name.localeCompare(right.name));
  const files: Esp32AgentWorkspaceSnapshotFile[] = [];
  for (const entry of entries) {
    const relativePath = basePath ? `${basePath}/${entry.name}` : entry.name;
    const fullPath = path.join(dirPath, entry.name);
    if (entry.isDirectory()) {
      files.push(...collectWorkspaceSnapshotFiles(fullPath, relativePath));
      continue;
    }
    if (!entry.isFile()) {
      continue;
    }
    files.push({
      path: relativePath,
      contentBase64: fs.readFileSync(fullPath).toString("base64"),
    });
  }
  return files;
}

function clearDirectoryContents(dirPath: string): void {
  fs.rmSync(dirPath, { recursive: true, force: true });
  fs.mkdirSync(dirPath, { recursive: true });
}

function restoreWorkspaceSnapshot(
  workspacePath: string,
  files: Esp32AgentWorkspaceSnapshotFile[],
): void {
  clearDirectoryContents(workspacePath);
  for (const entry of files) {
    const normalizedRelativePath = path.posix.normalize(entry.path.trim());
    if (
      !normalizedRelativePath ||
      normalizedRelativePath === "." ||
      normalizedRelativePath.startsWith("../") ||
      path.posix.isAbsolute(normalizedRelativePath)
    ) {
      throw new Error("Snapshot contains a file path outside the project workspace.");
    }
    const destinationPath = path.resolve(workspacePath, normalizedRelativePath);
    const workspacePrefix = `${path.resolve(workspacePath)}${path.sep}`;
    if (
      destinationPath !== path.resolve(workspacePath) &&
      !destinationPath.startsWith(workspacePrefix)
    ) {
      throw new Error("Snapshot restore path escapes the project workspace.");
    }
    fs.mkdirSync(path.dirname(destinationPath), { recursive: true });
    fs.writeFileSync(destinationPath, Buffer.from(entry.contentBase64, "base64"));
  }
}

function readProjectCheckpointRecord(
  checkpointPath: string,
): Esp32AgentCheckpointRecord {
  const raw = fs.readFileSync(checkpointPath, "utf8");
  return JSON.parse(raw) as Esp32AgentCheckpointRecord;
}

function writeProjectCheckpointRecord(
  project: Esp32AgentProjectManifest,
  record: Esp32AgentCheckpointRecord,
): void {
  const checkpointPath = getProjectCheckpointPath(project.projectRoot, record.id);
  fs.mkdirSync(path.dirname(checkpointPath), { recursive: true });
  fs.writeFileSync(checkpointPath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
}

function listProjectCheckpointRecords(
  project: Esp32AgentProjectManifest,
): Esp32AgentCheckpointRecord[] {
  if (!fs.existsSync(project.checkpointsPath)) {
    return [];
  }
  return fs
    .readdirSync(project.checkpointsPath, { withFileTypes: true })
    .filter(
      (entry) =>
        entry.isFile() &&
        entry.name.startsWith(CHECKPOINT_FILE_PREFIX) &&
        entry.name.endsWith(".json"),
    )
    .map((entry) =>
      readProjectCheckpointRecord(path.join(project.checkpointsPath, entry.name)),
    )
    .sort((left, right) => right.createdAt.localeCompare(left.createdAt));
}

function toCheckpointSummary(
  record: Esp32AgentCheckpointRecord,
): Esp32AgentCheckpointSummary {
  return {
    id: record.id,
    label: record.label,
    note: record.note,
    createdAt: record.createdAt,
    fileCount: record.fileCount,
  };
}

function writeProjectErrorLog(projectRoot: string, content: string): void {
  const logPath = getProjectErrorLogPath(projectRoot);
  fs.mkdirSync(path.dirname(logPath), { recursive: true });
  fs.writeFileSync(logPath, content, "utf8");
}

function readProjectChatHistory(
  projectRoot: string,
): Esp32AgentChatMessage[] {
  const chatHistoryPath = getProjectChatHistoryPath(projectRoot);
  if (!fs.existsSync(chatHistoryPath)) {
    return [];
  }
  try {
    const parsed = JSON.parse(fs.readFileSync(chatHistoryPath, "utf8"));
    if (!Array.isArray(parsed)) {
      return [];
    }
    return parsed.filter((entry) => {
      return (
        entry &&
        typeof entry === "object" &&
        typeof entry.id === "string" &&
        (entry.role === "user" || entry.role === "assistant") &&
        typeof entry.content === "string" &&
        typeof entry.createdAt === "string"
      );
    }) as Esp32AgentChatMessage[];
  } catch {
    return [];
  }
}

function writeProjectChatHistory(
  projectRoot: string,
  messages: Esp32AgentChatMessage[],
): void {
  const chatHistoryPath = getProjectChatHistoryPath(projectRoot);
  fs.mkdirSync(path.dirname(chatHistoryPath), { recursive: true });
  fs.writeFileSync(
    chatHistoryPath,
    `${JSON.stringify(messages, null, 2)}\n`,
    "utf8",
  );
}

function parseProjectBundleOrThrow(bundleContent: string): Esp32AgentProjectBundle {
  let bundle: unknown;
  try {
    bundle = JSON.parse(bundleContent);
  } catch {
    throw new Error("Project import file is not valid JSON.");
  }

  if (!bundle || typeof bundle !== "object" || Array.isArray(bundle)) {
    throw new Error("Project import bundle is invalid.");
  }

  const typedBundle = bundle as Partial<Esp32AgentProjectBundle>;
  if (
    typedBundle.format !== "whisplay-esp32-agent-project" ||
    typedBundle.version !== 1
  ) {
    throw new Error("Unsupported ESP32 agent import bundle format.");
  }
  if (!typedBundle.project?.name || !typedBundle.project?.presetId) {
    throw new Error("Import bundle is missing project metadata.");
  }
  if (!Array.isArray(typedBundle.files)) {
    throw new Error("Import bundle is missing workspace files.");
  }

  return {
    format: "whisplay-esp32-agent-project",
    version: 1,
    exportedAt:
      typeof typedBundle.exportedAt === "string"
        ? typedBundle.exportedAt
        : new Date().toISOString(),
    project: {
      name: String(typedBundle.project.name),
      presetId: String(typedBundle.project.presetId),
      boardFamily: typedBundle.project.boardFamily || "cyd",
      displayProfile: typedBundle.project.displayProfile || "standard",
      templateSourcePath: String(typedBundle.project.templateSourcePath || ""),
      agentModel: normalizeTextLlmModel(
        String(typedBundle.project.agentModel || DEFAULT_TEXT_LLM_MODEL),
      ),
      boardId: String(
        typedBundle.project.boardId || DEFAULT_ESP32_AGENT_BOARD_ID,
      ),
      boardLabel: String(typedBundle.project.boardLabel || ""),
      chipFamily:
        typedBundle.project.chipFamily === "esp32c3" ||
        typedBundle.project.chipFamily === "esp32s3" ||
        typedBundle.project.chipFamily === "unknown"
          ? typedBundle.project.chipFamily
          : "esp32",
      uploadPort: normalizeUploadPort(typedBundle.project.uploadPort),
    },
    files: typedBundle.files.map((entry) => ({
      path: String(entry.path || ""),
      contentBase64: String(entry.contentBase64 || ""),
    })),
    errorLog: String(typedBundle.errorLog || ""),
    checkpoints: Array.isArray(typedBundle.checkpoints)
      ? typedBundle.checkpoints.map((entry) => ({
          id: String(entry.id || randomUUID()),
          label: sanitizeCheckpointLabel(String(entry.label || "Imported savepoint")),
          note: String(entry.note || ""),
          createdAt:
            typeof entry.createdAt === "string"
              ? entry.createdAt
              : new Date().toISOString(),
          fileCount: Number(entry.fileCount || 0),
          errorLog: String(entry.errorLog || ""),
          files: Array.isArray(entry.files)
            ? entry.files.map((file) => ({
                path: String(file.path || ""),
                contentBase64: String(file.contentBase64 || ""),
              }))
            : [],
        }))
      : [],
  };
}

export function listEsp32AgentPresets(): Esp32AgentPresetSummary[] {
  return PRESET_DEFINITIONS.map(toPresetSummary);
}

export function createEsp32AgentProject(input: {
  name: string;
  presetId: string;
  agentModel?: string;
  boardId?: string;
}): Esp32AgentProjectManifest {
  const preset = getPresetDefinitionOrThrow(input.presetId);
  const templatePath = resolveTemplatePath(preset.templateSourcePath);
  ensureTemplateExists(templatePath);
  const board = resolveBoardForPresetOrThrow(preset, input.boardId);

  const excludedEntries = new Set(DEFAULT_EXCLUDED_ENTRIES);
  for (const entry of preset.excludedEntries || []) {
    excludedEntries.add(entry);
  }

  const manifest = createProjectManifest({
    name: input.name,
    preset,
    agentModel: input.agentModel,
    board,
  });
  copyTemplateTree(templatePath, manifest.workspacePath, excludedEntries);
  updateWorkspacePlatformIoBoard(manifest.workspacePath, manifest.boardId);
  return manifest;
}

export function importEsp32AgentProject(
  bundleContent: string,
): Esp32AgentProjectManifest {
  const bundle = parseProjectBundleOrThrow(bundleContent);
  const preset = getPresetDefinitionOrThrow(bundle.project.presetId);
  const board = resolveBoardForPresetOrThrow(preset, bundle.project.boardId);
  const manifest = createProjectManifest({
    name: bundle.project.name,
    preset,
    agentModel: bundle.project.agentModel,
    board,
  });

  restoreWorkspaceSnapshot(manifest.workspacePath, bundle.files);
  updateWorkspacePlatformIoBoard(manifest.workspacePath, manifest.boardId);
  writeProjectErrorLog(manifest.projectRoot, bundle.errorLog);

  for (const checkpoint of bundle.checkpoints) {
    writeProjectCheckpointRecord(manifest, {
      ...checkpoint,
      id: checkpoint.id || randomUUID(),
      label: sanitizeCheckpointLabel(checkpoint.label || "Imported savepoint"),
      fileCount: checkpoint.files.length,
    });
  }

  return updateProjectManifest(manifest.id, (project) => project);
}

export function listEsp32AgentProjects(): Esp32AgentProjectManifest[] {
  if (!fs.existsSync(esp32AgentProjectsDir)) {
    return [];
  }
  const entries = fs.readdirSync(esp32AgentProjectsDir, {
    withFileTypes: true,
  });
  return entries
    .filter((entry) => entry.isDirectory())
    .map((entry) =>
      readProjectManifest(path.join(esp32AgentProjectsDir, entry.name)),
    )
    .filter(
      (manifest): manifest is Esp32AgentProjectManifest => manifest !== null,
    )
    .sort((left, right) => right.updatedAt.localeCompare(left.updatedAt));
}

function getProjectManifestOrThrow(projectId: string): Esp32AgentProjectManifest {
  const project = listEsp32AgentProjects().find((entry) => entry.id === projectId);
  if (!project) {
    throw new Error(`ESP32 agent project not found: ${projectId}`);
  }
  return project;
}

function resolveWorkspacePathOrThrow(
  project: Esp32AgentProjectManifest,
  relativePath: string,
): string {
  const trimmedPath = relativePath.trim().replace(/\\/g, "/");
  if (!trimmedPath) {
    throw new Error("File path is required.");
  }
  const normalizedRelativePath = path.posix.normalize(trimmedPath);
  if (
    normalizedRelativePath.startsWith("../") ||
    normalizedRelativePath === ".." ||
    path.posix.isAbsolute(normalizedRelativePath)
  ) {
    throw new Error("File path must stay inside the project workspace.");
  }
  const resolvedPath = path.resolve(project.workspacePath, normalizedRelativePath);
  const workspacePrefix = `${path.resolve(project.workspacePath)}${path.sep}`;
  if (
    resolvedPath !== path.resolve(project.workspacePath) &&
    !resolvedPath.startsWith(workspacePrefix)
  ) {
    throw new Error("Resolved path escapes the project workspace.");
  }
  return resolvedPath;
}

function updateProjectManifest(
  projectId: string,
  updater: (project: Esp32AgentProjectManifest) => Esp32AgentProjectManifest,
): Esp32AgentProjectManifest {
  const existing = getProjectManifestOrThrow(projectId);
  const next = updater(existing);
  const updated = hydrateProjectManifest({
    ...next,
    updatedAt: new Date().toISOString(),
  });
  writeProjectManifest(existing.projectRoot, updated);
  return updated;
}

function buildFileTree(dirPath: string, basePath = ""): Esp32AgentFileNode[] {
  const entries = fs
    .readdirSync(dirPath, { withFileTypes: true })
    .filter((entry) => !entry.name.startsWith("."))
    .sort((left, right) => {
      if (left.isDirectory() && !right.isDirectory()) {
        return -1;
      }
      if (!left.isDirectory() && right.isDirectory()) {
        return 1;
      }
      return left.name.localeCompare(right.name);
    });

  return entries.map((entry) => {
    const relativePath = basePath
      ? `${basePath}/${entry.name}`
      : entry.name;
    if (entry.isDirectory()) {
      return {
        name: entry.name,
        path: relativePath,
        type: "directory" as const,
        children: buildFileTree(path.join(dirPath, entry.name), relativePath),
      };
    }
    return {
      name: entry.name,
      path: relativePath,
      type: "file" as const,
    };
  });
}

export function getEsp32AgentProject(projectId: string): Esp32AgentProjectManifest {
  return getProjectManifestOrThrow(projectId);
}

export function updateEsp32AgentProjectSettings(input: {
  projectId: string;
  agentModel?: string;
  boardId?: string;
  uploadPort?: string;
}): Esp32AgentProjectManifest {
  const updatedProject = updateProjectManifest(input.projectId, (project) => {
    const preset = getPresetDefinitionOrThrow(project.presetId);
    const board = resolveBoardForPresetOrThrow(
      preset,
      typeof input.boardId === "string" ? input.boardId : project.boardId,
    );
    return {
      ...project,
      agentModel: normalizeTextLlmModel(input.agentModel || project.agentModel),
      boardId: board.id,
      boardLabel: board.label,
      chipFamily: board.chipFamily,
      uploadPort:
        typeof input.uploadPort === "string"
          ? normalizeUploadPort(input.uploadPort)
          : project.uploadPort,
    };
  });
  updateWorkspacePlatformIoBoard(updatedProject.workspacePath, updatedProject.boardId);
  return updatedProject;
}

export function listEsp32AgentProjectFiles(
  projectId: string,
): Esp32AgentFileNode[] {
  const project = getProjectManifestOrThrow(projectId);
  return buildFileTree(project.workspacePath);
}

export function readEsp32AgentProjectFile(input: {
  projectId: string;
  relativePath: string;
}): { path: string; content: string } {
  const project = getProjectManifestOrThrow(input.projectId);
  const filePath = resolveWorkspacePathOrThrow(project, input.relativePath);
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    throw new Error(`Project file not found: ${input.relativePath}`);
  }
  return {
    path: input.relativePath,
    content: fs.readFileSync(filePath, "utf8"),
  };
}

export function writeEsp32AgentProjectFile(input: {
  projectId: string;
  relativePath: string;
  content: string;
}): Esp32AgentProjectManifest {
  const project = getProjectManifestOrThrow(input.projectId);
  const filePath = resolveWorkspacePathOrThrow(project, input.relativePath);
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, input.content, "utf8");
  return updateProjectManifest(input.projectId, (entry) => entry);
}

export function deleteEsp32AgentProjectFile(input: {
  projectId: string;
  relativePath: string;
}): Esp32AgentProjectManifest {
  const project = getProjectManifestOrThrow(input.projectId);
  const filePath = resolveWorkspacePathOrThrow(project, input.relativePath);
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    throw new Error(`Project file not found: ${input.relativePath}`);
  }
  fs.rmSync(filePath, { force: true });

  let currentDir = path.dirname(filePath);
  const workspaceRoot = path.resolve(project.workspacePath);
  while (currentDir.startsWith(workspaceRoot) && currentDir !== workspaceRoot) {
    if (fs.readdirSync(currentDir).length > 0) {
      break;
    }
    fs.rmdirSync(currentDir);
    currentDir = path.dirname(currentDir);
  }

  return updateProjectManifest(input.projectId, (entry) => entry);
}

export function readEsp32AgentProjectErrorLog(projectId: string): string {
  const project = getProjectManifestOrThrow(projectId);
  const logPath = getProjectErrorLogPath(project.projectRoot);
  if (!fs.existsSync(logPath)) {
    return "";
  }
  return fs.readFileSync(logPath, "utf8");
}

export function listEsp32AgentProjectChatMessages(
  projectId: string,
): Esp32AgentChatMessage[] {
  const project = getProjectManifestOrThrow(projectId);
  return readProjectChatHistory(project.projectRoot);
}

export function appendEsp32AgentProjectChatMessages(input: {
  projectId: string;
  messages: Array<{
    role: "user" | "assistant";
    content: string;
  }>;
}): Esp32AgentProjectManifest {
  const project = getProjectManifestOrThrow(input.projectId);
  const existing = readProjectChatHistory(project.projectRoot);
  const additions = input.messages
    .map((message) => ({
      id: randomUUID(),
      role: message.role,
      content: message.content.trim(),
      createdAt: new Date().toISOString(),
    }))
    .filter((message) => message.content);
  writeProjectChatHistory(project.projectRoot, [...existing, ...additions].slice(-40));
  return updateProjectManifest(input.projectId, (entry) => entry);
}

export function listEsp32AgentProjectWorkspaceTextFiles(
  projectId: string,
): Esp32AgentWorkspaceTextFile[] {
  const project = getProjectManifestOrThrow(projectId);
  return collectWorkspaceSnapshotFiles(project.workspacePath).map((entry) => ({
    path: entry.path,
    content: Buffer.from(entry.contentBase64, "base64").toString("utf8"),
  }));
}

export function writeEsp32AgentProjectErrorLog(input: {
  projectId: string;
  content: string;
}): Esp32AgentProjectManifest {
  const project = getProjectManifestOrThrow(input.projectId);
  writeProjectErrorLog(project.projectRoot, input.content);
  return updateProjectManifest(input.projectId, (entry) => entry);
}

export function listEsp32AgentProjectCheckpoints(
  projectId: string,
): Esp32AgentCheckpointSummary[] {
  const project = getProjectManifestOrThrow(projectId);
  return listProjectCheckpointRecords(project).map(toCheckpointSummary);
}

export function createEsp32AgentProjectCheckpoint(input: {
  projectId: string;
  label?: string;
  note?: string;
}): {
  project: Esp32AgentProjectManifest;
  checkpoint: Esp32AgentCheckpointSummary;
} {
  const project = getProjectManifestOrThrow(input.projectId);
  const record: Esp32AgentCheckpointRecord = {
    id: randomUUID(),
    label:
      sanitizeCheckpointLabel(input.label || "") ||
      `Savepoint ${new Date().toLocaleString()}`,
    note: (input.note || "").trim().slice(0, 240),
    createdAt: new Date().toISOString(),
    errorLog: readEsp32AgentProjectErrorLog(project.id),
    files: collectWorkspaceSnapshotFiles(project.workspacePath),
    fileCount: 0,
  };
  record.fileCount = record.files.length;
  writeProjectCheckpointRecord(project, record);
  const updatedProject = updateProjectManifest(project.id, (entry) => entry);
  return {
    project: updatedProject,
    checkpoint: toCheckpointSummary(record),
  };
}

export function restoreEsp32AgentProjectCheckpoint(input: {
  projectId: string;
  checkpointId: string;
}): {
  project: Esp32AgentProjectManifest;
  checkpoint: Esp32AgentCheckpointSummary;
} {
  const project = getProjectManifestOrThrow(input.projectId);
  const checkpoint = listProjectCheckpointRecords(project).find(
    (entry) => entry.id === input.checkpointId,
  );
  if (!checkpoint) {
    throw new Error(`ESP32 agent savepoint not found: ${input.checkpointId}`);
  }
  restoreWorkspaceSnapshot(project.workspacePath, checkpoint.files);
  writeProjectErrorLog(project.projectRoot, checkpoint.errorLog);
  const updatedProject = updateProjectManifest(project.id, (entry) => entry);
  return {
    project: updatedProject,
    checkpoint: toCheckpointSummary(checkpoint),
  };
}

export function exportEsp32AgentProject(projectId: string): {
  fileName: string;
  bundle: string;
} {
  const project = getProjectManifestOrThrow(projectId);
  const bundle: Esp32AgentProjectBundle = {
    format: "whisplay-esp32-agent-project",
    version: 1,
    exportedAt: new Date().toISOString(),
    project: {
      name: project.name,
      presetId: project.presetId,
      boardFamily: project.boardFamily,
      displayProfile: project.displayProfile,
      templateSourcePath: project.templateSourcePath,
      agentModel: project.agentModel,
      boardId: project.boardId,
      boardLabel: project.boardLabel,
      chipFamily: project.chipFamily,
      uploadPort: project.uploadPort,
    },
    files: collectWorkspaceSnapshotFiles(project.workspacePath),
    errorLog: readEsp32AgentProjectErrorLog(project.id),
    checkpoints: listProjectCheckpointRecords(project),
  };

  return {
    fileName: `${project.slug || "esp32-project"}-bundle.json`,
    bundle: `${JSON.stringify(bundle, null, 2)}\n`,
  };
}
