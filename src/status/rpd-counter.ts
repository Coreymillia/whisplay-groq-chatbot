import fs from "fs";
import path from "path";

interface RpdCounterState {
  localDate: string;
  messagesToday: number;
}

const COUNTER_PATH = path.resolve(
  __dirname,
  "../..",
  ".whisplay-groqhat-rpd-counter.json",
);

function getLocalDateKey(date = new Date()): string {
  const year = date.getFullYear();
  const month = `${date.getMonth() + 1}`.padStart(2, "0");
  const day = `${date.getDate()}`.padStart(2, "0");
  return `${year}-${month}-${day}`;
}

function sanitizeCounterState(
  input: Partial<RpdCounterState> | null | undefined,
): RpdCounterState {
  return {
    localDate:
      typeof input?.localDate === "string" && input.localDate.trim()
        ? input.localDate.trim()
        : getLocalDateKey(),
    messagesToday:
      typeof input?.messagesToday === "number" && Number.isFinite(input.messagesToday)
        ? Math.max(0, Math.round(input.messagesToday))
        : 0,
  };
}

function readCounterState(): RpdCounterState {
  try {
    if (!fs.existsSync(COUNTER_PATH)) {
      return sanitizeCounterState({});
    }
    return sanitizeCounterState(JSON.parse(fs.readFileSync(COUNTER_PATH, "utf8")));
  } catch (error) {
    console.warn("[rpd-counter] Failed to load counter state:", error);
    return sanitizeCounterState({});
  }
}

function writeCounterState(state: RpdCounterState): void {
  fs.writeFileSync(COUNTER_PATH, `${JSON.stringify(state, null, 2)}\n`, "utf8");
}

function ensureCurrentCounterState(): RpdCounterState {
  const state = readCounterState();
  const today = getLocalDateKey();
  if (state.localDate === today) {
    return state;
  }
  const nextState: RpdCounterState = {
    localDate: today,
    messagesToday: 0,
  };
  writeCounterState(nextState);
  return nextState;
}

function updateDisplay(messagesToday: number): void {
  try {
    const { display } = require("../device/display") as {
      display: (status: { groq_requests_today: number }) => Promise<void> | void;
    };
    void display({ groq_requests_today: messagesToday });
  } catch (error) {
    console.warn("[rpd-counter] Failed to update display:", error);
  }
}

export function getRpdToday(): number {
  return ensureCurrentCounterState().messagesToday;
}

export function syncRpdDisplay(): number {
  const messagesToday = getRpdToday();
  updateDisplay(messagesToday);
  return messagesToday;
}

export function recordRpdMessage(multiplier = 1): number {
  const increment = Math.max(0, Math.round(multiplier));
  if (increment <= 0) {
    return getRpdToday();
  }
  const state = ensureCurrentCounterState();
  const nextState: RpdCounterState = {
    localDate: state.localDate,
    messagesToday: state.messagesToday + increment,
  };
  writeCounterState(nextState);
  updateDisplay(nextState.messagesToday);
  return nextState.messagesToday;
}
