import {
  applyGeminiLowTierImageCharge,
  formatGeminiLowTierImageBalanceText,
  getRuntimeSettings,
} from "../config/runtime-settings";

function updateDisplay(balanceUsd: number): void {
  try {
    const { display } = require("../device/display") as {
      display: (
        status: {
          gemini_low_tier_image_balance_usd: number;
          gemini_low_tier_image_balance_text: string;
        },
      ) => Promise<void> | void;
    };
    void display({
      gemini_low_tier_image_balance_usd: balanceUsd,
      gemini_low_tier_image_balance_text:
        formatGeminiLowTierImageBalanceText(balanceUsd),
    });
  } catch (error) {
    console.warn("[gemini-image-cost] Failed to update display:", error);
  }
}

export function recordGeminiLowTierImageCharge(): number {
  const settings = applyGeminiLowTierImageCharge();
  updateDisplay(settings.geminiLowTierImageBalanceUsd);
  return settings.geminiLowTierImageBalanceUsd;
}

export function syncGeminiLowTierImageCostDisplay(): number {
  const balanceUsd = getRuntimeSettings().geminiLowTierImageBalanceUsd;
  updateDisplay(balanceUsd);
  return balanceUsd;
}
