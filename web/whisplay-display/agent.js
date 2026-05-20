const presetSelect = document.getElementById("agentPresetSelect");
const modelSelect = document.getElementById("agentModelSelect");
const projectNameInput = document.getElementById("agentProjectName");
const createBtn = document.getElementById("agentCreateBtn");
const importBtn = document.getElementById("agentImportBtn");
const importInput = document.getElementById("agentImportInput");
const createStatus = document.getElementById("agentCreateStatus");
const projectsList = document.getElementById("agentProjectsList");
const refreshProjectsBtn = document.getElementById("agentRefreshProjectsBtn");
const workspaceTitle = document.getElementById("agentWorkspaceTitle");
const workspaceSubtitle = document.getElementById("agentWorkspaceSubtitle");
const buildCommandInput = document.getElementById("agentBuildCommand");
const uploadCommandInput = document.getElementById("agentUploadCommand");
const copyBuildBtn = document.getElementById("agentCopyBuildBtn");
const copyUploadBtn = document.getElementById("agentCopyUploadBtn");
const saveModelBtn = document.getElementById("agentSaveModelBtn");
const exportBtn = document.getElementById("agentExportBtn");
const portSelect = document.getElementById("agentPortSelect");
const refreshPortsBtn = document.getElementById("agentRefreshPortsBtn");
const savePortBtn = document.getElementById("agentSavePortBtn");
const portStatus = document.getElementById("agentPortStatus");
const terminalInput = document.getElementById("agentTerminalInput");
const terminalOutput = document.getElementById("agentTerminalOutput");
const runTerminalBtn = document.getElementById("agentRunTerminalBtn");
const stopTerminalBtn = document.getElementById("agentStopTerminalBtn");
const refreshTerminalBtn = document.getElementById("agentTerminalRefreshBtn");
const terminalStatus = document.getElementById("agentTerminalStatus");
const personalityPrompt = document.getElementById("agentPersonalityPrompt");
const savePersonalityBtn = document.getElementById("agentSavePersonalityBtn");
const resetPersonalityBtn = document.getElementById("agentResetPersonalityBtn");
const personalityStatus = document.getElementById("agentPersonalityStatus");
const errorPersonalityPrompt = document.getElementById("agentErrorPersonalityPrompt");
const saveErrorPersonalityBtn = document.getElementById("agentSaveErrorPersonalityBtn");
const resetErrorPersonalityBtn = document.getElementById("agentResetErrorPersonalityBtn");
const errorPersonalityStatus = document.getElementById("agentErrorPersonalityStatus");
const chatMessages = document.getElementById("agentChatMessages");
const chatInput = document.getElementById("agentChatInput");
const sendBtn = document.getElementById("agentSendBtn");
const applyBtn = document.getElementById("agentApplyBtn");
const chatStatus = document.getElementById("agentChatStatus");
const proposalsList = document.getElementById("agentProposalsList");
const fileTree = document.getElementById("agentFileTree");
const editorPath = document.getElementById("agentEditorPath");
const fileEditor = document.getElementById("agentFileEditor");
const saveFileBtn = document.getElementById("agentSaveFileBtn");
const editorStatus = document.getElementById("agentEditorStatus");
const savepointLabelInput = document.getElementById("agentSavepointLabel");
const savepointNoteInput = document.getElementById("agentSavepointNote");
const savepointBtn = document.getElementById("agentSavepointBtn");
const savepointStatus = document.getElementById("agentSavepointStatus");
const savepointsList = document.getElementById("agentSavepointsList");
const errorLog = document.getElementById("agentErrorLog");
const saveErrorBtn = document.getElementById("agentSaveErrorBtn");
const fixFromErrorBtn = document.getElementById("agentFixFromErrorBtn");
const errorStatus = document.getElementById("agentErrorStatus");

let presets = [];
let modelOptions = [];
let projects = [];
let checkpoints = [];
let serialPorts = [];
let chatHistory = [];
let proposedOperations = [];
let activeProject = null;
let activeFilePath = "";
let defaultAgentPersonalityPrompt = "";
let defaultErrorAgentPersonalityPrompt = "";
let terminalState = null;
let terminalPollTimer = null;

function setText(node, value) {
  if (node) node.textContent = value;
}

function escapePathSegment(value) {
  return encodeURIComponent(value);
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data?.error || `Request failed: ${response.status}`);
  }
  return data;
}

function updateApplyButtonState() {
  if (!applyBtn) return;
  applyBtn.disabled = !activeProject || !proposedOperations.length;
}

function stopTerminalPolling() {
  if (terminalPollTimer) {
    window.clearInterval(terminalPollTimer);
    terminalPollTimer = null;
  }
}

