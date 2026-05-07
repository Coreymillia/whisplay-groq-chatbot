const http = require("http");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { WebSocketServer, WebSocket } = require("ws");

const PORT = Number.parseInt(process.env.PORT || "18991", 10);
const HOST = process.env.HOST || "0.0.0.0";
const DATA_DIR = path.resolve(process.env.GROQBOTNET_HUB_DATA_DIR || path.join(__dirname, "data"));
const NODES_PATH = path.join(DATA_DIR, "nodes.json");
const INVITES_PATH = path.join(DATA_DIR, "invites.json");
const LINKS_PATH = path.join(DATA_DIR, "links.json");

fs.mkdirSync(DATA_DIR, { recursive: true });

function nowIso() {
  return new Date().toISOString();
}

function readJson(filePath, fallback) {
  try {
    return JSON.parse(fs.readFileSync(filePath, "utf8"));
  } catch {
    return fallback;
  }
}

function writeJson(filePath, value) {
  fs.writeFileSync(filePath, JSON.stringify(value, null, 2));
}

function normalizeText(value) {
  return String(value || "").trim();
}

function normalizeHandle(value, fallback = "BotNet Node") {
  return normalizeText(value) || fallback;
}

function randomId(prefix) {
  return `${prefix}_${crypto.randomBytes(8).toString("hex")}`;
}

function randomToken() {
  return crypto.randomBytes(24).toString("hex");
}

function randomInviteCode() {
  return crypto
    .randomBytes(6)
    .toString("base64")
    .replace(/[^A-Z0-9]/gi, "")
    .slice(0, 10)
    .toUpperCase();
}

function addMinutes(dateIso, minutes) {
  return new Date(Date.parse(dateIso) + minutes * 60 * 1000).toISOString();
}

function sanitizeNodesMap(value) {
  const map = value && typeof value === "object" ? value : {};
  const next = {};
  for (const [nodeId, rawNode] of Object.entries(map)) {
    next[nodeId] = {
      id: normalizeText(rawNode?.id || nodeId),
      handle: normalizeHandle(rawNode?.handle),
      botName: normalizeText(rawNode?.botName || ""),
      deviceType: normalizeText(rawNode?.deviceType || ""),
      capabilities:
        rawNode?.capabilities && typeof rawNode.capabilities === "object" && !Array.isArray(rawNode.capabilities)
          ? rawNode.capabilities
          : {},
      authToken: normalizeText(rawNode?.authToken || ""),
      linkId: normalizeText(rawNode?.linkId || ""),
      createdAt: normalizeText(rawNode?.createdAt || nowIso()),
      updatedAt: normalizeText(rawNode?.updatedAt || nowIso()),
      lastSeenAt: normalizeText(rawNode?.lastSeenAt || ""),
      lastConnectedAt: normalizeText(rawNode?.lastConnectedAt || ""),
    };
  }
  return next;
}

function sanitizeInvitesMap(value) {
  const map = value && typeof value === "object" ? value : {};
  const next = {};
  for (const [code, rawInvite] of Object.entries(map)) {
    const normalizedCode = normalizeText(rawInvite?.code || code).toUpperCase();
    next[normalizedCode] = {
      code: normalizedCode,
      fromNodeId: normalizeText(rawInvite?.fromNodeId || ""),
      createdAt: normalizeText(rawInvite?.createdAt || nowIso()),
      expiresAt: normalizeText(rawInvite?.expiresAt || addMinutes(nowIso(), 30)),
      redeemedAt: normalizeText(rawInvite?.redeemedAt || ""),
      redeemedByNodeId: normalizeText(rawInvite?.redeemedByNodeId || ""),
      used: Boolean(rawInvite?.used),
    };
  }
  return next;
}

