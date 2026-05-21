import { getBotNetModelOption } from "../config/botnet-models";
import type { GroqHeaderBadgeMode } from "../config/runtime-settings";

export function formatGroqHeaderBadgeText(
  llmModel: string,
  badgeMode: GroqHeaderBadgeMode,
  requestsToday: number,
): string {
  const modelOption = getBotNetModelOption(llmModel);
  const safeRequestsToday = Number.isFinite(requestsToday)
    ? Math.max(0, Math.round(requestsToday))
    : 0;

  if (badgeMode === "rpd-remaining") {
    const rpdLimit = modelOption?.rateLimits?.rpd;
    if (typeof rpdLimit === "number" && Number.isFinite(rpdLimit) && rpdLimit > 0) {
      return String(Math.max(0, rpdLimit - safeRequestsToday));
    }
    return "?";
  }

  return modelOption?.shortLabel || modelOption?.label || "Model";
}
