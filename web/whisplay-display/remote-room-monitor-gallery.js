const dayList = document.getElementById("galleryDayList");
const dayTitle = document.getElementById("galleryDayTitle");
const hourTitle = document.getElementById("galleryHourTitle");
const hourList = document.getElementById("galleryHourList");
const pageStatus = document.getElementById("galleryStatus");
const pageMeta = document.getElementById("galleryMeta");
const photoGrid = document.getElementById("galleryPhotoGrid");
const selectAllBtn = document.getElementById("gallerySelectAllBtn");
const clearSelectionBtn = document.getElementById("galleryClearSelectionBtn");
const deleteDayBtn = document.getElementById("galleryDeleteDayBtn");
const deleteSelectedBtn = document.getElementById("galleryDeleteSelectedBtn");
const moveSelectedBtn = document.getElementById("galleryMoveSelectedBtn");
const savedCountText = document.getElementById("gallerySavedCount");
const slideshowInterval = document.getElementById("slideshowInterval");
const slideshowToggleBtn = document.getElementById("slideshowToggleBtn");
const viewer = document.getElementById("galleryViewer");
const viewerImage = document.getElementById("galleryViewerImage");
const viewerCaption = document.getElementById("galleryViewerCaption");
const viewerCloseBtn = document.getElementById("galleryViewerCloseBtn");
const viewerPrevBtn = document.getElementById("galleryViewerPrevBtn");
const viewerNextBtn = document.getElementById("galleryViewerNextBtn");

let remoteDays = [];
let remoteHours = [];
let activeDayKey = "";
let activeHourKey = "";
let activeHourPhotos = [];
let selectedFileNames = new Set();
let viewerIndex = 0;
let slideshowTimer = null;
let latestStatus = null;

function setStatus(message, isError = false) {
  if (!pageStatus) return;
  pageStatus.textContent = message;
  pageStatus.classList.toggle("error", Boolean(isError));
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const data = await response.json().catch(() => ({}));
  if (!response.ok || data?.ok === false) {
    throw new Error(data?.error || `Request failed: ${response.status}`);
  }
  return data;
}

function formatBytes(bytes) {
  const units = ["B", "KB", "MB", "GB"];
  let size = Number(bytes || 0);
  let unitIndex = 0;
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex += 1;
  }
  return `${size >= 100 || unitIndex === 0 ? Math.round(size) : size.toFixed(1)} ${units[unitIndex]}`;
}

function getActiveDay() {
  return remoteDays.find((day) => day.dayKey === activeDayKey) || null;
}

function getActiveHour() {
  return remoteHours.find((hour) => hour.hourKey === activeHourKey) || null;
}

function stopSlideshow() {
  if (slideshowTimer) {
    window.clearInterval(slideshowTimer);
    slideshowTimer = null;
  }
  if (slideshowToggleBtn) {
    slideshowToggleBtn.textContent = "Start Slideshow";
  }
}

function showViewerPhoto(index) {
  if (!activeHourPhotos.length || !viewerImage || !viewerCaption || !viewer) {
    return;
  }
  viewerIndex = (index + activeHourPhotos.length) % activeHourPhotos.length;
  const photo = activeHourPhotos[viewerIndex];
  viewerImage.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
  viewerImage.alt = photo.fileName;
  viewerCaption.textContent = `${photo.fileName} · ${new Date(photo.updatedAt).toLocaleString()} · ${formatBytes(photo.sizeBytes)}`;
  viewer.classList.add("open");
}

function toggleSlideshow() {
  if (slideshowTimer) {
    stopSlideshow();
    return;
  }
  const seconds = Math.max(0.5, Number(slideshowInterval?.value || 1));
  slideshowTimer = window.setInterval(() => {
    showViewerPhoto(viewerIndex + 1);
  }, seconds * 1000);
  if (slideshowToggleBtn) {
    slideshowToggleBtn.textContent = "Stop Slideshow";
  }
}