function sanitizeLinksMap(value) {
  const map = value && typeof value === "object" ? value : {};
  const next = {};
  for (const [linkId, rawLink] of Object.entries(map)) {
    next[linkId] = {
      id: normalizeText(rawLink?.id || linkId),
      nodeAId: normalizeText(rawLink?.nodeAId || ""),
      nodeBId: normalizeText(rawLink?.nodeBId || ""),
      createdAt: normalizeText(rawLink?.createdAt || nowIso()),
      updatedAt: normalizeText(rawLink?.updatedAt || nowIso()),
    };
  }
  return next;
}

let nodes = sanitizeNodesMap(readJson(NODES_PATH, {}));
let invites = sanitizeInvitesMap(readJson(INVITES_PATH, {}));
let links = sanitizeLinksMap(readJson(LINKS_PATH, {}));

const activeSockets = new Map();

function persistState() {
  writeJson(NODES_PATH, nodes);
  writeJson(INVITES_PATH, invites);
  writeJson(LINKS_PATH, links);
}

function buildBaseUrl(req) {
  const forwardedProto = normalizeText(req.headers["x-forwarded-proto"]);
  const protocol = forwardedProto || (req.socket.encrypted ? "https" : "http");
  return `${protocol}://${req.headers.host || `localhost:${PORT}`}`;
}

function buildWebSocketUrl(req) {
  return `${buildBaseUrl(req).replace(/^http/i, "ws")}/api/v1/botnet/connect`;
}

function getNode(nodeId) {
  const id = normalizeText(nodeId);
  return id ? nodes[id] || null : null;
}

function getLink(linkId) {
  const id = normalizeText(linkId);
  return id ? links[id] || null : null;
}

function getPeerNodeId(link, nodeId) {
  if (!link) {
    return "";
  }
  return link.nodeAId === nodeId ? link.nodeBId : link.nodeAId;
}

function isNodeOnline(nodeId) {
  const socket = activeSockets.get(nodeId);
  return Boolean(socket && socket.readyState === WebSocket.OPEN);
}

function getLinkStateForNode(nodeId) {
  const node = getNode(nodeId);
  if (!node || !node.linkId) {
    return {
      linkId: "",
      peerNodeId: "",
      peerHandle: "",
      peerOnline: false,
    };
  }
  const link = getLink(node.linkId);
  if (!link) {
    node.linkId = "";
    node.updatedAt = nowIso();
    persistState();
    return {
      linkId: "",
      peerNodeId: "",
      peerHandle: "",
      peerOnline: false,
    };
  }
  const peerNodeId = getPeerNodeId(link, nodeId);
  const peerNode = getNode(peerNodeId);
  return {
    linkId: link.id,
    peerNodeId,
    peerHandle: peerNode?.handle || "",
    peerOnline: isNodeOnline(peerNodeId),
  };
}

function sendJson(res, statusCode, payload) {
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(JSON.stringify(payload));
}

async function readBody(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  if (chunks.length === 0) {
    return {};
  }
  const text = Buffer.concat(chunks).toString("utf8");
  return text ? JSON.parse(text) : {};
}

function authenticateNode(nodeId, token) {
  const node = getNode(nodeId);
  if (!node || !token || node.authToken !== token) {
    return null;
  }
  node.lastSeenAt = nowIso();
  node.updatedAt = nowIso();
  persistState();
  return node;
}

function sendSocketMessage(nodeId, message) {
  const socket = activeSockets.get(nodeId);
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    return false;
  }
  socket.send(JSON.stringify(message));
  return true;
}

function pushLinkState(nodeId) {
  const node = getNode(nodeId);
  if (!node) {
    return;
  }
  const state = getLinkStateForNode(nodeId);
  sendSocketMessage(nodeId, {
    type: "botnet.link-state",
    ...state,
  });
}

function pushLinkStateForPair(nodeId) {
  const state = getLinkStateForNode(nodeId);
  pushLinkState(nodeId);
  if (state.peerNodeId) {
    pushLinkState(state.peerNodeId);
  }
}

