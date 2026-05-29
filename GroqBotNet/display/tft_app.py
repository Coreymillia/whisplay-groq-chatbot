from __future__ import annotations

import io
import json
import os
import random
import signal
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from datetime import datetime

import board
import digitalio
from PIL import Image, ImageDraw, ImageFont, ImageOps
from adafruit_rgb_display import st7735, st7789


BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
DIM = (130, 140, 154)
BLUE = (45, 120, 255)
GREEN = (68, 214, 116)
YELLOW = (255, 214, 10)
CARD = (15, 21, 32)
OUTLINE = (40, 58, 82)
CYAN = (90, 225, 255)


@dataclass
class Config:
    api_url: str
    poll_interval: float
    scroll_interval: float
    slideshow_interval_sec: float
    rotation: int
    spi_baudrate: int
    backlight_pin: str
    font_regular: str
    font_bold: str
    screensaver_timeout_sec: float
    screensaver_frame_interval: float
    display_profile: str
    oled_enabled: bool
    oled_driver: str
    oled_i2c_port: int
    oled_i2c_address: int

    @classmethod
    def from_env(cls) -> "Config":
        return cls(
            api_url=os.getenv("GROQBOTNET_API_URL", "http://127.0.0.1:18990/api/state").strip(),
            poll_interval=float(os.getenv("GROQBOTNET_POLL_INTERVAL", "3")),
            scroll_interval=float(os.getenv("GROQBOTNET_SCROLL_INTERVAL", "0.9")),
            slideshow_interval_sec=float(
                os.getenv("GROQBOTNET_AI_SLIDESHOW_INTERVAL_SEC", "8")
            ),
            rotation=int(os.getenv("DISPLAY_ROTATION", "90")),
            spi_baudrate=int(os.getenv("DISPLAY_SPI_BAUDRATE", "24000000")),
            backlight_pin=os.getenv("DISPLAY_BACKLIGHT_PIN", "D26").strip() or "D26",
            font_regular=os.getenv(
                "FONT_REGULAR",
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            ).strip(),
            font_bold=os.getenv(
                "FONT_BOLD",
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            ).strip(),
            screensaver_timeout_sec=float(
                os.getenv("GROQBOTNET_SCREENSAVER_TIMEOUT_SEC", "45")
            ),
            screensaver_frame_interval=float(
                os.getenv("GROQBOTNET_SCREENSAVER_FRAME_INTERVAL", "0.14")
            ),
            display_profile=os.getenv("DISPLAY_PROFILE", "auto").strip().lower() or "auto",
            oled_enabled=os.getenv("OLED_DISPLAY_ENABLED", "true").strip().lower()
            not in {"0", "false", "no", "off"},
            oled_driver=os.getenv("OLED_DISPLAY_DRIVER", "sh1106").strip().lower()
            or "sh1106",
            oled_i2c_port=int(os.getenv("OLED_DISPLAY_I2C_PORT", "1")),
            oled_i2c_address=int(os.getenv("OLED_DISPLAY_I2C_ADDRESS", "0x3c"), 0),
        )


