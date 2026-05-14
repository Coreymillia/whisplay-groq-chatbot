from PIL import Image, ImageDraw, ImageFont
import math
import os
import random
import time
import socket
import json
import sys
import threading
import signal

# from whisplay import WhisplayBoard
from whisplay import WhisplayBoard
from camera import CameraThread
from utils import ColorUtils, ImageUtils, TextUtils

STATUS_ICON_DIR = os.path.join(os.path.dirname(__file__), "status-bar-icon")
if STATUS_ICON_DIR not in sys.path:
    sys.path.append(STATUS_ICON_DIR)

from battery_icon import BatteryStatusIcon
from wifi_icon import WifiStatusIcon
from request_count_icon import RequestCountStatusIcon
from rag_icon import RagStatusIcon
from image_icon import ImageStatusIcon
from wireguard_icon import WireguardStatusIcon

scroll_thread = None
scroll_stop_event = threading.Event()

status_font_size=20
emoji_font_size=40
battery_font_size=13
IDLE_RENDER_INTERVAL = 0.5
IDLE_COMPATIBLE_STATUSES = {"idle", "last reply"}
RANDOM_SCREENSAVER_INTERVAL_SEC = 120
RANDOM_SCREENSAVER_MODES = (
    "matrix",
    "matrix-binary",
    "matrix-blue",
    "retro-geometry",
    "plasma",
    "neon-rain",
    "bouncing-balls",
    "kaleidoscope",
    "tetris-rain",
)

# Global variables
current_status = "Hello"
current_emoji = "😄"
current_text = "Waiting for message..."
current_battery_level = 100
current_battery_color = ColorUtils.get_rgb255_from_any("#55FF00")
current_scroll_top = 0
DEFAULT_SCROLL_SPEED = 0.9
MAX_SCROLL_SPEED = 2.6
DEFAULT_SCROLL_SPEED_FACTOR = 1.0
current_scroll_speed = DEFAULT_SCROLL_SPEED
current_scroll_speed_factor = DEFAULT_SCROLL_SPEED_FACTOR
current_scroll_sync_char_end = None
current_scroll_sync_duration_ms = None
current_scroll_sync_target_top = None
current_scroll_sync_speed = None
current_scroll_sync_hold_until = 0.0
current_transaction_id = None
current_image_path = ""
current_image = None
current_network_connected = None
current_wifi_signal_level = 0
current_groq_requests_today = 0
current_vpn_connected = False
current_rag_icon_visible = False
current_image_icon_visible = False
current_music_progress = None
current_music_duration_ms = None
current_audio_level = 0
current_header_mode = "emoji"
current_screensaver_mode = "off"
current_idle_timeout_sec = 120
current_screen_blank_timeout_sec = 0
current_hat_text_color = "white"
last_activity_at = time.time()
camera_mode = False
camera_capture_image_path = ""
camera_thread = None
render_thread = None
clients = {}
status_icon_factories = []


def normalize_scroll_speed_factor(value):
    try:
        return max(0.4, min(2.0, float(value)))
    except (TypeError, ValueError):
        return DEFAULT_SCROLL_SPEED_FACTOR


def to_device_scroll_speed(requested_speed, scroll_speed_factor):
    try:
        base_speed = float(requested_speed)
    except (TypeError, ValueError):
        raise ValueError(f"Invalid scroll_speed payload: {requested_speed}")
    normalized_speed = max(0.0, (base_speed / 3.0) * DEFAULT_SCROLL_SPEED)
    effective_speed = normalized_speed * normalize_scroll_speed_factor(scroll_speed_factor)
    return min(MAX_SCROLL_SPEED, max(0.0, effective_speed))