function ensureTerminalPolling() {
  stopTerminalPolling();
  if (!activeProject) {
    return;
  }
  terminalPollTimer = window.setInterval(() => {
    if (!activeProject) {
      stopTerminalPolling();
      return;
    }
    void loadTerminalState(activeProject.id).catch((error) => {
      stopTerminalPolling();
      setText(
        terminalStatus,
        error instanceof Error ? error.message : "Failed to refresh terminal output.",
      );
    });
  }, 2000);
}

function renderTerminal() {
  if (!terminalOutput || !terminalStatus) return;
  if (!activeProject) {
    if (terminalInput) terminalInput.value = "";
    terminalOutput.textContent = "Select a project to use the terminal.";
    setText(terminalStatus, "No terminal session loaded.");
    if (runTerminalBtn) runTerminalBtn.disabled = true;
    if (stopTerminalBtn) stopTerminalBtn.disabled = true;
    return;
  }

  const status = terminalState?.status || "idle";
  terminalOutput.textContent =
    terminalState?.output || "No terminal output yet for this project.";
  terminalOutput.scrollTop = terminalOutput.scrollHeight;
  if (runTerminalBtn) runTerminalBtn.disabled = status === "running";
  if (stopTerminalBtn) stopTerminalBtn.disabled = status !== "running";

  const suffix =
    terminalState?.command
      ? ` Last command: ${terminalState.command}`
      : " No command has been run yet.";
  if (status === "running") {
    setText(terminalStatus, `Running.${suffix}`);
    return;
  }
  if (status === "failed") {
    setText(
      terminalStatus,
      `Command failed${terminalState?.exitCode != null ? ` with code ${terminalState.exitCode}` : ""}.${suffix}`,
    );
    return;
  }
  if (status === "exited") {
    setText(
      terminalStatus,
      `Command completed${terminalState?.exitCode != null ? ` with code ${terminalState.exitCode}` : ""}.${suffix}`,
    );
    return;
  }
  if (status === "stopped") {
    setText(terminalStatus, `Command stopped.${suffix}`);
    return;
  }
  setText(terminalStatus, `Idle.${suffix}`);
}

function populatePresetOptions() {
  if (!presetSelect) return;
  presetSelect.innerHTML = "";
  presets.forEach((preset) => {
    const option = document.createElement("option");
    option.value = preset.id;
    option.textContent = preset.name;
    presetSelect.appendChild(option);
  });
}

function populateModelOptions(selectedValue) {
  if (!modelSelect) return;
  modelSelect.innerHTML = "";
  modelOptions.forEach((item) => {
    const option = document.createElement("option");
    option.value = item.id;
    option.textContent = item.label;
    modelSelect.appendChild(option);
  });
  modelSelect.value = selectedValue || modelOptions[0]?.id || "";
}

function populateSerialPortOptions(selectedValue) {
  if (!portSelect) return;
  portSelect.innerHTML = "";

  const defaultOption = document.createElement("option");
  defaultOption.value = "";
  defaultOption.textContent = serialPorts.length
    ? "No saved flash port"
    : "No USB serial ports detected";
  portSelect.appendChild(defaultOption);

  serialPorts.forEach((port) => {
    const option = document.createElement("option");
    option.value = port.path;
    option.textContent = port.label;
    portSelect.appendChild(option);
  });

  if (selectedValue && serialPorts.some((port) => port.path === selectedValue)) {
    portSelect.value = selectedValue;
    return;
  }
  if (!selectedValue && serialPorts.length === 1) {
    portSelect.value = serialPorts[0].path;
    return;
  }
  portSelect.value = "";
}

function renderProjects() {
  if (!projectsList) return;
  projectsList.innerHTML = "";
  if (!projects.length) {
    projectsList.innerHTML = '<div class="agent-status">No sandbox projects yet.</div>';
    return;
  }
  projects.forEach((project) => {
    const item = document.createElement("button");
    item.type = "button";
    item.className = `agent-project-item${activeProject?.id === project.id ? " active" : ""}`;
    item.addEventListener("click", () => void selectProject(project.id));

    const name = document.createElement("div");
    name.className = "agent-project-name";
    name.textContent = project.name;

    const meta = document.createElement("div");
    meta.className = "agent-project-meta";
    meta.textContent = `${project.presetId} · ${project.displayProfile} display`;

    item.appendChild(name);
    item.appendChild(meta);
    projectsList.appendChild(item);
  });
}

function renderChatHistory() {
  if (!chatMessages) return;
  chatMessages.innerHTML = "";
  if (!activeProject) {
    chatMessages.innerHTML = '<div class="agent-status">Select a sandbox project first.</div>';
    return;
  }
  if (!chatHistory.length) {
    chatMessages.innerHTML = '<div class="agent-status">No Agent conversation yet.</div>';
    return;
  }
  chatHistory.forEach((message) => {
    const wrapper = document.createElement("div");
    wrapper.className = `agent-chat-message ${message.role}`;

    const role = document.createElement("div");
    role.className = "agent-chat-role";
    role.textContent = message.role === "assistant" ? "Agent" : "You";

    const content = document.createElement("div");
    content.className = "agent-chat-content";
    content.textContent = message.content;

    wrapper.appendChild(role);
    wrapper.appendChild(content);
    chatMessages.appendChild(wrapper);
  });
  chatMessages.scrollTop = chatMessages.scrollHeight;
}

