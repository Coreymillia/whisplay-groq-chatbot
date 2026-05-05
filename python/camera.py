import argparse
import json
import os
import shutil
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
from io import BytesIO
from urllib.error import HTTPError, URLError
from urllib.request import urlopen
from PIL import Image

PYTHON_DIR = os.path.abspath(os.path.dirname(__file__))
if PYTHON_DIR not in sys.path:
    sys.path.insert(0, PYTHON_DIR)

from utils import ImageUtils

try:
    from picamera2 import Picamera2
except ImportError:
    Picamera2 = None


def _default_web_frame_path() -> str:
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    return os.path.join(project_root, "data", "camera_feed", "web_live.jpg")


def _default_settings_path() -> str:
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    return os.path.join(project_root, ".whisplay-groqhat-settings.json")


DAEMON_HOST = os.getenv("WHISPLAY_CAMERA_DAEMON_HOST", "127.0.0.1")
DAEMON_PORT = int(os.getenv("WHISPLAY_CAMERA_DAEMON_PORT", "18765"))
DAEMON_TIMEOUT_SEC = float(os.getenv("WHISPLAY_CAMERA_DAEMON_TIMEOUT_SEC", "2"))
DEFAULT_CAMERA_SOURCE = "pi-camera"
DEFAULT_ESP32_CAM_URL = "http://esp32-cam.local"


def _normalize_camera_source(value: str | None) -> str:
    return value if value in {"pi-camera", "esp32-cam"} else DEFAULT_CAMERA_SOURCE


def _normalize_camera_url(value: str | None) -> str:
    if value is None:
        return DEFAULT_ESP32_CAM_URL
    normalized = value.strip()
    if not normalized:
        return DEFAULT_ESP32_CAM_URL
    if not normalized.startswith(("http://", "https://")):
        normalized = f"http://{normalized}"
    return normalized.rstrip("/")


def _format_network_camera_error(base_url: str, error: Exception) -> str:
    return f"Could not reach ESP32-CAM at {base_url}: {error}"


