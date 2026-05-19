import fs from "fs";
import { Socket } from "net";
import { resolve } from "path";

const PISUGAR_HOST = process.env.PISUGAR_HOST || "127.0.0.1";
const PISUGAR_PORT = parseInt(process.env.PISUGAR_PORT || "8423", 10);
const PISUGAR_BUTTON_ACTIONS_ENABLED =
  (process.env.PISUGAR_BUTTON_ACTIONS_ENABLED || "true").toLowerCase() !== "false";

function sendPiSugarCommand(command: string, timeoutMs = 2000): Promise<string> {
  return new Promise((resolvePromise, rejectPromise) => {
    const socket = new Socket();
    let settled = false;
    let buffer = "";

    const finish = (error?: Error | null, response = "") => {
      if (settled) {
        return;
      }
      settled = true;
      socket.destroy();
      if (error) {
        rejectPromise(error);
        return;
      }
      resolvePromise(response.trim());
    };

    socket.setTimeout(timeoutMs);
    socket.connect(PISUGAR_PORT, PISUGAR_HOST, () => {
      socket.write(`${command}\n`);
    });
    socket.on("data", (chunk) => {
      buffer += chunk.toString("utf8");
      const newlineIndex = buffer.indexOf("\n");
      if (newlineIndex >= 0) {
        finish(null, buffer.slice(0, newlineIndex));
      }
    });
    socket.on("error", (error) => finish(error));
    socket.on("timeout", () =>
      finish(new Error("Timed out waiting for PiSugar service response.")));
    socket.on("close", () => {
      if (!settled) {
        finish(null, buffer);
      }
    });
  });
}

async function applyPiSugarCommand(
  command: string,
  description: string,
): Promise<void> {
  const response = await sendPiSugarCommand(command);
  console.log(
    `[PiSugar] ${description}${response ? ` -> ${response}` : ""}`,
  );
}

export async function startPiSugarButtonSupport(): Promise<void> {
  if (!PISUGAR_BUTTON_ACTIONS_ENABLED) {
    console.log("[PiSugar] Button actions disabled via env.");
    return;
  }

  const shortPressScript = resolve(__dirname, "../../scripts/pisugar-short-press.sh");
  const longPressScript = resolve(__dirname, "../../scripts/pisugar-long-press.sh");
  if (!fs.existsSync(shortPressScript) || !fs.existsSync(longPressScript)) {
    console.warn("[PiSugar] Button scripts are missing; skipping integration.");
    return;
  }

  let modelResponse = "";
  try {
    modelResponse = await sendPiSugarCommand("get model");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.log(`[PiSugar] No PiSugar service detected on ${PISUGAR_HOST}:${PISUGAR_PORT}: ${message}`);
    return;
  }

  const modelMatch = modelResponse.match(/^model:\s*(.+)$/i);
  const model = (modelMatch?.[1] || modelResponse).trim();
  if (!/pisugar/i.test(model)) {
    console.log(`[PiSugar] Battery service is present but not a PiSugar model: ${modelResponse}`);
    return;
  }

  const webEnabled = (process.env.WHISPLAY_WEB_ENABLED || "").toLowerCase() === "true";
  const commands: Array<{ command: string; description: string }> = [
    {
      command: "set_battery_output true",
      description: "Enabled battery output",
    },
    {
      command: "set_soft_poweroff false",
      description: "Disabled soft poweroff",
    },
    {
      command: `set_button_shell long /bin/bash ${longPressScript}`,
      description: "Configured long press preview action",
    },
  ];

  if (webEnabled) {
    commands.push(
      {
        command: "set_button_enable long 1",
        description: "Enabled long press preview handler",
      },
      {
        command: `set_button_shell single /bin/bash ${shortPressScript}`,
        description: "Configured short press photo action",
      },
      {
        command: "set_button_enable single 1",
        description: "Enabled short press handler",
      },
    );
  } else {
    commands.push(
      {
        command: "set_button_enable long 0",
        description: "Disabled long press preview handler because web UI is off",
      },
      {
        command: "set_button_enable single 0",
        description: "Disabled short press handler because web UI is off",
      },
    );
  }

  for (const entry of commands) {
    try {
      await applyPiSugarCommand(entry.command, entry.description);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      console.warn(`[PiSugar] ${entry.description} failed: ${message}`);
    }
  }

  console.log(
    `[PiSugar] Optional button support active for ${model}: short press = photo${webEnabled ? "" : " (disabled)"}; long press = live preview${webEnabled ? "" : " (disabled)"}.`,
  );
}
