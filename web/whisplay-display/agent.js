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
const errorStatus = document.getElementById("agentErrorStatus");

let presets = [];
let modelOptions = [];
let projects = [];
let checkpoints = [];
let activeProject = null;
let activeFilePath = "";

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

function renderWorkspaceSummary() {
  if (!activeProject) {
    setText(workspaceTitle, "No project selected");
    setText(workspaceSubtitle, "Select or create a sandbox project to begin.");
    if (buildCommandInput) buildCommandInput.value = "";
    if (uploadCommandInput) uploadCommandInput.value = "";
    if (exportBtn) exportBtn.disabled = true;
    setText(savepointStatus, "Select a project to use savepoints.");
    renderSavepoints();
    populateModelOptions(modelOptions[0]?.id || "");
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

async function selectProject(projectId) {
  const data = await fetchJson(`/api/esp32-agent/projects/${escapePathSegment(projectId)}`, {
    cache: "no-store",
  });
  activeProject = data.project || null;
  activeFilePath = "";
  checkpoints = [];
  if (fileEditor) {
    fileEditor.value = "";
  }
  setText(editorPath, "Choose a file from the tree.");
  setText(editorStatus, "No file loaded.");
  renderProjects();
  renderWorkspaceSummary();
  if (activeProject) {
    await Promise.all([
      loadProjectTree(activeProject.id),
      loadProjectCheckpoints(activeProject.id),
      loadErrorLog(activeProject.id),
    ]);
  } else {
    renderFileTree([]);
    renderSavepoints();
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
  if (fileEditor) {
    fileEditor.value = "";
  }
  setText(editorPath, "Choose a file from the tree.");
  setText(editorStatus, "Savepoint restored. Reload a file to continue.");
  setText(savepointStatus, "Savepoint restored.");
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
  void copyText(buildCommandInput?.value || "", createStatus, "Build command copied.");
});

copyUploadBtn?.addEventListener("click", () => {
  void copyText(uploadCommandInput?.value || "", createStatus, "Flash command copied.");
});

Promise.all([loadPresets(), loadGlobalSettings(), loadProjects()]).catch((error) => {
  const message = error instanceof Error ? error.message : "Failed to load ESP32 Agent UI.";
  setText(createStatus, message);
});
