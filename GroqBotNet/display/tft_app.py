from __future__ import annotations

import json
import os
import random
import signal
import textwrap
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import datetime

import board
import digitalio
from PIL import Image, ImageDraw, ImageFont
from adafruit_rgb_display import st7789


BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
DIM = (130, 140, 154)
BLUE = (45, 120, 255)
GREEN = (68, 214, 116)
YELLOW = (255, 214, 10)
RED = (255, 92, 92)
CARD = (15, 21, 32)
OUTLINE = (40, 58, 82)


@dataclass
class Config:
    api_url: str
    poll_interval: float
    scroll_interval: float
    rotation: int
    spi_baudrate: int
    backlight_pin: str
    font_regular: str
    font_bold: str
    screensaver_timeout_sec: float
    screensaver_frame_interval: float

    @classmethod
    def from_env(cls) -> "Config":
        return cls(
            api_url=os.getenv("GROQBOTNET_API_URL", "http://127.0.0.1:18990/api/state").strip(),
            poll_interval=float(os.getenv("GROQBOTNET_POLL_INTERVAL", "3")),
            scroll_interval=float(os.getenv("GROQBOTNET_SCROLL_INTERVAL", "0.9")),
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
        )


class GroqBotNetDisplay:
    def __init__(self, config: Config) -> None:
        self.config = config
        cs_pin = digitalio.DigitalInOut(board.CE0)
        dc_pin = digitalio.DigitalInOut(board.D25)
        reset_pin = digitalio.DigitalInOut(board.D24)

        backlight_pin = getattr(board, config.backlight_pin)
        self._backlight = digitalio.DigitalInOut(backlight_pin)
        self._backlight.switch_to_output(value=True)

        self._disp = st7789.ST7789(
            board.SPI(),
            cs=cs_pin,
            dc=dc_pin,
            rst=reset_pin,
            baudrate=config.spi_baudrate,
            rotation=config.rotation,
            width=135,
            height=240,
            x_offset=53,
            y_offset=40,
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

    def _render_matrix_screensaver(self, image: Image.Image, draw: ImageDraw.ImageDraw) -> None:
        cell_w = 10
        cell_h = 14
        cols = max(1, self.width // cell_w)
        rows = max(1, self.height // cell_h)

        if len(self.matrix_columns) != cols:
            self.matrix_columns = [random.randint(-rows, 0) for _ in range(cols)]

        draw.rectangle((0, 0, self.width, self.height), fill=BLACK)
        draw.text((6, 4), "GroqBotNet", fill=DIM, font=self.font_medium)
        draw.text((self.width - 78, 4), "MATRIX", fill=GREEN, font=self.font_small)

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

    def _wrap_text(self, draw: ImageDraw.ImageDraw, text: str, max_width: int) -> list[str]:
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

    def select_message(self, payload: dict) -> tuple[str, str, str, str]:
        conversations = payload.get("conversations") or []
        if not conversations:
            return ("idle", "No chats yet", "Open GroqBotNet in the browser and start a solo chat or botnet conversation.", "idle")

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
            return (mode, conversation.get("topic") or "Conversation", "Waiting for the first message.", conversation.get("status", "idle"))

        speaker = selected.get("speakerName") or ("GroqBotNet Bot" if preferred_type == "self" else "Peer Bot")
        message_text = selected.get("text") or ""
        return (
            mode,
            speaker,
            message_text,
            conversation.get("status", "idle"),
        )

    def render(self, payload: dict) -> None:
        image = Image.new("RGB", (self.width, self.height), BLACK)
        draw = ImageDraw.Draw(image)

        mode, speaker, message_text, status = self.select_message(payload)
        key = f"{mode}|{speaker}|{message_text}|{status}"
        now = time.monotonic()
        if key != self.message_id:
            self.message_id = key
            self.message_lines = self._wrap_text(draw, message_text, self.width - 16)
            self.scroll_index = 0
            self.last_scroll = now
            self.last_activity = now

        if status != "idle":
            self.last_activity = now

        if (
            self.config.screensaver_timeout_sec > 0
            and status == "idle"
            and (now - self.last_activity) >= self.config.screensaver_timeout_sec
        ):
            self._render_matrix_screensaver(image, draw)
            self._disp.image(image)
            return

        draw.rectangle((0, 0, self.width, 22), fill=BLUE)
        draw.text((6, 4), "GroqBotNet", fill=WHITE, font=self.font_title)
        draw.text((self.width - 86, 5), mode.upper()[:10], fill=(220, 235, 255), font=self.font_small)

        draw.rounded_rectangle((6, 28, self.width - 6, self.height - 8), radius=10, fill=CARD, outline=OUTLINE, width=1)
        draw.text((12, 36), speaker[:24], fill=GREEN if mode == "solo" else YELLOW, font=self.font_medium)
        draw.text((12, 54), f"status: {status}", fill=DIM, font=self.font_small)

        visible_line_count = 4
        if self.message_lines:
            if len(self.message_lines) > visible_line_count and time.monotonic() - self.last_scroll >= self.config.scroll_interval:
                self.scroll_index = (self.scroll_index + 1) % (len(self.message_lines) - visible_line_count + 1)
                self.last_scroll = time.monotonic()
            visible = self.message_lines[self.scroll_index : self.scroll_index + visible_line_count]
            y = 74
            for line in visible:
                draw.text((12, y), line, fill=WHITE, font=self.font_small)
                y += 15
        else:
            draw.text((12, 78), "Waiting for a reply...", fill=DIM, font=self.font_small)

        timestamp = datetime.now().strftime("%H:%M:%S")
        draw.text((12, self.height - 22), timestamp, fill=DIM, font=self.font_small)
        self._disp.image(image)

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
        except (urllib.error.URLError, TimeoutError, OSError, RuntimeError, json.JSONDecodeError) as exc:
            message = f"Display waiting for GroqBotNet: {exc}"
            if last_payload is not None:
                display.render(last_payload)
            elif message != last_error:
                display.show_error(message)
                last_error = message

        stop_event.wait(config.poll_interval)


if __name__ == "__main__":
    main()