function renderProposals() {
  if (!proposalsList) return;
  proposalsList.innerHTML = "";
  if (!activeProject) {
    proposalsList.innerHTML = '<div class="agent-status">Select a sandbox project first.</div>';
    updateApplyButtonState();
    return;
  }
  if (!proposedOperations.length) {
    proposalsList.innerHTML = '<div class="agent-status">No proposed file changes yet.</div>';
    updateApplyButtonState();
    return;
  }

  proposedOperations.forEach((operation) => {
    const item = document.createElement("div");
    item.className = "agent-proposal-item";

    const title = document.createElement("div");
    title.className = "agent-project-name";
    title.textContent =
      operation.type === "delete_file" ? "Delete file" : "Write file";

    const pathNode = document.createElement("div");
    pathNode.className = "agent-proposal-path";
    pathNode.textContent = operation.path;

    const summary = document.createElement("div");
    summary.className = "agent-proposal-summary";
    summary.textContent = operation.summary || "";

    item.appendChild(title);
    item.appendChild(pathNode);
    item.appendChild(summary);
    proposalsList.appendChild(item);
  });
  updateApplyButtonState();
}

function renderWorkspaceSummary() {
  if (!activeProject) {
    setText(workspaceTitle, "No project selected");
    setText(workspaceSubtitle, "Select or create a sandbox project to begin.");
    if (buildCommandInput) buildCommandInput.value = "";
    if (uploadCommandInput) uploadCommandInput.value = "";
    if (exportBtn) exportBtn.disabled = true;
    if (savePortBtn) savePortBtn.disabled = true;
    if (sendBtn) sendBtn.disabled = true;
    if (fixFromErrorBtn) fixFromErrorBtn.disabled = true;
    setText(portStatus, serialPorts.length ? "Select a project to save a flash port." : "No USB serial ports loaded.");
    setText(chatStatus, "Select a project to start chatting with the ESP32 Agent.");
    setText(savepointStatus, "Select a project to use savepoints.");
    terminalState = null;
    renderTerminal();
    renderChatHistory();
    renderProposals();
    renderSavepoints();
    populateModelOptions(modelOptions[0]?.id || "");
    populateSerialPortOptions("");
    return;
  }
  setText(workspaceTitle, activeProject.name);
  setText(
    workspaceSubtitle,
    `${activeProject.presetId} · ${activeProject.displayProfile} display · stored in sandbox workspace`,
  );
  if (buildCommandInput) buildCommandInput.value = activeProject.buildCommand || "";
  if (uploadCommandInput) uploadCommandInput.value = activeProject.uploadCommand || "";
  if (exportBtn) exportBtn.disabled = false;
  if (savePortBtn) savePortBtn.disabled = false;
  if (sendBtn) sendBtn.disabled = false;
   if (fixFromErrorBtn) fixFromErrorBtn.disabled = false;
  populateSerialPortOptions(activeProject.uploadPort || "");
  setText(
    portStatus,
    activeProject.uploadPort
      ? `Saved flash port: ${activeProject.uploadPort}`
      : serialPorts.length
        ? "Choose a detected USB serial port for this project."
        : "No USB serial ports detected on the Pi right now.",
  );
  updateApplyButtonState();
}

function createTreeNode(node) {
  const wrapper = document.createElement("div");
  wrapper.className = "agent-tree-node";

  const label = document.createElement("div");
  label.className = "agent-tree-label";
  label.textContent = node.type === "directory" ? "Directory" : "File";
  wrapper.appendChild(label);

  if (node.type === "directory") {
    const title = document.createElement("div");
    title.textContent = node.name;
    wrapper.appendChild(title);
    if (Array.isArray(node.children) && node.children.length) {
      const list = document.createElement("div");
      list.className = "agent-tree-list";
      node.children.forEach((child) => list.appendChild(createTreeNode(child)));
      wrapper.appendChild(list);
    }
    return wrapper;
  }

  const button = document.createElement("button");
  button.type = "button";
  button.className = `agent-tree-button file${activeFilePath === node.path ? " active" : ""}`;
  button.textContent = node.path;
  button.addEventListener("click", () => void loadProjectFile(node.path));
  wrapper.appendChild(button);
  return wrapper;
}

