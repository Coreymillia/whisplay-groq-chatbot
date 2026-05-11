import fetch, { RequestInit, Response } from "node-fetch";
import { fetch as UndiciFetch, ProxyAgent } from "undici";
import { HttpsProxyAgent } from "https-proxy-agent";
import { SocksProxyAgent } from "socks-proxy-agent";
import { Agent } from "http";
import dotenv from "dotenv";
import { recordGroqRequest } from "../status/groq-usage";

dotenv.config();

function shouldTrackGroqRequest(
  url: string,
  options: RequestInit,
): boolean {
  const method = `${options.method || "GET"}`.toUpperCase();
  return method !== "GET" && method !== "HEAD" && url.includes("api.groq.com");
}

/**
 * Automatically creates a proxy-enabled version of node-fetch
 * based on system environment variables (HTTP_PROXY, HTTPS_PROXY, ALL_PROXY).
 */
function createProxyFetch() {
  const httpsProxy = process.env.HTTPS_PROXY || process.env.https_proxy;
  const httpProxy = process.env.HTTP_PROXY || process.env.http_proxy;
  const allProxy = process.env.ALL_PROXY || process.env.all_proxy;

  let agent: Agent | undefined;

  const proxy = httpsProxy || httpProxy || allProxy;

  if (proxy) {
    if (proxy.startsWith("socks")) {
      agent = new SocksProxyAgent(proxy);
    } else {
      agent = new HttpsProxyAgent(proxy);
    }
  }

  return async function proxyFetch(
    url: string,
    options: RequestInit = {}
  ): Promise<Response> {
    if (shouldTrackGroqRequest(String(url), options)) {
      recordGroqRequest();
    }
    return fetch(url, { agent, ...options });
  };
}

export const proxyFetch = createProxyFetch();

function createUndiciProxyFetch() {
  const httpsProxy = process.env.HTTPS_PROXY || process.env.https_proxy;
  const httpProxy = process.env.HTTP_PROXY || process.env.http_proxy;
  const allProxy = process.env.ALL_PROXY || process.env.all_proxy;

  const proxyUrl = httpsProxy || httpProxy || allProxy;

  let dispatcher = undefined;

  if (proxyUrl) {
    console.log("[undici] Using proxy:", proxyUrl);
    dispatcher = new ProxyAgent(proxyUrl);
  } else {
    console.log("[undici] No proxy configured");
  }

  return async function undiciProxyFetch(
    url: string,
    options: RequestInit = {}
  ) {
    if (shouldTrackGroqRequest(String(url), options)) {
      recordGroqRequest();
    }
    // @ts-ignore
    return UndiciFetch(url, { dispatcher, ...options });
  };
}

export const undiciProxyFetch = createUndiciProxyFetch();
