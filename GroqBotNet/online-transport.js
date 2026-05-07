const DEFAULT_SESSION = {
  nodeId: "",
  authToken: "",
  websocketUrl: "",
  linkId: "",
  peerNodeId: "",
  peerHandle: "",
  peerOnline: false,
  lastError: "",
};

function normalizeUrl(value) {
  const trimmed = String(value || "").trim();
  if (!trimmed) {
    return "";
  }
  const withProtocol = /^(https?|wss?):\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
  return withProtocol.replace(/\/+$/, "");
}

function sanitizeSession(session) {
  return {
    nodeId: String(session?.nodeId || "").trim(),
    authToken: String(session?.authToken || "").trim(),
    websocketUrl: normalizeUrl(session?.websocketUrl),
    linkId: String(session?.linkId || "").trim(),
    peerNodeId: String(session?.peerNodeId || "").trim(),
    peerHandle: String(session?.peerHandle || "").trim(),
    peerOnline: Boolean(session?.peerOnline),
    lastError: String(session?.lastError || "").trim(),
  };
}

class BotNetHubTransport {
  constructor(options) {
    this.getSettings = options.getSettings;
    this.getSession = options.getSession;
    this.setSession = options.setSession;
    this.onPeerStart = options.onPeerStart;
    this.onPeerMessage = options.onPeerMessage;
    this.onStateChange = options.onStateChange || (() => {});
    this.socket = null;
    this.heartbeat = null;
    this.reconnectTimer = null;
    this.disconnectRequested = false;
  }

  isOnlineTransport(target) {
    return (target?.transportMode || this.getSettings().transportMode) === "online-hub";
  }

  isConnected() {
    return Boolean(this.socket && this.socket.readyState === 1);
  }

  defaultWebSocketUrl(hubUrl) {
    const normalized = normalizeUrl(hubUrl);
    if (!normalized) {
      return "";
    }
    return `${normalized.replace(/^http/i, "ws")}/api/v1/botnet/connect`;
  }

  getPublicState() {
    const settings = this.getSettings();
    const session = sanitizeSession(this.getSession() || DEFAULT_SESSION);
    return {
      transportMode: settings.transportMode || "lan-direct",
      hubUrl: settings.hubUrl || "",
      nodeHandle: settings.nodeHandle || settings.botName || "GroqBotNet Bot",
      nodeId: session.nodeId,
      registered: Boolean(session.nodeId && session.authToken),
      connected: this.isConnected(),
      websocketUrl: session.websocketUrl,
      linkId: session.linkId,
      peerNodeId: session.peerNodeId,
      peerHandle: session.peerHandle,
      peerOnline: session.peerOnline,
      lastError: session.lastError,
      authConfigured: Boolean(session.authToken),
    };
  }

  persistSession(update) {
    const next = sanitizeSession({
      ...DEFAULT_SESSION,
      ...(this.getSession() || DEFAULT_SESSION),
      ...update,
    });
    this.setSession(next);
    this.onStateChange();
    return next;
  }

  clearHeartbeat() {
    if (this.heartbeat) {
      clearInterval(this.heartbeat);
      this.heartbeat = null;
    }
  }

  clearReconnect() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  closeSocket() {
    if (this.socket) {
      this.socket.onopen = null;
      this.socket.onclose = null;
      this.socket.onerror = null;
      this.socket.onmessage = null;
      this.socket.close();
      this.socket = null;
    }
    this.clearHeartbeat();
  }