class SharedCameraService:
    def __init__(self):
        self.web_frame_path = _default_web_frame_path()
        self.settings_path = _default_settings_path()
        os.makedirs(os.path.dirname(self.web_frame_path), exist_ok=True)

        self.capture_width = max(64, int(os.getenv("WHISPLAY_CAMERA_WIDTH", "560")))
        self.capture_height = max(64, int(os.getenv("WHISPLAY_CAMERA_HEIGHT", "480")))
        self.capture_quality = max(30, min(100, int(os.getenv("WHISPLAY_CAMERA_QUALITY", "95"))))
        self.stream_quality = max(20, min(95, int(os.getenv("WHISPLAY_CAMERA_STREAM_QUALITY", "80"))))
        interval_ms = int(os.getenv("WHISPLAY_CAMERA_DAEMON_INTERVAL_MS", "200"))
        self.stream_interval_sec = max(0.05, interval_ms / 1000)
        self.network_timeout_sec = max(
            1.0,
            float(os.getenv("WHISPLAY_CAMERA_NETWORK_TIMEOUT_SEC", "5")),
        )
        self.pi_camera_timeout_sec = max(
            2.0,
            float(os.getenv("WHISPLAY_PI_CAMERA_TIMEOUT_SEC", "8")),
        )

        self.picam2 = None
        self.rpicam_still = shutil.which("rpicam-still")
        self.running = True
        self.stream_ref_count = 0
        self.state_lock = threading.Lock()
        self.camera_lock = threading.Lock()
        self._cached_settings = {}
        self._cached_settings_mtime = None

        self.worker = threading.Thread(target=self._stream_loop, daemon=True)
        self.worker.start()

    def _load_runtime_settings(self) -> dict:
        try:
            settings_mtime = os.path.getmtime(self.settings_path)
        except OSError:
            self._cached_settings = {}
            self._cached_settings_mtime = None
            return {}

        if self._cached_settings_mtime == settings_mtime:
            return dict(self._cached_settings)

        try:
            with open(self.settings_path, "r", encoding="utf-8") as handle:
                loaded = json.load(handle)
        except (OSError, ValueError, TypeError):
            loaded = {}

        if not isinstance(loaded, dict):
            loaded = {}

        self._cached_settings = loaded
        self._cached_settings_mtime = settings_mtime
        return dict(self._cached_settings)

    def _current_camera_source(self) -> str:
        settings = self._load_runtime_settings()
        return _normalize_camera_source(
            settings.get("cameraSource") or os.getenv("WHISPLAY_CAMERA_SOURCE"),
        )

    def _current_network_camera_url(self) -> str:
        settings = self._load_runtime_settings()
        return _normalize_camera_url(
            settings.get("esp32CamUrl") or os.getenv("WHISPLAY_ESP32_CAM_URL"),
        )

    def _network_endpoint(self, path: str) -> str:
        return f"{self._current_network_camera_url()}{path}"

    def _fetch_network_image_bytes(self) -> bytes:
        base_url = self._current_network_camera_url()
        request_url = f"{base_url}/latest.jpg"
        with urlopen(request_url, timeout=self.network_timeout_sec) as response:
            if response.status != 200:
                raise RuntimeError("ESP32-CAM snapshot request failed.")
            return response.read()

    def _fetch_network_image_bytes_safe(self) -> bytes:
        try:
            return self._fetch_network_image_bytes()
        except (OSError, ValueError, HTTPError, URLError) as error:
            raise RuntimeError(
                _format_network_camera_error(self._current_network_camera_url(), error)
            ) from error

    def _network_camera_ready(self) -> tuple[bool, str]:
        try:
            with urlopen(
                self._network_endpoint("/status"),
                timeout=self.network_timeout_sec,
            ) as response:
                if response.status != 200:
                    return False, "ESP32-CAM status endpoint returned an error."
                response.read()
        except (OSError, ValueError, HTTPError, URLError) as error:
            return False, _format_network_camera_error(
                self._current_network_camera_url(),
                error,
            )
        return True, ""

    def _ensure_camera_ready(self) -> None:
        if Picamera2 is None:
            raise RuntimeError("Picamera2 is unavailable")
        if self.picam2 is not None:
            return
        self.picam2 = Picamera2()
        self.picam2.configure(
            self.picam2.create_preview_configuration(
                main={"size": (self.capture_width, self.capture_height)}
            )
        )
        self.picam2.start()

    def _capture_picamera_image(self) -> Image.Image:
        self._ensure_camera_ready()
        frame = self.picam2.capture_array()
        image = Image.fromarray(frame)
        if image.mode != "RGB":
            image = image.convert("RGB")
        return image

    def _capture_rpicam_image(
        self,
        width: int,
        height: int,
        quality: int,
    ) -> Image.Image:
        if self.rpicam_still is None:
            raise RuntimeError("rpicam-still is unavailable")

        with tempfile.NamedTemporaryFile(suffix=".jpg", delete=False) as temp_file:
            temp_path = temp_file.name

        try:
            subprocess.run(
                [
                    self.rpicam_still,
                    "--output",
                    temp_path,
                    "--nopreview",
                    "--immediate",
                    "--encoding",
                    "jpg",
                    "--width",
                    str(width),
                    "--height",
                    str(height),
                    "--quality",
                    str(quality),
                ],
                check=True,
                capture_output=True,
                text=True,
                timeout=self.pi_camera_timeout_sec,
            )
            with Image.open(temp_path) as captured:
                if captured.mode != "RGB":
                    return captured.convert("RGB")
                return captured.copy()
        except subprocess.TimeoutExpired as error:
            raise RuntimeError("Pi camera capture timed out.") from error
        except subprocess.CalledProcessError as error:
            stderr = error.stderr.strip() if error.stderr else "unknown camera error"
            raise RuntimeError(f"Pi camera capture failed: {stderr}") from error
        finally:
            try:
                os.unlink(temp_path)
            except OSError:
                pass

    def _current_pi_camera_backend(self) -> str | None:
        if Picamera2 is not None:
            return "picamera2"
        if self.rpicam_still is not None:
            return "rpicam-still"
        return None

    def _capture_local_pi_image(self, quality: int) -> Image.Image:
        with self.camera_lock:
            backend = self._current_pi_camera_backend()
            if backend == "picamera2":
                try:
                    return self._capture_picamera_image()
                except Exception:
                    if self.rpicam_still is None:
                        raise
                    return self._capture_rpicam_image(
                        self.capture_width,
                        self.capture_height,
                        quality,
                    )
            if backend == "rpicam-still":
                return self._capture_rpicam_image(
                    self.capture_width,
                    self.capture_height,
                    quality,
                )
        raise RuntimeError(
            "Pi camera backend is unavailable. Install Picamera2 or ensure rpicam-still is available."
        )

    def _capture_network_image(self) -> Image.Image:
        data = self._fetch_network_image_bytes_safe()
        image = Image.open(BytesIO(data))
        if image.mode != "RGB":
            image = image.convert("RGB")
        return image

    def _capture_frame_image(self, quality: int | None = None) -> Image.Image:
        source = self._current_camera_source()
        if source == "esp32-cam":
            return self._capture_network_image()
        return self._capture_local_pi_image(quality or self.stream_quality)

    def _write_web_frame(self, image: Image.Image) -> None:
        temp_path = f"{self.web_frame_path}.tmp"
        image.save(temp_path, format="JPEG", quality=80)
        os.replace(temp_path, self.web_frame_path)

    def _stream_loop(self) -> None:
        while self.running:
            should_stream = False
            with self.state_lock:
                should_stream = self.stream_ref_count > 0
            if not should_stream:
                time.sleep(0.05)
                continue
            try:
                image = self._capture_frame_image()
                self._write_web_frame(image)
            except Exception:
                time.sleep(0.2)
                continue
            time.sleep(self.stream_interval_sec)

    def stop(self) -> None:
        self.running = False
        self.worker.join(timeout=1)
        with self.camera_lock:
            if self.picam2 is not None:
                try:
                    self.picam2.stop()
                except Exception:
                    pass
                self.picam2 = None

    def handle_command(self, payload: dict) -> dict:
        cmd = str(payload.get("cmd", "")).strip().lower()

        if cmd == "ping":
            with self.state_lock:
                active = self.stream_ref_count
            return {
                "ok": True,
                "stream_ref_count": active,
                "source": self._current_camera_source(),
                "pi_camera_backend": self._current_pi_camera_backend(),
            }

        if cmd == "status":
            with self.state_lock:
                active = self.stream_ref_count
            source = self._current_camera_source()
            ready = False
            error = ""
            pi_camera_backend = self._current_pi_camera_backend()
            if source == "esp32-cam":
                ready, error = self._network_camera_ready()
            else:
                ready = pi_camera_backend is not None
                if not ready:
                    error = "No Pi camera backend is available (Picamera2 or rpicam-still)."
            return {
                "ok": True,
                "stream_ref_count": active,
                "ready": ready,
                "source": source,
                "pi_camera_backend": pi_camera_backend,
                "esp32_cam_url": self._current_network_camera_url(),
                "error": error,
            }

        if cmd == "start_stream":
            with self.state_lock:
                self.stream_ref_count += 1
                active = self.stream_ref_count
            return {"ok": True, "stream_ref_count": active}

        if cmd == "stop_stream":
            with self.state_lock:
                self.stream_ref_count = max(0, self.stream_ref_count - 1)
                active = self.stream_ref_count
            return {"ok": True, "stream_ref_count": active}

        if cmd == "capture":
            target = str(payload.get("path", "")).strip()
            if not target:
                return {"ok": False, "error": "missing capture path"}
            target_path = os.path.abspath(target)
            os.makedirs(os.path.dirname(target_path), exist_ok=True)
            try:
                image = self._capture_frame_image(self.capture_quality)
                image.save(target_path, format="JPEG", quality=95)
                return {
                    "ok": True,
                    "path": target_path,
                    "source": self._current_camera_source(),
                    "pi_camera_backend": self._current_pi_camera_backend(),
                }
            except Exception as e:
                return {"ok": False, "error": str(e)}

        return {"ok": False, "error": f"unknown command: {cmd}"}