function detachLink(linkId) {
  const link = getLink(linkId);
  if (!link) {
    return;
  }
  for (const nodeId of [link.nodeAId, link.nodeBId]) {
    const node = getNode(nodeId);
    if (node && node.linkId === link.id) {
      node.linkId = "";
      node.updatedAt = nowIso();
    }
  }
  delete links[link.id];
  persistState();
  pushLinkState(link.nodeAId);
  pushLinkState(link.nodeBId);
}

function ensureSinglePrimaryLink(nodeId) {
  const node = getNode(nodeId);
  if (!node?.linkId) {
    return;
  }
  detachLink(node.linkId);
}

function createLink(nodeAId, nodeBId) {
  ensureSinglePrimaryLink(nodeAId);
  ensureSinglePrimaryLink(nodeBId);
  const nodeA = getNode(nodeAId);
  const nodeB = getNode(nodeBId);
  if (!nodeA || !nodeB) {
    throw new Error("Cannot create a link for a missing node.");
  }
  const createdAt = nowIso();
  const link = {
    id: randomId("link"),
    nodeAId,
    nodeBId,
    createdAt,
    updatedAt: createdAt,
  };
  links[link.id] = link;
  nodeA.linkId = link.id;
  nodeA.updatedAt = createdAt;
  nodeB.linkId = link.id;
  nodeB.updatedAt = createdAt;
  persistState();
  pushLinkState(nodeAId);
  pushLinkState(nodeBId);
  return link;
}

function buildNodeResponse(node, req) {
  const linkState = getLinkStateForNode(node.id);
  return {
    ok: true,
    nodeId: node.id,
    authToken: node.authToken,
    websocketUrl: buildWebSocketUrl(req),
    ...linkState,
  };
}

function requireLinkedPeer(node, expectedLinkId, expectedPeerId) {
  const link = getLink(node.linkId);
  if (!link || !node.linkId) {
    throw new Error("No active peer link exists for this node.");
  }
  if (expectedLinkId && link.id !== expectedLinkId) {
    throw new Error("Link is no longer active.");
  }
  const peerNodeId = getPeerNodeId(link, node.id);
  if (expectedPeerId && peerNodeId !== expectedPeerId) {
    throw new Error("Message target does not match the active peer link.");
  }
  return {
    link,
    peerNodeId,
  };
}

function cleanupExpiredInvites() {
  const now = Date.now();
  for (const [code, invite] of Object.entries(invites)) {
    if (invite.used || Date.parse(invite.expiresAt) <= now) {
      delete invites[code];
    }
  }
  persistState();
}

