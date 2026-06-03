#!/usr/bin/env python3
from __future__ import annotations

import asyncio
import codecs
import errno
import json
import mimetypes
import os
import pty
import posixpath
import select
import signal
import subprocess
import struct
import threading
import time
import uuid
from dataclasses import asdict, dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, quote, urlparse
from urllib.request import Request, urlopen

from PIL import Image, ImageOps

import fcntl
import termios

try:
    import websockets
    from websockets.exceptions import ConnectionClosed
except ImportError:  # pragma: no cover - optional runtime dependency
    websockets = None

    class ConnectionClosed(Exception):
        pass


PROJECT_DIR = Path(__file__).resolve().parent
WEB_DIR = PROJECT_DIR / "web"
DATA_DIR = PROJECT_DIR / "data"
SETTINGS_PATH = Path(
    os.getenv("PI3GROQ_SETTINGS_PATH", str(DATA_DIR / "settings.json"))
).expanduser()
TOUCH_FRAME_WIDTH = 480
TOUCH_FRAME_HEIGHT = 320
TOUCH_FRAME_MARGIN = 8
TOUCH_FRAME_QUALITY = 88
PI_AGENT_BINARY = Path(
    os.getenv("PI3GROQ_PI_AGENT_BIN", "~/.local/bin/pi-agent")
).expanduser()
PI_AGENT_WS_PORT = int(os.getenv("PI3GROQ_PI_AGENT_WS_PORT", "18601"))
PI_AGENT_HISTORY_LIMIT = 200_000
PI_AGENT_DEFAULT_COLS = 100
PI_AGENT_DEFAULT_ROWS = 30
PI_AGENT_PROJECTS_DIR = DATA_DIR / "pi-agent-projects"
PI_AGENT_PROJECT_METADATA_FILE = ".pi3groq-piagent-project.json"
PI_AGENT_TREE_MAX_DEPTH = 8
PI_AGENT_TREE_MAX_NODES = 500
PI_AGENT_MAX_FILE_BYTES = 512 * 1024


@dataclass
class AppSettings:
    mode: str = "companion"
    companionBaseUrl: str = ""
    pollIntervalMs: int = 2000
    touchDisplayMode: str = "slideshow-chat"
    touchDisplayRotationDeg: int = 270
    slideshowEnabled: bool = True
    slideshowIntervalSec: int = 8
    chatReturnTimeoutSec: int = 20


@dataclass
class PiAgentProjectManifest:
    id: str
    name: str
    slug: str
    starterPrompt: str
    projectRoot: str
    createdAt: str
    updatedAt: str


DEFAULT_SETTINGS = AppSettings()


def ensure_data_dir() -> None:
    SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)
    PI_AGENT_PROJECTS_DIR.mkdir(parents=True, exist_ok=True)


def slugify_project_name(value: str) -> str:
    lowered = "".join(
        character.lower() if character.isalnum() else "-"
        for character in str(value or "").strip()
    )
    collapsed = "-".join(part for part in lowered.split("-") if part)
    return collapsed[:40] or "project"


def get_pi_agent_project_metadata_path(project_root: Path) -> Path:
    return project_root / PI_AGENT_PROJECT_METADATA_FILE


def is_valid_pi_agent_project_id(value: str) -> bool:
    normalized = str(value or "").strip().lower()
    return len(normalized) == 32 and all(character in "0123456789abcdef" for character in normalized)


def serialize_pi_agent_project(project: PiAgentProjectManifest) -> dict[str, Any]:
    return {
        "id": project.id,
        "name": project.name,
        "slug": project.slug,
        "starterPrompt": project.starterPrompt,
        "projectRoot": project.projectRoot,
        "createdAt": project.createdAt,
        "updatedAt": project.updatedAt,
    }


def read_pi_agent_project_manifest(project_root: Path) -> PiAgentProjectManifest | None:
    metadata_path = get_pi_agent_project_metadata_path(project_root)
    if not metadata_path.is_file():
        return None
    try:
        raw = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError):
        return None
    if not isinstance(raw, dict):
        return None
    project_id = str(raw.get("id", "")).strip().lower()
    if not is_valid_pi_agent_project_id(project_id):
        return None
    return PiAgentProjectManifest(
        id=project_id,
        name=str(raw.get("name", "")).strip() or "PiAgent Project",
        slug=str(raw.get("slug", "")).strip() or slugify_project_name(raw.get("name", "")),
        starterPrompt=str(raw.get("starterPrompt", "") or ""),
        projectRoot=str(project_root.resolve()),
        createdAt=str(raw.get("createdAt", "")).strip() or time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        updatedAt=str(raw.get("updatedAt", "")).strip() or time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    )


def write_pi_agent_project_manifest(project: PiAgentProjectManifest) -> None:
    project_root = Path(project.projectRoot).resolve()
    project_root.mkdir(parents=True, exist_ok=True)
    metadata_path = get_pi_agent_project_metadata_path(project_root)
    metadata_path.write_text(
        json.dumps(serialize_pi_agent_project(project), indent=2) + "\n",
        encoding="utf-8",
    )


def list_pi_agent_projects() -> list[PiAgentProjectManifest]:
    ensure_data_dir()
    projects: list[PiAgentProjectManifest] = []
    for entry in PI_AGENT_PROJECTS_DIR.iterdir():
        if not entry.is_dir():
            continue
        manifest = read_pi_agent_project_manifest(entry)
        if manifest is not None:
            projects.append(manifest)
    return sorted(projects, key=lambda item: item.updatedAt, reverse=True)


def get_pi_agent_project_or_throw(project_id: str) -> PiAgentProjectManifest:
    normalized_id = str(project_id or "").strip().lower()
    if not is_valid_pi_agent_project_id(normalized_id):
        raise ValueError("Invalid PiAgent project id.")
    for project in list_pi_agent_projects():
        if project.id == normalized_id:
            return project
    raise FileNotFoundError(f"PiAgent project not found: {project_id}")