function renderFileTree(files) {
  if (!fileTree) return;
  fileTree.innerHTML = "";
  if (!files?.length) {
    fileTree.innerHTML = '<div class="agent-status">No files in this sandbox workspace.</div>';
    return;
  }
  files.forEach((node) => fileTree.appendChild(createTreeNode(node)));
}

function renderSavepoints() {
  if (!savepointsList) return;
  savepointsList.innerHTML = "";
  if (!activeProject) {
    savepointsList.innerHTML = '<div class="agent-status">Select a sandbox project first.</div>';
    return;
  }
  if (!checkpoints.length) {
    savepointsList.innerHTML = '<div class="agent-status">No savepoints yet.</div>';
    return;
  }
  checkpoints.forEach((checkpoint) => {
    const item = document.createElement("div");
    item.className = "agent-savepoint-item";

    const header = document.createElement("div");
    header.className = "agent-savepoint-header";

    const text = document.createElement("div");
    const title = document.createElement("div");
    title.className = "agent-project-name";
    title.textContent = checkpoint.label;
    text.appendChild(title);

    const meta = document.createElement("div");
    meta.className = "agent-project-meta";
    meta.textContent = `${new Date(checkpoint.createdAt).toLocaleString()} · ${checkpoint.fileCount} files`;
    text.appendChild(meta);

    if (checkpoint.note) {
      const note = document.createElement("div");
      note.className = "agent-savepoint-note";
      note.textContent = checkpoint.note;
      text.appendChild(note);
    }

    const restoreBtn = document.createElement("button");
    restoreBtn.type = "button";
    restoreBtn.className = "agent-button secondary compact";
    restoreBtn.textContent = "Restore";
    restoreBtn.addEventListener("click", () => void restoreSavepoint(checkpoint.id));

    header.appendChild(text);
    header.appendChild(restoreBtn);
    item.appendChild(header);
    savepointsList.appendChild(item);
  });
}

async function loadPresets() {
  const data = await fetchJson("/api/esp32-agent/presets", { cache: "no-store" });
  presets = Array.isArray(data.presets) ? data.presets : [];
  populatePresetOptions();
  setText(createStatus, "Choose a preset and create or import a sandbox project.");
}

async function loadGlobalSettings() {
  const data = await fetchJson("/api/settings", { cache: "no-store" });
  modelOptions = Array.isArray(data.llmModelOptions) ? data.llmModelOptions : [];
  populateModelOptions(data.settings?.llmModel || modelOptions[0]?.id || "");
}

async function loadAgentSettings() {
  const data = await fetchJson("/api/esp32-agent/settings", { cache: "no-store" });
  defaultAgentPersonalityPrompt = data.defaultPersonalityPrompt || "";
  defaultErrorAgentPersonalityPrompt = data.defaultErrorPersonalityPrompt || "";
  if (personalityPrompt) {
    personalityPrompt.value = data.settings?.personalityPrompt || defaultAgentPersonalityPrompt;
  }
  if (errorPersonalityPrompt) {
    errorPersonalityPrompt.value =
      data.settings?.errorPersonalityPrompt || defaultErrorAgentPersonalityPrompt;
  }
  setText(personalityStatus, "Agent coding prompt loaded.");
  setText(errorPersonalityStatus, "Agent error-fix prompt loaded.");
}

async function loadSerialPorts() {
  const data = await fetchJson("/api/esp32-agent/serial-ports", { cache: "no-store" });
  serialPorts = Array.isArray(data.ports) ? data.ports : [];
  populateSerialPortOptions(activeProject?.uploadPort || "");
  if (!serialPorts.length) {
    setText(portStatus, "No USB serial ports detected on the Pi right now.");
    return;
  }
  if (activeProject?.uploadPort) {
    setText(portStatus, `Detected ${serialPorts.length} USB serial port${serialPorts.length === 1 ? "" : "s"}.`);
    return;
  }
  setText(
    portStatus,
    serialPorts.length === 1
      ? "Detected 1 USB serial port. You can save it to this project."
      : `Detected ${serialPorts.length} USB serial ports. Choose one for flashing.`,
  );
}

async function loadProjects() {
  const data = await fetchJson("/api/esp32-agent/projects", { cache: "no-store" });
  projects = Array.isArray(data.projects) ? data.projects : [];
  if (activeProject) {
    activeProject = projects.find((project) => project.id === activeProject.id) || null;
  }
  renderProjects();
  renderWorkspaceSummary();
}

async function loadProjectTree(projectId) {
  const data = await fetchJson(`/api/esp32-agent/projects/${escapePathSegment(projectId)}/tree`, {
    cache: "no-store",
  });
  renderFileTree(data.files || []);
}