function renderDayList() {
  if (!dayList) return;
  dayList.innerHTML = "";
  if (!remoteDays.length) {
    dayList.innerHTML = '<div class="gallery-empty">No remote day folders yet.</div>';
    return;
  }
  remoteDays.forEach((day) => {
    const item = document.createElement("button");
    item.type = "button";
    item.className = `gallery-day-item${day.dayKey === activeDayKey ? " active" : ""}`;
    item.addEventListener("click", () => {
      void loadDayHours(day.dayKey);
    });

    const image = document.createElement("img");
    image.src = `${day.coverImageUrl}?ts=${day.updatedAt}`;
    image.alt = day.label;

    const name = document.createElement("div");
    name.className = "gallery-day-name";
    name.textContent = day.label;

    const meta = document.createElement("div");
    meta.className = "gallery-day-meta";
    meta.textContent = `${day.count} photo${day.count === 1 ? "" : "s"} · ${formatBytes(day.totalSizeBytes)}`;

    item.appendChild(image);
    item.appendChild(name);
    item.appendChild(meta);
    dayList.appendChild(item);
  });
}

function renderHourList() {
  if (!hourList) return;
  hourList.innerHTML = "";
  if (!activeDayKey) {
    hourList.innerHTML = '<div class="gallery-empty">Choose a day folder to view hour folders.</div>';
    return;
  }
  if (!remoteHours.length) {
    hourList.innerHTML = '<div class="gallery-empty">No hour folders found for this day.</div>';
    return;
  }
  remoteHours.forEach((hour) => {
    const item = document.createElement("button");
    item.type = "button";
    item.className = `gallery-hour-item${hour.hourKey === activeHourKey ? " active" : ""}`;
    item.addEventListener("click", () => {
      void loadHourPhotos(activeDayKey, hour.hourKey);
    });

    const image = document.createElement("img");
    image.src = `${hour.coverImageUrl}?ts=${hour.updatedAt}`;
    image.alt = hour.label;

    const name = document.createElement("div");
    name.className = "gallery-day-name";
    name.textContent = hour.label;

    const meta = document.createElement("div");
    meta.className = "gallery-day-meta";
    meta.textContent = `${hour.count} photo${hour.count === 1 ? "" : "s"} · ${formatBytes(hour.totalSizeBytes)}`;

    item.appendChild(image);
    item.appendChild(name);
    item.appendChild(meta);
    hourList.appendChild(item);
  });
}

function updateSelectionButtons() {
  const selectedCount = selectedFileNames.size;
  if (deleteSelectedBtn) {
    deleteSelectedBtn.disabled = selectedCount === 0;
    deleteSelectedBtn.textContent =
      selectedCount === 0 ? "Delete Selected Remote" : `Delete Selected Remote (${selectedCount})`;
  }
  if (deleteDayBtn) {
    deleteDayBtn.disabled = !activeDayKey;
  }
  if (moveSelectedBtn) {
    moveSelectedBtn.disabled = selectedCount === 0;
    moveSelectedBtn.textContent =
      selectedCount === 0 ? "Move Selected to Saved Gallery" : `Move Selected (${selectedCount})`;
  }
  if (clearSelectionBtn) {
    clearSelectionBtn.disabled = selectedCount === 0;
  }
  if (selectAllBtn) {
    selectAllBtn.disabled = activeHourPhotos.length === 0;
  }
}