def update_pi_agent_project_manifest(
    project_id: str,
    updater: Any,
) -> PiAgentProjectManifest:
    existing = get_pi_agent_project_or_throw(project_id)
    next_project = updater(existing)
    if not isinstance(next_project, PiAgentProjectManifest):
        raise TypeError("PiAgent project updater must return a PiAgentProjectManifest.")
    updated = PiAgentProjectManifest(
        **{
            **serialize_pi_agent_project(next_project),
            "updatedAt": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
    )
    write_pi_agent_project_manifest(updated)
    return updated


def create_pi_agent_project(name: str, starter_prompt: str = "") -> PiAgentProjectManifest:
    normalized_name = str(name or "").strip()
    if not normalized_name:
        raise ValueError("Project name is required.")
    ensure_data_dir()
    project_id = uuid.uuid4().hex
    slug = slugify_project_name(normalized_name)
    project_root = PI_AGENT_PROJECTS_DIR / f"{slug}-{project_id[:8]}"
    created_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    manifest = PiAgentProjectManifest(
        id=project_id,
        name=normalized_name,
        slug=slug,
        starterPrompt=str(starter_prompt or "").strip(),
        projectRoot=str(project_root.resolve()),
        createdAt=created_at,
        updatedAt=created_at,
    )
    project_root.mkdir(parents=True, exist_ok=True)
    readme_lines = [f"# {normalized_name}", ""]
    if manifest.starterPrompt:
        readme_lines.extend(
            [
                "## Starter Brief",
                manifest.starterPrompt,
                "",
            ]
        )
    else:
        readme_lines.append("Created from the Pi3Groq PiAgent browser workspace.")
    (project_root / "README.md").write_text("\n".join(readme_lines).strip() + "\n", encoding="utf-8")
    write_pi_agent_project_manifest(manifest)
    return manifest


def resolve_pi_agent_project_file_path(
    project: PiAgentProjectManifest,
    relative_path: str,
) -> Path:
    trimmed_path = str(relative_path or "").strip().replace("\\", "/")
    if not trimmed_path:
        raise ValueError("File path is required.")
    normalized_relative = posixpath.normpath(trimmed_path)
    if (
        normalized_relative.startswith("../")
        or normalized_relative == ".."
        or normalized_relative.startswith("/")
    ):
        raise ValueError("File path must stay inside the project workspace.")
    if normalized_relative.startswith("."):
        raise ValueError("Dotfiles are reserved and cannot be opened here.")
    project_root = Path(project.projectRoot).resolve()
    resolved_path = (project_root / normalized_relative).resolve()
    if project_root not in resolved_path.parents and resolved_path != project_root:
        raise ValueError("Resolved path escapes the project workspace.")
    if resolved_path.name.startswith("."):
        raise ValueError("Dotfiles are reserved and cannot be opened here.")
    return resolved_path


def build_pi_agent_file_tree(
    dir_path: Path,
    *,
    base_path: str = "",
    depth: int = 0,
    counter: list[int] | None = None,
) -> tuple[list[dict[str, Any]], bool]:
    if counter is None:
        counter = [0]
    if depth >= PI_AGENT_TREE_MAX_DEPTH:
        return [], True
    entries = sorted(
        (
            entry
            for entry in dir_path.iterdir()
            if not entry.name.startswith(".")
        ),
        key=lambda entry: (0 if entry.is_dir() else 1, entry.name.lower()),
    )
    nodes: list[dict[str, Any]] = []
    truncated = False
    for entry in entries:
        if counter[0] >= PI_AGENT_TREE_MAX_NODES:
            truncated = True
            break
        counter[0] += 1
        relative_path = f"{base_path}/{entry.name}" if base_path else entry.name
        if entry.is_dir():
            children, child_truncated = build_pi_agent_file_tree(
                entry,
                base_path=relative_path,
                depth=depth + 1,
                counter=counter,
            )
            nodes.append(
                {
                    "name": entry.name,
                    "path": relative_path,
                    "type": "directory",
                    "children": children,
                }
            )
            truncated = truncated or child_truncated
            continue
        nodes.append(
            {
                "name": entry.name,
                "path": relative_path,
                "type": "file",
            }
        )
    return nodes, truncated


def normalize_base_url(value: str) -> str:
    normalized = (value or "").strip()
    if not normalized:
        return ""
    if not normalized.startswith(("http://", "https://")):
        normalized = f"http://{normalized}"
    return normalized.rstrip("/")


def normalize_mode(value: Any) -> str:
    return "standalone" if str(value).strip().lower() == "standalone" else "companion"


def normalize_poll_interval(value: Any) -> int:
    try:
        interval = int(value)
    except (TypeError, ValueError):
        interval = DEFAULT_SETTINGS.pollIntervalMs
    return min(10000, max(500, interval))


def normalize_touch_display_mode(value: Any) -> str:
    normalized = str(value or "").strip().lower()
    return "mirror" if normalized == "mirror" else "slideshow-chat"


def normalize_touch_display_rotation(value: Any) -> int:
    try:
        rotation = int(value)
    except (TypeError, ValueError):
        rotation = DEFAULT_SETTINGS.touchDisplayRotationDeg
    return rotation if rotation in {0, 90, 180, 270} else DEFAULT_SETTINGS.touchDisplayRotationDeg


def normalize_bool(value: Any, default: bool = False) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False
    return default


def normalize_slideshow_interval(value: Any) -> int:
    try:
        interval = int(value)
    except (TypeError, ValueError):
        interval = DEFAULT_SETTINGS.slideshowIntervalSec
    return min(30, max(3, interval))


def normalize_chat_return_timeout(value: Any) -> int:
    try:
        interval = int(value)
    except (TypeError, ValueError):
        interval = DEFAULT_SETTINGS.chatReturnTimeoutSec
    return min(300, max(5, interval))


def coerce_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def normalize_touch_frame_dimension(value: Any, default: int) -> int:
    try:
        dimension = int(value)
    except (TypeError, ValueError):
        return default
    return min(1024, max(120, dimension))


def render_touch_frame(
    image_bytes: bytes,
    *,
    width: int = TOUCH_FRAME_WIDTH,
    height: int = TOUCH_FRAME_HEIGHT,
) -> bytes:
    with Image.open(BytesIO(image_bytes)) as source:
        source = ImageOps.exif_transpose(source).convert("RGB")
        foreground = ImageOps.contain(
            source,
            (
                max(1, width - TOUCH_FRAME_MARGIN),
                max(1, height - TOUCH_FRAME_MARGIN),
            ),
            method=Image.LANCZOS,
        )

        canvas = Image.new("RGB", (width, height), (0, 0, 0))
        paste_x = max(0, (width - foreground.width) // 2)
        paste_y = max(0, (height - foreground.height) // 2)
        canvas.paste(foreground, (paste_x, paste_y))

        output = BytesIO()
        canvas.save(
            output,
            format="JPEG",
            quality=TOUCH_FRAME_QUALITY,
            optimize=False,
            progressive=False,
            subsampling=2,
        )
        return output.getvalue()


def load_settings() -> AppSettings:
    ensure_data_dir()
    if not SETTINGS_PATH.exists():
        return AppSettings()
    try:
        raw = json.loads(SETTINGS_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError):
        return AppSettings()
    if not isinstance(raw, dict):
        return AppSettings()
    return AppSettings(
        mode=normalize_mode(raw.get("mode")),
        companionBaseUrl=normalize_base_url(str(raw.get("companionBaseUrl", ""))),
        pollIntervalMs=normalize_poll_interval(raw.get("pollIntervalMs")),
        touchDisplayMode=normalize_touch_display_mode(raw.get("touchDisplayMode")),
        touchDisplayRotationDeg=normalize_touch_display_rotation(
            raw.get("touchDisplayRotationDeg"),
        ),
        slideshowEnabled=normalize_bool(
            raw.get("slideshowEnabled"),
            DEFAULT_SETTINGS.slideshowEnabled,
        ),
        slideshowIntervalSec=normalize_slideshow_interval(
            raw.get("slideshowIntervalSec"),
        ),
        chatReturnTimeoutSec=normalize_chat_return_timeout(
            raw.get("chatReturnTimeoutSec"),
        ),
    )


def save_settings(settings: AppSettings) -> AppSettings:
    ensure_data_dir()
    normalized = AppSettings(
        mode=normalize_mode(settings.mode),
        companionBaseUrl=normalize_base_url(settings.companionBaseUrl),
        pollIntervalMs=normalize_poll_interval(settings.pollIntervalMs),
        touchDisplayMode=normalize_touch_display_mode(settings.touchDisplayMode),
        touchDisplayRotationDeg=normalize_touch_display_rotation(
            settings.touchDisplayRotationDeg,
        ),
        slideshowEnabled=normalize_bool(
            settings.slideshowEnabled,
            DEFAULT_SETTINGS.slideshowEnabled,
        ),
        slideshowIntervalSec=normalize_slideshow_interval(
            settings.slideshowIntervalSec,
        ),
        chatReturnTimeoutSec=normalize_chat_return_timeout(
            settings.chatReturnTimeoutSec,
        ),
    )
    SETTINGS_PATH.write_text(
        json.dumps(asdict(normalized), indent=2) + "\n",
        encoding="utf-8",
    )
    return normalized


def apply_touch_rotation(rotation_deg: int) -> None:
    env = os.environ.copy()
    env["PI3GROQ_TFT_ROTATE"] = str(rotation_deg)
    try:
        subprocess.run(
            ["bash", str(PROJECT_DIR / "scripts" / "install-touch-kiosk.sh")],
            cwd=str(PROJECT_DIR),
            env=env,
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(1)
        subprocess.Popen(
            ["sudo", "reboot"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except OSError:
        return


def read_json_request(
    handler: BaseHTTPRequestHandler,
    *,
    max_length: int = 1_000_000,
) -> dict[str, Any]:
    length = int(handler.headers.get("Content-Length", "0") or "0")
    if length <= 0:
        return {}
    if length > max_length:
        raise ValueError("Request body is too large.")
    raw = handler.rfile.read(length)
    if not raw:
        return {}
    parsed = json.loads(raw.decode("utf-8"))
    return parsed if isinstance(parsed, dict) else {}


def json_response(
    handler: BaseHTTPRequestHandler,
    payload: dict[str, Any],
    status: int = HTTPStatus.OK,
) -> None:
    data = json.dumps(payload).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Cache-Control", "no-store")
    handler.send_header("Content-Length", str(len(data)))
    handler.end_headers()
    handler.wfile.write(data)


def fetch_remote_json(
    url: str,
    *,
    method: str = "GET",
    body: dict[str, Any] | None = None,
    timeout: float = 10.0,
) -> dict[str, Any]:
    encoded_body = None
    headers = {"Accept": "application/json"}
    if body is not None:
        encoded_body = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    request = Request(url, data=encoded_body, headers=headers, method=method)
    with urlopen(request, timeout=timeout) as response:
        response_body = response.read().decode("utf-8")
    parsed = json.loads(response_body)
    if not isinstance(parsed, dict):
        raise RuntimeError("Remote response was not a JSON object.")
    return parsed


def fetch_remote_binary(url: str, timeout: float = 10.0) -> tuple[bytes, str]:
    with urlopen(url, timeout=timeout) as response:
        data = response.read()
        content_type = response.headers.get_content_type() or "application/octet-stream"
    return data, content_type


def build_companion_state(settings: AppSettings) -> dict[str, Any]:
    if not settings.companionBaseUrl:
        return {
            "ok": False,
            "connected": False,
            "error": "Save a Whisplay companion URL first.",
            "settings": asdict(settings),
            "companion": None,
        }

    state = fetch_remote_json(f"{settings.companionBaseUrl}/api/state", timeout=8.0)
    image_path = state.get("image")
    if isinstance(image_path, str) and image_path.startswith("/"):
        state["remote_image_proxy_url"] = (
            f"/api/companion/image?path={quote(image_path, safe='/')}"
            f"&rev={state.get('image_revision', 0)}"
        )
        state["touch_image_proxy_url"] = (
            f"/api/companion/image?path={quote(image_path, safe='/')}"
            f"&rev={state.get('image_revision', 0)}&frame=touch"
        )
    state["remoteBaseUrl"] = settings.companionBaseUrl
    return {
        "ok": True,
        "connected": bool(state.get("ready")),
        "settings": asdict(settings),
        "companion": state,
    }


def build_generated_images_payload(
    settings: AppSettings,
    *,
    limit: int = 200,
) -> dict[str, Any]:
    if not settings.companionBaseUrl:
        return {
            "ok": False,
            "connected": False,
            "error": "Save a Whisplay companion URL first.",
            "settings": asdict(settings),
            "photos": [],
            "totalCount": 0,
        }

    normalized_limit = min(400, max(1, int(limit)))
    payload = fetch_remote_json(
        f"{settings.companionBaseUrl}/api/generated-images?limit={normalized_limit}",
        timeout=12.0,
    )
    remote_photos = payload.get("photos")
    photos: list[dict[str, Any]] = []
    if isinstance(remote_photos, list):
        for raw_photo in remote_photos:
            if not isinstance(raw_photo, dict):
                continue
            file_name = str(raw_photo.get("fileName", "")).strip()
            if not file_name:
                continue
            updated_at = coerce_int(raw_photo.get("updatedAt", 0) or 0)
            size_bytes = coerce_int(raw_photo.get("sizeBytes", 0) or 0)
            photos.append(
                {
                    "fileName": file_name,
                    "updatedAt": updated_at,
                    "sizeBytes": size_bytes,
                    "fullscreenImageUrl": (
                        f"/api/companion/generated-image?fileName={quote(file_name)}"
                        f"&updatedAt={updated_at}&variant=full"
                    ),
                    "companionImageUrl": (
                        f"/api/companion/generated-image?fileName={quote(file_name)}"
                        f"&updatedAt={updated_at}"
                    ),
                    "touchImageUrl": (
                        f"/api/companion/generated-image?fileName={quote(file_name)}"
                        f"&updatedAt={updated_at}&variant=touch"
                    ),
                }
            )

    return {
        "ok": True,
        "connected": True,
        "settings": asdict(settings),
        "photos": photos,
        "totalCount": coerce_int(payload.get("totalCount", len(photos)) or len(photos), len(photos)),
        "selectedFileName": str(payload.get("selectedFileName", "") or ""),
        "status": payload.get("status"),
    }


class PiAgentSessionManager:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._process: subprocess.Popen[bytes] | None = None
        self._master_fd: int | None = None
        self._session_id: str | None = None
        self._project_id: str | None = None
        self._project_name: str | None = None
        self._history = ""
        self._last_error = ""
        self._reader_thread: threading.Thread | None = None
        self._bridge: PiAgentWebSocketBridge | None = None
        self._exit_code: int | None = None

    def attach_bridge(self, bridge: PiAgentWebSocketBridge) -> None:
        self._bridge = bridge

    def binary_path(self) -> str:
        return str(PI_AGENT_BINARY)

    def is_available(self) -> bool:
        return PI_AGENT_BINARY.is_file() and os.access(PI_AGENT_BINARY, os.X_OK)

    def status(self) -> dict[str, Any]:
        with self._lock:
            return self._status_unlocked()

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            status = self._status_unlocked()
            status["history"] = self._history
            return status

    def start(
        self,
        cols: int,
        rows: int,
        project: PiAgentProjectManifest,
    ) -> dict[str, Any]:
        with self._lock:
            if not self.is_available():
                raise RuntimeError(
                    f"PiAgent binary is missing or not executable: {self.binary_path()}"
                )
            if self._bridge is None or not self._bridge.is_running():
                raise RuntimeError("PiAgent websocket bridge is not available.")
            if self._process is not None and self._process.poll() is None:
                if self._project_id == project.id:
                    return self._status_unlocked()
                raise RuntimeError(
                    f"PiAgent is already running in {self._project_name or 'another project'}. Stop it first."
                )

            master_fd, slave_fd = pty.openpty()
            self._set_winsize(master_fd, cols, rows)
            env = os.environ.copy()
            env["HOME"] = str(Path.home())
            env["PATH"] = f"{Path.home() / '.local' / 'bin'}:{env.get('PATH', '')}"
            env.setdefault("TERM", "xterm-256color")
            env.setdefault("COLORTERM", "truecolor")
            process = subprocess.Popen(
                [self.binary_path()],
                stdin=slave_fd,
                stdout=slave_fd,
                stderr=slave_fd,
                cwd=project.projectRoot,
                env=env,
                start_new_session=True,
                close_fds=True,
            )
            os.close(slave_fd)
            self._process = process
            self._master_fd = master_fd
            self._session_id = uuid.uuid4().hex
            self._project_id = project.id
            self._project_name = project.name
            self._history = ""
            self._last_error = ""
            self._exit_code = None
            self._reader_thread = threading.Thread(
                target=self._reader_loop,
                args=(self._session_id,),
                daemon=True,
            )
            self._reader_thread.start()
            status = self._status_unlocked()

        self._broadcast(
            {
                "type": "snapshot",
                "sessionId": status.get("sessionId"),
                "currentProjectId": status.get("currentProjectId"),
                "currentProjectName": status.get("currentProjectName"),
                "history": "",
                "running": status.get("running"),
                "pid": status.get("pid"),
                "exitCode": status.get("exitCode"),
            }
        )
        return status

    def stop(self) -> dict[str, Any]:
        with self._lock:
            process = self._process
            session_id = self._session_id
            master_fd = self._master_fd
            self._process = None
            self._master_fd = None
            self._session_id = None
            self._project_id = None
            self._project_name = None
            self._exit_code = None
        if process is not None and process.poll() is None:
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        if master_fd is not None:
            try:
                os.close(master_fd)
            except OSError:
                pass
        self._broadcast(
            {
                "type": "status",
                "sessionId": session_id,
                "running": False,
                "exitCode": process.returncode if process is not None else None,
                "stopped": True,
            }
        )
        return self.status()

    def _status_unlocked(self) -> dict[str, Any]:
        process = self._process
        running = process is not None and process.poll() is None
        return {
            "available": self.is_available(),
            "running": running,
            "sessionId": self._session_id,
            "currentProjectId": self._project_id,
            "currentProjectName": self._project_name,
            "pid": process.pid if running and process is not None else None,
            "binaryPath": self.binary_path(),
            "lastError": self._last_error,
            "exitCode": self._exit_code,
            "historySize": len(self._history),
            "websocketEnabled": self._bridge is not None and self._bridge.is_running(),
            "websocketPort": self._bridge.port if self._bridge is not None else None,
            "websocketError": self._bridge.error if self._bridge is not None else "",
        }

    def write_input(self, data: str) -> None:
        if not data:
            return
        encoded = data.encode("utf-8", errors="replace")
        with self._lock:
            master_fd = self._master_fd
            process = self._process
        if master_fd is None or process is None or process.poll() is not None:
            raise RuntimeError("PiAgent is not running.")
        os.write(master_fd, encoded)

    def resize(self, cols: int, rows: int) -> None:
        with self._lock:
            master_fd = self._master_fd
            process = self._process
        if master_fd is None or process is None or process.poll() is not None:
            return
        self._set_winsize(master_fd, cols, rows)
        try:
            os.killpg(process.pid, signal.SIGWINCH)
        except OSError:
            return

    def _reader_loop(self, session_id: str) -> None:
        decoder = codecs.getincrementaldecoder("utf-8")("replace")
        process: subprocess.Popen[bytes] | None = None
        while True:
            with self._lock:
                if self._session_id != session_id:
                    return
                process = self._process
                master_fd = self._master_fd
            if process is None or master_fd is None:
                break
            try:
                ready, _, _ = select.select([master_fd], [], [], 0.25)
                if ready:
                    chunk = os.read(master_fd, 65536)
                    if not chunk:
                        break
                    text = decoder.decode(chunk)
                    if text:
                        self._append_history(text)
                        self._broadcast(
                            {
                                "type": "output",
                                "sessionId": session_id,
                                "data": text,
                            }
                        )
                elif process.poll() is not None:
                    break
            except OSError as error:
                if error.errno == errno.EIO:
                    break
                self._set_error(str(error))
                break

        trailing = decoder.decode(b"", final=True)
        if trailing:
            self._append_history(trailing)
            self._broadcast(
                {"type": "output", "sessionId": session_id, "data": trailing}
            )

        exit_code = None
        if process is not None:
            try:
                exit_code = process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                exit_code = process.poll()

        with self._lock:
            if self._session_id == session_id:
                self._process = None
                self._session_id = None
                self._project_id = None
                self._project_name = None
                self._exit_code = exit_code
                master_fd = self._master_fd
                self._master_fd = None
            else:
                master_fd = None

        if master_fd is not None:
            try:
                os.close(master_fd)
            except OSError:
                pass

        self._broadcast(
            {
                "type": "status",
                "sessionId": session_id,
                "running": False,
                "exitCode": exit_code,
            }
        )

    def _append_history(self, text: str) -> None:
        with self._lock:
            self._history = f"{self._history}{text}"[-PI_AGENT_HISTORY_LIMIT:]

    def _set_error(self, message: str) -> None:
        with self._lock:
            self._last_error = message

    def _broadcast(self, payload: dict[str, Any]) -> None:
        if self._bridge is not None:
            self._bridge.broadcast(payload)

    @staticmethod
    def _set_winsize(fd: int, cols: int, rows: int) -> None:
        safe_cols = max(40, min(240, int(cols or PI_AGENT_DEFAULT_COLS)))
        safe_rows = max(12, min(80, int(rows or PI_AGENT_DEFAULT_ROWS)))
        size = struct.pack("HHHH", safe_rows, safe_cols, 0, 0)
        fcntl.ioctl(fd, termios.TIOCSWINSZ, size)


class PiAgentWebSocketBridge:
    def __init__(
        self,
        host: str,
        port: int,
        session_manager: PiAgentSessionManager,
    ) -> None:
        self.host = host
        self.port = port
        self.session_manager = session_manager
        self.error = ""
        self._clients: set[Any] = set()
        self._thread: threading.Thread | None = None
        self._loop: asyncio.AbstractEventLoop | None = None
        self._started = threading.Event()

    def start(self) -> None:
        if websockets is None:
            self.error = "Python websockets module is not installed."
            self._started.set()
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        self._started.wait(timeout=5)

    def is_running(self) -> bool:
        return self._loop is not None and not self._loop.is_closed() and not self.error

    def broadcast(self, payload: dict[str, Any]) -> None:
        loop = self._loop
        if loop is None or loop.is_closed():
            return
        asyncio.run_coroutine_threadsafe(self._broadcast(payload), loop)

    def _run(self) -> None:
        try:
            asyncio.run(self._serve())
        except OSError as error:
            self.error = str(error)
            self._started.set()

    async def _serve(self) -> None:
        self._loop = asyncio.get_running_loop()
        async with websockets.serve(self._handle_client, self.host, self.port):
            self._started.set()
            await asyncio.Future()

    async def _broadcast(self, payload: dict[str, Any]) -> None:
        if not self._clients:
            return
        data = json.dumps(payload)
        stale_clients: list[Any] = []
        for client in tuple(self._clients):
            try:
                await client.send(data)
            except ConnectionClosed:
                stale_clients.append(client)
        for client in stale_clients:
            self._clients.discard(client)

    async def _handle_client(self, websocket: Any) -> None:
        self._clients.add(websocket)
        try:
            await websocket.send(
                json.dumps(
                    {
                        "type": "snapshot",
                        **self.session_manager.snapshot(),
                    }
                )
            )
            async for message in websocket:
                try:
                    payload = json.loads(message)
                except json.JSONDecodeError:
                    continue
                if not isinstance(payload, dict):
                    continue
                event_type = str(payload.get("type", "")).strip().lower()
                if event_type == "input":
                    self.session_manager.write_input(str(payload.get("data", "")))
                elif event_type == "resize":
                    self.session_manager.resize(
                        int(payload.get("cols", PI_AGENT_DEFAULT_COLS)),
                        int(payload.get("rows", PI_AGENT_DEFAULT_ROWS)),
                    )
        finally:
            self._clients.discard(websocket)


class Pi3GroqHTTPServer(ThreadingHTTPServer):
    def __init__(
        self,
        server_address: tuple[str, int],
        handler_class: type[BaseHTTPRequestHandler],
        pi_agent_session: PiAgentSessionManager,
    ) -> None:
        super().__init__(server_address, handler_class)
        self.pi_agent_session = pi_agent_session


class Pi3GroqHandler(BaseHTTPRequestHandler):
    server_version = "Pi3Groq/0.1"

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        route = parsed.path

        if route in {"/", "/index.html"}:
            self.serve_file(WEB_DIR / "index.html", "text/html; charset=utf-8")
            return
        if route == "/hdmi":
            self.serve_file(WEB_DIR / "hdmi.html", "text/html; charset=utf-8")
            return
        if route.startswith("/static/"):
            relative = route.removeprefix("/static/")
            self.serve_static_file(relative)
            return
        if route == "/api/settings":
            json_response(self, {"ok": True, "settings": asdict(load_settings())})
            return
        if route == "/api/pi-agent/status":
            json_response(
                self,
                {"ok": True, "piAgent": self.server.pi_agent_session.status()},
            )
            return
        if route == "/api/pi-agent/projects":
            json_response(
                self,
                {
                    "ok": True,
                    "projects": [
                        serialize_pi_agent_project(project)
                        for project in list_pi_agent_projects()
                    ],
                },
            )
            return
        if route.startswith("/api/pi-agent/projects/"):
            self.handle_pi_agent_project_get(route, parsed)
            return
        if route == "/api/companion/state":
            try:
                json_response(self, build_companion_state(load_settings()))
            except (HTTPError, URLError, TimeoutError, OSError, RuntimeError, ValueError) as error:
                json_response(
                    self,
                    {
                        "ok": False,
                        "connected": False,
                        "error": str(error),
                        "settings": asdict(load_settings()),
                        "companion": None,
                    },
                    status=HTTPStatus.BAD_GATEWAY,
                )
            return
        if route == "/api/companion/generated-images":
            try:
                query = parse_qs(parsed.query)
                requested_limit = (query.get("limit") or ["200"])[0]
                json_response(
                    self,
                    build_generated_images_payload(
                        load_settings(),
                        limit=int(requested_limit or "200"),
                    ),
                )
            except (HTTPError, URLError, TimeoutError, OSError, RuntimeError, ValueError) as error:
                json_response(
                    self,
                    {
                        "ok": False,
                        "connected": False,
                        "error": str(error),
                        "settings": asdict(load_settings()),
                        "photos": [],
                        "totalCount": 0,
                    },
                    status=HTTPStatus.BAD_GATEWAY,
                )
            return
        if route == "/api/companion/image":
            self.proxy_remote_image(parsed)
            return
        if route == "/api/companion/generated-image":
            self.proxy_generated_image(parsed)
            return

        json_response(
            self,
            {"ok": False, "error": f"Route not found: {route}"},
            status=HTTPStatus.NOT_FOUND,
        )

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        route = parsed.path

        if route == "/api/settings":
            try:
                body = read_json_request(self)
                previous_settings = load_settings()
                settings = save_settings(
                    AppSettings(
                        mode=normalize_mode(body.get("mode", "companion")),
                        companionBaseUrl=str(body.get("companionBaseUrl", "")),
                        pollIntervalMs=normalize_poll_interval(body.get("pollIntervalMs")),
                        touchDisplayMode=normalize_touch_display_mode(
                            body.get("touchDisplayMode"),
                        ),
                        touchDisplayRotationDeg=normalize_touch_display_rotation(
                            body.get("touchDisplayRotationDeg"),
                        ),
                        slideshowEnabled=normalize_bool(
                            body.get("slideshowEnabled"),
                            DEFAULT_SETTINGS.slideshowEnabled,
                        ),
                        slideshowIntervalSec=normalize_slideshow_interval(
                            body.get("slideshowIntervalSec"),
                        ),
                        chatReturnTimeoutSec=normalize_chat_return_timeout(
                            body.get("chatReturnTimeoutSec"),
                        ),
                    )
                )
                rotation_changed = (
                    previous_settings.touchDisplayRotationDeg
                    != settings.touchDisplayRotationDeg
                )
                json_response(
                    self,
                    {
                        "ok": True,
                        "settings": asdict(settings),
                        "touchDisplayRotationApplyPending": rotation_changed,
                    },
                )
                if rotation_changed:
                    threading.Thread(
                        target=apply_touch_rotation,
                        args=(settings.touchDisplayRotationDeg,),
                        daemon=True,
                    ).start()
            except (ValueError, TypeError, OSError) as error:
                json_response(
                    self,
                    {"ok": False, "error": str(error)},
                    status=HTTPStatus.BAD_REQUEST,
                )
            return

        if route == "/api/companion/input":
            settings = load_settings()
            if not settings.companionBaseUrl:
                json_response(
                    self,
                    {"ok": False, "error": "Save a Whisplay companion URL first."},
                    status=HTTPStatus.BAD_REQUEST,
                )
                return
            try:
                body = read_json_request(self)
                text = str(body.get("text", "")).strip()
                if not text:
                    raise ValueError("Missing input text.")
                result = fetch_remote_json(
                    f"{settings.companionBaseUrl}/api/input/text",
                    method="POST",
                    body={"text": text},
                    timeout=12.0,
                )
                json_response(self, {"ok": True, "result": result})
            except (HTTPError, URLError, TimeoutError, OSError, RuntimeError, ValueError) as error:
                json_response(
                    self,
                    {"ok": False, "error": str(error)},
                    status=HTTPStatus.BAD_GATEWAY,
                )
            return
        if route == "/api/pi-agent/start":
            try:
                body = read_json_request(self)
                project = get_pi_agent_project_or_throw(str(body.get("projectId", "")))
                status = self.server.pi_agent_session.start(
                    int(body.get("cols", PI_AGENT_DEFAULT_COLS)),
                    int(body.get("rows", PI_AGENT_DEFAULT_ROWS)),
                    project,
                )
                json_response(self, {"ok": True, "piAgent": status})
            except (RuntimeError, ValueError, TypeError) as error:
                json_response(
                    self,
                    {"ok": False, "error": str(error)},
                    status=HTTPStatus.BAD_REQUEST,
                )
            return
        if route == "/api/pi-agent/stop":
            json_response(
                self,
                {"ok": True, "piAgent": self.server.pi_agent_session.stop()},
            )
            return
        if route == "/api/pi-agent/projects":
            try:
                body = read_json_request(self, max_length=64_000)
                project = create_pi_agent_project(
                    str(body.get("name", "")),
                    str(body.get("starterPrompt", "") or ""),
                )
                json_response(
                    self,
                    {
                        "ok": True,
                        "project": serialize_pi_agent_project(project),
                    },
                )
            except (ValueError, TypeError) as error:
                json_response(
                    self,
                    {"ok": False, "error": str(error)},
                    status=HTTPStatus.BAD_REQUEST,
                )
            return
        if route.startswith("/api/pi-agent/projects/"):
            self.handle_pi_agent_project_post(route)
            return

        json_response(
            self,
            {"ok": False, "error": f"Route not found: {route}"},
            status=HTTPStatus.NOT_FOUND,
        )

    def log_message(self, format: str, *args: Any) -> None:
        print(f"[Pi3Groq] {self.address_string()} - {format % args}")

    def handle_pi_agent_project_get(self, route: str, parsed: Any) -> None:
        parts = [part for part in route.split("/") if part]
        if len(parts) < 4:
            json_response(
                self,
                {"ok": False, "error": "Invalid PiAgent project route."},
                status=HTTPStatus.NOT_FOUND,
            )
            return
        project_id = parts[3]
        try:
            project = get_pi_agent_project_or_throw(project_id)
        except (ValueError, FileNotFoundError) as error:
            json_response(
                self,
                {"ok": False, "error": str(error)},
                status=HTTPStatus.NOT_FOUND,
            )
            return

        if len(parts) == 4:
            json_response(self, {"ok": True, "project": serialize_pi_agent_project(project)})
            return
        if len(parts) == 5 and parts[4] == "tree":
            files, truncated = build_pi_agent_file_tree(Path(project.projectRoot))
            json_response(
                self,
                {
                    "ok": True,
                    "project": serialize_pi_agent_project(project),
                    "files": files,
                    "truncated": truncated,
                },
            )
            return
        if len(parts) == 5 and parts[4] == "file":
            requested_path = (parse_qs(parsed.query).get("path") or [""])[0]
            try:
                file_path = resolve_pi_agent_project_file_path(project, requested_path)
                if not file_path.is_file():
                    raise FileNotFoundError(f"Project file not found: {requested_path}")
                if file_path.stat().st_size > PI_AGENT_MAX_FILE_BYTES:
                    raise ValueError("File is too large for the browser editor.")
                json_response(
                    self,
                    {
                        "ok": True,
                        "file": {
                            "path": requested_path,
                            "content": file_path.read_text(encoding="utf-8"),
                            "mtimeMs": int(file_path.stat().st_mtime * 1000),
                        },
                    },
                )
            except (ValueError, FileNotFoundError, OSError) as error:
                json_response(
                    self,
                    {"ok": False, "error": str(error)},
                    status=HTTPStatus.BAD_REQUEST,
                )
            return

        json_response(
            self,
            {"ok": False, "error": f"Route not found: {route}"},
            status=HTTPStatus.NOT_FOUND,
        )

    def handle_pi_agent_project_post(self, route: str) -> None:
        parts = [part for part in route.split("/") if part]
        if len(parts) != 5 or parts[4] != "file":
            json_response(
                self,
                {"ok": False, "error": f"Route not found: {route}"},
                status=HTTPStatus.NOT_FOUND,
            )
            return
        project_id = parts[3]
        try:
            project = get_pi_agent_project_or_throw(project_id)
            body = read_json_request(self, max_length=PI_AGENT_MAX_FILE_BYTES + 32_000)
            requested_path = str(body.get("path", ""))
            expected_mtime_ms = body.get("expectedMtimeMs")
            file_path = resolve_pi_agent_project_file_path(project, requested_path)
            if (
                expected_mtime_ms is not None
                and file_path.exists()
                and int(file_path.stat().st_mtime * 1000) != int(expected_mtime_ms)
            ):
                json_response(
                    self,
                    {
                        "ok": False,
                        "error": "File changed on disk since it was opened. Reload it before saving.",
                    },
                    status=HTTPStatus.CONFLICT,
                )
                return
            content = str(body.get("content", ""))
            if len(content.encode("utf-8")) > PI_AGENT_MAX_FILE_BYTES:
                raise ValueError("File content is too large for the browser editor.")
            file_path.parent.mkdir(parents=True, exist_ok=True)
            file_path.write_text(content, encoding="utf-8")
            updated_project = update_pi_agent_project_manifest(project.id, lambda current: current)
            json_response(
                self,
                {
                    "ok": True,
                    "project": serialize_pi_agent_project(updated_project),
                    "file": {
                        "path": requested_path,
                        "mtimeMs": int(file_path.stat().st_mtime * 1000),
                    },
                },
            )
        except (ValueError, TypeError, FileNotFoundError, OSError) as error:
            json_response(
                self,
                {"ok": False, "error": str(error)},
                status=HTTPStatus.BAD_REQUEST,
            )

    def serve_static_file(self, relative_path: str) -> None:
        safe_relative = posixpath.normpath(relative_path).lstrip("/")
        target = (WEB_DIR / safe_relative).resolve()
        if WEB_DIR.resolve() not in target.parents and target != WEB_DIR.resolve():
            json_response(
                self,
                {"ok": False, "error": "Invalid static path."},
                status=HTTPStatus.BAD_REQUEST,
            )
            return
        if not target.is_file():
            json_response(
                self,
                {"ok": False, "error": "Static file not found."},
                status=HTTPStatus.NOT_FOUND,
            )
            return
        mime_type, _ = mimetypes.guess_type(str(target))
        self.serve_file(target, mime_type or "application/octet-stream")

    def serve_file(self, file_path: Path, content_type: str) -> None:
        if not file_path.is_file():
            json_response(
                self,
                {"ok": False, "error": "File not found."},
                status=HTTPStatus.NOT_FOUND,
            )
            return
        data = file_path.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def proxy_remote_image(self, parsed: Any) -> None:
        settings = load_settings()
        if not settings.companionBaseUrl:
            json_response(
                self,
                {"ok": False, "error": "Save a Whisplay companion URL first."},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        query = parse_qs(parsed.query)
        requested_path = (query.get("path") or [""])[0].strip()
        frame = (query.get("frame") or ["raw"])[0].strip().lower()
        frame_width = normalize_touch_frame_dimension(
            (query.get("frameWidth") or [TOUCH_FRAME_WIDTH])[0],
            TOUCH_FRAME_WIDTH,
        )
        frame_height = normalize_touch_frame_dimension(
            (query.get("frameHeight") or [TOUCH_FRAME_HEIGHT])[0],
            TOUCH_FRAME_HEIGHT,
        )
        if not requested_path.startswith("/"):
            json_response(
                self,
                {"ok": False, "error": "Invalid image path."},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        remote_url = f"{settings.companionBaseUrl}{requested_path}"
        try:
            data, content_type = fetch_remote_binary(remote_url, timeout=12.0)
        except (HTTPError, URLError, TimeoutError, OSError) as error:
            json_response(
                self,
                {"ok": False, "error": str(error)},
                status=HTTPStatus.BAD_GATEWAY,
            )
            return

        if frame == "touch":
            data = render_touch_frame(data, width=frame_width, height=frame_height)
            content_type = "image/jpeg"

        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def proxy_generated_image(self, parsed: Any) -> None:
        settings = load_settings()
        if not settings.companionBaseUrl:
            json_response(
                self,
                {"ok": False, "error": "Save a Whisplay companion URL first."},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        query = parse_qs(parsed.query)
        file_name = (query.get("fileName") or [""])[0].strip()
        variant = (query.get("variant") or ["companion"])[0].strip().lower()
        frame_width = normalize_touch_frame_dimension(
            (query.get("frameWidth") or [TOUCH_FRAME_WIDTH])[0],
            TOUCH_FRAME_WIDTH,
        )
        frame_height = normalize_touch_frame_dimension(
            (query.get("frameHeight") or [TOUCH_FRAME_HEIGHT])[0],
            TOUCH_FRAME_HEIGHT,
        )
        if not file_name or "/" in file_name or "\\" in file_name:
            json_response(
                self,
                {"ok": False, "error": "Invalid generated image file name."},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        remote_variant = "image" if variant == "full" else "companion"
        remote_url = f"{settings.companionBaseUrl}/api/generated-images/{remote_variant}/{quote(file_name)}"
        try:
            data, content_type = fetch_remote_binary(remote_url, timeout=15.0)
        except (HTTPError, URLError, TimeoutError, OSError) as error:
            json_response(
                self,
                {"ok": False, "error": str(error)},
                status=HTTPStatus.BAD_GATEWAY,
            )
            return

        if variant == "touch":
            full_remote_url = (
                f"{settings.companionBaseUrl}/api/generated-images/image/{quote(file_name)}"
            )
            try:
                data, _ = fetch_remote_binary(full_remote_url, timeout=15.0)
            except (HTTPError, URLError, TimeoutError, OSError) as error:
                json_response(
                    self,
                    {"ok": False, "error": str(error)},
                    status=HTTPStatus.BAD_GATEWAY,
                )
                return
            data = render_touch_frame(data, width=frame_width, height=frame_height)
            content_type = "image/jpeg"

        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main() -> None:
    host = os.getenv("PI3GROQ_HOST", "127.0.0.1")
    port = int(os.getenv("PI3GROQ_PORT", "18600"))
    ensure_data_dir()
    pi_agent_session = PiAgentSessionManager()
    pi_agent_bridge = PiAgentWebSocketBridge(
        host,
        PI_AGENT_WS_PORT,
        pi_agent_session,
    )
    pi_agent_session.attach_bridge(pi_agent_bridge)
    pi_agent_bridge.start()
    if pi_agent_bridge.error:
        print(f"[Pi3Groq] PiAgent websocket bridge unavailable: {pi_agent_bridge.error}")
    else:
        print(
            f"[Pi3Groq] PiAgent websocket bridge listening at ws://{host}:{PI_AGENT_WS_PORT}"
        )
    server = Pi3GroqHTTPServer((host, port), Pi3GroqHandler, pi_agent_session)
    print(f"[Pi3Groq] Serving companion UI at http://{host}:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Pi3Groq] Shutting down.")
    finally:
        pi_agent_session.stop()
        server.server_close()


if __name__ == "__main__":
    main()