SERVICE_INSTANCE = None


class CameraDaemonHandler(socketserver.StreamRequestHandler):
    def handle(self):
        global SERVICE_INSTANCE
        while True:
            raw = self.rfile.readline()
            if not raw:
                return
            try:
                payload = json.loads(raw.decode("utf-8").strip() or "{}")
            except Exception:
                self.wfile.write(b'{"ok": false, "error": "invalid json"}\n')
                self.wfile.flush()
                continue
            response = SERVICE_INSTANCE.handle_command(payload)
            self.wfile.write((json.dumps(response) + "\n").encode("utf-8"))
            self.wfile.flush()


class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


def run_daemon(host: str, port: int) -> None:
    global SERVICE_INSTANCE
    SERVICE_INSTANCE = SharedCameraService()
    server = ThreadedTCPServer((host, port), CameraDaemonHandler)
    print(f"[CameraDaemon] Listening on {host}:{port}")
    try:
        server.serve_forever()
    finally:
        server.server_close()
        SERVICE_INSTANCE.stop()


def camera_daemon_request(
    cmd: str,
    payload: dict | None = None,
    timeout: float = DAEMON_TIMEOUT_SEC,
) -> dict:
    data = {"cmd": cmd}
    if payload:
        data.update(payload)
    with socket.create_connection((DAEMON_HOST, DAEMON_PORT), timeout=timeout) as sock:
        sock.sendall((json.dumps(data) + "\n").encode("utf-8"))
        sock_file = sock.makefile("r")
        line = sock_file.readline().strip()
        if not line:
            return {"ok": False, "error": "empty response"}
        return json.loads(line)


