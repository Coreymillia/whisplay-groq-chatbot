import { getTextLlmModelOption } from "../config/text-llm-models";
import type { GroqHeaderBadgeMode } from "../config/runtime-settings";

export function formatGroqHeaderBadgeText(
  llmModel: string,
  badgeMode: GroqHeaderBadgeMode,
  requestsToday: number,
): string {
  const modelOption = getTextLlmModelOption(llmModel);
  const safeRequestsToday = Number.isFinite(requestsToday)
    ? Math.max(0, Math.round(requestsToday))
    : 0;

  if (badgeMode === "rpd-remaining") {
    const rpdLimit = modelOption?.rateLimits?.rpd;
    if (typeof rpdLimit === "number" && Number.isFinite(rpdLimit) && rpdLimit > 0) {
      return String(Math.max(0, rpdLimit - safeRequestsToday));
    }
    return modelOption?.shortLabel || modelOption?.label || "Model";
  }

  return modelOption?.shortLabel || modelOption?.label || "Model";
}
