const settingsForm = document.getElementById("settingsForm");
const modeSelect = document.getElementById("modeSelect");
const companionBaseUrlInput = document.getElementById("companionBaseUrlInput");
const pollIntervalSelect = document.getElementById("pollIntervalSelect");
const touchDisplayModeSelect = document.getElementById("touchDisplayModeSelect");
const touchDisplayRotationSelect = document.getElementById("touchDisplayRotationSelect");
const slideshowEnabledCheckbox = document.getElementById("slideshowEnabledCheckbox");
const slideshowIntervalSelect = document.getElementById("slideshowIntervalSelect");
const chatReturnTimeoutSelect = document.getElementById("chatReturnTimeoutSelect");
const settingsStatus = document.getElementById("settingsStatus");
const companionStatus = document.getElementById("companionStatus");
const conversationLog = document.getElementById("conversationLog");
const chatForm = document.getElementById("chatForm");
const chatInput = document.getElementById("chatInput");
const sendBtn = document.getElementById("sendBtn");
const openWhisplayBtn = document.getElementById("openWhisplayBtn");
const remoteStatusText = document.getElementById("remoteStatusText");
const remoteEmojiText = document.getElementById("remoteEmojiText");
const remoteReplyText = document.getElementById("remoteReplyText");
const remoteImage = document.getElementById("remoteImage");
const remoteImageEmpty = document.getElementById("remoteImageEmpty");
const llmModelBadge = document.getElementById("llmModelBadge");
const groqRequestsBadge = document.getElementById("groqRequestsBadge");
const geminiBalanceBadge = document.getElementById("geminiBalanceBadge");
const piAgentStatus = document.getElementById("piAgentStatus");
const piAgentStartBtn = document.getElementById("piAgentStartBtn");
const piAgentStopBtn = document.getElementById("piAgentStopBtn");
const piAgentTerminal = document.getElementById("piAgentTerminal");
const piAgentProjectName = document.getElementById("piAgentProjectName");
const piAgentProjectStarter = document.getElementById("piAgentProjectStarter");
const piAgentProjectCreateBtn = document.getElementById("piAgentProjectCreateBtn");
const piAgentProjectStatus = document.getElementById("piAgentProjectStatus");
const piAgentProjectsList = document.getElementById("piAgentProjectsList");
const piAgentWorkspaceStatus = document.getElementById("piAgentWorkspaceStatus");
const piAgentFileTree = document.getElementById("piAgentFileTree");
const piAgentFilePath = document.getElementById("piAgentFilePath");
const piAgentFileEditor = document.getElementById("piAgentFileEditor");
const piAgentSaveFileBtn = document.getElementById("piAgentSaveFileBtn");
const piAgentEditorStatus = document.getElementById("piAgentEditorStatus");

let settings = null;
let pollTimer = null;
let pendingSend = false;
let lastReplyText = "";
let turns = [];
let piAgentState = null;
let piAgentSocket = null;
let piAgentTerminalReady = false;
let piAgentFitAddon = null;
let piAgentTerm = null;
let piAgentProjects = [];
let activePiAgentProject = null;
let activePiAgentFilePath = "";
let activePiAgentFileMtimeMs = null;