async function loadProjectCheckpoints(projectId) {
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(projectId)}/checkpoints`,
    { cache: "no-store" },
  );
  checkpoints = Array.isArray(data.checkpoints) ? data.checkpoints : [];
  renderSavepoints();
  setText(
    savepointStatus,
    checkpoints.length
      ? `Loaded ${checkpoints.length} savepoint${checkpoints.length === 1 ? "" : "s"}.`
      : "No savepoints yet.",
  );
}

async function loadProjectChat(projectId) {
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(projectId)}/chat`,
    { cache: "no-store" },
  );
  chatHistory = Array.isArray(data.messages) ? data.messages : [];
  renderChatHistory();
}

async function loadErrorLog(projectId) {
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(projectId)}/error-log`,
    { cache: "no-store" },
  );
  if (errorLog) {
    errorLog.value = data.content || "";
  }
  setText(errorStatus, data.content ? "Loaded saved error log." : "No error log saved.");
}

async function loadTerminalState(projectId) {
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(projectId)}/terminal`,
    { cache: "no-store" },
  );
  terminalState = data.terminal || null;
  renderTerminal();
  if (terminalState?.status === "running") {
    ensureTerminalPolling();
  } else {
    stopTerminalPolling();
  }
}

async function selectProject(projectId) {
  const data = await fetchJson(`/api/esp32-agent/projects/${escapePathSegment(projectId)}`, {
    cache: "no-store",
  });
  activeProject = data.project || null;
  activeFilePath = "";
  checkpoints = [];
  proposedOperations = [];
  chatHistory = [];
  terminalState = null;
  if (fileEditor) {
    fileEditor.value = "";
  }
  if (chatInput) {
    chatInput.value = "";
  }
  setText(editorPath, "Choose a file from the tree.");
  setText(editorStatus, "No file loaded.");
  renderProjects();
  renderWorkspaceSummary();
  renderChatHistory();
  renderProposals();
  if (activeProject) {
    await Promise.all([
      loadProjectTree(activeProject.id),
      loadProjectCheckpoints(activeProject.id),
      loadProjectChat(activeProject.id),
      loadErrorLog(activeProject.id),
      loadSerialPorts(),
      loadTerminalState(activeProject.id),
    ]);
  } else {
    stopTerminalPolling();
    renderFileTree([]);
    renderSavepoints();
    renderTerminal();
  }
}

async function createProject() {
  const name = projectNameInput?.value?.trim() || "";
  if (!name) {
    setText(createStatus, "Enter a project name first.");
    return;
  }
  setText(createStatus, "Creating sandbox project…");
  const data = await fetchJson("/api/esp32-agent/projects", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      name,
      presetId: presetSelect?.value || "",
      agentModel: modelSelect?.value || "",
    }),
  });
  if (projectNameInput) {
    projectNameInput.value = "";
  }
  setText(createStatus, "Sandbox project created.");
  await loadProjects();
  if (data.project?.id) {
    await selectProject(data.project.id);
  }
}

async function importProjectBundle(file) {
  if (!file) {
    setText(createStatus, "Choose a project bundle file first.");
    return;
  }
  setText(createStatus, "Importing sandbox project bundle…");
  const bundleContent = await file.text();
  const data = await fetchJson("/api/esp32-agent/projects/import", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      bundleContent,
    }),
  });
  setText(createStatus, "Project bundle imported.");
  await loadProjects();
  if (data.project?.id) {
    await selectProject(data.project.id);
  }
}

async function exportProjectBundle() {
  if (!activeProject) {
    setText(createStatus, "Select a project first.");
    return;
  }
  setText(createStatus, "Exporting project bundle…");
  const response = await fetch(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/export`,
    { cache: "no-store" },
  );
  if (!response.ok) {
    const data = await response.json().catch(() => ({}));
    throw new Error(data?.error || `Request failed: ${response.status}`);
  }
  const blob = await response.blob();
  const contentDisposition = response.headers.get("Content-Disposition") || "";
  const match = /filename="([^"]+)"/i.exec(contentDisposition);
  const fileName = match?.[1] || `${activeProject.slug || "esp32-project"}-bundle.json`;
  const objectUrl = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = objectUrl;
  link.download = fileName;
  document.body.appendChild(link);
  link.click();
  link.remove();
  URL.revokeObjectURL(objectUrl);
  setText(createStatus, "Project bundle downloaded.");
}

async function loadProjectFile(relativePath) {
  if (!activeProject) return;
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/file?path=${encodeURIComponent(relativePath)}`,
    { cache: "no-store" },
  );
  activeFilePath = data.file?.path || relativePath;
  if (fileEditor) {
    fileEditor.value = data.file?.content || "";
  }
  setText(editorPath, activeFilePath);
  setText(editorStatus, "File loaded.");
  await loadProjectTree(activeProject.id);
}