const server = http.createServer(async (req, res) => {
  try {
    if (!req.url) {
      sendJson(res, 400, { ok: false, error: "Missing URL." });
      return;
    }

    cleanupExpiredInvites();
    const url = new URL(req.url, buildBaseUrl(req));

    if (req.method === "GET" && (url.pathname === "/health" || url.pathname === "/api/v1/botnet/health")) {
      sendJson(res, 200, {
        ok: true,
        service: "groqbotnet-hub",
        nodes: Object.keys(nodes).length,
        onlineNodes: Array.from(activeSockets.keys()).filter((nodeId) => isNodeOnline(nodeId)).length,
      });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/v1/botnet/nodes/register") {
      const body = await readBody(req);
      const requestedNodeId = normalizeText(body.nodeId);
      let node = getNode(requestedNodeId);
      const stamp = nowIso();
      if (!node) {
        const nodeId = requestedNodeId || randomId("node");
        node = {
          id: nodeId,
          handle: normalizeHandle(body.handle, "BotNet Node"),
          botName: normalizeText(body.botName),
          deviceType: normalizeText(body.deviceType),
          capabilities:
            body.capabilities && typeof body.capabilities === "object" && !Array.isArray(body.capabilities)
              ? body.capabilities
              : {},
          authToken: randomToken(),
          linkId: "",
          createdAt: stamp,
          updatedAt: stamp,
          lastSeenAt: stamp,
          lastConnectedAt: "",
        };
        nodes[nodeId] = node;
      } else {
        node.handle = normalizeHandle(body.handle, node.handle);
        node.botName = normalizeText(body.botName) || node.botName;
        node.deviceType = normalizeText(body.deviceType) || node.deviceType;
        node.capabilities =
          body.capabilities && typeof body.capabilities === "object" && !Array.isArray(body.capabilities)
            ? body.capabilities
            : node.capabilities;
        if (!node.authToken) {
          node.authToken = randomToken();
        }
        node.updatedAt = stamp;
        node.lastSeenAt = stamp;
      }
      persistState();
      sendJson(res, 200, buildNodeResponse(node, req));
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/v1/botnet/invites/create") {
      const body = await readBody(req);
      const node = authenticateNode(body.nodeId, body.token);
      if (!node) {
        sendJson(res, 401, { ok: false, error: "Node authentication failed." });
        return;
      }
      const invite = {
        code: randomInviteCode(),
        fromNodeId: node.id,
        createdAt: nowIso(),
        expiresAt: addMinutes(nowIso(), 30),
        redeemedAt: "",
        redeemedByNodeId: "",
        used: false,
      };
      invites[invite.code] = invite;
      persistState();
      sendJson(res, 200, {
        ok: true,
        inviteCode: invite.code,
        expiresAt: invite.expiresAt,
      });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/v1/botnet/invites/redeem") {
      const body = await readBody(req);
      const node = authenticateNode(body.nodeId, body.token);
      if (!node) {
        sendJson(res, 401, { ok: false, error: "Node authentication failed." });
        return;
      }
      const inviteCode = normalizeText(body.inviteCode).toUpperCase();
      const invite = invites[inviteCode];
      if (!invite || invite.used) {
        sendJson(res, 404, { ok: false, error: "Invite code is invalid or already used." });
        return;
      }
      if (Date.parse(invite.expiresAt) <= Date.now()) {
        delete invites[inviteCode];
        persistState();
        sendJson(res, 410, { ok: false, error: "Invite code has expired." });
        return;
      }
      if (invite.fromNodeId === node.id) {
        sendJson(res, 400, { ok: false, error: "This node cannot redeem its own invite code." });
        return;
      }
      const sourceNode = getNode(invite.fromNodeId);
      if (!sourceNode) {
        delete invites[inviteCode];
        persistState();
        sendJson(res, 404, { ok: false, error: "Invite owner is no longer registered." });
        return;
      }
      const link = createLink(sourceNode.id, node.id);
      invite.used = true;
      invite.redeemedAt = nowIso();
      invite.redeemedByNodeId = node.id;
      persistState();
      sendJson(res, 200, {
        ok: true,
        linkId: link.id,
        peerNodeId: sourceNode.id,
        peerHandle: sourceNode.handle,
        peerOnline: isNodeOnline(sourceNode.id),
      });
      return;
    }

    sendJson(res, 404, { ok: false, error: "Not found." });
  } catch (error) {
    sendJson(res, 500, {
      ok: false,
      error: error instanceof Error ? error.message : "Unknown hub error.",
    });
  }
});

const wss = new WebSocketServer({ noServer: true });

server.on("upgrade", (req, socket, head) => {
  try {
    if (!req.url) {
      socket.write("HTTP/1.1 400 Bad Request\r\n\r\n");
      socket.destroy();
      return;
    }
    const url = new URL(req.url, buildBaseUrl(req));
    if (url.pathname !== "/api/v1/botnet/connect") {
      socket.write("HTTP/1.1 404 Not Found\r\n\r\n");
      socket.destroy();
      return;
    }
    const nodeId = normalizeText(url.searchParams.get("nodeId"));
    const token = normalizeText(url.searchParams.get("token"));
    const node = authenticateNode(nodeId, token);
    if (!node) {
      socket.write("HTTP/1.1 401 Unauthorized\r\n\r\n");
      socket.destroy();
      return;
    }
    wss.handleUpgrade(req, socket, head, (ws) => {
      ws.nodeId = node.id;
      wss.emit("connection", ws, req, node);
    });
  } catch {
    socket.write("HTTP/1.1 500 Internal Server Error\r\n\r\n");
    socket.destroy();
  }
});