function renderPhotos() {
  if (!photoGrid) return;
  photoGrid.innerHTML = "";

  const activeDay = getActiveDay();
  const activeHour = getActiveHour();
  if (dayTitle) {
    dayTitle.textContent = activeDay?.label || "Remote Room Monitor Day";
  }
  if (hourTitle) {
    hourTitle.textContent = activeHour
      ? `${activeHour.label} · ${activeHour.count} photo${activeHour.count === 1 ? "" : "s"}`
      : activeDayKey
        ? "Choose an hour folder to load photos."
        : "Choose a day folder first.";
  }

  if (!activeHourPhotos.length) {
    photoGrid.innerHTML = `<div class="gallery-empty">${
      activeDayKey
        ? "Choose an hour folder to browse its remote captures."
        : "Select a day folder to browse remote captures."
    }</div>`;
    updateSelectionButtons();
    return;
  }

  activeHourPhotos.forEach((photo, index) => {
    const card = document.createElement("div");
    card.className = "gallery-photo-card";

    const img = document.createElement("img");
    img.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
    img.alt = photo.fileName;
    img.style.cursor = "pointer";
    img.addEventListener("click", () => {
      showViewerPhoto(index);
    });

    const name = document.createElement("div");
    name.className = "gallery-photo-name";
    name.textContent = photo.fileName;

    const meta = document.createElement("div");
    meta.className = "gallery-meta";
    meta.textContent = `${new Date(photo.updatedAt).toLocaleString()} · ${formatBytes(photo.sizeBytes)}`;

    const selectRow = document.createElement("div");
    selectRow.className = "gallery-selection-row";

    const checkLabel = document.createElement("label");
    checkLabel.className = "gallery-check";
    const check = document.createElement("input");
    check.type = "checkbox";
    check.checked = selectedFileNames.has(photo.fileName);
    check.addEventListener("change", () => {
      if (check.checked) {
        selectedFileNames.add(photo.fileName);
      } else {
        selectedFileNames.delete(photo.fileName);
      }
      updateSelectionButtons();
    });
    const checkText = document.createElement("span");
    checkText.textContent = "Select";
    checkLabel.appendChild(check);
    checkLabel.appendChild(checkText);

    const actions = document.createElement("div");
    actions.className = "gallery-photo-actions";

    const downloadLink = document.createElement("a");
    downloadLink.className = "gallery-link secondary";
    downloadLink.href = photo.imageUrl;
    downloadLink.download = photo.fileName;
    downloadLink.textContent = "Download";

    const moveBtn = document.createElement("button");
    moveBtn.type = "button";
    moveBtn.className = "gallery-button";
    moveBtn.textContent = "Move to Saved";
    moveBtn.addEventListener("click", () => {
      void moveSelectedPhotos([photo.fileName]);
    });

    actions.appendChild(downloadLink);
    actions.appendChild(moveBtn);
    selectRow.appendChild(checkLabel);
    selectRow.appendChild(actions);

    card.appendChild(img);
    card.appendChild(name);
    card.appendChild(meta);
    card.appendChild(selectRow);
    photoGrid.appendChild(card);
  });
  updateSelectionButtons();
}

async function loadHourPhotos(dayKey, hourKey) {
  activeDayKey = dayKey;
  activeHourKey = hourKey;
  selectedFileNames.clear();
  renderDayList();
  renderHourList();
  updateSelectionButtons();
  setStatus("Loading remote hour folder...");
  const payload = await fetchJson(
    `/api/remote-room-monitor/gallery/day/${encodeURIComponent(dayKey)}/hour/${encodeURIComponent(hourKey)}?ts=${Date.now()}`,
    { cache: "no-store" },
  );
  latestStatus = payload.status || latestStatus;
  updateMeta(latestStatus);
  activeHourPhotos = Array.isArray(payload.photos) ? payload.photos : [];
  renderPhotos();
  const hourLabel = getActiveHour()?.label || hourKey;
  setStatus(
    activeHourPhotos.length
      ? `Loaded ${activeHourPhotos.length} remote photo${activeHourPhotos.length === 1 ? "" : "s"} for ${hourLabel}.`
      : `No remote photos found for ${hourLabel}.`,
  );
}