async function saveActiveFile() {
  if (!activeProject || !activeFilePath) {
    setText(editorStatus, "Choose a file before saving.");
    return;
  }
  setText(editorStatus, "Saving file…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/file`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        path: activeFilePath,
        content: fileEditor?.value || "",
      }),
    },
  );
  activeProject = data.project || activeProject;
  setText(editorStatus, "File saved.");
  await loadProjects();
  await loadProjectTree(activeProject.id);
}

async function createSavepoint() {
  if (!activeProject) {
    setText(savepointStatus, "Select a project first.");
    return;
  }
  setText(savepointStatus, "Creating savepoint…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/checkpoints`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        label: savepointLabelInput?.value || "",
        note: savepointNoteInput?.value || "",
      }),
    },
  );
  activeProject = data.project || activeProject;
  if (savepointLabelInput) savepointLabelInput.value = "";
  if (savepointNoteInput) savepointNoteInput.value = "";
  setText(savepointStatus, "Savepoint created.");
  await loadProjects();
  await loadProjectCheckpoints(activeProject.id);
}

async function restoreSavepoint(checkpointId) {
  if (!activeProject) {
    setText(savepointStatus, "Select a project first.");
    return;
  }
  setText(savepointStatus, "Restoring savepoint…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/checkpoints/${escapePathSegment(checkpointId)}/restore`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
    },
  );
  activeProject = data.project || activeProject;
  activeFilePath = "";
  proposedOperations = [];
  if (fileEditor) {
    fileEditor.value = "";
  }
  setText(editorPath, "Choose a file from the tree.");
  setText(editorStatus, "Savepoint restored. Reload a file to continue.");
  setText(savepointStatus, "Savepoint restored.");
  renderProposals();
  await loadProjects();
  await Promise.all([
    loadProjectTree(activeProject.id),
    loadProjectCheckpoints(activeProject.id),
    loadErrorLog(activeProject.id),
  ]);
}

async function saveAgentModel() {
  await fetchJson("/api/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      llmModel: modelSelect?.value || "",
    }),
  });
  await loadGlobalSettings();
  setText(createStatus, "Device AI model saved.");
}

async function saveProjectPort() {
  if (!activeProject) {
    setText(portStatus, "Select a project first.");
    return;
  }
  setText(portStatus, "Saving flash port…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/settings`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        uploadPort: portSelect?.value || "",
      }),
    },
  );
  activeProject = data.project || activeProject;
  await loadProjects();
  renderWorkspaceSummary();
  setText(
    portStatus,
    activeProject.uploadPort
      ? `Saved flash port: ${activeProject.uploadPort}`
      : "Cleared saved flash port for this project.",
  );
}

async function saveAgentPersonalityPrompt() {
  setText(personalityStatus, "Saving Agent prompt…");
  const data = await fetchJson("/api/esp32-agent/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      personalityPrompt: personalityPrompt?.value || "",
      errorPersonalityPrompt: errorPersonalityPrompt?.value || "",
    }),
  });
  defaultAgentPersonalityPrompt = data.defaultPersonalityPrompt || defaultAgentPersonalityPrompt;
  defaultErrorAgentPersonalityPrompt =
    data.defaultErrorPersonalityPrompt || defaultErrorAgentPersonalityPrompt;
  if (personalityPrompt) {
    personalityPrompt.value = data.settings?.personalityPrompt || defaultAgentPersonalityPrompt;
  }
  if (errorPersonalityPrompt) {
    errorPersonalityPrompt.value =
      data.settings?.errorPersonalityPrompt || defaultErrorAgentPersonalityPrompt;
  }
  setText(personalityStatus, "Agent coding prompt saved.");
  setText(errorPersonalityStatus, "Agent error-fix prompt saved.");
}

function resetAgentPersonalityPrompt() {
  if (personalityPrompt) {
    personalityPrompt.value = defaultAgentPersonalityPrompt;
  }
  setText(personalityStatus, "Reset to the default ESP32 Agent prompt. Save to apply it.");
}

async function saveErrorAgentPersonalityPrompt() {
  setText(errorPersonalityStatus, "Saving Agent error-fix prompt…");
  const data = await fetchJson("/api/esp32-agent/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      personalityPrompt: personalityPrompt?.value || "",
      errorPersonalityPrompt: errorPersonalityPrompt?.value || "",
    }),
  });
  defaultAgentPersonalityPrompt = data.defaultPersonalityPrompt || defaultAgentPersonalityPrompt;
  defaultErrorAgentPersonalityPrompt =
    data.defaultErrorPersonalityPrompt || defaultErrorAgentPersonalityPrompt;
  if (personalityPrompt) {
    personalityPrompt.value = data.settings?.personalityPrompt || defaultAgentPersonalityPrompt;
  }
  if (errorPersonalityPrompt) {
    errorPersonalityPrompt.value =
      data.settings?.errorPersonalityPrompt || defaultErrorAgentPersonalityPrompt;
  }
  setText(errorPersonalityStatus, "Agent error-fix prompt saved.");
}