function escapeHtml(value) {
  return String(value || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

function formatTime(value) {
  return new Date(value).toLocaleTimeString([], {
    hour: "numeric",
    minute: "2-digit",
  });
}

function pushTurn(role, text) {
  const trimmed = String(text || "").trim();
  if (!trimmed) {
    return;
  }
  const latest = turns[turns.length - 1];
  if (latest && latest.role === role && latest.text === trimmed) {
    return;
  }
  turns.push({ role, text: trimmed, timestamp: Date.now() });
  if (turns.length > 120) {
    turns = turns.slice(turns.length - 120);
  }
  renderConversation();
}

function renderConversation() {
  if (!turns.length) {
    conversationLog.innerHTML = '<div class="muted">No local chat turns yet. Type here to send through Whisplay.</div>';
    return;
  }
  conversationLog.innerHTML = turns
    .map(
      (turn) => `
        <article class="turn ${turn.role}">
          <div class="turn-meta">
            <strong>${turn.role === "user" ? "You" : "Whisplay"}</strong>
            <span>${formatTime(turn.timestamp)}</span>
          </div>
          <pre class="turn-text">${escapeHtml(turn.text)}</pre>
        </article>
      `,
    )
    .join("");
  conversationLog.scrollTop = conversationLog.scrollHeight;
}

function updateSettingsForm(nextSettings) {
  settings = nextSettings;
  modeSelect.value = nextSettings.mode || "companion";
  companionBaseUrlInput.value = nextSettings.companionBaseUrl || "";
  pollIntervalSelect.value = String(nextSettings.pollIntervalMs || 2000);
  touchDisplayModeSelect.value = nextSettings.touchDisplayMode || "slideshow-chat";
  touchDisplayRotationSelect.value = String(nextSettings.touchDisplayRotationDeg ?? 270);
  slideshowEnabledCheckbox.checked = nextSettings.slideshowEnabled !== false;
  slideshowIntervalSelect.value = String(nextSettings.slideshowIntervalSec || 8);
  chatReturnTimeoutSelect.value = String(nextSettings.chatReturnTimeoutSec || 20);
  const baseUrl = (nextSettings.companionBaseUrl || "").trim();
  openWhisplayBtn.href = baseUrl ? baseUrl : "#";
  openWhisplayBtn.setAttribute("aria-disabled", baseUrl ? "false" : "true");
}

async function loadSettings() {
  const response = await fetch("/api/settings", { cache: "no-store" });
  const payload = await response.json();
  if (!payload.ok) {
    throw new Error(payload.error || "Failed to load Pi3Groq settings.");
  }
  updateSettingsForm(payload.settings);
}

function setSaveStatus(message, isError = false) {
  settingsStatus.textContent = message;
  settingsStatus.style.color = isError ? "var(--danger)" : "";
}

function renderRemoteState(payload) {
  const state = payload.companion;
  if (!payload.ok || !state) {
    companionStatus.textContent = payload.error || "Whisplay companion is not connected.";
    remoteStatusText.textContent = "--";
    remoteEmojiText.textContent = "!";
    remoteReplyText.textContent = payload.error || "Waiting for a saved Whisplay URL.";
    llmModelBadge.textContent = "No model";
    groqRequestsBadge.textContent = "RPD --";
    geminiBalanceBadge.textContent = "$ --";
    remoteImage.hidden = true;
    remoteImageEmpty.hidden = false;
    return;
  }

  companionStatus.textContent = state.ready
    ? `Connected to ${state.remoteBaseUrl || payload.settings.companionBaseUrl}`
    : `Whisplay reachable but not ready: ${state.status || "unknown status"}`;
  remoteStatusText.textContent = state.status || "--";
  remoteEmojiText.textContent = state.emoji || "🙂";
  remoteReplyText.textContent = state.text || "No reply text yet.";
  llmModelBadge.textContent = state.llm_model || "No model";
  groqRequestsBadge.textContent = `RPD ${state.groq_requests_today ?? "--"}`;
  geminiBalanceBadge.textContent = state.gemini_low_tier_image_balance_text || "$ --";

  if (state.text && state.text !== lastReplyText) {
    lastReplyText = state.text;
    pushTurn("bot", state.text);
  }

  if (state.remote_image_proxy_url) {
    remoteImage.src = state.remote_image_proxy_url;
    remoteImage.hidden = false;
    remoteImageEmpty.hidden = true;
  } else {
    remoteImage.hidden = true;
    remoteImageEmpty.hidden = false;
  }
}

async function pollState() {
  try {
    const response = await fetch("/api/companion/state", { cache: "no-store" });
    const payload = await response.json();
    renderRemoteState(payload);
  } catch (error) {
    renderRemoteState({
      ok: false,
      error: error instanceof Error ? error.message : "State request failed.",
      companion: null,
      settings: settings || {},
    });
  }
}

function startPolling() {
  if (pollTimer) {
    window.clearInterval(pollTimer);
  }
  const interval = Number(settings?.pollIntervalMs || 2000);
  pollTimer = window.setInterval(pollState, interval);
}

async function saveSettings(event) {
  event.preventDefault();
  setSaveStatus("Saving companion settings...");
  try {
    const response = await fetch("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        mode: modeSelect.value,
        companionBaseUrl: companionBaseUrlInput.value.trim(),
        pollIntervalMs: Number(pollIntervalSelect.value),
        touchDisplayMode: touchDisplayModeSelect.value,
        touchDisplayRotationDeg: Number(touchDisplayRotationSelect.value),
        slideshowEnabled: slideshowEnabledCheckbox.checked,
        slideshowIntervalSec: Number(slideshowIntervalSelect.value),
        chatReturnTimeoutSec: Number(chatReturnTimeoutSelect.value),
      }),
    });
    const payload = await response.json();
    if (!response.ok || !payload.ok) {
      throw new Error(payload.error || "Failed to save settings.");
    }
    updateSettingsForm(payload.settings);
    setSaveStatus(
      payload.touchDisplayRotationApplyPending
        ? "Saved. Pi3Groq is rebooting to apply the new touch display rotation."
        : "Saved. Pi3Groq will now poll the updated Whisplay URL.",
    );
    startPolling();
    await pollState();
  } catch (error) {
    setSaveStatus(error instanceof Error ? error.message : "Failed to save settings.", true);
  }
}