async function loadDayHours(dayKey) {
  activeDayKey = dayKey;
  activeHourKey = "";
  remoteHours = [];
  selectedFileNames.clear();
  activeHourPhotos = [];
  renderDayList();
  renderHourList();
  renderPhotos();
  setStatus("Loading remote hour folders...");
  const payload = await fetchJson(
    `/api/remote-room-monitor/gallery/day/${encodeURIComponent(dayKey)}/hours?ts=${Date.now()}`,
    { cache: "no-store" },
  );
  latestStatus = payload.status || latestStatus;
  remoteHours = Array.isArray(payload.hours) ? payload.hours : [];
  updateMeta(latestStatus);
  renderHourList();
  const nextHourKey = remoteHours.some((hour) => hour.hourKey === activeHourKey)
    ? activeHourKey
    : remoteHours[0]?.hourKey || "";
  if (nextHourKey) {
    await loadHourPhotos(dayKey, nextHourKey);
    return;
  }
  activeHourKey = "";
  renderPhotos();
  setStatus("No hour folders found for that day.");
}

function updateMeta(status) {
  latestStatus = status || latestStatus;
  if (!pageMeta || !latestStatus) {
    return;
  }
  const peerLabel = latestStatus.peerUrl || "GroqBotNet peer";
  const summary = [
    peerLabel,
    `${latestStatus.totalCount || 0} capture${latestStatus.totalCount === 1 ? "" : "s"}`,
    `${latestStatus.dayCount || remoteDays.length} day folder${(latestStatus.dayCount || remoteDays.length) === 1 ? "" : "s"}`,
    activeDayKey && remoteHours.length
      ? `${remoteHours.length} hour folder${remoteHours.length === 1 ? "" : "s"} in ${getActiveDay()?.label || activeDayKey}`
      : null,
    `${latestStatus.savedCount || 0} saved here`,
    latestStatus.freeSpaceBytes ? `Remote free ${formatBytes(latestStatus.freeSpaceBytes)}` : null,
    latestStatus.freeSpaceReserveBytes ? `Remote reserve ${formatBytes(latestStatus.freeSpaceReserveBytes)}` : null,
  ].filter(Boolean);
  pageMeta.textContent = summary.join(" · ");
  if (savedCountText) {
    savedCountText.textContent = `${latestStatus.savedCount || 0} saved`;
  }
}

async function loadRemoteGallery() {
  setStatus("Loading remote room monitor folders...");
  const payload = await fetchJson(`/api/remote-room-monitor/gallery/days?ts=${Date.now()}`, {
    cache: "no-store",
  });
  remoteDays = Array.isArray(payload.days) ? payload.days : [];
  updateMeta(payload.status || null);
  renderDayList();
  if (!activeDayKey && remoteDays.length) {
    await loadDayHours(remoteDays[0].dayKey);
    updateMeta(payload.status || null);
    return;
  }
  if (activeDayKey && remoteDays.some((day) => day.dayKey === activeDayKey)) {
    await loadDayHours(activeDayKey);
    updateMeta(payload.status || null);
    return;
  }
  activeDayKey = "";
  activeHourKey = "";
  remoteHours = [];
  activeHourPhotos = [];
  renderHourList();
  renderPhotos();
  setStatus(remoteDays.length ? "Choose a remote day folder." : "No remote room monitor folders yet.");
}

async function moveSelectedPhotos(fileNames) {
  const uniqueNames = [...new Set(fileNames.filter(Boolean))];
  if (!uniqueNames.length) {
    setStatus("Select at least one photo first.", true);
    return;
  }
  setStatus(`Moving ${uniqueNames.length} remote photo${uniqueNames.length === 1 ? "" : "s"} to Saved Gallery...`);
  const payload = await fetchJson("/api/remote-room-monitor/gallery/move", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ fileNames: uniqueNames }),
  });
  uniqueNames.forEach((fileName) => selectedFileNames.delete(fileName));
  updateMeta(payload.status || null);
  await loadRemoteGallery();
  setStatus(
    payload.moved?.length
      ? `Moved ${payload.moved.length} remote photo${payload.moved.length === 1 ? "" : "s"} into Saved Gallery.${payload.skipped?.length ? ` Skipped ${payload.skipped.length}.` : ""}`
      : "No remote photos were moved.",
  );
}

