import { ChildProcessWithoutNullStreams, spawn } from "child_process";
import { getEsp32AgentProject } from "./workspace";

type TerminalStatus = "idle" | "running" | "exited" | "failed" | "stopped";

interface TerminalSession {
  process: ChildProcessWithoutNullStreams | null;
  status: TerminalStatus;
  command: string;
  output: string;
  exitCode: number | null;
  startedAt: string | null;
  updatedAt: string | null;
}

export interface Esp32AgentTerminalSnapshot {
  status: TerminalStatus;
  command: string;
  output: string;
  exitCode: number | null;
  startedAt: string | null;
  updatedAt: string | null;
}

const MAX_OUTPUT_CHARS = 200000;
const terminalSessions = new Map<string, TerminalSession>();

function trimOutput(output: string): string {
  if (output.length <= MAX_OUTPUT_CHARS) {
    return output;
  }
  return output.slice(output.length - MAX_OUTPUT_CHARS);
}

function getSession(projectId: string): TerminalSession {
  const existing = terminalSessions.get(projectId);
  if (existing) {
    return existing;
  }
  const next: TerminalSession = {
    process: null,
    status: "idle",
    command: "",
    output: "",
    exitCode: null,
    startedAt: null,
    updatedAt: null,
  };
  terminalSessions.set(projectId, next);
  return next;
}

function toSnapshot(session: TerminalSession): Esp32AgentTerminalSnapshot {
  return {
    status: session.status,
    command: session.command,
    output: session.output,
    exitCode: session.exitCode,
    startedAt: session.startedAt,
    updatedAt: session.updatedAt,
  };
}

function appendOutput(session: TerminalSession, chunk: string): void {
  session.output = trimOutput(`${session.output}${chunk}`);
  session.updatedAt = new Date().toISOString();
}

export function getEsp32AgentTerminalSnapshot(
  projectId: string,
): Esp32AgentTerminalSnapshot {
  getEsp32AgentProject(projectId);
  return toSnapshot(getSession(projectId));
}

export function startEsp32AgentTerminalCommand(input: {
  projectId: string;
  command: string;
}): Esp32AgentTerminalSnapshot {
  const project = getEsp32AgentProject(input.projectId);
  const command = input.command.trim();
  if (!command) {
    throw new Error("Terminal command is required.");
  }

  const session = getSession(input.projectId);
  if (session.process && session.status === "running") {
    throw new Error("A terminal command is already running for this project.");
  }

  const homeDir = process.env.HOME || "";
  const child = spawn("bash", ["-lc", command], {
    cwd: project.workspacePath,
    env: {
      ...process.env,
      PATH: `${homeDir ? `${homeDir}/.local/bin:` : ""}${process.env.PATH || ""}`,
    },
  });

  session.process = child;
  session.status = "running";
  session.command = command;
  session.output = `$ ${command}\n`;
  session.exitCode = null;
  session.startedAt = new Date().toISOString();
  session.updatedAt = session.startedAt;

  child.stdout.on("data", (chunk: Buffer) => {
    appendOutput(session, chunk.toString("utf8"));
  });

  child.stderr.on("data", (chunk: Buffer) => {
    appendOutput(session, chunk.toString("utf8"));
  });

  child.on("error", (error) => {
    session.status = "failed";
    session.exitCode = null;
    session.process = null;
    appendOutput(session, `\n[terminal error] ${error.message}\n`);
  });

  child.on("close", (code) => {
    session.exitCode = code;
    if (session.status !== "stopped") {
      session.status = code === 0 ? "exited" : "failed";
    }
    session.process = null;
    appendOutput(
      session,
      session.status === "stopped"
        ? ""
        : `\n[process ${code === 0 ? "completed" : "exited"}${code !== null ? ` with code ${code}` : ""}]\n`,
    );
  });

  return toSnapshot(session);
}

export function stopEsp32AgentTerminalCommand(
  projectId: string,
): Esp32AgentTerminalSnapshot {
  getEsp32AgentProject(projectId);
  const session = getSession(projectId);
  if (session.process && session.status === "running") {
    session.process.kill("SIGTERM");
    session.process = null;
    session.status = "stopped";
    session.exitCode = null;
    appendOutput(session, "\n[process stopped]\n");
  }
  return toSnapshot(session);
}