async function sendMessage(event) {
  event.preventDefault();
  if (pendingSend) {
    return;
  }
  const text = chatInput.value.trim();
  if (!text) {
    return;
  }
  pendingSend = true;
  sendBtn.disabled = true;
  pushTurn("user", text);
  chatInput.value = "";
  try {
    const response = await fetch("/api/companion/input", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ text }),
    });
    const payload = await response.json();
    if (!response.ok || !payload.ok) {
      throw new Error(payload.error || "Failed to send message to Whisplay.");
    }
    await pollState();
  } catch (error) {
    pushTurn("bot", `Send failed: ${error instanceof Error ? error.message : "unknown error"}`);
  } finally {
    pendingSend = false;
    sendBtn.disabled = false;
    chatInput.focus();
  }
}

function setPiAgentStatus(message, isError = false) {
  if (!piAgentStatus) {
    return;
  }
  piAgentStatus.textContent = message;
  piAgentStatus.style.color = isError ? "var(--danger)" : "";
}

function updatePiAgentButtons() {
  if (!piAgentStartBtn || !piAgentStopBtn) {
    return;
  }
  const running = Boolean(piAgentState?.running);
  const available = Boolean(piAgentState?.available);
  piAgentStartBtn.disabled = !available || running || !activePiAgentProject;
  piAgentStopBtn.disabled = !running;
}

function setPiAgentProjectStatus(message, isError = false) {
  if (!piAgentProjectStatus) {
    return;
  }
  piAgentProjectStatus.textContent = message;
  piAgentProjectStatus.style.color = isError ? "var(--danger)" : "";
}

function setPiAgentEditorStatus(message, isError = false) {
  if (!piAgentEditorStatus) {
    return;
  }
  piAgentEditorStatus.textContent = message;
  piAgentEditorStatus.style.color = isError ? "var(--danger)" : "";
}