async function deleteSelectedPhotos(fileNames) {
  const uniqueNames = [...new Set(fileNames.filter(Boolean))];
  if (!uniqueNames.length) {
    setStatus("Select at least one photo first.", true);
    return;
  }
  if (!window.confirm(`Delete ${uniqueNames.length} selected remote room monitor photo${uniqueNames.length === 1 ? "" : "s"}?`)) {
    return;
  }
  setStatus(`Deleting ${uniqueNames.length} selected remote photo${uniqueNames.length === 1 ? "" : "s"}...`);
  const payload = await fetchJson("/api/remote-room-monitor/gallery", {
    method: "DELETE",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ fileNames: uniqueNames }),
  });
  uniqueNames.forEach((fileName) => selectedFileNames.delete(fileName));
  updateMeta(payload.status || null);
  await loadRemoteGallery();
  setStatus(
    payload.deleted?.length
      ? `Deleted ${payload.deleted.length} remote photo${payload.deleted.length === 1 ? "" : "s"}.${payload.skipped?.length ? ` Skipped ${payload.skipped.length}.` : ""}`
      : "No remote photos were deleted.",
  );
}

async function deleteCurrentDay() {
  if (!activeDayKey) {
    setStatus("Choose a remote day folder first.", true);
    return;
  }
  const activeDayLabel = getActiveDay()?.label || activeDayKey;
  if (!window.confirm(`Delete all remote room monitor photos from ${activeDayLabel}?`)) {
    return;
  }
  setStatus(`Deleting all remote photos from ${activeDayLabel}...`);
  const payload = await fetchJson(`/api/remote-room-monitor/gallery/day/${encodeURIComponent(activeDayKey)}`, {
    method: "DELETE",
  });
  selectedFileNames.clear();
  activeDayKey = "";
  activeHourKey = "";
  updateMeta(payload.status || null);
  await loadRemoteGallery();
  setStatus(
    payload.deleted?.length
      ? `Deleted ${payload.deleted.length} remote photo${payload.deleted.length === 1 ? "" : "s"} from ${activeDayLabel}.${payload.skipped?.length ? ` Skipped ${payload.skipped.length}.` : ""}`
      : `No remote photos were deleted from ${activeDayLabel}.`,
  );
}

selectAllBtn?.addEventListener("click", () => {
  activeHourPhotos.forEach((photo) => selectedFileNames.add(photo.fileName));
  renderPhotos();
});

clearSelectionBtn?.addEventListener("click", () => {
  selectedFileNames.clear();
  renderPhotos();
});

moveSelectedBtn?.addEventListener("click", () => {
  void moveSelectedPhotos([...selectedFileNames]).catch((error) => {
    setStatus(error instanceof Error ? error.message : "Move failed.", true);
  });
});

deleteSelectedBtn?.addEventListener("click", () => {
  void deleteSelectedPhotos([...selectedFileNames]).catch((error) => {
    setStatus(error instanceof Error ? error.message : "Delete failed.", true);
  });
});

deleteDayBtn?.addEventListener("click", () => {
  void deleteCurrentDay().catch((error) => {
    setStatus(error instanceof Error ? error.message : "Delete failed.", true);
  });
});

viewerCloseBtn?.addEventListener("click", () => {
  stopSlideshow();
  viewer?.classList.remove("open");
});

viewerPrevBtn?.addEventListener("click", () => {
  showViewerPhoto(viewerIndex - 1);
});

viewerNextBtn?.addEventListener("click", () => {
  showViewerPhoto(viewerIndex + 1);
});

slideshowToggleBtn?.addEventListener("click", () => {
  toggleSlideshow();
});

viewer?.addEventListener("click", (event) => {
  if (event.target === viewer) {
    stopSlideshow();
    viewer.classList.remove("open");
  }
});

void loadRemoteGallery().catch((error) => {
  setStatus(error instanceof Error ? error.message : "Failed to load remote room monitor gallery.", true);
});
