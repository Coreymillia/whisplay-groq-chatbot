import { execFile } from "child_process";
import fs from "fs";
import { Socket } from "net";
import { resolve } from "path";

const CAMERA_DAEMON_HOST = process.env.WHISPLAY_CAMERA_DAEMON_HOST || "127.0.0.1";
const CAMERA_DAEMON_PORT = parseInt(process.env.WHISPLAY_CAMERA_DAEMON_PORT || "18765", 10);

export function sendCameraDaemonCommand(
  cmd: string,
  payload: Record<string, unknown> = {},
  timeoutMs = 4000,
): Promise<Record<string, unknown>> {
  return new Promise((resolvePromise, rejectPromise) => {
    const socket = new Socket();
    let settled = false;
    let buffer = "";

    const finish = (error: Error | null, result?: Record<string, unknown>) => {
      if (settled) {
        return;
      }
      settled = true;
      socket.destroy();
      if (error) {
        rejectPromise(error);
        return;
      }
      resolvePromise(result || {});
    };

    socket.setTimeout(timeoutMs);
    socket.connect(CAMERA_DAEMON_PORT, CAMERA_DAEMON_HOST, () => {
      socket.write(`${JSON.stringify({ cmd, ...payload })}\n`);
    });
    socket.on("data", (chunk) => {
      buffer += chunk.toString("utf8");
      const newlineIndex = buffer.indexOf("\n");
      if (newlineIndex < 0) {
        return;
      }
      const line = buffer.slice(0, newlineIndex).trim();
      if (!line) {
        finish(new Error("Camera daemon returned an empty response."));
        return;
      }
      try {
        finish(null, JSON.parse(line) as Record<string, unknown>);
      } catch (error) {
        finish(
          error instanceof Error
            ? error
            : new Error("Camera daemon returned invalid JSON."),
        );
      }
    });
    socket.on("error", (error) => finish(error));
    socket.on("timeout", () =>
      finish(new Error("Timed out waiting for the camera daemon.")));
    socket.on("close", () => {
      if (!settled && buffer.trim()) {
        try {
          finish(null, JSON.parse(buffer.trim()) as Record<string, unknown>);
          return;
        } catch {
          // fall through
        }
      }
      if (!settled) {
        finish(new Error("Camera daemon closed before returning a response."));
      }
    });
  });
}

export function ensureCameraDaemonReady(): Promise<void> {
  return new Promise((resolvePromise, rejectPromise) => {
    const pythonDir = resolve(__dirname, "../../python");
    const scriptPath = resolve(pythonDir, "camera.py");
    execFile(
      "python3",
      [scriptPath, "--ensure-daemon"],
      { cwd: pythonDir },
      (error, stdout, stderr) => {
        if (error) {
          rejectPromise(
            new Error(stderr?.trim() || stdout?.trim() || error.message),
          );
          return;
        }
        resolvePromise();
      },
    );
  });
}

export async function captureCameraImage(
  imagePath: string,
  timeoutMs = 8000,
): Promise<void> {
  await ensureCameraDaemonReady();
  const response = await sendCameraDaemonCommand(
    "capture",
    { path: imagePath },
    timeoutMs,
  );
  if (!response.ok || !fs.existsSync(imagePath)) {
    throw new Error(
      typeof response.error === "string"
        ? response.error
        : "Capture failed.",
    );
  }
}