function escapeProjectHtml(value) {
  return String(value || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

function renderPiAgentProjects() {
  if (!piAgentProjectsList) {
    return;
  }
  piAgentProjectsList.innerHTML = "";
  if (!piAgentProjects.length) {
    piAgentProjectsList.innerHTML = '<div class="muted">No PiAgent projects yet.</div>';
    return;
  }
  piAgentProjects.forEach((project) => {
    const item = document.createElement("div");
    item.className = "pi-agent-project-item";
    const button = document.createElement("button");
    button.type = "button";
    button.className = `pi-agent-project-button${activePiAgentProject?.id === project.id ? " active" : ""}`;
    button.innerHTML = `<strong>${escapeProjectHtml(project.name)}</strong><div class="pi-agent-project-meta">${new Date(project.updatedAt).toLocaleString()}</div>`;
    button.addEventListener("click", () => {
      void selectPiAgentProject(project.id).catch((error) => {
        setPiAgentProjectStatus(error instanceof Error ? error.message : "Failed to open project.", true);
      });
    });
    item.appendChild(button);
    piAgentProjectsList.appendChild(item);
  });
}

function createPiAgentTreeNode(node) {
  const wrapper = document.createElement("div");
  wrapper.className = "pi-agent-tree-node";
  if (node.type === "directory") {
    wrapper.innerHTML = `<strong>${escapeProjectHtml(node.name)}</strong>`;
    if (Array.isArray(node.children) && node.children.length) {
      const children = document.createElement("div");
      children.className = "pi-agent-tree-children";
      node.children.forEach((child) => children.appendChild(createPiAgentTreeNode(child)));
      wrapper.appendChild(children);
    }
    return wrapper;
  }
  const button = document.createElement("button");
  button.type = "button";
  button.className = `pi-agent-tree-button${activePiAgentFilePath === node.path ? " active" : ""}`;
  button.textContent = node.path;
  button.addEventListener("click", () => {
    void loadPiAgentFile(node.path).catch((error) => {
      setPiAgentEditorStatus(error instanceof Error ? error.message : "Failed to load file.", true);
    });
  });
  wrapper.appendChild(button);
  return wrapper;
}

function renderPiAgentFileTree(files, truncated = false) {
  if (!piAgentFileTree) {
    return;
  }
  piAgentFileTree.innerHTML = "";
  if (!activePiAgentProject) {
    piAgentFileTree.innerHTML = '<div class="muted">Select a PiAgent project first.</div>';
    return;
  }
  if (!files?.length) {
    piAgentFileTree.innerHTML = '<div class="muted">No visible files in this project yet.</div>';
    return;
  }
  files.forEach((node) => piAgentFileTree.appendChild(createPiAgentTreeNode(node)));
  if (truncated) {
    const note = document.createElement("div");
    note.className = "muted";
    note.textContent = "Tree truncated to keep the browser view lightweight.";
    piAgentFileTree.appendChild(note);
  }
}

function resetPiAgentEditor() {
  activePiAgentFilePath = "";
  activePiAgentFileMtimeMs = null;
  if (piAgentFileEditor) {
    piAgentFileEditor.value = "";
  }
  if (piAgentFilePath) {
    piAgentFilePath.textContent = "Choose a file from the project tree.";
  }
  setPiAgentEditorStatus("No file loaded.");
}

async function loadPiAgentProjects() {
  const response = await fetch("/api/pi-agent/projects", { cache: "no-store" });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to load PiAgent projects.");
  }
  piAgentProjects = Array.isArray(payload.projects) ? payload.projects : [];
  if (activePiAgentProject) {
    activePiAgentProject =
      piAgentProjects.find((project) => project.id === activePiAgentProject.id) || null;
  } else if (piAgentState?.currentProjectId) {
    activePiAgentProject =
      piAgentProjects.find((project) => project.id === piAgentState.currentProjectId) || null;
  }
  renderPiAgentProjects();
  updatePiAgentButtons();
}

async function loadPiAgentProjectTree(projectId) {
  const response = await fetch(`/api/pi-agent/projects/${encodeURIComponent(projectId)}/tree`, {
    cache: "no-store",
  });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to load PiAgent project tree.");
  }
  renderPiAgentFileTree(payload.files || [], Boolean(payload.truncated));
}

