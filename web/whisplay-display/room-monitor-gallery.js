const mode = document.body.dataset.galleryView || "monitor";

const dayList = document.getElementById("galleryDayList");
const dayTitle = document.getElementById("galleryDayTitle");
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

let roomMonitorDays = [];
let activeDayKey = "";
let activeDayPhotos = [];
let selectedFileNames = new Set();
let savedPhotos = [];
let viewerIndex = 0;
let slideshowTimer = null;

function setStatus(message, isError = false) {
  if (!pageStatus) return;
  pageStatus.textContent = message;
  pageStatus.classList.toggle("error", Boolean(isError));
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
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
  if (!savedPhotos.length || !viewerImage || !viewerCaption || !viewer) {
    return;
  }
  viewerIndex = (index + savedPhotos.length) % savedPhotos.length;
  const photo = savedPhotos[viewerIndex];
  viewerImage.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
  viewerImage.alt = photo.fileName;
  viewerCaption.textContent = `${photo.fileName} · ${new Date(photo.updatedAt).toLocaleString()} · ${formatBytes(photo.sizeBytes)}`;
  viewer.classList.add("open");
}

function startSlideshow() {
  stopSlideshow();
  const seconds = Number(slideshowInterval?.value || 1);
  slideshowTimer = window.setInterval(() => {
    showViewerPhoto(viewerIndex + 1);
  }, Math.max(0.5, seconds) * 1000);
  if (slideshowToggleBtn) {
    slideshowToggleBtn.textContent = "Stop Slideshow";
  }
}

function toggleSlideshow() {
  if (slideshowTimer) {
    stopSlideshow();
    return;
  }
  startSlideshow();
}