function resetErrorAgentPersonalityPrompt() {
  if (errorPersonalityPrompt) {
    errorPersonalityPrompt.value = defaultErrorAgentPersonalityPrompt;
  }
  setText(
    errorPersonalityStatus,
    "Reset to the default ESP32 error-fix prompt. Save to apply it.",
  );
}

async function sendAgentPrompt(mode = "general") {
  if (!activeProject) {
    setText(chatStatus, "Select a project first.");
    return;
  }
  const prompt =
    chatInput?.value?.trim() ||
    (mode === "error_fix"
      ? "Use the saved error log to make the smallest practical fix for this project."
      : "");
  if (!prompt) {
    setText(chatStatus, "Enter a message for the Agent first.");
    return;
  }
  setText(
    chatStatus,
    mode === "error_fix"
      ? "Asking the ESP32 error-fix agent…"
      : "Asking the ESP32 Agent…",
  );
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/chat`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        prompt,
        mode,
      }),
    },
  );
  if (chatInput) {
    chatInput.value = "";
  }
  chatHistory = Array.isArray(data.messages) ? data.messages : chatHistory;
  proposedOperations = Array.isArray(data.operations) ? data.operations : [];
  renderChatHistory();
  renderProposals();
  setText(
    chatStatus,
    proposedOperations.length
      ? `${mode === "error_fix" ? "Error-fix agent" : "Agent"} prepared ${proposedOperations.length} proposed change${proposedOperations.length === 1 ? "" : "s"}.`
      : `${mode === "error_fix" ? "Error-fix agent" : "Agent"} replied without proposing file changes.`,
  );
}

async function applyProposedChanges() {
  if (!activeProject) {
    setText(chatStatus, "Select a project first.");
    return;
  }
  if (!proposedOperations.length) {
    setText(chatStatus, "No proposed changes to apply.");
    return;
  }
  setText(chatStatus, "Applying proposed changes with an automatic savepoint…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/apply`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        operations: proposedOperations,
      }),
    },
  );
  activeProject = data.project || activeProject;
  proposedOperations = [];
  activeFilePath = "";
  if (fileEditor) {
    fileEditor.value = "";
  }
  setText(editorPath, "Choose a file from the tree.");
  setText(editorStatus, "Agent changes applied. Reload a file to review.");
  renderProposals();
  await loadProjects();
  await Promise.all([
    loadProjectTree(activeProject.id),
    loadProjectCheckpoints(activeProject.id),
    loadProjectChat(activeProject.id),
  ]);
  setText(
    chatStatus,
    `Applied ${data.appliedCount || 0} change${data.appliedCount === 1 ? "" : "s"} and created savepoint "${data.checkpoint?.label || "Before agent apply"}".`,
  );
}

async function saveErrorLog() {
  if (!activeProject) {
    setText(errorStatus, "Select a project first.");
    return;
  }
  setText(errorStatus, "Saving error log…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/error-log`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        content: errorLog?.value || "",
      }),
    },
  );
  activeProject = data.project || activeProject;
  setText(errorStatus, "Error log saved with this sandbox project.");
  await loadProjects();
}

