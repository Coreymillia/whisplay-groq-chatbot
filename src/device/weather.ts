import { getRuntimeSettings } from "../config/runtime-settings";

const WEATHER_CACHE_MS = 10 * 60 * 1000;
const NWS_HEADERS = {
  "User-Agent": "WhisplayGroqHat/1.0 weather feature",
  Accept: "application/geo+json, application/json;q=0.9",
};

export interface WeatherSnapshot {
  locationLabel: string;
  forecastText: string;
  alertsText: string;
  combinedText: string;
  fetchedAt: number;
}

let cachedWeatherKey = "";
let cachedWeatherSnapshot: WeatherSnapshot | null = null;

function normalizeCoordinate(
  value: number | null | undefined,
  min: number,
  max: number,
): number | null {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return null;
  }
  return Math.min(max, Math.max(min, value));
}

function getConfiguredCoordinates(): { latitude: number; longitude: number } | null {
  const settings = getRuntimeSettings();
  const latitude = normalizeCoordinate(settings.weatherLatitude, -90, 90);
  const longitude = normalizeCoordinate(settings.weatherLongitude, -180, 180);
  if (latitude === null || longitude === null) {
    return null;
  }
  return { latitude, longitude };
}

export function isWeatherConfigured(): boolean {
  return getConfiguredCoordinates() !== null;
}

async function fetchJson(
  url: string,
  requestLabel: "points lookup" | "forecast lookup" | "alerts lookup",
): Promise<any> {
  const response = await fetch(url, { headers: NWS_HEADERS });
  if (!response.ok) {
    const responseText = await response.text();
    let problemDetail = "";
    if (responseText) {
      try {
        const problem = JSON.parse(responseText) as {
          detail?: unknown;
          title?: unknown;
        };
        problemDetail = String(problem.detail || problem.title || "").trim();
      } catch {
        problemDetail = responseText.trim();
      }
    }
    if (requestLabel === "points lookup" && response.status === 404) {
      throw new Error(
        "NWS has no forecast data for the saved latitude/longitude. Check Settings and make sure the coordinates are correct.",
      );
    }
    const suffix = problemDetail ? `: ${problemDetail}` : "";
    throw new Error(`NWS ${requestLabel} failed (${response.status})${suffix}`);
  }
  return response.json();
}

function summarizeForecastPeriods(periods: any[]): string {
  return periods
    .slice(0, 4)
    .map((period) => {
      const name = String(period?.name || "Forecast");
      const temperature = period?.temperature;
      const temperatureUnit = period?.temperatureUnit || "F";
      const windSpeed = String(period?.windSpeed || "").trim();
      const windDirection = String(period?.windDirection || "").trim();
      const details = String(period?.detailedForecast || "")
        .replace(/\s+/g, " ")
        .trim();

      const temperatureText =
        typeof temperature === "number"
          ? `${temperature}\u00b0${temperatureUnit}`
          : "temperature unavailable";
      const windText = windSpeed
        ? ` Wind ${windSpeed}${windDirection ? ` ${windDirection}` : ""}.`
        : "";
      return `${name}: ${temperatureText}. ${details}${windText}`.trim();
    })
    .join("\n");
}

function summarizeAlerts(features: any[]): string {
  if (!Array.isArray(features) || features.length === 0) {
    return "No active weather alerts.";
  }

  return features
    .slice(0, 3)
    .map((feature) => {
      const properties = feature?.properties || {};
      const event = String(properties.event || "Alert").trim();
      const headline = String(properties.headline || "")
        .replace(/\s+/g, " ")
        .trim();
      const urgency = String(properties.urgency || "").trim();
      const certainty = String(properties.certainty || "").trim();
      const description = String(properties.description || "")
        .replace(/\s+/g, " ")
        .trim()
        .slice(0, 220);
      return `${event}${headline ? ` - ${headline}` : ""}${urgency ? ` [${urgency}]` : ""}${certainty ? ` (${certainty})` : ""}. ${description}`.trim();
    })
    .join("\n");
}

export async function fetchWeatherSnapshot(
  forceRefresh = false,
): Promise<WeatherSnapshot> {
  const coordinates = getConfiguredCoordinates();
  if (!coordinates) {
    throw new Error("Set weather latitude and longitude in Settings first.");
  }

  const cacheKey = `${coordinates.latitude.toFixed(4)},${coordinates.longitude.toFixed(4)}`;
  const now = Date.now();
  if (
    !forceRefresh &&
    cachedWeatherSnapshot &&
    cachedWeatherKey === cacheKey &&
    now - cachedWeatherSnapshot.fetchedAt < WEATHER_CACHE_MS
  ) {
    return cachedWeatherSnapshot;
  }

  const points = await fetchJson(
    `https://api.weather.gov/points/${coordinates.latitude},${coordinates.longitude}`,
    "points lookup",
  );
  const forecastUrl = String(points?.properties?.forecast || "").trim();
  if (!forecastUrl) {
    throw new Error("NWS forecast URL was unavailable for this location.");
  }

  const relativeLocation = points?.properties?.relativeLocation?.properties || {};
  const locationLabel =
    [relativeLocation.city, relativeLocation.state]
      .map((value: unknown) => String(value || "").trim())
      .filter(Boolean)
      .join(", ") || cacheKey;

  const forecast = await fetchJson(forecastUrl, "forecast lookup");
  const periods = Array.isArray(forecast?.properties?.periods)
    ? forecast.properties.periods
    : [];
  if (!periods.length) {
    throw new Error("NWS forecast periods were unavailable.");
  }

  const alerts = await fetchJson(
    `https://api.weather.gov/alerts/active?point=${coordinates.latitude},${coordinates.longitude}`,
    "alerts lookup",
  );
  const alertFeatures = Array.isArray(alerts?.features) ? alerts.features : [];

  const forecastText = summarizeForecastPeriods(periods);
  const alertsText = summarizeAlerts(alertFeatures);
  const combinedText = `Location: ${locationLabel}\n\nForecast:\n${forecastText}\n\nAlerts:\n${alertsText}`;

  cachedWeatherKey = cacheKey;
  cachedWeatherSnapshot = {
    locationLabel,
    forecastText,
    alertsText,
    combinedText,
    fetchedAt: now,
  };
  return cachedWeatherSnapshot;
}
