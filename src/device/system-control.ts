import { spawn } from "child_process";

interface ShutdownAttempt {
  label: string;
  command: string;
  args: string[];
}

const SHUTDOWN_ATTEMPTS: ShutdownAttempt[] = [
  {
    label: "sudo systemctl poweroff",
    command: "sudo",
    args: ["-n", "systemctl", "poweroff"],
  },
  {
    label: "sudo shutdown -h now",
    command: "sudo",
    args: ["-n", "shutdown", "-h", "now"],
  },
  {
    label: "sudo poweroff",
    command: "sudo",
    args: ["-n", "poweroff"],
  },
  {
    label: "systemctl poweroff",
    command: "systemctl",
    args: ["poweroff"],
  },
  {
    label: "loginctl poweroff",
    command: "loginctl",
    args: ["poweroff"],
  },
  {
    label: "shutdown -h now",
    command: "shutdown",
    args: ["-h", "now"],
  },
  {
    label: "poweroff",
    command: "poweroff",
    args: [],
  },
];

function runShutdownAttempt(attempt: ShutdownAttempt): Promise<void> {
  return new Promise((resolve, reject) => {
    let settled = false;
    const child = spawn(attempt.command, attempt.args, {
      detached: true,
      stdio: "ignore",
    });

    const successTimer = setTimeout(() => {
      if (settled) {
        return;
      }
      settled = true;
      resolve();
    }, 1000);

    child.once("error", (error) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(successTimer);
      reject(
        new Error(`${attempt.label} failed to start: ${error.message}`),
      );
    });

    child.once("exit", (code, signal) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(successTimer);
      if (code === 0) {
        resolve();
        return;
      }
      reject(
        new Error(
          `${attempt.label} exited with ${code ?? signal ?? "unknown status"}`,
        ),
      );
    });

    child.unref();
  });
}

export async function requestSystemShutdown(): Promise<void> {
  const errors: string[] = [];

  for (const attempt of SHUTDOWN_ATTEMPTS) {
    try {
      console.log(`[System] Trying shutdown via ${attempt.label}`);
      await runShutdownAttempt(attempt);
      console.log(`[System] Shutdown requested via ${attempt.label}`);
      return;
    } catch (error) {
      const message =
        error instanceof Error ? error.message : String(error);
      console.error(`[System] ${message}`);
      errors.push(message);
    }
  }

  throw new Error(`Unable to request shutdown. ${errors.join(" | ")}`);
}
