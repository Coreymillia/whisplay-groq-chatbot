import fs from "fs";
import path from "path";

interface GroqUsageState {
  localDate: string;
  requestsToday: number;
}

const USAGE_PATH = path.resolve(
  __dirname,
  "../..",
  ".whisplay-groqhat-groq-usage.json",
);

function getLocalDateKey(date = new Date()): string {
  const year = date.getFullYear();
  const month = `${date.getMonth() + 1}`.padStart(2, "0");
  const day = `${date.getDate()}`.padStart(2, "0");
  return `${year}-${month}-${day}`;
}

function sanitizeUsageState(input: Partial<GroqUsageState> | null | undefined): GroqUsageState {
  return {
    localDate:
      typeof input?.localDate === "string" && input.localDate.trim()
        ? input.localDate.trim()
        : getLocalDateKey(),
    requestsToday:
      typeof input?.requestsToday === "number" && Number.isFinite(input.requestsToday)
        ? Math.max(0, Math.round(input.requestsToday))
        : 0,
  };
}

function readUsageState(): GroqUsageState {
  try {
    if (!fs.existsSync(USAGE_PATH)) {
      return sanitizeUsageState({});
    }
    return sanitizeUsageState(JSON.parse(fs.readFileSync(USAGE_PATH, "utf8")));
  } catch (error) {
    console.warn("[groq-usage] Failed to load usage state:", error);
    return sanitizeUsageState({});
  }
}

function writeUsageState(state: GroqUsageState): void {
  fs.writeFileSync(USAGE_PATH, `${JSON.stringify(state, null, 2)}\n`, "utf8");
}

function ensureCurrentUsageState(): GroqUsageState {
  const state = readUsageState();
  const today = getLocalDateKey();
  if (state.localDate === today) {
    return state;
  }
  const nextState: GroqUsageState = {
    localDate: today,
    requestsToday: 0,
  };
  writeUsageState(nextState);
  return nextState;
}

function updateDisplay(requestsToday: number): void {
  try {
    const { display } = require("../device/display") as {
      display: (status: { groq_requests_today: number }) => Promise<void> | void;
    };
    void display({ groq_requests_today: requestsToday });
  } catch (error) {
    console.warn("[groq-usage] Failed to update display:", error);
  }
}

export function getGroqRequestsToday(): number {
  return ensureCurrentUsageState().requestsToday;
}

export function syncGroqUsageDisplay(): number {
  const requestsToday = getGroqRequestsToday();
  updateDisplay(requestsToday);
  return requestsToday;
}

export function recordGroqRequest(): number {
  const state = ensureCurrentUsageState();
  const nextState: GroqUsageState = {
    localDate: state.localDate,
    requestsToday: state.requestsToday + 1,
  };
  writeUsageState(nextState);
  updateDisplay(nextState.requestsToday);
  return nextState.requestsToday;
}