async function runTerminalCommand() {
  if (!activeProject) {
    setText(terminalStatus, "Select a project first.");
    return;
  }
  const command = terminalInput?.value?.trim() || "";
  if (!command) {
    setText(terminalStatus, "Enter a terminal command first.");
    return;
  }
  setText(terminalStatus, "Starting terminal command…");
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/terminal/run`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ command }),
    },
  );
  terminalState = data.terminal || null;
  renderTerminal();
  ensureTerminalPolling();
}

async function stopTerminalCommand() {
  if (!activeProject) {
    setText(terminalStatus, "Select a project first.");
    return;
  }
  const data = await fetchJson(
    `/api/esp32-agent/projects/${escapePathSegment(activeProject.id)}/terminal/stop`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
    },
  );
  terminalState = data.terminal || null;
  renderTerminal();
  stopTerminalPolling();
}

async function queueCommandToTerminal(command, statusNode, successMessage) {
  if (!command) {
    return;
  }
  if (terminalInput) {
    terminalInput.value = command;
  }
  setText(statusNode, successMessage);
  try {
    await navigator.clipboard.writeText(command);
  } catch {
    // Terminal fill is the primary action; clipboard is only a best-effort bonus.
  }
}

async function copyText(value, statusNode, successMessage) {
  if (!value) return;
  await navigator.clipboard.writeText(value);
  setText(statusNode, successMessage);
}

createBtn?.addEventListener("click", () => {
  void createProject().catch((error) => {
    setText(createStatus, error instanceof Error ? error.message : "Failed to create project.");
  });
});

importBtn?.addEventListener("click", () => {
  importInput?.click();
});

importInput?.addEventListener("change", () => {
  const file = importInput.files?.[0];
  void importProjectBundle(file).catch((error) => {
    setText(createStatus, error instanceof Error ? error.message : "Failed to import project.");
  }).finally(() => {
    if (importInput) {
      importInput.value = "";
    }
  });
});

refreshProjectsBtn?.addEventListener("click", () => {
  void loadProjects().catch((error) => {
    setText(createStatus, error instanceof Error ? error.message : "Failed to refresh projects.");
  });
});

refreshPortsBtn?.addEventListener("click", () => {
  void loadSerialPorts().catch((error) => {
    setText(portStatus, error instanceof Error ? error.message : "Failed to refresh USB serial ports.");
  });
});

savePortBtn?.addEventListener("click", () => {
  void saveProjectPort().catch((error) => {
    setText(portStatus, error instanceof Error ? error.message : "Failed to save flash port.");
  });
});

saveFileBtn?.addEventListener("click", () => {
  void saveActiveFile().catch((error) => {
    setText(editorStatus, error instanceof Error ? error.message : "Failed to save file.");
  });
});

savepointBtn?.addEventListener("click", () => {
  void createSavepoint().catch((error) => {
    setText(savepointStatus, error instanceof Error ? error.message : "Failed to create savepoint.");
  });
});

saveModelBtn?.addEventListener("click", () => {
  void saveAgentModel().catch((error) => {
    setText(createStatus, error instanceof Error ? error.message : "Failed to save model.");
  });
});

savePersonalityBtn?.addEventListener("click", () => {
  void saveAgentPersonalityPrompt().catch((error) => {
    setText(personalityStatus, error instanceof Error ? error.message : "Failed to save Agent prompt.");
  });
});

resetPersonalityBtn?.addEventListener("click", () => {
  resetAgentPersonalityPrompt();
});

saveErrorPersonalityBtn?.addEventListener("click", () => {
  void saveErrorAgentPersonalityPrompt().catch((error) => {
    setText(
      errorPersonalityStatus,
      error instanceof Error ? error.message : "Failed to save Agent error prompt.",
    );
  });
});

resetErrorPersonalityBtn?.addEventListener("click", () => {
  resetErrorAgentPersonalityPrompt();
});

sendBtn?.addEventListener("click", () => {
  void sendAgentPrompt().catch((error) => {
    setText(chatStatus, error instanceof Error ? error.message : "Failed to talk to the Agent.");
  });
});

fixFromErrorBtn?.addEventListener("click", () => {
  void sendAgentPrompt("error_fix").catch((error) => {
    setText(
      chatStatus,
      error instanceof Error ? error.message : "Failed to run the error-fix Agent.",
    );
  });
});

applyBtn?.addEventListener("click", () => {
  void applyProposedChanges().catch((error) => {
    setText(chatStatus, error instanceof Error ? error.message : "Failed to apply Agent changes.");
  });
});

exportBtn?.addEventListener("click", () => {
  void exportProjectBundle().catch((error) => {
    setText(createStatus, error instanceof Error ? error.message : "Failed to export project.");
  });
});

saveErrorBtn?.addEventListener("click", () => {
  void saveErrorLog().catch((error) => {
    setText(errorStatus, error instanceof Error ? error.message : "Failed to save error log.");
  });
});

copyBuildBtn?.addEventListener("click", () => {
  void queueCommandToTerminal(
    buildCommandInput?.value || "",
    createStatus,
    "Build command sent to the terminal box.",
  );
});

copyUploadBtn?.addEventListener("click", () => {
  void queueCommandToTerminal(
    uploadCommandInput?.value || "",
    createStatus,
    "Flash command sent to the terminal box.",
  );
});

runTerminalBtn?.addEventListener("click", () => {
  void runTerminalCommand().catch((error) => {
    setText(
      terminalStatus,
      error instanceof Error ? error.message : "Failed to start terminal command.",
    );
  });
});

stopTerminalBtn?.addEventListener("click", () => {
  void stopTerminalCommand().catch((error) => {
    setText(
      terminalStatus,
      error instanceof Error ? error.message : "Failed to stop terminal command.",
    );
  });
});

refreshTerminalBtn?.addEventListener("click", () => {
  if (!activeProject) {
    setText(terminalStatus, "Select a project first.");
    return;
  }
  void loadTerminalState(activeProject.id).catch((error) => {
    setText(
      terminalStatus,
      error instanceof Error ? error.message : "Failed to refresh terminal output.",
    );
  });
});

Promise.all([
  loadPresets(),
  loadGlobalSettings(),
  loadAgentSettings(),
  loadSerialPorts(),
  loadProjects(),
]).catch((error) => {
  const message = error instanceof Error ? error.message : "Failed to load ESP32 Agent UI.";
  setText(createStatus, message);
});