async function loadPiAgentFile(relativePath) {
  if (!activePiAgentProject) {
    return;
  }
  const response = await fetch(
    `/api/pi-agent/projects/${encodeURIComponent(activePiAgentProject.id)}/file?path=${encodeURIComponent(relativePath)}`,
    { cache: "no-store" },
  );
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to load PiAgent file.");
  }
  activePiAgentFilePath = payload.file?.path || relativePath;
  activePiAgentFileMtimeMs = payload.file?.mtimeMs ?? null;
  if (piAgentFileEditor) {
    piAgentFileEditor.value = payload.file?.content || "";
  }
  if (piAgentFilePath) {
    piAgentFilePath.textContent = activePiAgentFilePath;
  }
  setPiAgentEditorStatus("File loaded.");
  await loadPiAgentProjectTree(activePiAgentProject.id);
}

async function savePiAgentFile() {
  if (!activePiAgentProject || !activePiAgentFilePath) {
    setPiAgentEditorStatus("Choose a file before saving.", true);
    return;
  }
  setPiAgentEditorStatus("Saving file...");
  const response = await fetch(
    `/api/pi-agent/projects/${encodeURIComponent(activePiAgentProject.id)}/file`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        path: activePiAgentFilePath,
        content: piAgentFileEditor?.value || "",
        expectedMtimeMs: activePiAgentFileMtimeMs,
      }),
    },
  );
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to save PiAgent file.");
  }
  activePiAgentProject = payload.project || activePiAgentProject;
  activePiAgentFileMtimeMs = payload.file?.mtimeMs ?? activePiAgentFileMtimeMs;
  renderPiAgentProjects();
  setPiAgentEditorStatus("File saved.");
  await loadPiAgentProjectTree(activePiAgentProject.id);
}

async function selectPiAgentProject(projectId) {
  const response = await fetch(`/api/pi-agent/projects/${encodeURIComponent(projectId)}`, {
    cache: "no-store",
  });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to select PiAgent project.");
  }
  activePiAgentProject = payload.project || null;
  resetPiAgentEditor();
  renderPiAgentProjects();
  updatePiAgentButtons();
  if (piAgentWorkspaceStatus) {
    piAgentWorkspaceStatus.textContent = activePiAgentProject
      ? `Selected ${activePiAgentProject.name}. Start PiAgent to open this project in the terminal, or browse files here.`
      : "Select a PiAgent project to view its files.";
  }
  if (activePiAgentProject) {
    await loadPiAgentProjectTree(activePiAgentProject.id);
  } else {
    renderPiAgentFileTree([]);
  }
}

async function createPiAgentProject() {
  const name = piAgentProjectName?.value?.trim() || "";
  if (!name) {
    setPiAgentProjectStatus("Enter a project name first.", true);
    return;
  }
  setPiAgentProjectStatus("Creating PiAgent project...");
  const response = await fetch("/api/pi-agent/projects", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      name,
      starterPrompt: piAgentProjectStarter?.value?.trim() || "",
    }),
  });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to create PiAgent project.");
  }
  if (piAgentProjectName) {
    piAgentProjectName.value = "";
  }
  if (piAgentProjectStarter) {
    piAgentProjectStarter.value = "";
  }
  setPiAgentProjectStatus("PiAgent project created.");
  await loadPiAgentProjects();
  if (payload.project?.id) {
    await selectPiAgentProject(payload.project.id);
  }
}

