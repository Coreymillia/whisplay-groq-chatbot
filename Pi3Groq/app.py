#!/usr/bin/env python3
from __future__ import annotations

import json
import mimetypes
import os
import posixpath
import subprocess
import threading
import time
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


DEFAULT_SETTINGS = AppSettings()


def ensure_data_dir() -> None:
    SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)


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


def read_json_request(handler: BaseHTTPRequestHandler) -> dict[str, Any]:
    length = int(handler.headers.get("Content-Length", "0") or "0")
    if length <= 0:
        return {}
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

        json_response(
            self,
            {"ok": False, "error": f"Route not found: {route}"},
            status=HTTPStatus.NOT_FOUND,
        )

    def log_message(self, format: str, *args: Any) -> None:
        print(f"[Pi3Groq] {self.address_string()} - {format % args}")

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
    server = ThreadingHTTPServer((host, port), Pi3GroqHandler)
    print(f"[Pi3Groq] Serving companion UI at http://{host}:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Pi3Groq] Shutting down.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
