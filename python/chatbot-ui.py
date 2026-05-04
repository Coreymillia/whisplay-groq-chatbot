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
from rag_icon import RagStatusIcon
from image_icon import ImageStatusIcon
from wireguard_icon import WireguardStatusIcon

scroll_thread = None
scroll_stop_event = threading.Event()

status_font_size=20
emoji_font_size=40
battery_font_size=13
IDLE_RENDER_INTERVAL = 0.5

# Global variables
current_status = "Hello"
current_emoji = "😄"
current_text = "Waiting for message..."
current_battery_level = 100
current_battery_color = ColorUtils.get_rgb255_from_any("#55FF00")
current_scroll_top = 0
DEFAULT_SCROLL_SPEED = 0.25
MAX_SCROLL_SPEED = 0.5
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
current_vpn_connected = False
current_rag_icon_visible = False
current_image_icon_visible = False
current_music_progress = None
current_music_duration_ms = None
current_header_mode = "emoji"
current_screensaver_mode = "off"
current_idle_timeout_sec = 120
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
    normalized_speed = (base_speed / 3.0) * DEFAULT_SCROLL_SPEED
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
                display_lines.append(line)
                fin_show_lines = True
            elif fin_show_lines is False:
                render_y += line_height
        
        # render_text
        render_text = "\n".join(display_lines)
        if self.current_render_text != render_text:
            self.current_render_text = render_text
            show_text_image = Image.new("RGBA", (self.whisplay.LCD_WIDTH, render_y + len(display_lines) * line_height), (0, 0, 0, 255))
            show_text_draw = ImageDraw.Draw(show_text_image)
            for line in display_lines:
                TextUtils.draw_mixed_text(show_text_draw, show_text_image, line, font, (10, render_y))
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
        if current_scroll_top > max_scroll_top:
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

    def request_render(self):
        self.render_event.set()
                
    def should_show_screensaver(self):
        return (
            current_screensaver_mode != "off"
            and current_idle_timeout_sec > 0
            and current_status == "idle"
            and not current_image_path
            and (time.time() - last_activity_at) >= current_idle_timeout_sec
        )

    def get_screensaver_wait_timeout(self):
        if (
            current_screensaver_mode == "off"
            or current_idle_timeout_sec <= 0
            or current_status != "idle"
            or current_image_path
        ):
            return None
        remaining = current_idle_timeout_sec - (time.time() - last_activity_at)
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
        else:
            self.draw_matrix_region(
                draw,
                width,
                height,
                self.header_matrix_font if header else self.screensaver_matrix_font,
                self.get_matrix_speed(status, screensaver=True),
                y_offset,
            )

    def render_screensaver_frame(self, image, draw, width, height, status):
        self.render_visual_mode(current_screensaver_mode, image, draw, width, height, status)

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
            self.render_event.wait(wait_timeout)
            self.render_event.clear()
            
    def stop(self):
        self.running = False
        self.render_event.set()

def update_display_data(status=None, emoji=None, text=None,
                   scroll_speed=None, scroll_speed_factor=None, scroll_sync=None, battery_level=None, battery_color=None, image_path=None,
                   network_connected=None, vpn_connected=None, rag_icon_visible=None, image_icon_visible=None, transaction_id=None,
                   wifi_signal_level=None,
                   music_progress=None, music_duration_ms=None, header_mode=None,
                   screensaver_mode=None, idle_timeout_sec=None):
    global current_status, current_emoji, current_text, current_battery_level
    global current_battery_color, current_scroll_top, current_scroll_speed, current_image_path
    global current_scroll_speed_factor
    global current_scroll_sync_char_end, current_scroll_sync_duration_ms
    global current_scroll_sync_target_top, current_scroll_sync_speed
    global current_scroll_sync_hold_until
    global current_network_connected, current_vpn_connected, current_rag_icon_visible, current_image_icon_visible, current_transaction_id
    global current_wifi_signal_level
    global current_music_progress, current_music_duration_ms
    global current_header_mode, current_screensaver_mode, current_idle_timeout_sec
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
                    vpn_connected = content.get("vpn_connected", None)
                    rag_icon_visible = content.get("rag_icon_visible", None)
                    image_icon_visible = content.get("image_icon_visible", None)
                    music_progress = content.get("music_progress", None)
                    music_duration_ms = content.get("music_duration_ms", None)
                    header_mode = content.get("header_mode", None)
                    screensaver_mode = content.get("screensaver_mode", None)
                    idle_timeout_sec = content.get("idle_timeout_sec", None)
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
                             (vpn_connected is not None) or \
                             (rag_icon_visible is not None) or (image_icon_visible is not None) or (scroll_sync is not None) or \
                             (music_progress is not None) or (music_duration_ms is not None) or \
                             (header_mode is not None) or (screensaver_mode is not None) or (idle_timeout_sec is not None):
                        update_display_data(status=status, emoji=emoji,
                                     text=text, scroll_speed=scroll_speed, scroll_speed_factor=scroll_speed_factor, scroll_sync=scroll_sync,
                                     battery_level=battery_level, battery_color=battery_tuple,
                                                  image_path=image_path, network_connected=network_connected,
                                                  wifi_signal_level=wifi_signal_level,
                                      vpn_connected=vpn_connected,
                                                  rag_icon_visible=rag_icon_visible,
                                          image_icon_visible=image_icon_visible,
                                                  transaction_id=transaction_id,
                                                  music_progress=music_progress,
                                                  music_duration_ms=music_duration_ms,
                                                  header_mode=header_mode,
                                                  screensaver_mode=screensaver_mode,
                                                  idle_timeout_sec=idle_timeout_sec)

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
    