function ensurePiAgentTerminal() {
  if (piAgentTerminalReady || !piAgentTerminal) {
    return;
  }
  if (!window.Terminal || !window.FitAddon || !window.FitAddon.FitAddon) {
    setPiAgentStatus("PiAgent terminal assets failed to load.", true);
    return;
  }
  piAgentTerm = new window.Terminal({
    cursorBlink: true,
    fontSize: 15,
    fontFamily: "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace",
    scrollback: 4000,
    theme: {
      background: "#05080d",
      foreground: "#e8f1fb",
      cursor: "#5dd3ff",
      selectionBackground: "rgba(93, 211, 255, 0.3)",
    },
  });
  piAgentFitAddon = new window.FitAddon.FitAddon();
  piAgentTerm.loadAddon(piAgentFitAddon);
  piAgentTerm.open(piAgentTerminal);
  piAgentFitAddon.fit();
  piAgentTerm.focus();
  piAgentTerm.writeln("PiAgent browser terminal ready.");
  piAgentTerm.writeln("Press Start PiAgent, then use /login and choose your model inside the terminal.");
  piAgentTerm.onData((data) => {
    if (!piAgentSocket || piAgentSocket.readyState !== WebSocket.OPEN) {
      return;
    }
    piAgentSocket.send(JSON.stringify({ type: "input", data }));
  });
  const sendResize = () => {
    if (!piAgentTerm || !piAgentFitAddon) {
      return;
    }
    piAgentFitAddon.fit();
    if (piAgentSocket && piAgentSocket.readyState === WebSocket.OPEN) {
      piAgentSocket.send(
        JSON.stringify({
          type: "resize",
          cols: piAgentTerm.cols,
          rows: piAgentTerm.rows,
        }),
      );
    }
  };
  window.addEventListener("resize", sendResize);
  piAgentTerminal.addEventListener("click", () => piAgentTerm?.focus());
  piAgentTerminalReady = true;
}

function applyPiAgentState(nextState) {
  piAgentState = nextState;
  const wsError = String(nextState?.websocketError || "").trim();
  const lastError = String(nextState?.lastError || "").trim();
  if (!nextState?.available) {
    setPiAgentStatus(
      `PiAgent is not installed at ${nextState?.binaryPath || "~/.local/bin/pi-agent"}.`,
      true,
    );
  } else if (wsError) {
    setPiAgentStatus(`PiAgent websocket bridge error: ${wsError}`, true);
  } else if (nextState?.running) {
    setPiAgentStatus(
      `PiAgent is running${nextState?.currentProjectName ? ` in ${nextState.currentProjectName}` : " on the Pi3 companion"}${nextState?.pid ? ` (pid ${nextState.pid})` : ""}. Use /login and model selection in the terminal below.`,
    );
  } else if (typeof nextState?.exitCode === "number") {
    setPiAgentStatus(`PiAgent stopped with exit code ${nextState.exitCode}.`);
  } else if (lastError) {
    setPiAgentStatus(lastError, true);
  } else {
    setPiAgentStatus("PiAgent is installed locally on the Pi3 companion and ready to start.");
  }
  updatePiAgentButtons();
}

async function loadPiAgentStatus() {
  const response = await fetch("/api/pi-agent/status", { cache: "no-store" });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to load PiAgent status.");
  }
  applyPiAgentState(payload.piAgent || {});
  return payload.piAgent || {};
}

function getPiAgentSocketUrl() {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  const host = window.location.hostname || "127.0.0.1";
  const port = Number(piAgentState?.websocketPort || 18601);
  return `${protocol}//${host}:${port}`;
}

