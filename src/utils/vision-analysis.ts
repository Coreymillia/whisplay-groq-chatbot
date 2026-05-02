export interface VisionAnalysisSnapshot {
  question: string;
  rawResponse: string;
  relayResponse: string;
  updatedAt: number;
  ok: boolean;
}

let latestVisionAnalysis: VisionAnalysisSnapshot | null = null;

export function getLatestVisionAnalysis(): VisionAnalysisSnapshot | null {
  return latestVisionAnalysis;
}

export function setLatestVisionAnalysis(snapshot: VisionAnalysisSnapshot): void {
  latestVisionAnalysis = snapshot;
}

export function clearLatestVisionAnalysis(): void {
  latestVisionAnalysis = null;
}
