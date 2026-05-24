(function () {
  const root = document.body;
  const titleEl = document.getElementById("galleryPageTitle");
  const subtitleEl = document.getElementById("galleryPageSubtitle");
  const statusEl = document.getElementById("galleryStatus");
  const metaEl = document.getElementById("galleryMeta");
  const dayListEl = document.getElementById("galleryDayList");
  const dayTitleEl = document.getElementById("galleryDayTitle");
  const photoGridEl = document.getElementById("galleryPhotoGrid");
  const selectAllBtn = document.getElementById("gallerySelectAllBtn");
  const clearSelectionBtn = document.getElementById("galleryClearSelectionBtn");
  const deleteSelectedBtn = document.getElementById("galleryDeleteSelectedBtn");
  const deleteDayBtn = document.getElementById("galleryDeleteDayBtn");
  const viewerEl = document.getElementById("galleryViewer");
  const viewerImageEl = document.getElementById("galleryViewerImage");
  const viewerCaptionEl = document.getElementById("galleryViewerCaption");
  const viewerPrevBtn = document.getElementById("galleryViewerPrevBtn");
  const viewerNextBtn = document.getElementById("galleryViewerNextBtn");
  const viewerCloseBtn = document.getElementById("galleryViewerCloseBtn");
  const slideshowToggleBtn = document.getElementById("slideshowToggleBtn");
  const slideshowIntervalEl = document.getElementById("slideshowInterval");

  const daysApi = root.dataset.galleryDaysApi || "";
  const dayApi = root.dataset.galleryDayApi || "";
  const deleteApi = root.dataset.galleryDeleteApi || "";
  const deleteDayApi = root.dataset.galleryDeleteDayApi || "";
  const selectApi = root.dataset.gallerySelectApi || "";
  const galleryKind = root.dataset.galleryKind || "gallery";

  let dayGroups = [];
  let activeDayKey = "";
  let activePhotos = [];
  let selectedFileName = "";
  let selectedFiles = new Set();
  let viewerIndex = -1;
  let slideshowTimer = null;

  titleEl.textContent = root.dataset.galleryTitle || titleEl.textContent;
  subtitleEl.textContent = root.dataset.gallerySubtitle || subtitleEl.textContent;

  function setStatus(message, isError = false) {
    statusEl.textContent = message;
    statusEl.classList.toggle("error", Boolean(isError));
  }

  function formatBytes(bytes) {
    if (!Number.isFinite(bytes) || bytes <= 0) return "0 B";
    const units = ["B", "KB", "MB", "GB", "TB"];
    const index = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
    const value = bytes / 1024 ** index;
    return `${value.toFixed(value >= 10 || index === 0 ? 0 : 1)} ${units[index]}`;
  }

  function updateMeta(status) {
    if (!status) {
      metaEl.textContent = "";
      return;
    }
    const free = typeof status.freeBytes === "number" ? `Free ${formatBytes(status.freeBytes)}` : null;
    const reserved = typeof status.reserveBytes === "number" ? `Reserve ${formatBytes(status.reserveBytes)}` : null;
    const total = typeof status.totalSizeBytes === "number" ? `Gallery ${formatBytes(status.totalSizeBytes)}` : null;
    metaEl.textContent = [free, reserved, total].filter(Boolean).join(" • ");
  }

  function stopSlideshow() {
    if (slideshowTimer) {
      clearInterval(slideshowTimer);
      slideshowTimer = null;
    }
    if (slideshowToggleBtn) {
      slideshowToggleBtn.textContent = "Start Slideshow";
    }
  }

  function renderDays() {
    dayListEl.innerHTML = "";
    if (!dayGroups.length) {
      const empty = document.createElement("div");
      empty.className = "gallery-empty";
      empty.textContent = "No gallery days yet.";
      dayListEl.appendChild(empty);
      return;
    }
    dayGroups.forEach((day) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = `gallery-day-item${day.dayKey === activeDayKey ? " active" : ""}`;
      button.addEventListener("click", () => {
        loadDay(day.dayKey);
      });

      if (day.coverImageUrl) {
        const image = document.createElement("img");
        image.src = `${day.coverImageUrl}?ts=${day.updatedAt}`;
        image.alt = day.label;
        button.appendChild(image);
      }

      const name = document.createElement("div");
      name.className = "gallery-day-name";
      name.textContent = day.label;

      const meta = document.createElement("div");
      meta.className = "gallery-day-meta";
      meta.textContent = `${day.count} photo${day.count === 1 ? "" : "s"} • ${new Date(day.updatedAt).toLocaleString()}`;

      button.appendChild(name);
      button.appendChild(meta);
      dayListEl.appendChild(button);
    });
  }

  function openViewer(index) {
    if (!activePhotos.length) return;
    viewerIndex = index;
    const photo = activePhotos[viewerIndex];
    viewerImageEl.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
    viewerImageEl.alt = photo.fileName;
    viewerCaptionEl.textContent = `${photo.fileName} • ${new Date(photo.updatedAt).toLocaleString()}`;
    viewerEl.classList.add("open");
  }

  function showViewerStep(delta) {
    if (!activePhotos.length) return;
    const nextIndex = (viewerIndex + delta + activePhotos.length) % activePhotos.length;
    openViewer(nextIndex);
  }

  function renderPhotos() {
    photoGridEl.innerHTML = "";
    const activeDay = dayGroups.find((day) => day.dayKey === activeDayKey);
    dayTitleEl.textContent = activeDay ? activeDay.label : "Gallery Day";
    if (!activePhotos.length) {
      const empty = document.createElement("div");
      empty.className = "gallery-empty";
      empty.textContent = "No photos for this day.";
      photoGridEl.appendChild(empty);
      return;
    }
    activePhotos.forEach((photo, index) => {
      const card = document.createElement("div");
      card.className = "gallery-photo-card";

      const image = document.createElement("img");
      image.src = `${photo.imageUrl}?ts=${photo.updatedAt}`;
      image.alt = photo.fileName;
      image.loading = "lazy";
      image.addEventListener("click", () => openViewer(index));

      const name = document.createElement("div");
      name.className = "gallery-photo-name";
      name.textContent = photo.fileName;

      const actionRow = document.createElement("div");
      actionRow.className = "gallery-photo-actions";

      const openBtn = document.createElement("button");
      openBtn.type = "button";
      openBtn.className = "gallery-button secondary";
      openBtn.textContent = "Open";
      openBtn.addEventListener("click", () => openViewer(index));

      const selectBtn = document.createElement("button");
      selectBtn.type = "button";
      selectBtn.className = "gallery-button";
      selectBtn.textContent =
        photo.fileName === selectedFileName
          ? "Selected for Editing"
          : "Select for Editing";
      selectBtn.disabled = !selectApi || photo.fileName === selectedFileName;
      selectBtn.addEventListener("click", () => {
        selectPhoto(photo.fileName).catch((error) => {
          setStatus(error instanceof Error ? error.message : String(error), true);
        });
      });

      const downloadLink = document.createElement("a");
      downloadLink.className = "gallery-link";
      downloadLink.textContent = "Download";
      downloadLink.href = photo.imageUrl;
      downloadLink.download = photo.fileName;

      actionRow.appendChild(openBtn);
      actionRow.appendChild(selectBtn);
      actionRow.appendChild(downloadLink);

      const selectionRow = document.createElement("div");
      selectionRow.className = "gallery-selection-row";

      const checkLabel = document.createElement("label");
      checkLabel.className = "gallery-check";
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.checked = selectedFiles.has(photo.fileName);
      checkbox.addEventListener("change", () => {
        if (checkbox.checked) {
          selectedFiles.add(photo.fileName);
        } else {
          selectedFiles.delete(photo.fileName);
        }
      });
      checkLabel.appendChild(checkbox);
      checkLabel.append(` Select ${photo.fileName}`);

      selectionRow.appendChild(checkLabel);

      card.appendChild(image);
      card.appendChild(name);
      card.appendChild(actionRow);
      card.appendChild(selectionRow);
      photoGridEl.appendChild(card);
    });
  }

  async function loadDay(dayKey) {
    activeDayKey = dayKey;
    selectedFiles.clear();
    stopSlideshow();
    renderDays();
    setStatus("Loading day...");
    const response = await fetch(`${dayApi}${encodeURIComponent(dayKey)}?ts=${Date.now()}`, { cache: "no-store" });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    activePhotos = Array.isArray(payload.photos) ? payload.photos : [];
    selectedFileName = payload.selectedFileName || "";
    renderPhotos();
    setStatus(`${activePhotos.length} photo${activePhotos.length === 1 ? "" : "s"} on ${dayGroups.find((day) => day.dayKey === dayKey)?.label || dayKey}.`);
  }

  async function loadDays() {
    setStatus("Loading gallery...");
    const response = await fetch(`${daysApi}?ts=${Date.now()}`, { cache: "no-store" });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    dayGroups = Array.isArray(payload.days) ? payload.days : [];
    updateMeta(payload.status || null);
    if (!dayGroups.length) {
      activeDayKey = "";
      activePhotos = [];
      renderDays();
      renderPhotos();
      setStatus("No images in this gallery yet.");
      return;
    }
    if (!activeDayKey || !dayGroups.some((day) => day.dayKey === activeDayKey)) {
      activeDayKey = dayGroups[0].dayKey;
    }
    renderDays();
    await loadDay(activeDayKey);
  }

  async function deleteSelected() {
    if (!selectedFiles.size) {
      setStatus("Select at least one photo first.", true);
      return;
    }
    if (!window.confirm(`Delete ${selectedFiles.size} selected photo${selectedFiles.size === 1 ? "" : "s"}?`)) {
      return;
    }
    const response = await fetch(deleteApi, {
      method: "DELETE",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ fileNames: Array.from(selectedFiles) }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    await loadDays();
    setStatus(`Deleted ${payload.deleted || 0} selected photo${payload.deleted === 1 ? "" : "s"}.`);
  }

  async function deleteCurrentDay() {
    if (!activeDayKey) {
      setStatus("Choose a day first.", true);
      return;
    }
    const activeDay = dayGroups.find((day) => day.dayKey === activeDayKey);
    if (!window.confirm(`Delete every photo from ${activeDay?.label || activeDayKey}?`)) {
      return;
    }
    const response = await fetch(`${deleteDayApi}${encodeURIComponent(activeDayKey)}`, {
      method: "DELETE",
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    activeDayKey = "";
    await loadDays();
    setStatus(`Deleted ${payload.deleted || 0} photo${payload.deleted === 1 ? "" : "s"} from ${activeDay?.label || "the selected day"}.`);
  }

  async function selectPhoto(fileName) {
    if (!selectApi) {
      return;
    }
    const response = await fetch(selectApi, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ fileName }),
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    selectedFileName = payload.selectedFileName || fileName;
    renderPhotos();
    setStatus(`Selected ${selectedFileName} for ${galleryKind} editing.`);
  }

  selectAllBtn?.addEventListener("click", () => {
    activePhotos.forEach((photo) => selectedFiles.add(photo.fileName));
    renderPhotos();
    setStatus(`Selected ${selectedFiles.size} photo${selectedFiles.size === 1 ? "" : "s"}.`);
  });

  clearSelectionBtn?.addEventListener("click", () => {
    selectedFiles.clear();
    renderPhotos();
    setStatus("Selection cleared.");
  });

  deleteSelectedBtn?.addEventListener("click", () => {
    deleteSelected().catch((error) => {
      setStatus(error instanceof Error ? error.message : String(error), true);
    });
  });

  deleteDayBtn?.addEventListener("click", () => {
    deleteCurrentDay().catch((error) => {
      setStatus(error instanceof Error ? error.message : String(error), true);
    });
  });

  viewerPrevBtn?.addEventListener("click", () => showViewerStep(-1));
  viewerNextBtn?.addEventListener("click", () => showViewerStep(1));
  viewerCloseBtn?.addEventListener("click", () => {
    stopSlideshow();
    viewerEl.classList.remove("open");
  });
  viewerEl?.addEventListener("click", (event) => {
    if (event.target === viewerEl) {
      stopSlideshow();
      viewerEl.classList.remove("open");
    }
  });

  slideshowToggleBtn?.addEventListener("click", () => {
    if (!activePhotos.length) {
      return;
    }
    if (slideshowTimer) {
      stopSlideshow();
      return;
    }
    if (viewerIndex < 0) {
      openViewer(0);
    }
    const intervalSeconds = Math.max(0.5, Number(slideshowIntervalEl?.value || 2));
    slideshowTimer = window.setInterval(() => showViewerStep(1), intervalSeconds * 1000);
    slideshowToggleBtn.textContent = "Stop Slideshow";
  });

  loadDays().catch((error) => {
    setStatus(error instanceof Error ? error.message : String(error), true);
  });
})();