class OledStatusDisplay:
    def __init__(self, config: Config) -> None:
        self.config = config
        self.device = self._create_device()
        self.width = int(getattr(self.device, "width", 128))
        self.height = int(getattr(self.device, "height", 64))
        self.font_small = self._load_font(config.font_regular, 10)
        self.font_bold = self._load_font(config.font_bold, 12)
        self.rain_columns: list[int] = []
        self.last_rain_frame = 0.0

    @staticmethod
    def _load_font(path: str, size: int):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            return ImageFont.load_default()

    def _create_device(self):
        try:
            from smbus2 import SMBus
        except ImportError as exc:
            raise RuntimeError("Missing smbus2 for SH1106 OLED support.") from exc

        return SimpleI2COledDevice(
            bus=SMBus(self.config.oled_i2c_port),
            address=self.config.oled_i2c_address,
            driver_name=self.config.oled_driver,
        )

    def _wrap_text(
        self, draw: ImageDraw.ImageDraw, text: str, max_width: int
    ) -> list[str]:
        if not text.strip():
            return []

        words = text.replace("\n", " ").split()
        lines: list[str] = []
        current = words[0]
        for word in words[1:]:
            candidate = f"{current} {word}"
            if draw.textlength(candidate, font=self.font_small) <= max_width:
                current = candidate
            else:
                lines.append(current)
                current = word
        lines.append(current)
        return lines

    def _render_rain(self, image: Image.Image, draw: ImageDraw.ImageDraw) -> None:
        cell_w = 8
        cell_h = 10
        cols = max(1, self.width // cell_w)
        rows = max(1, self.height // cell_h)

        if len(self.rain_columns) != cols:
            self.rain_columns = [random.randint(-rows, 0) for _ in range(cols)]

        now = time.monotonic()
        if now - self.last_rain_frame >= 0.12:
            self.rain_columns = [value + 1 for value in self.rain_columns]
            for i, value in enumerate(self.rain_columns):
                if value - 6 > rows:
                    self.rain_columns[i] = random.randint(-rows, 0)
            self.last_rain_frame = now

        for col_idx, head_row in enumerate(self.rain_columns):
            x = col_idx * cell_w
            for tail in range(6):
                row = head_row - tail
                if row < 0 or row >= rows:
                    continue
                y = row * cell_h
                char = random.choice("01:/.")
                draw.text((x, y), char, fill=1, font=self.font_small)

    def _get_idle_mode(self, payload: dict) -> str:
        settings = payload.get("settings")
        mode = str((settings or {}).get("companionOledIdleMode") or "rain").strip().lower()
        return "header" if mode == "header" else "rain"

    def render(
        self,
        payload: dict,
        *,
        mode: str,
        speaker: str,
        message_text: str,
        status: str,
        idle_active: bool,
    ) -> None:
        companion = payload.get("companionSnapshot") or {}
        image = Image.new("1", (self.width, self.height), 0)
        draw = ImageDraw.Draw(image)

        status_value = str(companion.get("status") or "").strip().lower()
        reply_value = str(companion.get("replyMessage") or "").strip().lower()
        edit_mode = (
            "photo" in status_value
            or "camera" in status_value
            or "image" in status_value
            or "answering" in status_value
            or reply_value.startswith("[camera]")
            or "edit this photo" in reply_value
            or "image file saved." in reply_value
            or bool(companion.get("editHelperText"))
        )
        if edit_mode:
            helper_text = (
                str(companion.get("editHelperText") or "").strip()
                or str(companion.get("replyMessage") or "").strip()
                or "Editing photo."
            )
            lines = self._wrap_text(draw, helper_text.replace("[camera]", "").strip(), self.width - 2)
            y = 0
            for line in lines[:5]:
                draw.text((0, y), line[:24], fill=1, font=self.font_small)
                y += 12
            self.device.display(image)
            return

        if idle_active and self._get_idle_mode(payload) == "rain":
            self._render_rain(image, draw)
            self.device.display(image)
            return

        now_text = datetime.now().strftime("%H:%M")
        status_text = (status or "idle").upper()[:10]
        draw.text((0, 0), now_text, fill=1, font=self.font_bold)
        draw.text((56, 0), status_text, fill=1, font=self.font_small)

        model_label = str(companion.get("modelLabel") or companion.get("modelTag") or "BOT").strip()
        requests_today = int(companion.get("requestsToday") or 0)
        remaining_requests = companion.get("remainingRequests")
        remaining_text = "--" if remaining_requests in (None, "") else str(remaining_requests)
        balance_text = str(companion.get("balanceText") or "").strip() or "--"

        draw.text((0, 12), f"RPD {requests_today}", fill=1, font=self.font_small)
        draw.text((64, 12), f"REM {remaining_text}"[:12], fill=1, font=self.font_small)
        draw.text((0, 24), model_label[:16], fill=1, font=self.font_bold)
        draw.text((0, 36), balance_text[:21], fill=1, font=self.font_small)

        self.device.display(image)


class SimpleI2COledDevice:
    width = 128
    height = 64

    def __init__(self, *, bus, address: int, driver_name: str) -> None:
        self.bus = bus
        self.address = address
        self.driver_name = driver_name
        self.column_offset = 2 if driver_name == "sh1106" else 0
        if driver_name not in {"sh1106", "ssd1306"}:
            raise RuntimeError(f"Unsupported OLED_DISPLAY_DRIVER '{driver_name}'.")
        self._initialize()

    def _command(self, *commands: int) -> None:
        for command in commands:
            self.bus.write_byte_data(self.address, 0x00, command & 0xFF)

    def _data(self, payload: list[int]) -> None:
        for start in range(0, len(payload), 16):
            self.bus.write_i2c_block_data(
                self.address,
                0x40,
                [value & 0xFF for value in payload[start : start + 16]],
            )

    def _initialize(self) -> None:
        if self.driver_name == "sh1106":
            sequence = [
                0xAE,
                0xD5,
                0x80,
                0xA8,
                0x3F,
                0xD3,
                0x00,
                0x40,
                0xAD,
                0x8B,
                0xA1,
                0xC8,
                0xDA,
                0x12,
                0x81,
                0x7F,
                0xD9,
                0x22,
                0xDB,
                0x20,
                0xA4,
                0xA6,
                0xAF,
            ]
        else:
            sequence = [
                0xAE,
                0xD5,
                0x80,
                0xA8,
                0x3F,
                0xD3,
                0x00,
                0x40,
                0x8D,
                0x14,
                0x20,
                0x00,
                0xA1,
                0xC8,
                0xDA,
                0x12,
                0x81,
                0x7F,
                0xD9,
                0xF1,
                0xDB,
                0x40,
                0xA4,
                0xA6,
                0xAF,
            ]
        self._command(*sequence)

    def display(self, image: Image.Image) -> None:
        frame = image.convert("1")
        if frame.size != (self.width, self.height):
            frame = frame.resize((self.width, self.height))

        pixels = frame.load()
        for page in range(self.height // 8):
            column = self.column_offset
            self._command(
                0xB0 + page,
                column & 0x0F,
                0x10 | ((column >> 4) & 0x0F),
            )
            payload: list[int] = []
            for x in range(self.width):
                value = 0
                for bit in range(8):
                    if pixels[x, page * 8 + bit]:
                        value |= 1 << bit
                payload.append(value)
            self._data(payload)


class GroqBotNetDisplay:
    def __init__(self, config: Config) -> None:
        self.config = config
        self.api_base_url = self._derive_api_base_url(config.api_url)

        cs_pin = digitalio.DigitalInOut(board.CE0)
        dc_pin = digitalio.DigitalInOut(board.D25)
        reset_pin = digitalio.DigitalInOut(board.D24)

        backlight_pin = getattr(board, config.backlight_pin)
        self._backlight = digitalio.DigitalInOut(backlight_pin)
        self._backlight.switch_to_output(value=True)

        self.profile_name, self._disp = self._create_display(
            board.SPI(),
            cs_pin,
            dc_pin,
            reset_pin,
        )
        if self._disp.rotation % 180 == 90:
            self.width = self._disp.height
            self.height = self._disp.width
        else:
            self.width = self._disp.width
            self.height = self._disp.height

        self.font_small = self._load_font(config.font_regular, 12)
        self.font_medium = self._load_font(config.font_bold, 14)
        self.font_title = self._load_font(config.font_bold, 16)
        self.message_id = ""
        self.message_lines: list[str] = []
        self.scroll_index = 0
        self.last_scroll = 0.0
        self.last_activity = time.monotonic()
        self.last_screensaver_frame = 0.0
        self.matrix_columns: list[int] = []
        self.current_ai_image_key = ""
        self.current_ai_image: Image.Image | None = None
        self.ai_slideshow_keys: list[str] = []
        self.ai_slideshow_index = 0
        self.last_ai_slide_change = 0.0
        self.current_companion_image_key = ""
        self.current_companion_image: Image.Image | None = None
        self.current_source_key = ""
        self._oled = None
        if config.oled_enabled:
            try:
                self._oled = OledStatusDisplay(config)
                print(
                    f"Top OLED ready: driver={config.oled_driver} bus={config.oled_i2c_port} address=0x{config.oled_i2c_address:02x}",
                    flush=True,
                )
            except Exception as exc:
                print(f"Top OLED disabled: {exc}", flush=True)

    @staticmethod
    def _derive_api_base_url(api_url: str) -> str:
        parsed = urllib.parse.urlsplit(api_url)
        return urllib.parse.urlunsplit((parsed.scheme, parsed.netloc, "", "", ""))

    def _create_display(
        self,
        spi_bus,
        cs_pin: digitalio.DigitalInOut,
        dc_pin: digitalio.DigitalInOut,
        reset_pin: digitalio.DigitalInOut,
    ):
        profiles = {
            "st7735s-128x160": lambda: st7735.ST7735R(
                spi_bus,
                cs=cs_pin,
                dc=dc_pin,
                rst=reset_pin,
                baudrate=self.config.spi_baudrate,
                rotation=self.config.rotation,
                width=128,
                height=160,
            ),
            "st7789-mini": lambda: st7789.ST7789(
                spi_bus,
                cs=cs_pin,
                dc=dc_pin,
                rst=reset_pin,
                baudrate=self.config.spi_baudrate,
                rotation=self.config.rotation,
                width=135,
                height=240,
                x_offset=53,
                y_offset=40,
            ),
        }

        if self.config.display_profile == "auto":
            candidates = ["st7735s-128x160", "st7789-mini"]
        elif self.config.display_profile in profiles:
            candidates = [self.config.display_profile]
        else:
            raise RuntimeError(
                f"Unsupported DISPLAY_PROFILE '{self.config.display_profile}'."
            )

        last_error: Exception | None = None
        for profile in candidates:
            try:
                return profile, profiles[profile]()
            except Exception as exc:  # hardware init fallback
                last_error = exc
        raise RuntimeError(
            f"Failed to initialize any TFT profile ({', '.join(candidates)}): {last_error}"
        )

    def _render_matrix_screensaver(
        self, image: Image.Image, draw: ImageDraw.ImageDraw
    ) -> None:
        cell_w = 10
        cell_h = 14
        cols = max(1, self.width // cell_w)
        rows = max(1, self.height // cell_h)

        if len(self.matrix_columns) != cols:
            self.matrix_columns = [random.randint(-rows, 0) for _ in range(cols)]

        draw.rectangle((0, 0, self.width, self.height), fill=BLACK)
        draw.text((6, 4), "GroqBotNet", fill=DIM, font=self.font_medium)
        draw.text(
            (self.width - 72, 4),
            self.profile_name.upper()[:8],
            fill=GREEN,
            font=self.font_small,
        )

        now = time.monotonic()
        if now - self.last_screensaver_frame >= self.config.screensaver_frame_interval:
            self.matrix_columns = [value + 1 for value in self.matrix_columns]
            for i, value in enumerate(self.matrix_columns):
                if value - 10 > rows:
                    self.matrix_columns[i] = random.randint(-rows, 0)
            self.last_screensaver_frame = now

        for col_idx, head_row in enumerate(self.matrix_columns):
            x = col_idx * cell_w + 2
            for tail in range(10):
                row = head_row - tail
                if row < 0 or row >= rows:
                    continue
                y = row * cell_h + 20
                if y > self.height - 8:
                    continue
                if tail == 0:
                    color = (200, 255, 200)
                elif tail < 4:
                    color = (80, 220, 120)
                else:
                    color = (30, 120, 60)
                char = random.choice("01#@$%&*")
                draw.text((x, y), char, fill=color, font=self.font_small)

        draw.text(
            (8, self.height - 18),
            datetime.now().strftime("%H:%M:%S"),
            fill=(40, 150, 80),
            font=self.font_small,
        )

    @staticmethod
    def _load_font(path: str, size: int):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            return ImageFont.load_default()

    def _wrap_text(
        self, draw: ImageDraw.ImageDraw, text: str, max_width: int
    ) -> list[str]:
        if not text.strip():
            return []

        words = text.replace("\n", " ").split()
        lines: list[str] = []
        current = words[0]
        for word in words[1:]:
            candidate = f"{current} {word}"
            if draw.textlength(candidate, font=self.font_small) <= max_width:
                current = candidate
            else:
                lines.append(current)
                current = word
        lines.append(current)
        return lines

    def _fit_image_for_display(self, source: Image.Image) -> Image.Image:
        fitted = ImageOps.contain(source.convert("RGB"), (self.width, self.height))
        canvas = Image.new("RGB", (self.width, self.height), BLACK)
        offset_x = (self.width - fitted.width) // 2
        offset_y = (self.height - fitted.height) // 2
        canvas.paste(fitted, (offset_x, offset_y))
        return canvas

    def _fetch_ai_image(self, image_url: str) -> Image.Image:
        full_url = urllib.parse.urljoin(f"{self.api_base_url}/", image_url.lstrip("/"))
        request = urllib.request.Request(full_url, headers={"Cache-Control": "no-store"})
        with urllib.request.urlopen(request, timeout=20) as response:
            payload = response.read()
        with Image.open(io.BytesIO(payload)) as image:
            return self._fit_image_for_display(image)

    def _get_runtime_settings(self, payload: dict) -> dict:
        settings = payload.get("settings")
        return settings if isinstance(settings, dict) else {}

    def _get_idle_timeout_sec(self, payload: dict) -> float:
        settings = self._get_runtime_settings(payload)
        try:
            return max(0.0, float(settings.get("companionIdleTimeoutSec", self.config.screensaver_timeout_sec)))
        except (TypeError, ValueError):
            return self.config.screensaver_timeout_sec

    def _get_idle_mode(self, payload: dict) -> str:
        settings = self._get_runtime_settings(payload)
        mode = str(settings.get("companionIdleMode") or "slideshow").strip().lower()
        return "matrix" if mode == "matrix" else "slideshow"

    def _get_scroll_interval(self, payload: dict) -> float:
        settings = self._get_runtime_settings(payload)
        try:
            return max(0.15, float(settings.get("companionScrollSpeedSec", self.config.scroll_interval)))
        except (TypeError, ValueError):
            return self.config.scroll_interval

    def _get_text_color_mode(self, payload: dict) -> str:
        settings = self._get_runtime_settings(payload)
        mode = str(settings.get("companionTextColor") or "multicolor").strip().lower()
        if mode in {"white", "green", "cyan", "yellow", "multicolor"}:
            return mode
        return "multicolor"

    def _get_text_line_color(self, color_mode: str, line_index: int):
        if color_mode == "white":
            return WHITE
        if color_mode == "green":
            return GREEN
        if color_mode == "cyan":
            return CYAN
        if color_mode == "yellow":
            return YELLOW
        palette = (WHITE, CYAN, YELLOW, GREEN)
        return palette[line_index % len(palette)]

    def _is_edit_mode(self, payload: dict) -> bool:
        companion = payload.get("companionSnapshot") or {}
        status = str(companion.get("status") or "").strip().lower()
        reply_message = str(companion.get("replyMessage") or "").strip().lower()
        return (
            "photo" in status
            or "camera" in status
            or "image" in status
            or "answering" in status
            or reply_message.startswith("[camera]")
            or "edit this photo" in reply_message
            or "image file saved." in reply_message
            or bool(companion.get("editHelperText"))
        )

    def _companion_photo_active(self, payload: dict) -> bool:
        companion = payload.get("companionSnapshot") or {}
        return bool(companion.get("imageUrl")) and self._is_edit_mode(payload)

    def _display_companion_image(self, payload: dict) -> bool:
        companion = payload.get("companionSnapshot") or {}
        image_url = str(companion.get("imageUrl") or "").strip()
        image_key = f"{image_url}|{companion.get('imageRevision')}"
        if not image_url:
            self.current_companion_image = None
            self.current_companion_image_key = ""
            return False

        if (
            image_key != self.current_companion_image_key
            or self.current_companion_image is None
        ):
            try:
                self.current_companion_image = self._fetch_ai_image(image_url)
                self.current_companion_image_key = image_key
            except Exception:
                return False
        if self.current_companion_image is None:
            return False
        self._disp.image(self.current_companion_image)
        return True

    def _get_ai_slideshow_photos(self, payload: dict) -> list[dict]:
        ai_archive = payload.get("aiImageArchive") or {}
        photos = [
            photo
            for photo in (ai_archive.get("photos") or [])
            if isinstance(photo, dict)
            and str(photo.get("fileName") or "").strip()
            and str(photo.get("imageUrl") or "").strip()
        ]
        if photos:
            return photos

        image_url = str(ai_archive.get("latestImageUrl") or "").strip()
        image_key = str(ai_archive.get("latestFileName") or image_url).strip()
        if not image_url or not image_key:
            return []
        return [{"fileName": image_key, "imageUrl": image_url}]

    def _display_ai_image(self, payload: dict) -> bool:
        photos = self._get_ai_slideshow_photos(payload)
        if not photos:
            if self.current_ai_image is not None:
                self._disp.image(self.current_ai_image)
                return True
            self.ai_slideshow_keys = []
            self.ai_slideshow_index = 0
            self.current_ai_image = None
            self.current_ai_image_key = ""
            return False

        photo_keys = [str(photo.get("fileName") or "").strip() for photo in photos]
        if photo_keys != self.ai_slideshow_keys:
            self.ai_slideshow_keys = photo_keys
            self.ai_slideshow_index = 0
            self.last_ai_slide_change = 0.0
            self.current_ai_image = None
            self.current_ai_image_key = ""

        now = time.monotonic()
        if (
            len(photos) > 1
            and self.last_ai_slide_change > 0
            and now - self.last_ai_slide_change >= self.config.slideshow_interval_sec
        ):
            self.ai_slideshow_index = (self.ai_slideshow_index + 1) % len(photos)
            self.current_ai_image = None
            self.current_ai_image_key = ""

        selected = photos[self.ai_slideshow_index % len(photos)]
        image_url = str(selected.get("imageUrl") or "").strip()
        image_key = str(selected.get("fileName") or image_url).strip()
        if not image_url or not image_key:
            return False

        if image_key != self.current_ai_image_key or self.current_ai_image is None:
            try:
                self.current_ai_image = self._fetch_ai_image(image_url)
                self.current_ai_image_key = image_key
                self.last_ai_slide_change = now
            except Exception:
                if self.current_ai_image is not None:
                    self._disp.image(self.current_ai_image)
                    return True
                return False
        if self.current_ai_image is None:
            return False

        self._disp.image(self.current_ai_image)
        return True

    def _render_idle_placeholder(self, image: Image.Image, draw: ImageDraw.ImageDraw) -> None:
        draw.rounded_rectangle(
            (6, 14, self.width - 6, self.height - 14),
            radius=8,
            fill=CARD,
            outline=OUTLINE,
        )
        draw.text((14, 26), "AI saver", fill=CYAN, font=self.font_bold)
        draw.text((14, 46), "Waiting for art", fill=DIM, font=self.font_small)

    def _select_source(self, payload: dict) -> tuple[str, dict]:
        settings = payload.get("settings") or {}
        mode = str(settings.get("tftDisplayMode") or "auto").strip().lower()
        companion = payload.get("companionSnapshot") or {}
        if mode == "companion":
            return "companion", companion
        if mode == "local":
            return "local", payload
        if companion.get("configured") and (
            companion.get("reachable") or str(companion.get("replyMessage") or "").strip()
        ):
            return "companion", companion
        return "local", payload

    def _idle_mode_active(self, payload: dict) -> bool:
        timeout_sec = self._get_idle_timeout_sec(payload)
        if timeout_sec <= 0:
            return False
        return (time.monotonic() - self.last_activity) >= timeout_sec

    def _is_live_activity_status(self, status: str) -> bool:
        normalized = str(status or "").strip().lower()
        if not normalized:
            return False
        active_tokens = (
            "listen",
            "thinking",
            "answer",
            "recogn",
            "record",
            "wake",
            "typing",
            "sending",
            "processing",
        )
        return any(token in normalized for token in active_tokens)

    def select_message(self, payload: dict) -> tuple[str, str, str, str]:
        source_name, source_payload = self._select_source(payload)
        if source_name == "companion":
            reply_text = str(source_payload.get("replyMessage") or "").strip()
            status = str(source_payload.get("status") or "idle").strip() or "idle"
            model_tag = str(source_payload.get("modelTag") or "Whisplay").strip() or "Whisplay"
            body_text = reply_text or "Waiting for Whisplay."
            speaker = model_tag
            return ("companion", speaker, body_text, status)

        conversations = source_payload.get("conversations") or []
        if not conversations:
            archive = payload.get("aiImageArchive") or {}
            if archive.get("latestFileName"):
                return (
                    "archive",
                    "AI Image Archive",
                    archive.get("latestFileName") or "Latest AI image ready.",
                    "displaying",
                )
            return (
                "idle",
                "No chats yet",
                "Open GroqBotNet in the browser and start a solo chat or botnet conversation.",
                "idle",
            )

        conversation = conversations[0]
        mode = conversation.get("mode", "botnet")
        messages = conversation.get("messages") or []

        preferred_type = "self" if mode == "solo" else "peer"
        selected = None
        for message in reversed(messages):
            if message.get("kind") == "message" and message.get("speakerType") == preferred_type:
                selected = message
                break

        if selected is None:
            for message in reversed(messages):
                if message.get("kind") == "event":
                    selected = message
                    break

        if selected is None and messages:
            selected = messages[-1]

        if selected is None:
            return (
                mode,
                conversation.get("topic") or "Conversation",
                "Waiting for the first message.",
                conversation.get("status", "idle"),
            )

        speaker = selected.get("speakerName") or (
            "GroqBotNet Bot" if preferred_type == "self" else "Peer Bot"
        )
        message_text = selected.get("text") or ""
        return (
            mode,
            speaker,
            message_text,
            conversation.get("status", "idle"),
        )

    def render(self, payload: dict) -> None:
        mode, speaker, message_text, status = self.select_message(payload)
        key = f"{mode}|{speaker}|{message_text}|{status}"
        now = time.monotonic()
        if key != self.message_id:
            self.message_id = key
            self.current_source_key = mode
            image = Image.new("RGB", (self.width, self.height), BLACK)
            draw = ImageDraw.Draw(image)
            self.message_lines = self._wrap_text(draw, message_text, self.width - 16)
            self.scroll_index = 0
            self.last_scroll = now
            self.last_activity = now

        if self._is_live_activity_status(status):
            self.last_activity = now

        idle_active = self._idle_mode_active(payload)
        self._render_oled(
            payload,
            mode=mode,
            speaker=speaker,
            message_text=message_text,
            status=status,
            idle_active=idle_active,
        )

        if not idle_active and self._companion_photo_active(payload) and self._display_companion_image(payload):
            return

        image = Image.new("RGB", (self.width, self.height), BLACK)
        draw = ImageDraw.Draw(image)

        if self._get_idle_timeout_sec(payload) > 0 and idle_active:
            idle_mode = self._get_idle_mode(payload)
            if idle_mode == "slideshow":
                if self._display_ai_image(payload):
                    return
                self._render_idle_placeholder(image, draw)
            else:
                self._render_matrix_screensaver(image, draw)
            self._disp.image(image)
            return

        draw.rounded_rectangle(
            (4, 4, self.width - 4, self.height - 4),
            radius=8,
            fill=CARD,
            outline=OUTLINE,
            width=1,
        )

        visible_line_count = 8
        if self.message_lines:
            if (
                len(self.message_lines) > visible_line_count
                and time.monotonic() - self.last_scroll >= self._get_scroll_interval(payload)
            ):
                max_scroll_index = len(self.message_lines) - visible_line_count
                if self.scroll_index < max_scroll_index:
                    self.scroll_index += 1
                    self.last_scroll = time.monotonic()
            visible = self.message_lines[
                self.scroll_index : self.scroll_index + visible_line_count
            ]
            y = 14
            color_mode = self._get_text_color_mode(payload)
            for line in visible:
                draw.text((10, y), line, fill=self._get_text_line_color(color_mode, (y - 14) // 16), font=self.font_small)
                y += 16
        else:
            draw.text((10, 20), "Waiting for a reply...", fill=DIM, font=self.font_small)
        self._disp.image(image)

    def _render_oled(
        self,
        payload: dict,
        *,
        mode: str,
        speaker: str,
        message_text: str,
        status: str,
        idle_active: bool,
    ) -> None:
        if self._oled is None:
            return
        try:
            self._oled.render(
                payload,
                mode=mode,
                speaker=speaker,
                message_text=message_text,
                status=status,
                idle_active=idle_active,
            )
        except Exception as exc:
            print(f"Top OLED render failed: {exc}", flush=True)
            self._oled = None

    def show_error(self, text: str) -> None:
        payload = {
            "conversations": [
                {
                    "mode": "idle",
                    "status": "error",
                    "messages": [
                        {
                            "kind": "event",
                            "speakerName": "GroqBotNet",
                            "text": text,
                        }
                    ],
                }
            ]
        }
        self.render(payload)


def fetch_state(api_url: str) -> dict:
    request = urllib.request.Request(api_url, headers={"Cache-Control": "no-store"})
    with urllib.request.urlopen(request, timeout=5) as response:
        return json.loads(response.read().decode("utf-8"))


def main() -> None:
    config = Config.from_env()
    stop_event = threading.Event()

    def handle_signal(_signum: int, _frame: object) -> None:
        stop_event.set()

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)

    display = GroqBotNetDisplay(config)
    last_payload = None
    last_error = ""
    display.show_error("Starting GroqBotNet TFT display...")

    while not stop_event.is_set():
        try:
            payload = fetch_state(config.api_url)
            last_payload = payload
            last_error = ""
            display.render(payload)
        except (
            urllib.error.URLError,
            TimeoutError,
            OSError,
            RuntimeError,
            json.JSONDecodeError,
        ) as exc:
            message = f"Display waiting for GroqBotNet: {exc}"
            if last_payload is not None:
                display.render(last_payload)
            elif message != last_error:
                display.show_error(message)
                last_error = message

        stop_event.wait(config.poll_interval)


if __name__ == "__main__":
    main()