function renderDayList() {
  if (!dayList) return;
  dayList.innerHTML = "";
  if (!roomMonitorDays.length) {
    dayList.innerHTML = '<div class="gallery-empty">No daily room monitor folders yet.</div>';
    return;
  }
  roomMonitorDays.forEach((day) => {
    const item = document.createElement("button");
    item.type = "button";
    item.className = `gallery-day-item${day.dayKey === activeDayKey ? " active" : ""}`;
    item.addEventListener("click", () => {
      void loadDayPhotos(day.dayKey);
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

function updateSelectionButtons() {
  const selectedCount = selectedFileNames.size;
  if (deleteSelectedBtn) {
    deleteSelectedBtn.disabled = selectedCount === 0;
    deleteSelectedBtn.textContent =
      selectedCount === 0 ? "Delete Selected" : `Delete Selected (${selectedCount})`;
  }
  if (deleteDayBtn) {
    deleteDayBtn.disabled = mode !== "monitor" || activeDayPhotos.length === 0;
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
    selectAllBtn.disabled = mode !== "monitor" || activeDayPhotos.length === 0;
  }
}

function renderMonitorPhotos() {
  if (!photoGrid) return;
  photoGrid.innerHTML = "";
  if (dayTitle) {
    dayTitle.textContent = activeDayKey
      ? roomMonitorDays.find((day) => day.dayKey === activeDayKey)?.label || "Room Monitor Day"
      : "Room Monitor Day";
  }
  if (!activeDayPhotos.length) {
    photoGrid.innerHTML = '<div class="gallery-empty">Select a day folder to browse its captures.</div>';
    updateSelectionButtons();
    return;
  }
  activeDayPhotos.forEach((photo) => {
    const card = document.createElement("div");
    card.className = "gallery-photo-card";

    const img = document.createElement("img");
    img.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
    img.alt = photo.fileName;

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

function renderSavedPhotos() {
  if (!photoGrid) return;
  photoGrid.innerHTML = "";
  if (dayTitle) {
    dayTitle.textContent = "Saved Gallery";
  }
  if (!savedPhotos.length) {
    photoGrid.innerHTML = '<div class="gallery-empty">No saved gallery photos yet.</div>';
    return;
  }
  savedPhotos.forEach((photo, index) => {
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

    const actions = document.createElement("div");
    actions.className = "gallery-photo-actions";

    const openBtn = document.createElement("button");
    openBtn.type = "button";
    openBtn.className = "gallery-button";
    openBtn.textContent = "View Fullscreen";
    openBtn.addEventListener("click", () => {
      showViewerPhoto(index);
    });

    const downloadLink = document.createElement("a");
    downloadLink.className = "gallery-link secondary";
    downloadLink.href = photo.imageUrl;
    downloadLink.download = photo.fileName;
    downloadLink.textContent = "Download";

    actions.appendChild(openBtn);
    actions.appendChild(downloadLink);
    card.appendChild(img);
    card.appendChild(name);
    card.appendChild(meta);
    card.appendChild(actions);
    photoGrid.appendChild(card);
  });
}

async function loadDayPhotos(dayKey) {
  activeDayKey = dayKey;
  selectedFileNames.clear();
  renderDayList();
  updateSelectionButtons();
  setStatus("Loading day folder...");
  const payload = await fetchJson(`/api/room-monitor/gallery/day/${encodeURIComponent(dayKey)}?ts=${Date.now()}`, {
    cache: "no-store",
  });
  activeDayPhotos = Array.isArray(payload.photos) ? payload.photos : [];
  renderMonitorPhotos();
  setStatus(
    activeDayPhotos.length
      ? `Loaded ${activeDayPhotos.length} photo${activeDayPhotos.length === 1 ? "" : "s"} for ${dayKey}.`
      : `No photos found for ${dayKey}.`,
  );
}

async function loadMonitorGallery() {
  setStatus("Loading room monitor folders...");
  const payload = await fetchJson(`/api/room-monitor/gallery/days?ts=${Date.now()}`, {
    cache: "no-store",
  });
  roomMonitorDays = Array.isArray(payload.days) ? payload.days : [];
  renderDayList();
  if (pageMeta && payload.status) {
    pageMeta.textContent = `${payload.status.totalCount || 0} capture${payload.status.totalCount === 1 ? "" : "s"} across ${payload.status.dayCount || roomMonitorDays.length} day folder${(payload.status.dayCount || roomMonitorDays.length) === 1 ? "" : "s"} · ${payload.status.savedCount || 0} saved`;
  }
  if (savedCountText && payload.status) {
    savedCountText.textContent = `${payload.status.savedCount || 0} saved`;
  }
  if (!activeDayKey && roomMonitorDays.length) {
    await loadDayPhotos(roomMonitorDays[0].dayKey);
    return;
  }
  if (activeDayKey && roomMonitorDays.some((day) => day.dayKey === activeDayKey)) {
    await loadDayPhotos(activeDayKey);
    return;
  }
  activeDayPhotos = [];
  renderMonitorPhotos();
  setStatus(roomMonitorDays.length ? "Choose a day folder." : "No room monitor folders yet.");
}

async function moveSelectedPhotos(fileNames) {
  const uniqueNames = [...new Set(fileNames.filter(Boolean))];
  if (!uniqueNames.length) {
    setStatus("Select at least one photo first.", true);
    return;
  }
  setStatus(`Moving ${uniqueNames.length} photo${uniqueNames.length === 1 ? "" : "s"} to Saved Gallery...`);
  const payload = await fetchJson("/api/room-monitor/gallery/move", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ fileNames: uniqueNames }),
  });
  uniqueNames.forEach((fileName) => selectedFileNames.delete(fileName));
  if (savedCountText) {
    savedCountText.textContent = `${payload.savedCount || 0} saved`;
  }
  await loadMonitorGallery();
  setStatus(
    payload.moved?.length
      ? `Moved ${payload.moved.length} photo${payload.moved.length === 1 ? "" : "s"} to Saved Gallery.`
      : "No photos were moved.",
    false,
  );
}

async function deleteSelectedPhotos(fileNames) {
  const uniqueNames = [...new Set(fileNames.filter(Boolean))];
  if (!uniqueNames.length) {
    setStatus("Select at least one photo first.", true);
    return;
  }
  if (!window.confirm(`Delete ${uniqueNames.length} selected room monitor photo${uniqueNames.length === 1 ? "" : "s"}?`)) {
    return;
  }
  setStatus(`Deleting ${uniqueNames.length} selected photo${uniqueNames.length === 1 ? "" : "s"}...`);
  const payload = await fetchJson("/api/room-monitor/gallery", {
    method: "DELETE",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ fileNames: uniqueNames }),
  });
  uniqueNames.forEach((fileName) => selectedFileNames.delete(fileName));
  if (savedCountText) {
    savedCountText.textContent = `${payload.savedCount || 0} saved`;
  }
  await loadMonitorGallery();
  setStatus(
    payload.deleted
      ? `Deleted ${payload.deleted} room monitor photo${payload.deleted === 1 ? "" : "s"}.`
      : "No photos were deleted.",
  );
}

async function deleteCurrentDay() {
  if (!activeDayKey) {
    setStatus("Choose a day folder first.", true);
    return;
  }
  const activeDayLabel = roomMonitorDays.find((day) => day.dayKey === activeDayKey)?.label || activeDayKey;
  if (!window.confirm(`Delete all room monitor photos from ${activeDayLabel}?`)) {
    return;
  }
  setStatus(`Deleting all photos from ${activeDayLabel}...`);
  const payload = await fetchJson(`/api/room-monitor/gallery/day/${encodeURIComponent(activeDayKey)}`, {
    method: "DELETE",
  });
  selectedFileNames.clear();
  activeDayKey = "";
  if (savedCountText) {
    savedCountText.textContent = `${payload.savedCount || 0} saved`;
  }
  await loadMonitorGallery();
  setStatus(
    payload.deleted
      ? `Deleted ${payload.deleted} room monitor photo${payload.deleted === 1 ? "" : "s"} from ${activeDayLabel}.`
      : `No photos were deleted from ${activeDayLabel}.`,
  );
}

async function loadSavedGallery() {
  setStatus("Loading saved gallery...");
  const payload = await fetchJson(`/api/room-monitor/saved?ts=${Date.now()}`, {
    cache: "no-store",
  });
  savedPhotos = Array.isArray(payload.photos) ? payload.photos : [];
  renderSavedPhotos();
  if (pageMeta) {
    pageMeta.textContent = `${savedPhotos.length} saved photo${savedPhotos.length === 1 ? "" : "s"} ready for fullscreen viewing and slideshow.`;
  }
  setStatus(savedPhotos.length ? "Saved gallery loaded." : "No saved gallery photos yet.");
}

selectAllBtn?.addEventListener("click", () => {
  activeDayPhotos.forEach((photo) => selectedFileNames.add(photo.fileName));
  renderMonitorPhotos();
});

clearSelectionBtn?.addEventListener("click", () => {
  selectedFileNames.clear();
  renderMonitorPhotos();
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

if (mode === "saved") {
  if (selectAllBtn) selectAllBtn.style.display = "none";
  if (clearSelectionBtn) clearSelectionBtn.style.display = "none";
  if (deleteDayBtn) deleteDayBtn.style.display = "none";
  if (deleteSelectedBtn) deleteSelectedBtn.style.display = "none";
  if (moveSelectedBtn) moveSelectedBtn.style.display = "none";
  if (savedCountText) savedCountText.style.display = "none";
  void loadSavedGallery().catch((error) => {
    setStatus(error instanceof Error ? error.message : "Failed to load saved gallery.", true);
  });
} else {
  void loadMonitorGallery().catch((error) => {
    setStatus(error instanceof Error ? error.message : "Failed to load room monitor gallery.", true);
  });
}