function connectPiAgentSocket() {
  if (piAgentSocket && (piAgentSocket.readyState === WebSocket.OPEN || piAgentSocket.readyState === WebSocket.CONNECTING)) {
    return;
  }
  ensurePiAgentTerminal();
  piAgentSocket = new WebSocket(getPiAgentSocketUrl());
  piAgentSocket.addEventListener("open", () => {
    if (piAgentTerm) {
      piAgentTerm.focus();
      piAgentSocket.send(
        JSON.stringify({
          type: "resize",
          cols: piAgentTerm.cols,
          rows: piAgentTerm.rows,
        }),
      );
    }
  });
  piAgentSocket.addEventListener("message", (event) => {
    let payload = null;
    try {
      payload = JSON.parse(event.data);
    } catch (_error) {
      return;
    }
    if (!payload || typeof payload !== "object") {
      return;
    }
    if (payload.type === "snapshot") {
      applyPiAgentState({ ...(piAgentState || {}), ...payload });
      if (piAgentTerm) {
        piAgentTerm.reset();
        if (payload.history) {
          piAgentTerm.write(String(payload.history));
        }
      }
      return;
    }
    if (payload.type === "output") {
      if (piAgentTerm && payload.data) {
        piAgentTerm.write(String(payload.data));
      }
      return;
    }
    if (payload.type === "status") {
      applyPiAgentState({ ...(piAgentState || {}), ...payload });
      if (piAgentTerm && payload.running === false) {
        const exitCode = typeof payload.exitCode === "number" ? payload.exitCode : "unknown";
        piAgentTerm.writeln("");
        piAgentTerm.writeln(`[PiAgent exited: ${exitCode}]`);
      }
    }
  });
  piAgentSocket.addEventListener("close", () => {
    piAgentSocket = null;
  });
  piAgentSocket.addEventListener("error", () => {
    setPiAgentStatus("PiAgent terminal connection failed.", true);
  });
}

async function startPiAgent() {
  ensurePiAgentTerminal();
  if (!piAgentTerm || !activePiAgentProject) {
    return;
  }
  piAgentStartBtn.disabled = true;
  setPiAgentStatus(`Starting PiAgent in ${activePiAgentProject.name}...`);
  const response = await fetch("/api/pi-agent/start", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      projectId: activePiAgentProject.id,
      cols: piAgentTerm.cols,
      rows: piAgentTerm.rows,
    }),
  });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to start PiAgent.");
  }
  applyPiAgentState(payload.piAgent || {});
  piAgentTerm.reset();
  connectPiAgentSocket();
}

async function stopPiAgent() {
  piAgentStopBtn.disabled = true;
  const response = await fetch("/api/pi-agent/stop", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
  });
  const payload = await response.json();
  if (!response.ok || !payload.ok) {
    throw new Error(payload.error || "Failed to stop PiAgent.");
  }
  applyPiAgentState(payload.piAgent || {});
}

settingsForm.addEventListener("submit", saveSettings);
chatForm.addEventListener("submit", sendMessage);
piAgentStartBtn?.addEventListener("click", async () => {
  try {
    await startPiAgent();
  } catch (error) {
    setPiAgentStatus(error instanceof Error ? error.message : "Failed to start PiAgent.", true);
    updatePiAgentButtons();
  }
});
piAgentStopBtn?.addEventListener("click", async () => {
  try {
    await stopPiAgent();
  } catch (error) {
    setPiAgentStatus(error instanceof Error ? error.message : "Failed to stop PiAgent.", true);
    updatePiAgentButtons();
  }
});
piAgentProjectCreateBtn?.addEventListener("click", async () => {
  try {
    await createPiAgentProject();
  } catch (error) {
    setPiAgentProjectStatus(error instanceof Error ? error.message : "Failed to create project.", true);
  }
});
piAgentSaveFileBtn?.addEventListener("click", async () => {
  try {
    await savePiAgentFile();
  } catch (error) {
    setPiAgentEditorStatus(error instanceof Error ? error.message : "Failed to save file.", true);
  }
});

window.addEventListener("load", async () => {
  renderConversation();
  ensurePiAgentTerminal();
  renderPiAgentProjects();
  renderPiAgentFileTree([]);
  resetPiAgentEditor();
  try {
    await loadSettings();
    startPolling();
    await pollState();
    await loadPiAgentStatus();
    await loadPiAgentProjects();
    if (activePiAgentProject?.id) {
      await selectPiAgentProject(activePiAgentProject.id);
    }
    connectPiAgentSocket();
  } catch (error) {
    setSaveStatus(error instanceof Error ? error.message : "Failed to load settings.", true);
    setPiAgentStatus(error instanceof Error ? error.message : "Failed to load PiAgent.", true);
  }
});