def resolve_font_path(custom_font_path=None):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    candidates = [
        custom_font_path,
        os.path.join(script_dir, "NotoSansSC-Bold.ttf"),
        os.path.join(repo_root, "NotoSansSC-Bold.ttf"),
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for font_path in candidates:
        if font_path and os.path.exists(font_path):
            return font_path
    raise FileNotFoundError("No usable font file found for chatbot UI rendering.")


def register_status_icon_factory(factory, priority=100):
    status_icon_factories.append({"priority": priority, "factory": factory})


def note_activity():
    global last_activity_at
    last_activity_at = time.time()

class RenderThread(threading.Thread):
    def __init__(self, whisplay, font_path, fps=30):
        super().__init__()
        self.whisplay = whisplay
        self.font_path = font_path
        self.fps = fps
        self.render_init_screen()
        # Clear logo after 1 second and start running loop
        time.sleep(1)
        self.running = True
        self.status_font = ImageFont.truetype(self.font_path, status_font_size)
        self.emoji_font = ImageFont.truetype(self.font_path, emoji_font_size)
        self.battery_font = ImageFont.truetype(self.font_path, battery_font_size)
        self.main_text_font = ImageFont.truetype(self.font_path, 20)
        self.music_time_font = ImageFont.truetype(self.font_path, 10)
        self.header_matrix_font = ImageFont.truetype(self.font_path, 14)
        self.screensaver_matrix_font = ImageFont.truetype(self.font_path, 16)
        self.header_effect_height = emoji_font_size + 6
        self.main_text_line_height = self.main_text_font.getmetrics()[0] + self.main_text_font.getmetrics()[1]
        self.text_cache_image = None
        self.current_render_text = ""
        self.current_render_style = ""
        self.pending_auto_scroll_after_hold = False
        self.render_event = threading.Event()
        self.green_matrix_columns = self.create_rain_columns(10, "01ABCDEF", self.whisplay.LCD_HEIGHT)
        self.binary_matrix_columns = self.create_rain_columns(7, "01", self.whisplay.LCD_HEIGHT)
        self.blue_matrix_columns = self.create_rain_columns(8, "01ABCDEF[]{}<>+-*/", self.whisplay.LCD_HEIGHT)
        self.header_green_matrix_columns = self.create_rain_columns(10, "01ABCDEF", self.header_effect_height)
        self.header_binary_matrix_columns = self.create_rain_columns(8, "01", self.header_effect_height)
        self.header_blue_matrix_columns = self.create_rain_columns(8, "01ABCDEF[]{}<>+-*/", self.header_effect_height)
        self.neon_streams = self.create_neon_streams(4, self.whisplay.LCD_HEIGHT)
        self.header_neon_streams = self.create_neon_streams(5, self.header_effect_height)
        self.retro_shapes = [self.create_retro_shape(self.whisplay.LCD_HEIGHT) for _ in range(8)]
        self.header_retro_shapes = [self.create_retro_shape(self.header_effect_height) for _ in range(4)]
        self.header_vu_history = [0.0] * 48
        self.header_vu_phase = 0.0
        self.header_vu_level = 0.0
        self.bouncing_balls = [self.create_bouncing_ball(self.whisplay.LCD_HEIGHT) for _ in range(7)]
        self.kaleido_rotation = 0.0
        self.kaleido_elements = [self.create_kaleido_element(self.whisplay.LCD_HEIGHT) for _ in range(14)]
        self.tetris_pieces = []
        self.tetris_spawn_timer = 0
        self.random_screensaver_mode = random.choice(RANDOM_SCREENSAVER_MODES)
        self.random_screensaver_started_at = time.time()

    def render_init_screen(self):
        # Display logo on startup
        logo_path = os.path.join("img", "logo.png")
        if os.path.exists(logo_path):
            logo_image = Image.open(logo_path).convert("RGBA")
            logo_image = logo_image.resize((whisplay.LCD_WIDTH, whisplay.LCD_HEIGHT), Image.LANCZOS)
            rgb565_data = ImageUtils.image_to_rgb565(logo_image, whisplay.LCD_WIDTH, whisplay.LCD_HEIGHT)
            whisplay.set_backlight(100)
            whisplay.draw_image(0, 0, whisplay.LCD_WIDTH, whisplay.LCD_HEIGHT, rgb565_data)

    def render_frame(self, status, emoji, text, scroll_top, battery_level, battery_color):
        global current_scroll_speed, current_image_path, current_image, camera_mode
        self.pending_auto_scroll_after_hold = False
        if camera_mode:
            return False  # Skip rendering if in camera mode
        if self.should_blank_screen():
            self.whisplay.set_backlight(0)
            return False
        self.whisplay.set_backlight(100)
        if self.should_show_screensaver():
            saver_image = Image.new("RGBA", (self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT), (0, 0, 0, 255))
            saver_draw = ImageDraw.Draw(saver_image)
            self.render_screensaver_frame(
                saver_image,
                saver_draw,
                self.whisplay.LCD_WIDTH,
                self.whisplay.LCD_HEIGHT,
                status,
            )
            self.whisplay.draw_image(
                0,
                0,
                self.whisplay.LCD_WIDTH,
                self.whisplay.LCD_HEIGHT,
                ImageUtils.image_to_rgb565(saver_image, self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT),
            )
            return True
        if current_image_path not in [None, ""]:
            # Try to load image from path
            if current_image is not None:
                rgb565_data = ImageUtils.image_to_rgb565(current_image, self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT)
                self.whisplay.draw_image(0, 0, self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT, rgb565_data)
            elif os.path.exists(current_image_path):
                try:
                    image = Image.open(current_image_path).convert("RGBA") # 1024x1024
                    # crop center and resize to fit screen ratio
                    img_w, img_h = image.size
                    screen_ratio = self.whisplay.LCD_WIDTH / self.whisplay.LCD_HEIGHT
                    img_ratio = img_w / img_h
                    if img_ratio > screen_ratio:
                        # crop width
                        new_w = int(img_h * screen_ratio)
                        left = (img_w - new_w) // 2
                        image = image.crop((left, 0, left + new_w, img_h))
                    else:
                        # crop height
                        new_h = int(img_w / screen_ratio)
                        top = (img_h - new_h) // 2
                        image = image.crop((0, top, img_w, top + new_h))
                    image = image.resize((self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT), Image.LANCZOS)
                    current_image = image
                    rgb565_data = ImageUtils.image_to_rgb565(image, self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT)
                    self.whisplay.draw_image(0, 0, self.whisplay.LCD_WIDTH, self.whisplay.LCD_HEIGHT, rgb565_data)
                except Exception as e:
                    print(f"[Render] Failed to load image {current_image_path}: {e}")
            return False
        else:
            current_image = None
            header_height = 88 + 10  # header + margin
            # create a black background image for header
            image = Image.new("RGBA", (self.whisplay.LCD_WIDTH, header_height), (0, 0, 0, 255))
            draw = ImageDraw.Draw(image)
            
            clock_font_size = 24
            # clock_font = ImageFont.truetype(self.font_path, clock_font_size)

            # current_time = time.strftime("%H:%M:%S")
            # draw.text((self.whisplay.LCD_WIDTH // 2, self.whisplay.LCD_HEIGHT // 2), current_time, font=clock_font, fill=(255, 255, 255, 255))
            
            # render header
            self.render_header(image, draw, status, emoji, battery_level, battery_color)
            self.whisplay.draw_image(0, 0, self.whisplay.LCD_WIDTH, header_height, ImageUtils.image_to_rgb565(image, self.whisplay.LCD_WIDTH, header_height))

            # render music progress bar if active
            progress_bar_height = 0
            if current_music_progress is not None:
                progress_bar_height = 22
                pb_image = Image.new("RGBA", (self.whisplay.LCD_WIDTH, progress_bar_height), (0, 0, 0, 255))
                pb_draw = ImageDraw.Draw(pb_image)
                margin = 10
                bar_w = self.whisplay.LCD_WIDTH - 2 * margin
                bar_h = 4
                # time labels above the bar
                elapsed_ms = int((current_music_duration_ms or 0) * min(1.0, max(0.0, current_music_progress)))
                total_ms = current_music_duration_ms or 0
                elapsed_str = "%d:%02d" % (elapsed_ms // 60000, (elapsed_ms % 60000) // 1000)
                total_str = "%d:%02d" % (total_ms // 60000, (total_ms % 60000) // 1000)
                pb_draw.text((margin, 0), elapsed_str, font=self.music_time_font, fill=(180, 180, 180, 255))
                total_bbox = self.music_time_font.getbbox(total_str)
                total_w = total_bbox[2] - total_bbox[0]
                pb_draw.text((margin + bar_w - total_w, 0), total_str, font=self.music_time_font, fill=(180, 180, 180, 255))
                # progress bar below time labels
                bar_y = progress_bar_height - bar_h - 2
                # background track
                pb_draw.rounded_rectangle([margin, bar_y, margin + bar_w, bar_y + bar_h], radius=2, fill=(60, 60, 60, 255))
                # filled portion
                fill_w = max(0, int(bar_w * min(1.0, max(0.0, current_music_progress))))
                if fill_w > 0:
                    pb_draw.rounded_rectangle([margin, bar_y, margin + fill_w, bar_y + bar_h], radius=2, fill=(0, 102, 170, 255))
                self.whisplay.draw_image(0, header_height, self.whisplay.LCD_WIDTH, progress_bar_height, ImageUtils.image_to_rgb565(pb_image, self.whisplay.LCD_WIDTH, progress_bar_height))

            # render main text area
            text_area_height = self.whisplay.LCD_HEIGHT - header_height - progress_bar_height
            text_bg_image = Image.new("RGBA", (self.whisplay.LCD_WIDTH, text_area_height), (0, 0, 0, 255))
            text_draw = ImageDraw.Draw(text_bg_image)
            animation_active = self.render_main_text(text_bg_image, text_area_height, text_draw, text, current_scroll_speed)
            self.whisplay.draw_image(0, header_height + progress_bar_height, self.whisplay.LCD_WIDTH, text_area_height, ImageUtils.image_to_rgb565(text_bg_image, self.whisplay.LCD_WIDTH, text_area_height))

            return animation_active or current_header_mode != "emoji"

        

    def compute_scroll_target_from_char_end(self, lines, line_height, area_height, char_end):
        if char_end is None or char_end <= 0:
            return 0
        total_chars = 0
        target_line = 0
        for i, line in enumerate(lines):
            total_chars += len(line)
            if total_chars >= char_end:
                target_line = i
                break
            if i < len(lines) - 1:
                total_chars += 1
        target_top = target_line * line_height - (area_height // 2)
        return max(0, target_top)

    def render_main_text(self, main_text_image, area_height, draw, text, scroll_speed=2):
        global current_scroll_top, current_scroll_sync_char_end
        global current_scroll_sync_duration_ms, current_scroll_sync_target_top
        global current_scroll_sync_speed, current_scroll_sync_hold_until
        """Render main text content, wrap lines according to screen width, only display currently visible part"""
        if not text:
            self.pending_auto_scroll_after_hold = False
            return False
        # Use main text font
        font = self.main_text_font
        lines = TextUtils.wrap_text(draw, text, font, self.whisplay.LCD_WIDTH - 20)

        # Line height
        line_height = self.main_text_line_height

        max_scroll_top = max(0, (len(lines) + 1) * line_height - area_height)

        if current_scroll_sync_char_end is not None and current_scroll_sync_duration_ms is not None:
            target_top = self.compute_scroll_target_from_char_end(
                lines, line_height, area_height, current_scroll_sync_char_end
            )
            target_top = min(max_scroll_top, target_top)
            target_top = max(current_scroll_top, target_top)
            duration_ms = max(1, current_scroll_sync_duration_ms)
            frames = max(1, int(duration_ms * self.fps / 1000))
            current_scroll_sync_target_top = target_top
            current_scroll_sync_speed = (target_top - current_scroll_top) / frames
            current_scroll_sync_char_end = None
            current_scroll_sync_duration_ms = None

        # Calculate currently visible lines
        display_lines = []
        render_y = 0
        fin_show_lines = False
        for i, line in enumerate(lines):
            if (i + 1) * line_height >= current_scroll_top and i * line_height - current_scroll_top <= area_height:
                display_lines.append((i, line))
                fin_show_lines = True
            elif fin_show_lines is False:
                render_y += line_height
        
        # render_text
        render_text = "\n".join(line for _, line in display_lines)
        render_style = str(current_hat_text_color or "white")
        if self.current_render_text != render_text or self.current_render_style != render_style:
            self.current_render_text = render_text
            self.current_render_style = render_style
            show_text_image = Image.new("RGBA", (self.whisplay.LCD_WIDTH, render_y + len(display_lines) * line_height), (0, 0, 0, 255))
            show_text_draw = ImageDraw.Draw(show_text_image)
            for line_index, line in display_lines:
                TextUtils.draw_mixed_text(
                    show_text_draw,
                    show_text_image,
                    line,
                    font,
                    (10, render_y),
                    fill=self.resolve_text_fill(line_index),
                )
                render_y += line_height
            # Update cache image
            self.text_cache_image = show_text_image
        # Draw text_cache_image to main_text_image
        main_text_image.paste(self.text_cache_image, (0, -int(current_scroll_top)), self.text_cache_image)

        # Update scroll position
        if current_scroll_sync_speed is not None and current_scroll_sync_target_top is not None:
            remaining = current_scroll_sync_target_top - current_scroll_top
            if abs(remaining) <= abs(current_scroll_sync_speed):
                current_scroll_top = current_scroll_sync_target_top
                current_scroll_sync_speed = None
                current_scroll_sync_target_top = None
            else:
                current_scroll_top += current_scroll_sync_speed
        elif (
            scroll_speed > 0
            and current_scroll_top < max_scroll_top
            and time.time() >= current_scroll_sync_hold_until
        ):
            current_scroll_top += scroll_speed
        if current_status == "last reply" and scroll_speed > 0 and max_scroll_top > 0:
            if current_scroll_top >= max_scroll_top:
                current_scroll_top = 0
                current_scroll_sync_speed = None
                current_scroll_sync_target_top = None
                current_scroll_sync_hold_until = time.time() + 0.4
                TextUtils.clean_line_image_cache()
        elif current_scroll_top > max_scroll_top:
            current_scroll_top = max_scroll_top
        self.pending_auto_scroll_after_hold = (
            scroll_speed > 0
            and current_scroll_top < max_scroll_top
            and time.time() < current_scroll_sync_hold_until
        )
        return (
            (
                current_scroll_sync_speed is not None
                and current_scroll_sync_target_top is not None
            )
            or (
                scroll_speed > 0
                and current_scroll_top < max_scroll_top
                and time.time() >= current_scroll_sync_hold_until
            )
        )

    def resolve_text_fill(self, line_index):
        palette_name = str(current_hat_text_color or "white")
        color_map = {
            "white": (255, 255, 255, 255),
            "green": (110, 255, 150, 255),
            "cyan": (120, 235, 255, 255),
            "amber": (255, 210, 110, 255),
            "pink": (255, 150, 220, 255),
            "purple": (205, 160, 255, 255),
            "blue": (135, 180, 255, 255),
        }
        if palette_name == "multi-line":
            palette = [
                (255, 255, 255, 255),
                (110, 255, 150, 255),
                (120, 235, 255, 255),
                (255, 210, 110, 255),
                (255, 150, 220, 255),
                (205, 160, 255, 255),
            ]
            return palette[line_index % len(palette)]
        return color_map.get(palette_name, color_map["white"])

    def request_render(self):
        self.render_event.set()
                
    def should_show_screensaver(self):
        return (
            current_screensaver_mode != "off"
            and current_idle_timeout_sec > 0
            and current_status in IDLE_COMPATIBLE_STATUSES
            and not current_image_path
            and (time.time() - last_activity_at) >= current_idle_timeout_sec
        )

    def should_blank_screen(self):
        return (
            current_screen_blank_timeout_sec > 0
            and current_status in IDLE_COMPATIBLE_STATUSES
            and not current_image_path
            and (time.time() - last_activity_at) >= current_screen_blank_timeout_sec
        )

    def get_screensaver_wait_timeout(self):
        if (
            current_screensaver_mode == "off"
            or current_idle_timeout_sec <= 0
            or current_status not in IDLE_COMPATIBLE_STATUSES
            or current_image_path
        ):
            return None
        remaining = current_idle_timeout_sec - (time.time() - last_activity_at)
        return max(0.0, remaining)

    def get_screen_blank_wait_timeout(self):
        if (
            current_screen_blank_timeout_sec <= 0
            or current_status not in IDLE_COMPATIBLE_STATUSES
            or current_image_path
        ):
            return None
        remaining = current_screen_blank_timeout_sec - (time.time() - last_activity_at)
        return max(0.0, remaining)

    def get_matrix_speed(self, status, screensaver=False):
        if screensaver:
            return 3.2
        speed_map = {
            "idle": 0.8,
            "sleep": 0.8,
            "listening": 1.6,
            "recognizing": 2.0,
            "thinking": 3.4,
            "answering": 2.6,
            "answer": 2.6,
            "settings": 1.3,
            "error": 2.4,
            "music": 1.8,
            "camera": 1.5,
        }
        return speed_map.get(status or "", 1.2)

    def create_rain_columns(self, spacing, charset, area_height):
        columns = []
        for x in range(0, self.whisplay.LCD_WIDTH, spacing):
            seed_count = max(4, int(area_height / 12))
            columns.append({
                "x": x,
                "chars": [
                    {
                        "char": random.choice(charset),
                        "y": random.uniform(-area_height * 0.2, area_height + 6),
                        "brightness": random.randint(70, 190),
                        "speed": random.uniform(1.0, 2.6),
                    }
                    for _ in range(random.randint(max(1, seed_count // 2), seed_count))
                ],
                "spawn_timer": random.randint(0, 10),
                "speed_multiplier": random.uniform(0.8, 1.4),
                "charset": charset,
                "area_height": area_height,
            })
        return columns

    def update_rain_columns(self, columns, spawn_range, speed_range, fade_step=2):
        for col in columns:
            if col["spawn_timer"] <= 0:
                col["chars"].append({
                    "char": random.choice(col["charset"]),
                    "y": -12,
                    "brightness": random.randint(200, 255),
                    "speed": random.uniform(speed_range[0], speed_range[1]) * col["speed_multiplier"],
                })
                col["spawn_timer"] = random.randint(spawn_range[0], spawn_range[1])
            else:
                col["spawn_timer"] -= 1

            survivors = []
            for char in col["chars"]:
                char["y"] += char["speed"]
                char["brightness"] = max(0, char["brightness"] - fade_step)
                if char["y"] <= col["area_height"] + 18 and char["brightness"] > 20:
                    survivors.append(char)
            col["chars"] = survivors

    def draw_rain_columns(self, draw, columns, font, theme, y_offset=0, area_height=None):
        for col in columns:
            if not col["chars"]:
                continue
            head_char = max(col["chars"], key=lambda item: item["y"])
            for char in sorted(col["chars"], key=lambda item: item["y"]):
                y = int(char["y"])
                max_height = area_height if area_height is not None else self.whisplay.LCD_HEIGHT
                if y < -12 or y > max_height:
                    continue
                brightness = char["brightness"]
                is_head = char is head_char
                if theme == "blue":
                    if is_head:
                        color = (255, 255, 255, 255)
                    elif brightness > 180:
                        color = (190, 220, 255, 255)
                    elif brightness > 120:
                        color = (40, 110, min(255, brightness + 70), 255)
                    else:
                        color = (0, 35, max(40, brightness), 255)
                else:
                    if is_head:
                        color = (220, 255, 220, 255) if theme == "binary" else (180, 255, 200, 255)
                    elif brightness > 180:
                        color = (0, 255, 0, 255)
                    elif brightness > 120:
                        color = (0, max(120, brightness), 30, 255)
                    else:
                        color = (0, max(40, brightness // 2), 0, 255)
                draw.text((col["x"], y_offset + y), char["char"], font=font, fill=color)

    def create_neon_streams(self, spacing, area_height):
        streams = []
        for x in range(0, self.whisplay.LCD_WIDTH, spacing):
            seed_count = max(4, int(area_height / 14))
            streams.append({
                "x": x,
                "particles": [
                    {
                        "y": random.uniform(-2, area_height + 4),
                        "brightness": random.randint(120, 255),
                        "speed": random.uniform(0.7, 2.2),
                        "size": 1 if random.random() < 0.85 else 2,
                    }
                    for _ in range(random.randint(max(1, seed_count // 2), seed_count))
                ],
                "spawn_timer": random.randint(0, 4),
                "color_bias": random.choice(["green", "blue", "mixed"]),
                "area_height": area_height,
            })
        return streams

    def update_neon_streams(self, streams, fade_step=2):
        for stream in streams:
            if stream["spawn_timer"] <= 0:
                stream["particles"].append({
                    "y": random.uniform(-4, -1),
                    "brightness": random.randint(210, 255),
                    "speed": random.uniform(0.7, 2.5),
                    "size": 1 if random.random() < 0.8 else 2,
                })
                stream["spawn_timer"] = random.randint(1, 5)
            else:
                stream["spawn_timer"] -= 1

            survivors = []
            for particle in stream["particles"]:
                particle["y"] += particle["speed"]
                particle["brightness"] = max(0, particle["brightness"] - fade_step)
                if particle["y"] <= stream["area_height"] + 6 and particle["brightness"] > 18:
                    survivors.append(particle)
            stream["particles"] = survivors

    def draw_neon_streams(self, image, streams, y_offset=0, area_height=None):
        self.update_neon_streams(streams)
        max_height = area_height if area_height is not None else self.whisplay.LCD_HEIGHT
        for stream in streams:
            for particle in stream["particles"]:
                x = stream["x"]
                y = int(particle["y"])
                if y < 0 or y >= max_height:
                    continue
                fade = particle["brightness"] / 255.0
                if stream["color_bias"] == "blue":
                    base = (0, 140, 255)
                elif stream["color_bias"] == "mixed":
                    base = (0, 255, 180) if random.random() < 0.5 else (0, 140, 255)
                else:
                    base = (0, 255, 90)
                color = (
                    int(base[0] * fade),
                    int(base[1] * fade),
                    int(base[2] * fade),
                    255,
                )
                for dx in range(particle["size"]):
                    for dy in range(particle["size"]):
                        nx = x + dx
                        ny = y_offset + y + dy
                        if 0 <= nx < self.whisplay.LCD_WIDTH and 0 <= ny < self.whisplay.LCD_HEIGHT:
                            image.putpixel((nx, ny), color)

    def create_retro_shape(self, area_height):
        return {
            "type": random.choice(["circle", "rectangle", "triangle", "line", "polygon"]),
            "x": random.randint(0, self.whisplay.LCD_WIDTH),
            "y": random.randint(0, area_height),
            "size": random.randint(6, 22),
            "color": random.choice([
                (255, 0, 255),
                (0, 255, 255),
                (255, 255, 0),
                (255, 128, 0),
                (128, 0, 255),
                (0, 255, 128),
            ]),
            "angle": random.uniform(0, math.pi * 2),
            "speed": random.uniform(0.4, 1.6),
            "rotation_speed": random.uniform(-0.08, 0.08),
            "direction": random.uniform(0, math.pi * 2),
            "life": random.randint(100, 260),
            "max_life": random.randint(100, 260),
            "pulse_speed": random.uniform(0.04, 0.18),
            "width": random.randint(8, 22),
            "height": random.randint(8, 22),
            "area_height": area_height,
        }

    def update_retro_shapes(self, shapes, area_height, target_count):
        shapes[:] = [shape for shape in shapes if shape["life"] > 0]
        while len(shapes) < target_count:
            shapes.append(self.create_retro_shape(area_height))

        now = time.time()
        for shape in shapes:
            shape["x"] += math.cos(shape["direction"]) * shape["speed"]
            shape["y"] += math.sin(shape["direction"]) * shape["speed"]
            shape["angle"] += shape["rotation_speed"]
            if shape["x"] < 0 or shape["x"] > self.whisplay.LCD_WIDTH:
                shape["direction"] = math.pi - shape["direction"]
                shape["x"] = max(0, min(self.whisplay.LCD_WIDTH, shape["x"]))
            if shape["y"] < 0 or shape["y"] > area_height:
                shape["direction"] = -shape["direction"]
                shape["y"] = max(0, min(area_height, shape["y"]))
            pulse = 1.0 + 0.3 * math.sin(now * 4 * shape["pulse_speed"] * 10)
            shape["current_size"] = shape["size"] * pulse
            shape["life"] -= 1

    def draw_retro_geometry(self, draw, shapes, width, area_height, y_offset=0):
        self.update_retro_shapes(shapes, area_height, 8 if area_height > 80 else 4)
        t = time.time()
        if int(t * 2) % 5 == 0:
            for x in range(0, width, 20):
                draw.line([(x, y_offset), (x, y_offset + area_height)], fill=(28, 28, 42, 255), width=1)
            for y in range(0, area_height, 20):
                draw.line([(0, y_offset + y), (width, y_offset + y)], fill=(28, 28, 42, 255), width=1)

        for shape in shapes:
            x = int(shape["x"])
            y = int(shape["y"]) + y_offset
            size = int(shape.get("current_size", shape["size"]))
            age_factor = max(0.2, shape["life"] / shape["max_life"])
            color = tuple(int(channel * age_factor) for channel in shape["color"]) + (255,)
            angle = shape["angle"]
            if shape["type"] == "circle":
                draw.ellipse([x - size // 2, y - size // 2, x + size // 2, y + size // 2], outline=color, width=2)
            elif shape["type"] == "rectangle":
                draw.rectangle([x - shape["width"] // 2, y - shape["height"] // 2, x + shape["width"] // 2, y + shape["height"] // 2], outline=color, width=2)
            elif shape["type"] == "line":
                x1 = x + int(size * math.cos(angle))
                y1 = y + int(size * math.sin(angle))
                x2 = x - int(size * math.cos(angle))
                y2 = y - int(size * math.sin(angle))
                draw.line([(x1, y1), (x2, y2)], fill=color, width=2)
            else:
                sides = 3 if shape["type"] == "triangle" else 5
                points = []
                for i in range(sides):
                    px = x + int(size * math.cos(angle + i * 2 * math.pi / sides))
                    py = y + int(size * math.sin(angle + i * 2 * math.pi / sides))
                    points.append((px, py))
                draw.polygon(points, outline=color, width=2)

    def draw_plasma(self, draw, width, height, y_offset=0):
        t = time.time()
        block_size = 3 if height < 70 else 4
        cx = width / 2
        cy = height / 2
        for y in range(0, height, block_size):
            for x in range(0, width, block_size):
                value = 0
                value += math.sin((x + t * 35) / 16.0)
                value += math.sin((y + t * 28) / 11.0)
                value += math.sin((x + y + t * 24) / 18.0)
                dist = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
                value += math.sin(dist / 11.0 + t * 7.5)
                value = (value + 4) / 8
                r = int(128 + 127 * math.sin(value * math.pi * 2))
                g = int(128 + 127 * math.sin(value * math.pi * 2 + math.pi / 3))
                b = int(128 + 127 * math.sin(value * math.pi * 2 + 2 * math.pi / 3))
                draw.rectangle(
                    [x, y_offset + y, min(width, x + block_size), min(y_offset + height, y_offset + y + block_size)],
                    fill=(r, g, b, 255),
                )

    def create_bouncing_ball(self, area_height):
        radius = random.randint(6, 13)
        return {
            "x": random.uniform(radius + 4, self.whisplay.LCD_WIDTH - radius - 4),
            "y": random.uniform(radius + 4, area_height - radius - 4),
            "vx": random.choice([-1, 1]) * random.uniform(0.8, 2.2),
            "vy": random.choice([-1, 1]) * random.uniform(0.8, 2.2),
            "radius": radius,
            "color": random.choice([
                (255, 0, 255),
                (0, 255, 255),
                (255, 255, 0),
                (0, 255, 90),
                (255, 120, 0),
                (120, 160, 255),
            ]),
            "trail": [],
            "glow_phase": random.uniform(0, math.pi * 2),
        }

    def update_bouncing_balls(self, balls, area_height):
        for ball in balls:
            ball["trail"].append((ball["x"], ball["y"]))
            if len(ball["trail"]) > 10:
                ball["trail"].pop(0)
            ball["x"] += ball["vx"]
            ball["y"] += ball["vy"]
            radius = ball["radius"]
            if ball["x"] - radius <= 0 or ball["x"] + radius >= self.whisplay.LCD_WIDTH:
                ball["vx"] *= -1
                ball["x"] = max(radius, min(self.whisplay.LCD_WIDTH - radius, ball["x"]))
            if ball["y"] - radius <= 0 or ball["y"] + radius >= area_height:
                ball["vy"] *= -1
                ball["y"] = max(radius, min(area_height - radius, ball["y"]))
            ball["glow_phase"] += 0.11

    def draw_bouncing_balls(self, draw, width, area_height, y_offset=0):
        self.update_bouncing_balls(self.bouncing_balls, area_height)
        for ball in self.bouncing_balls:
            trail = ball["trail"]
            for index, (tx, ty) in enumerate(trail):
                fade = (index + 1) / max(1, len(trail))
                r, g, b = ball["color"]
                trail_color = (
                    int(r * fade * 0.28),
                    int(g * fade * 0.28),
                    int(b * fade * 0.28),
                    255,
                )
                size = max(2, int(ball["radius"] * fade * 0.55))
                draw.ellipse(
                    [
                        int(tx) - size,
                        y_offset + int(ty) - size,
                        int(tx) + size,
                        y_offset + int(ty) + size,
                    ],
                    fill=trail_color,
                )
        for ball in self.bouncing_balls:
            x = int(ball["x"])
            y = y_offset + int(ball["y"])
            radius = ball["radius"]
            glow = 0.5 + 0.5 * math.sin(ball["glow_phase"])
            glow_radius = radius + 3
            r, g, b = ball["color"]
            glow_color = (
                int(r * 0.18 * glow),
                int(g * 0.18 * glow),
                int(b * 0.18 * glow),
                255,
            )
            draw.ellipse(
                [x - glow_radius, y - glow_radius, x + glow_radius, y + glow_radius],
                outline=glow_color,
                width=2,
            )
            draw.ellipse(
                [x - radius, y - radius, x + radius, y + radius],
                fill=ball["color"] + (255,),
                outline=(255, 255, 255, 255),
                width=1,
            )
            highlight = max(2, radius // 3)
            draw.ellipse(
                [x - radius // 3 - highlight, y - radius // 3 - highlight, x - radius // 3 + highlight, y - radius // 3 + highlight],
                fill=(255, 255, 255, 120),
            )

    def create_kaleido_element(self, area_height):
        max_distance = math.hypot(self.whisplay.LCD_WIDTH, area_height) * 0.65
        return {
            "angle": random.uniform(0, math.pi / 8),
            "distance": random.uniform(16, max_distance),
            "color": random.choice([
                (255, 0, 140),
                (128, 0, 255),
                (0, 180, 255),
                (0, 255, 200),
                (255, 220, 0),
                (255, 120, 0),
            ]),
            "size": random.randint(5, 16),
            "shape": random.choice(["circle", "diamond", "square", "star"]),
            "rotation": random.uniform(0, math.pi * 2),
            "rotation_speed": random.uniform(-0.08, 0.08),
            "pulse_phase": random.uniform(0, math.pi * 2),
            "pulse_speed": random.uniform(0.04, 0.12),
            "life": random.randint(260, 540),
        }

    def update_kaleidoscope(self, area_height):
        max_distance = math.hypot(self.whisplay.LCD_WIDTH, area_height) * 0.72
        self.kaleido_rotation += 0.018
        self.kaleido_elements[:] = [item for item in self.kaleido_elements if item["life"] > 0]
        while len(self.kaleido_elements) < 14:
            self.kaleido_elements.append(self.create_kaleido_element(area_height))
        for item in self.kaleido_elements:
            item["rotation"] += item["rotation_speed"]
            item["pulse_phase"] += item["pulse_speed"]
            item["distance"] += random.uniform(-0.6, 0.6)
            item["distance"] = max(12, min(max_distance, item["distance"]))
            item["angle"] += random.uniform(-0.01, 0.01)
            item["life"] -= 1

    def draw_kaleido_shape(self, draw, x, y, size, shape, color, rotation):
        if shape == "circle":
            draw.ellipse([x - size, y - size, x + size, y + size], fill=color)
            return
        if shape == "diamond":
            points = [(x, y - size), (x + size, y), (x, y + size), (x - size, y)]
            draw.polygon(points, fill=color)
            return
        if shape == "square":
            draw.rectangle([x - size, y - size, x + size, y + size], fill=color)
            return
        points = []
        for index in range(8):
            angle = rotation + index * (math.pi / 4)
            radius = size if index % 2 == 0 else size * 0.45
            points.append((int(x + math.cos(angle) * radius), int(y + math.sin(angle) * radius)))
        draw.polygon(points, fill=color)

    def draw_kaleidoscope(self, draw, width, area_height, y_offset=0):
        self.update_kaleidoscope(area_height)
        center_x = width / 2.0
        center_y = y_offset + area_height / 2.0
        segments = 8
        segment_angle = (math.pi * 2.0) / segments
        for item in self.kaleido_elements:
            pulse = 1.0 + 0.25 * math.sin(item["pulse_phase"])
            size = max(3, int(item["size"] * pulse))
            for seg_index in range(segments):
                for direction in (1, -1):
                    angle = self.kaleido_rotation + (seg_index * segment_angle) + (item["angle"] * direction)
                    x = int(center_x + math.cos(angle) * item["distance"])
                    y = int(center_y + math.sin(angle) * item["distance"])
                    self.draw_kaleido_shape(
                        draw,
                        x,
                        y,
                        size,
                        item["shape"],
                        item["color"] + (220,),
                        item["rotation"],
                    )

    def create_tetris_piece(self, area_height):
        block = 14
        shapes = [
            [(0, 1), (1, 1), (2, 1), (3, 1)],
            [(0, 0), (0, 1), (1, 1), (2, 1)],
            [(2, 0), (0, 1), (1, 1), (2, 1)],
            [(1, 0), (2, 0), (1, 1), (2, 1)],
            [(1, 0), (2, 0), (0, 1), (1, 1)],
            [(1, 0), (0, 1), (1, 1), (2, 1)],
            [(0, 0), (1, 0), (1, 1), (2, 1)],
        ]
        return {
            "blocks": random.choice(shapes),
            "x": random.randint(-1, max(1, (self.whisplay.LCD_WIDTH // block) - 4)),
            "y": random.uniform(-8, -2),
            "speed": random.uniform(0.18, 0.46),
            "drift_phase": random.uniform(0, math.pi * 2),
            "color": random.choice([
                (0, 255, 255),
                (255, 255, 0),
                (180, 0, 255),
                (0, 255, 90),
                (255, 110, 0),
                (0, 120, 255),
                (255, 0, 120),
            ]),
        }

    def update_tetris_rain(self, area_height):
        if self.tetris_spawn_timer <= 0:
            self.tetris_pieces.append(self.create_tetris_piece(area_height))
            self.tetris_spawn_timer = random.randint(6, 16)
        else:
            self.tetris_spawn_timer -= 1
        survivors = []
        for piece in self.tetris_pieces:
            piece["y"] += piece["speed"]
            piece["drift_phase"] += 0.04
            max_y = max(block_y for _, block_y in piece["blocks"])
            if (piece["y"] + max_y) * 14 < area_height + 24:
                survivors.append(piece)
        self.tetris_pieces = survivors[-18:]

    def draw_tetris_rain(self, draw, width, area_height, y_offset=0):
        self.update_tetris_rain(area_height)
        block = 14
        for piece in self.tetris_pieces:
            x_offset = math.sin(piece["drift_phase"]) * 4.0
            for block_x, block_y in piece["blocks"]:
                px = int((piece["x"] + block_x) * block + x_offset)
                py = y_offset + int((piece["y"] + block_y) * block)
                if py > y_offset + area_height or px > width or px + block < 0:
                    continue
                fill = piece["color"] + (255,)
                edge = (255, 255, 255, 180)
                draw.rounded_rectangle(
                    [px, py, px + block - 2, py + block - 2],
                    radius=2,
                    fill=fill,
                    outline=edge,
                    width=1,
                )
                draw.line(
                    [(px + 2, py + 2), (px + block - 5, py + 2)],
                    fill=(255, 255, 255, 120),
                    width=1,
                )

    def resolve_active_screensaver_mode(self):
        if current_screensaver_mode != "random-shift":
            return current_screensaver_mode
        now = time.time()
        if (
            self.random_screensaver_mode not in RANDOM_SCREENSAVER_MODES
            or (now - self.random_screensaver_started_at) >= RANDOM_SCREENSAVER_INTERVAL_SEC
        ):
            choices = [mode for mode in RANDOM_SCREENSAVER_MODES if mode != self.random_screensaver_mode]
            self.random_screensaver_mode = random.choice(choices or list(RANDOM_SCREENSAVER_MODES))
            self.random_screensaver_started_at = now
        return self.random_screensaver_mode

    def render_visual_mode(self, mode, image, draw, width, height, status, y_offset=0, header=False):
        if mode == "matrix":
            columns = self.header_green_matrix_columns if header else self.green_matrix_columns
            font = self.header_matrix_font if header else self.screensaver_matrix_font
            self.update_rain_columns(columns, (2, 8) if header else (1, 4), (1.0, 3.0) if header else (1.4, 3.8), 2 if header else 1)
            self.draw_rain_columns(draw, columns, font, "green", y_offset, height)
        elif mode == "matrix-binary":
            columns = self.header_binary_matrix_columns if header else self.binary_matrix_columns
            font = self.header_matrix_font if header else self.screensaver_matrix_font
            self.update_rain_columns(columns, (1, 6) if header else (1, 4), (1.0, 2.8) if header else (1.3, 3.4), 2 if header else 1)
            self.draw_rain_columns(draw, columns, font, "binary", y_offset, height)
        elif mode == "matrix-blue":
            columns = self.header_blue_matrix_columns if header else self.blue_matrix_columns
            font = self.header_matrix_font if header else self.screensaver_matrix_font
            self.update_rain_columns(columns, (2, 8) if header else (1, 4), (0.9, 2.6) if header else (1.2, 3.2), 2 if header else 1)
            self.draw_rain_columns(draw, columns, font, "blue", y_offset, height)
        elif mode == "retro-geometry":
            shapes = self.header_retro_shapes if header else self.retro_shapes
            self.draw_retro_geometry(draw, shapes, width, height, y_offset)
        elif mode == "plasma":
            self.draw_plasma(draw, width, height, y_offset)
        elif mode == "neon-rain":
            streams = self.header_neon_streams if header else self.neon_streams
            self.draw_neon_streams(image, streams, y_offset, height)
        elif not header and mode == "bouncing-balls":
            self.draw_bouncing_balls(draw, width, height, y_offset)
        elif not header and mode == "kaleidoscope":
            self.draw_kaleidoscope(draw, width, height, y_offset)
        elif not header and mode == "tetris-rain":
            self.draw_tetris_rain(draw, width, height, y_offset)
        elif mode == "vu-bars":
            self.draw_vu_bars(draw, width, height, y_offset)
        elif mode == "vu-scope":
            self.draw_vu_scope(draw, width, height, y_offset)
        elif mode == "vu-wave":
            self.draw_vu_wave(draw, width, height, y_offset)
        else:
            self.draw_matrix_region(
                draw,
                width,
                height,
                self.header_matrix_font if header else self.screensaver_matrix_font,
                self.get_matrix_speed(status, screensaver=True),
                y_offset,
            )

    def get_header_vu_level(self):
        global current_audio_level
        try:
            target_level = max(0.0, min(1.0, float(current_audio_level) / 100.0))
        except (TypeError, ValueError):
            target_level = 0.0
        if target_level >= self.header_vu_level:
            self.header_vu_level = (self.header_vu_level * 0.35) + (target_level * 0.65)
        else:
            self.header_vu_level = (self.header_vu_level * 0.82) + (target_level * 0.18)
        return max(0.0, min(1.0, self.header_vu_level))

    def draw_vu_bars(self, draw, width, height, y_offset):
        level = self.get_header_vu_level()
        bar_count = 10
        margin_x = 10
        gap = 4
        usable_width = max(10, width - (margin_x * 2))
        bar_width = max(6, (usable_width - (gap * (bar_count - 1))) // bar_count)
        for index in range(bar_count):
            x0 = margin_x + index * (bar_width + gap)
            x1 = x0 + bar_width
            fill_ratio = max(0.0, min(1.0, (level * bar_count) - index))
            draw.rounded_rectangle((x0, y_offset, x1, y_offset + height), radius=2, outline=(20, 40, 30, 220), fill=(0, 0, 0, 0))
            if fill_ratio <= 0:
                continue
            lit_height = max(3, int(fill_ratio * (height - 2)))
            y0 = y_offset + height - lit_height
            if index >= 8:
                color = (255, 96, 96, 235)
            elif index >= 6:
                color = (255, 210, 90, 230)
            else:
                color = (64, 255, 140, 225)
            draw.rounded_rectangle((x0 + 1, y0, x1 - 1, y_offset + height - 1), radius=2, fill=color)

    def draw_vu_scope(self, draw, width, height, y_offset):
        level = self.get_header_vu_level()
        self.header_vu_phase += 0.6 + level * 0.45
        sample = math.sin(self.header_vu_phase) * (0.25 + level * 0.75)
        self.header_vu_history.append(sample)
        history_length = max(20, min(width // 4, 60))
        self.header_vu_history = self.header_vu_history[-history_length:]
        center_y = y_offset + (height / 2.0)
        amplitude = max(4.0, (height / 2.0 - 4.0) * (0.3 + level * 0.7))
        draw.line((0, center_y, width, center_y), fill=(28, 58, 44, 180), width=1)
        points = []
        for index, sample_value in enumerate(self.header_vu_history):
            x = int(index * (width - 1) / max(1, history_length - 1))
            y = center_y - (sample_value * amplitude)
            points.append((x, y))
        if len(points) > 1:
            draw.line(points, fill=(80, 255, 160, 235), width=2)
            last_x, last_y = points[-1]
            draw.ellipse((last_x - 2, last_y - 2, last_x + 2, last_y + 2), fill=(220, 255, 235, 240))

    def draw_vu_wave(self, draw, width, height, y_offset):
        level = self.get_header_vu_level()
        self.header_vu_phase += 0.22 + level * 0.18
        center_y = y_offset + (height / 2.0)
        amplitude = max(3.0, (height / 2.0 - 4.0) * (0.2 + level * 0.8))
        points = []
        for x in range(width):
            phase = self.header_vu_phase + (x / max(1, width - 1)) * (math.pi * 2.0)
            harmonic = math.sin((phase * 2.0) + (level * 3.0)) * 0.22
            y = center_y - ((math.sin(phase) + harmonic) * amplitude)
            points.append((x, y))
        draw.line((0, center_y, width, center_y), fill=(34, 28, 62, 160), width=1)
        draw.line(points, fill=(110, 170, 255, 235), width=2)

    def render_screensaver_frame(self, image, draw, width, height, status):
        active_mode = self.resolve_active_screensaver_mode()
        self.render_visual_mode(active_mode, image, draw, width, height, status)

    def draw_matrix_region(self, draw, width, height, font, speed, y_offset):
        charset = "01ABCDEF"
        line_height = max(font.getbbox("A")[3] - font.getbbox("A")[1] + 1, font.size)
        column_step = max(9, font.size - 2)
        tail_base = 4 if y_offset > 0 else 8
        now = time.time()
        for x in range(0, width + column_step, column_step):
            column = x // column_step
            rng = random.Random((column * 9973) + 17)
            tail_length = tail_base + rng.randint(0, 4)
            offset = rng.randint(0, height + tail_length * line_height)
            head_y = int((now * speed * line_height * 1.5 + offset) % (height + tail_length * line_height)) - tail_length * line_height
            for tail_index in range(tail_length):
                y = head_y - (tail_index * line_height)
                if y < 0 or y >= height:
                    continue
                alpha = max(60, 255 - tail_index * 38)
                green = max(80, 255 - tail_index * 28)
                fill = (150, 255, 180, alpha) if tail_index == 0 else (0, green, 80, alpha)
                char_index = int(now * 10 + column * 7 + tail_index * 11) % len(charset)
                draw.text((x, y_offset + y), charset[char_index], font=font, fill=fill)

    def render_header(self, image, draw, status, emoji, battery_level, battery_color):
        global current_status, current_emoji, current_battery_level, current_battery_color
        global status_font_size, emoji_font_size, battery_font_size
        
        status_font = self.status_font
        emoji_font = self.emoji_font
        battery_font = self.battery_font

        image_width = self.whisplay.LCD_WIDTH

        ascent_status, _ = status_font.getmetrics()
        ascent_emoji, _ = emoji_font.getmetrics()

        top_height = status_font_size + emoji_font_size + 20

        # Draw status centered
        status_bbox = status_font.getbbox(current_status)
        status_w = status_bbox[2] - status_bbox[0]
        TextUtils.draw_mixed_text(draw, image, current_status, status_font, (whisplay.CornerHeight, 0))

        header_body_y = status_font_size + 8
        if current_header_mode != "emoji":
            self.render_visual_mode(
                current_header_mode,
                image,
                draw,
                image_width,
                self.header_effect_height,
                current_status,
                header_body_y,
                True,
            )
        else:
            emoji_bbox = emoji_font.getbbox(current_emoji)
            emoji_w = emoji_bbox[2] - emoji_bbox[0]
            TextUtils.draw_mixed_text(draw, image, current_emoji, emoji_font, ((image_width - emoji_w) // 2, header_body_y))
        
        # Draw battery icon
        status_icon_context = {
            "battery_level": battery_level,
            "battery_color": battery_color,
            "battery_font": battery_font,
            "status_font_size": status_font_size,
            "network_connected": current_network_connected,
            "wifi_signal_level": current_wifi_signal_level,
            "groq_requests_today": current_groq_requests_today,
            "vpn_connected": current_vpn_connected,
            "rag_icon_visible": current_rag_icon_visible,
            "image_icon_visible": current_image_icon_visible,
        }
        status_icons = self.build_status_icons(status_icon_context)
        self.render_status_icons(draw, status_icons, image_width)
        
        return top_height

    def build_status_icons(self, context):
        icons = []
        battery_level = context.get("battery_level")
        battery_color = context.get("battery_color")
        battery_font = context.get("battery_font")
        status_font_size = context.get("status_font_size")

        if battery_level is not None:
            icons.append(BatteryStatusIcon(battery_level, battery_color, battery_font, status_font_size))
        if context.get("wifi_signal_level"):
            icons.append(WifiStatusIcon(status_font_size, context.get("wifi_signal_level")))
        if context.get("groq_requests_today") is not None:
            icons.append(
                RequestCountStatusIcon(
                    context.get("groq_requests_today"),
                    battery_font,
                    status_font_size,
                )
            )
        if context.get("vpn_connected"):
            icons.append(WireguardStatusIcon(status_font_size))
        if context.get("image_icon_visible"):
            icons.append(ImageStatusIcon(status_font_size))
        if context.get("rag_icon_visible"):
            icons.append(RagStatusIcon(status_font_size))

        for item in sorted(status_icon_factories, key=lambda entry: entry["priority"]):
            icon_list = item["factory"](context)
            if icon_list:
                icons.extend(icon_list)
        return icons

    def render_status_icons(self, draw, icons, image_width):
        if not icons:
            return
        right_margin = 10
        icon_gap = 8
        cursor_x = image_width - right_margin
        for icon in icons:
            icon_width, _ = icon.measure()
            icon_x = cursor_x - icon_width
            icon_y = icon.get_top_y()
            icon.render(draw, icon_x, icon_y)
            cursor_x = icon_x - icon_gap

    def run(self):
        frame_interval = 1 / self.fps
        while self.running:
            animation_active = self.render_frame(current_status, current_emoji, current_text, current_scroll_top, current_battery_level, current_battery_color)
            if animation_active:
                time.sleep(frame_interval)
                continue

            wait_timeout = None
            if self.pending_auto_scroll_after_hold:
                wait_timeout = max(0.0, current_scroll_sync_hold_until - time.time())
            screensaver_wait_timeout = self.get_screensaver_wait_timeout()
            if screensaver_wait_timeout is not None:
                wait_timeout = (
                    screensaver_wait_timeout
                    if wait_timeout is None
                    else min(wait_timeout, screensaver_wait_timeout)
                )
            screen_blank_wait_timeout = self.get_screen_blank_wait_timeout()
            if screen_blank_wait_timeout is not None:
                wait_timeout = (
                    screen_blank_wait_timeout
                    if wait_timeout is None
                    else min(wait_timeout, screen_blank_wait_timeout)
                )
            self.render_event.wait(wait_timeout)
            self.render_event.clear()
            
    def stop(self):
        self.running = False
        self.render_event.set()

def update_display_data(status=None, emoji=None, text=None,
                   scroll_speed=None, scroll_speed_factor=None, scroll_sync=None, battery_level=None, battery_color=None, image_path=None,
                   network_connected=None, vpn_connected=None, rag_icon_visible=None, image_icon_visible=None, transaction_id=None,
                   wifi_signal_level=None, groq_requests_today=None, audio_level=None,
                   music_progress=None, music_duration_ms=None, header_mode=None,
                   screensaver_mode=None, idle_timeout_sec=None, screen_blank_timeout_sec=None,
                   hat_text_color=None):
    global current_status, current_emoji, current_text, current_battery_level
    global current_battery_color, current_scroll_top, current_scroll_speed, current_image_path
    global current_scroll_speed_factor
    global current_scroll_sync_char_end, current_scroll_sync_duration_ms
    global current_scroll_sync_target_top, current_scroll_sync_speed
    global current_scroll_sync_hold_until
    global current_network_connected, current_vpn_connected, current_rag_icon_visible, current_image_icon_visible, current_transaction_id
    global current_wifi_signal_level
    global current_groq_requests_today
    global current_music_progress, current_music_duration_ms
    global current_audio_level
    global current_header_mode, current_screensaver_mode, current_idle_timeout_sec, current_screen_blank_timeout_sec
    global current_hat_text_color
    global render_thread

    if (
        status is not None
        or emoji is not None
        or text is not None
        or image_path is not None
        or scroll_sync is not None
        or transaction_id is not None
        or music_progress is not None
        or music_duration_ms is not None
        or header_mode is not None
        or screensaver_mode is not None
        or idle_timeout_sec is not None
        or screen_blank_timeout_sec is not None
        or hat_text_color is not None
    ):
        note_activity()

    next_text = text
    if text is not None:
        previous_text = current_text or ""
        incoming_text = text or ""
        same_transaction = (
            transaction_id is not None
            and current_transaction_id is not None
            and transaction_id == current_transaction_id
        )
        regressive_update = (
            len(incoming_text) > 0
            and len(incoming_text) < len(previous_text)
            and previous_text.startswith(incoming_text)
        )
        if same_transaction and regressive_update:
            next_text = previous_text
        elif (
            transaction_id is not None
            and current_transaction_id is not None
            and transaction_id != current_transaction_id
        ):
            current_scroll_top = 0
            current_scroll_sync_char_end = None
            current_scroll_sync_duration_ms = None
            current_scroll_sync_target_top = None
            current_scroll_sync_speed = None
            TextUtils.clean_line_image_cache()
        elif not incoming_text.startswith(previous_text):
            if not previous_text.startswith(incoming_text):
                current_scroll_top = 0
                current_scroll_sync_char_end = None
                current_scroll_sync_duration_ms = None
                current_scroll_sync_target_top = None
                current_scroll_sync_speed = None
                TextUtils.clean_line_image_cache()
    if scroll_sync is not None:
        try:
            char_end = scroll_sync.get("char_end", None)
            duration_ms = scroll_sync.get("duration_ms", None)
            if char_end is not None and duration_ms is not None:
                current_scroll_sync_char_end = int(char_end)
                current_scroll_sync_duration_ms = int(duration_ms)
                hold_seconds = max(0.3, (current_scroll_sync_duration_ms / 1000.0) + 0.2)
                current_scroll_sync_hold_until = max(
                    current_scroll_sync_hold_until,
                    time.time() + hold_seconds,
                )
        except Exception as e:
            print(f"[Display] Invalid scroll_sync payload: {e}")
    if scroll_speed_factor is not None:
        try:
            current_scroll_speed_factor = normalize_scroll_speed_factor(scroll_speed_factor)
        except (TypeError, ValueError):
            print(f"[Display] Invalid scroll_speed_factor payload: {scroll_speed_factor}")
    if scroll_speed is not None:
        try:
            current_scroll_speed = to_device_scroll_speed(
                scroll_speed,
                current_scroll_speed_factor,
            )
        except (TypeError, ValueError):
            print(f"[Display] Invalid scroll_speed payload: {scroll_speed}")
    if network_connected is not None:
        current_network_connected = network_connected
    if wifi_signal_level is not None:
        try:
            current_wifi_signal_level = max(0, min(3, int(wifi_signal_level)))
        except (TypeError, ValueError):
            print(f"[Display] Invalid wifi_signal_level payload: {wifi_signal_level}")
    if groq_requests_today is not None:
        try:
            current_groq_requests_today = max(0, int(groq_requests_today))
        except (TypeError, ValueError):
            print(f"[Display] Invalid groq_requests_today payload: {groq_requests_today}")
    if audio_level is not None:
        try:
            current_audio_level = max(0, min(100, int(audio_level)))
        except (TypeError, ValueError):
            print(f"[Display] Invalid audio_level payload: {audio_level}")
    if vpn_connected is not None:
        current_vpn_connected = vpn_connected
    if rag_icon_visible is not None:
        current_rag_icon_visible = rag_icon_visible
    if image_icon_visible is not None:
        current_image_icon_visible = image_icon_visible
    if transaction_id is not None:
        current_transaction_id = transaction_id
    current_status = status if status is not None else current_status
    current_emoji = emoji if emoji is not None else current_emoji
    current_text = next_text if text is not None else current_text
    current_battery_level = battery_level if battery_level is not None else current_battery_level
    current_battery_color = battery_color if battery_color is not None else current_battery_color
    current_image_path = image_path if image_path is not None else current_image_path
    if music_progress is not None:
        current_music_progress = music_progress if music_progress >= 0 else None
    if music_duration_ms is not None:
        current_music_duration_ms = music_duration_ms if music_duration_ms > 0 else None
    if header_mode is not None:
        current_header_mode = str(header_mode)
    if screensaver_mode is not None:
        current_screensaver_mode = str(screensaver_mode)
    if idle_timeout_sec is not None:
        try:
            current_idle_timeout_sec = max(0, int(idle_timeout_sec))
        except (TypeError, ValueError):
            print(f"[Display] Invalid idle_timeout_sec payload: {idle_timeout_sec}")
    if screen_blank_timeout_sec is not None:
        try:
            current_screen_blank_timeout_sec = max(0, int(screen_blank_timeout_sec))
        except (TypeError, ValueError):
            print(f"[Display] Invalid screen_blank_timeout_sec payload: {screen_blank_timeout_sec}")
    if hat_text_color is not None:
        current_hat_text_color = str(hat_text_color)
    if render_thread is not None:
        render_thread.request_render()


def send_to_all_clients(message):
    """Send message to all connected clients"""
    message_json = json.dumps(message).encode("utf-8") + b"\n"
    for addr, client_socket in clients.items():
        try:
            client_socket.sendall(message_json)
            # Use ellipsis for long messages
            if len(message_json) > 100:
                display_message = message_json[:50] + b"..." + message_json[-50:]
            else:
                display_message = message_json
            print(f"[Server] Sent notification to client {addr}: {display_message}")
        except Exception as e:
            print(f"[Server] Failed to send notification to client {addr}: {e}")

def exit_camera_mode():
    global camera_mode, camera_thread, render_thread
    print("[Camera] Exiting camera mode...")
    note_activity()
    if camera_thread is not None:
        camera_thread.stop()
        camera_thread = None
    notification = {"event": "exit_camera_mode"}
    send_to_all_clients(notification)
    camera_mode = False
    if render_thread is not None:
        render_thread.request_render()

def on_button_pressed():
    """Function executed when button is pressed"""
    print("[Server] Button pressed")
    note_activity()
    notification = {"event": "button_pressed"}
    send_to_all_clients(notification)

def on_button_release():
    """Function executed when button is released"""
    print("[Server] Button released")
    note_activity()
    notification = {"event": "button_released"}
    send_to_all_clients(notification)

def handle_client(client_socket, addr, whisplay):
    global camera_capture_image_path, camera_mode, camera_thread, render_thread
    print(f"[Socket] Client {addr} connected")
    clients[addr] = client_socket
    try:
        buffer = ""
        while True:
            data = client_socket.recv(4096).decode("utf-8")
            if not data:
                break
            buffer += data
            
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                if not line.strip():
                    continue
                        
                # print(f"[Socket - {addr}] Received data: {line}")
                try:
                    content = json.loads(line)
                    transaction_id = content.get("transaction_id", None)
                    status = content.get("status", None)
                    emoji = content.get("emoji", None)
                    text = content.get("text", None)
                    rgbled = content.get("RGB", None)
                    brightness = content.get("brightness", None)
                    scroll_speed = content.get("scroll_speed", None)
                    scroll_speed_factor = content.get("scroll_speed_factor", None)
                    scroll_sync = content.get("scroll_sync", None)
                    response_to_client = content.get("response", None)
                    battery_level = content.get("battery_level", None)
                    battery_color = content.get("battery_color", None)
                    image_path = content.get("image", None)
                    network_connected = content.get("network_connected", None)
                    wifi_signal_level = content.get("wifi_signal_level", None)
                    audio_level = content.get("audio_level", None)
                    groq_requests_today = content.get("groq_requests_today", None)
                    vpn_connected = content.get("vpn_connected", None)
                    rag_icon_visible = content.get("rag_icon_visible", None)
                    image_icon_visible = content.get("image_icon_visible", None)
                    music_progress = content.get("music_progress", None)
                    music_duration_ms = content.get("music_duration_ms", None)
                    header_mode = content.get("header_mode", None)
                    screensaver_mode = content.get("screensaver_mode", None)
                    idle_timeout_sec = content.get("idle_timeout_sec", None)
                    screen_blank_timeout_sec = content.get("screen_blank_timeout_sec", None)
                    hat_text_color = content.get("hat_text_color", None)
                    capture_image_path = content.get("capture_image_path", None)
                    trigger_camera_capture = content.get("camera_capture", None)
                    # boolean to enable camera mode
                    set_camera_mode = content.get("camera_mode", None)

                    if rgbled:
                        rgb255_tuple = ColorUtils.get_rgb255_from_any(rgbled)
                        whisplay.set_rgb_fade(*rgb255_tuple, duration_ms=500)
                    
                    if battery_color:
                        battery_tuple = ColorUtils.get_rgb255_from_any(battery_color)
                    else:
                        battery_tuple = None
                        
                    if brightness:
                        whisplay.set_backlight(brightness)
                        
                    if capture_image_path is not None:
                        camera_capture_image_path = capture_image_path
                    
                    if set_camera_mode is not None:
                        if set_camera_mode:
                            print("[Camera] Entering camera mode...")
                            camera_mode = True
                            camera_thread = CameraThread(whisplay, camera_capture_image_path)
                            camera_thread.start()
                        else:
                            print("[Camera] Exiting camera mode...")
                            if camera_thread is not None:
                                camera_thread.stop()
                                camera_thread = None
                            camera_mode = False
                        if render_thread is not None:
                            render_thread.request_render()

                    if trigger_camera_capture:
                        print("[Camera] Capturing image by command...")
                        if camera_thread is not None:
                            camera_thread.capture()
                            notification = {"event": "camera_capture"}
                            send_to_all_clients(notification)

                    if (text is not None) or (status is not None) or (emoji is not None) or \
                       (battery_level is not None) or (battery_color is not None) or \
                              (image_path is not None) or (network_connected is not None) or \
                               (wifi_signal_level is not None) or \
                               (groq_requests_today is not None) or \
                               (audio_level is not None) or \
                               (vpn_connected is not None) or \
                              (rag_icon_visible is not None) or (image_icon_visible is not None) or (scroll_sync is not None) or \
                               (music_progress is not None) or (music_duration_ms is not None) or \
                               (header_mode is not None) or (screensaver_mode is not None) or (idle_timeout_sec is not None) or \
                               (screen_blank_timeout_sec is not None) or (hat_text_color is not None):
                        update_display_data(status=status, emoji=emoji,
                                     text=text, scroll_speed=scroll_speed, scroll_speed_factor=scroll_speed_factor, scroll_sync=scroll_sync,
                                     battery_level=battery_level, battery_color=battery_tuple,
                                                    image_path=image_path, network_connected=network_connected,
                                                    wifi_signal_level=wifi_signal_level,
                                                    groq_requests_today=groq_requests_today,
                                                    audio_level=audio_level,
                                        vpn_connected=vpn_connected,
                                                  rag_icon_visible=rag_icon_visible,
                                          image_icon_visible=image_icon_visible,
                                                  transaction_id=transaction_id,
                                                  music_progress=music_progress,
                                                   music_duration_ms=music_duration_ms,
                                                    header_mode=header_mode,
                                                    screensaver_mode=screensaver_mode,
                                                    idle_timeout_sec=idle_timeout_sec,
                                                    screen_blank_timeout_sec=screen_blank_timeout_sec,
                                                    hat_text_color=hat_text_color)

                    client_socket.send(b"OK\n")
                    if response_to_client:
                        try:
                            response_bytes = json.dumps({"response": response_to_client}).encode("utf-8") + b"\n"
                            client_socket.send(response_bytes)
                            print(f"[Socket - {addr}] Sent response: {response_to_client}")
                        except Exception as e:
                            print(f"[Socket - {addr}] Response sending error: {e}")
                            
                except json.JSONDecodeError:
                    client_socket.send(b"ERROR: invalid JSON\n")
                except Exception as e:
                    print(f"[Socket - {addr}] Data processing error: {e}")
                    client_socket.send(f"ERROR: {e}\n".encode("utf-8"))

    except Exception as e:
        print(f"[Socket - {addr}] Connection error: {e}")
    finally:
        print(f"[Socket] Client {addr} disconnected")
        del clients[addr]
        client_socket.close()

def start_socket_server(render_thread, host='0.0.0.0', port=12345):
    # Register button events
    whisplay.on_button_press(on_button_pressed)
    whisplay.on_button_release(on_button_release)

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((host, port))
    server_socket.listen(5)  # Allow more connections
    print(f"[Socket] Listening on {host}:{port} ...")

    try:
        while True:
            client_socket, addr = server_socket.accept()
            client_thread = threading.Thread(target=handle_client, 
                                           args=(client_socket, addr, whisplay))
            client_thread.daemon = True
            client_thread.start()
    except KeyboardInterrupt:
        print("[Socket] Server stopped")
    finally:
        render_thread.stop()
        server_socket.close()


if __name__ == "__main__":
    whisplay = WhisplayBoard()
    print(f"[LCD] Initialization finished: {whisplay.LCD_WIDTH}x{whisplay.LCD_HEIGHT}")
    
    # read CUSTOM_FONT_PATH from environment variable
    custom_font_path = os.getenv("CUSTOM_FONT_PATH", None)
    
    # start render thread
    render_thread = RenderThread(whisplay, resolve_font_path(custom_font_path), fps=30)
    render_thread.start()
    start_socket_server(render_thread, host='0.0.0.0', port=12345)
    
    def cleanup_and_exit(signum, frame):
        print("[System] Exiting...")
        render_thread.stop()
        whisplay.cleanup()
        sys.exit(0)
        
    signal.signal(signal.SIGTERM, cleanup_and_exit)
    signal.signal(signal.SIGINT, cleanup_and_exit)
    signal.signal(signal.SIGKILL, cleanup_and_exit)
    signal.signal(signal.SIGQUIT, cleanup_and_exit)
    signal.signal(signal.SIGSTOP, cleanup_and_exit)
    try:
        # Keep the main thread alive
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        cleanup_and_exit(None, None)
    