  reconnectLater() {
    this.clearReconnect();
    const settings = this.getSettings();
    const session = this.getSession();
    if (
      this.disconnectRequested ||
      !settings.enabled ||
      !this.isOnlineTransport(settings) ||
      !session?.nodeId ||
      !session?.authToken ||
      !settings.hubUrl
    ) {
      return;
    }
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect().catch((error) => {
        this.persistSession({
          lastError: error instanceof Error ? error.message : "Failed to reconnect to the hub.",
        });
      });
    }, 5000);
  }

  startHeartbeat() {
    this.clearHeartbeat();
    this.heartbeat = setInterval(() => {
      const session = this.getSession();
      if (!this.isConnected() || !session?.nodeId) {
        this.clearHeartbeat();
        return;
      }
      this.socket.send(
        JSON.stringify({
          type: "botnet.ping",
          nodeId: session.nodeId,
        }),
      );
    }, 30000);
  }

  async test() {
    const settings = this.getSettings();
    const hubUrl = normalizeUrl(settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    const probePaths = ["/api/v1/botnet/health", "/health"];
    let lastError = "Hub check failed.";
    for (const probePath of probePaths) {
      try {
        const response = await fetch(`${hubUrl}${probePath}`);
        if (response.ok) {
          this.persistSession({ lastError: "" });
          return { ok: true, message: "Hub is reachable." };
        }
        lastError = `Hub returned HTTP ${response.status}.`;
      } catch (error) {
        lastError = error instanceof Error ? error.message : "Hub check failed.";
      }
    }
    this.persistSession({ lastError });
    throw new Error(lastError);
  }

  async register() {
    const settings = this.getSettings();
    const hubUrl = normalizeUrl(settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    const nodeHandle = String(settings.nodeHandle || settings.botName || "").trim();
    if (!nodeHandle) {
      throw new Error("Node handle is required.");
    }
    const currentSession = sanitizeSession(this.getSession() || DEFAULT_SESSION);
    const response = await fetch(`${hubUrl}/api/v1/botnet/nodes/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        nodeId: currentSession.nodeId || undefined,
        handle: nodeHandle,
        deviceType: "groqbotnet-zero",
        botName: settings.botName,
        capabilities: {
          modes: ["auto-bot", "persona-relay"],
          relayTransport: true,
          transportModes: ["lan-direct", "online-hub"],
        },
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `Hub registration failed with HTTP ${response.status}`);
    }
    const nodeId = String(payload.nodeId || payload?.node?.id || currentSession.nodeId || "").trim();
    const authToken = String(
      payload.authToken ||
        payload.token ||
        payload.sessionToken ||
        payload?.session?.token ||
        currentSession.authToken ||
        "",
    ).trim();
    if (!nodeId || !authToken) {
      throw new Error("Hub registration response did not include node credentials.");
    }
    this.disconnectRequested = false;
    return this.persistSession({
      nodeId,
      authToken,
      websocketUrl:
        String(payload.websocketUrl || payload.wsUrl || currentSession.websocketUrl || "").trim() ||
        this.defaultWebSocketUrl(hubUrl),
      linkId: String(payload.linkId || currentSession.linkId || "").trim(),
      peerNodeId: String(payload.peerNodeId || currentSession.peerNodeId || "").trim(),
      peerHandle: String(payload.peerHandle || currentSession.peerHandle || "").trim(),
      peerOnline:
        typeof payload.peerOnline === "boolean" ? payload.peerOnline : currentSession.peerOnline,
      lastError: "",
    });
  }

  async createInvite() {
    const settings = this.getSettings();
    const session = sanitizeSession(this.getSession() || DEFAULT_SESSION);
    const hubUrl = normalizeUrl(settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    if (!session.nodeId || !session.authToken) {
      throw new Error("Register this node with the hub first.");
    }
    const response = await fetch(`${hubUrl}/api/v1/botnet/invites/create`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        nodeId: session.nodeId,
        token: session.authToken,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `Invite creation failed with HTTP ${response.status}`);
    }
    this.persistSession({ lastError: "" });
    return {
      inviteCode: String(payload.inviteCode || "").trim(),
      expiresAt: String(payload.expiresAt || "").trim(),
    };
  }

  async redeemInvite(inviteCode) {
    const settings = this.getSettings();
    const hubUrl = normalizeUrl(settings.hubUrl);
    if (!hubUrl) {
      throw new Error("Hub URL is not configured.");
    }
    const cleanedCode = String(inviteCode || "").trim().toUpperCase();
    if (!cleanedCode) {
      throw new Error("Invite code is required.");
    }
    let session = sanitizeSession(this.getSession() || DEFAULT_SESSION);
    if (!session.nodeId || !session.authToken) {
      session = await this.register();
    }
    const response = await fetch(`${hubUrl}/api/v1/botnet/invites/redeem`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        nodeId: session.nodeId,
        token: session.authToken,
        inviteCode: cleanedCode,
      }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `Invite redeem failed with HTTP ${response.status}`);
    }
    this.persistSession({
      linkId: String(payload.linkId || session.linkId || "").trim(),
      peerNodeId: String(payload.peerNodeId || session.peerNodeId || "").trim(),
      peerHandle: String(payload.peerHandle || session.peerHandle || "").trim(),
      peerOnline:
        typeof payload.peerOnline === "boolean" ? payload.peerOnline : session.peerOnline,
      lastError: "",
    });
    return this.getPublicState();
  }

  async connect() {
    const settings = this.getSettings();
    if (!this.isOnlineTransport(settings)) {
      throw new Error("Set transport mode to Online Hub first.");
    }
    if (!settings.enabled) {
      throw new Error("BotNet mode is turned off.");
    }
    let session = sanitizeSession(this.getSession() || DEFAULT_SESSION);
    if (!session.nodeId || !session.authToken) {
      session = await this.register();
    }
    if (this.isConnected()) {
      return this.getPublicState();
    }
    const socketBase = session.websocketUrl || this.defaultWebSocketUrl(settings.hubUrl);
    if (!socketBase) {
      throw new Error("Hub websocket URL is not configured.");
    }

    const socketUrl = new URL(socketBase);
    socketUrl.searchParams.set("nodeId", session.nodeId);
    socketUrl.searchParams.set("token", session.authToken);

    this.disconnectRequested = false;
    this.closeSocket();
    this.clearReconnect();

    await new Promise((resolve, reject) => {
      const socket = new WebSocket(socketUrl.toString());
      let settled = false;

      socket.onopen = () => {
        this.socket = socket;
        this.persistSession({ lastError: "" });
        this.startHeartbeat();
        socket.send(
          JSON.stringify({
            type: "botnet.hello",
            nodeId: session.nodeId,
            token: session.authToken,
            handle: settings.nodeHandle || settings.botName,
            linkId: session.linkId || undefined,
          }),
        );
        if (!settled) {
          settled = true;
          resolve();
        }
      };

      socket.onmessage = async (event) => {
        try {
          await this.handleHubMessage(event.data);
        } catch (error) {
          this.persistSession({
            lastError:
              error instanceof Error ? error.message : "Failed to process hub message.",
          });
        }
      };

      socket.onclose = () => {
        this.closeSocket();
        this.onStateChange();
        this.reconnectLater();
      };

      socket.onerror = (event) => {
        const errorMessage =
          event?.message ||
          (event?.error instanceof Error ? event.error.message : "") ||
          "Hub websocket error.";
        this.persistSession({ lastError: errorMessage });
        if (!settled) {
          settled = true;
          reject(new Error(errorMessage));
        }
      };
    });

    return this.getPublicState();
  }

  disconnect() {
    this.disconnectRequested = true;
    this.clearReconnect();
    this.closeSocket();
    this.onStateChange();
    return this.getPublicState();
  }

  async sendEvent(eventType, conversation, payload) {
    const settings = this.getSettings();
    const session = sanitizeSession(this.getSession() || DEFAULT_SESSION);
    if (!this.isOnlineTransport(conversation || settings)) {
      throw new Error("Conversation is not using online hub transport.");
    }
    if (!this.isConnected()) {
      throw new Error("Hub relay session is not connected.");
    }
    const linkId = conversation.linkId || session.linkId;
    const peerNodeId = conversation.peerNodeId || session.peerNodeId;
    if (!linkId || !peerNodeId) {
      throw new Error("No online peer link is active yet.");
    }
    this.socket.send(
      JSON.stringify({
        type: eventType,
        nodeId: session.nodeId,
        token: session.authToken,
        linkId,
        toNodeId: peerNodeId,
        payload: {
          ...payload,
          transportMode: "online-hub",
          linkId,
          senderNodeId: session.nodeId,
          peerNodeId,
        },
      }),
    );
  }

  async handleHubMessage(rawMessage) {
    const text =
      typeof rawMessage === "string" ? rawMessage : Buffer.from(rawMessage).toString("utf8");
    const message = JSON.parse(text);
    const type = String(message.type || "").trim();
    const payload =
      message && typeof message.payload === "object" && !Array.isArray(message.payload)
        ? message.payload
        : message;

    switch (type) {
      case "botnet.welcome":
      case "botnet.session":
        this.persistSession({
          nodeId: String(message.nodeId || this.getSession()?.nodeId || "").trim(),
          websocketUrl:
            String(message.websocketUrl || this.getSession()?.websocketUrl || "").trim() ||
            this.getSession()?.websocketUrl ||
            "",
          lastError: "",
        });
        return;
      case "botnet.link-state":
        this.persistSession({
          linkId: String(message.linkId || this.getSession()?.linkId || "").trim(),
          peerNodeId: String(message.peerNodeId || this.getSession()?.peerNodeId || "").trim(),
          peerHandle: String(message.peerHandle || this.getSession()?.peerHandle || "").trim(),
          peerOnline:
            typeof message.peerOnline === "boolean"
              ? message.peerOnline
              : this.getSession()?.peerOnline,
          lastError: "",
        });
        return;
      case "botnet.peer-start":
        await this.onPeerStart(payload);
        return;
      case "botnet.peer-message":
        await this.onPeerMessage(payload);
        return;
      case "botnet.error":
        this.persistSession({
          lastError: String(message.error || message.message || "Hub returned an error.").trim(),
        });
        return;
      case "botnet.pong":
      case "botnet.ack":
      default:
        return;
    }
  }
}

module.exports = {
  BotNetHubTransport,
  DEFAULT_SESSION,
  sanitizeSession,
  normalizeUrl,
};