def ensure_camera_daemon(timeout_sec: float = 3.0) -> bool:
    try:
        response = camera_daemon_request("ping", timeout=0.4)
        if response.get("ok"):
            return True
    except Exception:
        pass

    subprocess.Popen(
        [sys.executable, os.path.abspath(__file__), "--daemon"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        stdin=subprocess.DEVNULL,
        start_new_session=True,
    )

    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        try:
            response = camera_daemon_request("ping", timeout=0.5)
            if response.get("ok"):
                return True
        except Exception:
            time.sleep(0.1)
    return False


class CameraThread(threading.Thread):
    def __init__(self, whisplay, image_path):
        super().__init__()
        self.whisplay = whisplay
        self.running = False
        self.capture_image = None
        self.image_path = image_path
        self.web_frame_path = _default_web_frame_path()
        self.frame_poll_sec = max(
            0.03,
            int(os.getenv("WHISPLAY_CAMERA_UI_POLL_MS", "80")) / 1000,
        )
        self._stream_started = False

    def start(self):
        self.running = True
        return super().start()

    def _draw_image_to_display(self, image: Image.Image) -> None:
        if image.mode != "RGB":
            image = image.convert("RGB")
        image = image.resize((self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT), Image.LANCZOS)
        pixel_bytes = ImageUtils.image_to_rgb565(
            image,
            self.whisplay.LCD_WIDTH,
            self.whisplay.LCD_HEIGHT,
        )
        self.whisplay.draw_image(
            0,
            0,
            self.whisplay.LCD_WIDTH,
            self.whisplay.LCD_HEIGHT,
            pixel_bytes,
        )

    def run(self):
        if not ensure_camera_daemon():
            print("[Camera] Failed to connect/start camera daemon")
            return
        response = camera_daemon_request("start_stream")
        self._stream_started = bool(response.get("ok"))

        while self.running and self.capture_image is None:
            if os.path.exists(self.web_frame_path):
                try:
                    image = Image.open(self.web_frame_path).convert("RGB")
                    self._draw_image_to_display(image)
                except Exception:
                    pass
            time.sleep(self.frame_poll_sec)

        if self.capture_image is not None:
            self._draw_image_to_display(self.capture_image)
            time.sleep(2)

    def capture(self):
        response = camera_daemon_request("capture", {"path": self.image_path})
        if not response.get("ok"):
            print(f"[Camera] Capture failed: {response.get('error', 'unknown error')}")
            return
        if os.path.exists(self.image_path):
            self.capture_image = Image.open(self.image_path).convert("RGB")
            print(f"[Camera] Captured image saved to {self.image_path}")

    def stop(self):
        self.running = False
        if self._stream_started:
            try:
                camera_daemon_request("stop_stream")
            except Exception:
                pass
            self._stream_started = False
        if self.is_alive():
            self.join()


def _main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--daemon", action="store_true", help="Run camera daemon")
    parser.add_argument(
        "--ensure-daemon",
        action="store_true",
        help="Ensure daemon is running and exit",
    )
    args = parser.parse_args()

    if args.ensure_daemon:
        ok = ensure_camera_daemon()
        print("[CameraDaemon] ready" if ok else "[CameraDaemon] failed")
        return 0 if ok else 1

    if args.daemon:
        run_daemon(DAEMON_HOST, DAEMON_PORT)
        return 0

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(_main())