wss.on("connection", (ws, req, node) => {
  const existing = activeSockets.get(node.id);
  if (existing && existing !== ws && existing.readyState === WebSocket.OPEN) {
    existing.close(1000, "Replaced by a newer hub session.");
  }

  activeSockets.set(node.id, ws);
  node.lastConnectedAt = nowIso();
  node.lastSeenAt = nowIso();
  node.updatedAt = nowIso();
  persistState();

  ws.send(
    JSON.stringify({
      type: "botnet.welcome",
      nodeId: node.id,
      websocketUrl: buildWebSocketUrl(req),
    }),
  );
  pushLinkState(node.id);
  pushLinkStateForPair(node.id);

  ws.on("message", (raw) => {
    try {
      const message = JSON.parse(Buffer.from(raw).toString("utf8"));
      const type = normalizeText(message.type);
      const currentNode = authenticateNode(message.nodeId || ws.nodeId, message.token || node.authToken);
      if (!currentNode || currentNode.id !== ws.nodeId) {
        ws.send(JSON.stringify({ type: "botnet.error", error: "Node authentication failed." }));
        return;
      }

      if (type === "botnet.hello") {
        ws.send(
          JSON.stringify({
            type: "botnet.session",
            nodeId: currentNode.id,
            websocketUrl: buildWebSocketUrl(req),
          }),
        );
        pushLinkState(currentNode.id);
        return;
      }

      if (type === "botnet.ping") {
        ws.send(JSON.stringify({ type: "botnet.pong", nodeId: currentNode.id }));
        return;
      }

      if (
        type === "botnet.peer-start" ||
        type === "botnet.peer-message" ||
        type === "botnet.start" ||
        type === "botnet.message"
      ) {
        const outgoingType =
          type === "botnet.peer-start" || type === "botnet.start"
            ? "botnet.peer-start"
            : "botnet.peer-message";
        const { peerNodeId, link } = requireLinkedPeer(
          currentNode,
          normalizeText(message.linkId),
          normalizeText(message.toNodeId),
        );
        const delivered = sendSocketMessage(peerNodeId, {
          type: outgoingType,
          payload:
            message.payload && typeof message.payload === "object" && !Array.isArray(message.payload)
              ? message.payload
              : {},
        });
        if (!delivered) {
          ws.send(JSON.stringify({ type: "botnet.error", error: "Peer node is offline." }));
          return;
        }
        link.updatedAt = nowIso();
        currentNode.lastSeenAt = nowIso();
        currentNode.updatedAt = nowIso();
        persistState();
        ws.send(
          JSON.stringify({
            type: "botnet.ack",
            delivered: true,
            peerNodeId,
            linkId: link.id,
          }),
        );
        return;
      }

      ws.send(JSON.stringify({ type: "botnet.error", error: "Unsupported hub message type." }));
    } catch (error) {
      ws.send(
        JSON.stringify({
          type: "botnet.error",
          error: error instanceof Error ? error.message : "Failed to process hub message.",
        }),
      );
    }
  });

  ws.on("close", () => {
    if (activeSockets.get(node.id) === ws) {
      activeSockets.delete(node.id);
    }
    node.lastSeenAt = nowIso();
    node.updatedAt = nowIso();
    persistState();
    pushLinkStateForPair(node.id);
  });
});

server.listen(PORT, HOST, () => {
  console.log(`[GroqBotNetHub] Listening on http://${HOST}:${PORT}`);
});
