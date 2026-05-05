import argparse
import json
import os
from typing import Callable

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps


EFFECT_CHOICES = [
    "retro",
    "comic",
    "sketch",
    "pixelate",
    "halftone",
    "edge",
    "spooky",
    "dreamy",
    "warm",
    "cyberpunk",
    "glitch",
    "vhs",
    "auto-contrast",
    "colors-pop",
]


def _to_array(image: Image.Image) -> np.ndarray:
    return np.asarray(image.convert("RGB"), dtype=np.uint8)


def _from_array(array: np.ndarray) -> Image.Image:
    clipped = np.clip(array, 0, 255).astype(np.uint8)
    return Image.fromarray(clipped, mode="RGB")


def _blend_tint(image: Image.Image, color: tuple[int, int, int], strength: float) -> Image.Image:
    overlay = Image.new("RGB", image.size, color)
    return Image.blend(image, overlay, max(0.0, min(1.0, strength)))


def _apply_vignette(image: Image.Image, strength: float = 0.4) -> Image.Image:
    width, height = image.size
    y_indices, x_indices = np.indices((height, width))
    x_norm = (x_indices - width / 2) / max(1.0, width / 2)
    y_norm = (y_indices - height / 2) / max(1.0, height / 2)
    distance = np.sqrt(x_norm ** 2 + y_norm ** 2)
    mask = 1.0 - np.clip((distance - 0.15) / 0.85, 0.0, 1.0) * strength
    data = _to_array(image).astype(np.float32)
    data *= mask[:, :, None]
    return _from_array(data)


def _add_grain(image: Image.Image, amount: float = 0.05) -> Image.Image:
    data = _to_array(image).astype(np.float32)
    noise = np.random.normal(0, 255 * amount, data.shape)
    return _from_array(data + noise)


def _shift_channel(array: np.ndarray, channel_index: int, dx: int, dy: int) -> np.ndarray:
    shifted = np.roll(array[:, :, channel_index], dy, axis=0)
    shifted = np.roll(shifted, dx, axis=1)
    result = array.copy()
    result[:, :, channel_index] = shifted
    return result


def _effect_retro(image: Image.Image) -> Image.Image:
    toned = ImageOps.autocontrast(image)
    toned = ImageEnhance.Color(toned).enhance(0.7)
    sepia = np.array(
        [
            [0.393, 0.769, 0.189],
            [0.349, 0.686, 0.168],
            [0.272, 0.534, 0.131],
        ]
    )
    data = _to_array(toned).astype(np.float32)
    sepia_data = data @ sepia.T
    result = _from_array(sepia_data)
    result = ImageEnhance.Contrast(result).enhance(1.08)
    result = _apply_vignette(result, 0.42)
    return _add_grain(result, 0.035)


def _effect_comic(image: Image.Image) -> Image.Image:
    color = image.filter(ImageFilter.SMOOTH_MORE)
    color = ImageOps.posterize(color, 3)
    color = ImageEnhance.Color(color).enhance(1.35)
    edges = ImageOps.grayscale(image).filter(ImageFilter.FIND_EDGES)
    edges = ImageOps.autocontrast(edges)
    edges = edges.point(lambda value: 255 if value > 45 else 0)
    edges = ImageOps.invert(edges).convert("RGB")
    result = Image.blend(color, edges, 0.28)
    result = ImageEnhance.Color(result).enhance(1.35)
    return ImageEnhance.Contrast(result).enhance(1.2)


def _effect_sketch(image: Image.Image) -> Image.Image:
    gray = ImageOps.grayscale(image)
    inverted = ImageOps.invert(gray)
    blurred = inverted.filter(ImageFilter.GaussianBlur(radius=12))
    base = np.asarray(gray, dtype=np.float32)
    dodge = np.asarray(blurred, dtype=np.float32)
    sketched = np.clip((base * 255) / np.maximum(1, 255 - dodge), 0, 255).astype(np.uint8)
    result = Image.fromarray(sketched, mode="L").convert("RGB")
    return ImageEnhance.Contrast(result).enhance(1.1)


def _effect_pixelate(image: Image.Image) -> Image.Image:
    width, height = image.size
    scaled_w = max(16, width // 18)
    scaled_h = max(16, height // 18)
    return image.resize((scaled_w, scaled_h), Image.Resampling.BILINEAR).resize(
        (width, height),
        Image.Resampling.NEAREST,
    )


def _effect_halftone(image: Image.Image) -> Image.Image:
    gray = ImageOps.grayscale(image)
    width, height = gray.size
    step = 8
    canvas = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(canvas)
    pixels = np.asarray(gray, dtype=np.uint8)
    for y in range(0, height, step):
        for x in range(0, width, step):
            block = pixels[y:min(y + step, height), x:min(x + step, width)]
            value = float(np.mean(block)) / 255.0
            radius = max(0.8, (1.0 - value) * (step / 2))
            cx = x + step / 2
            cy = y + step / 2
            draw.ellipse(
                [cx - radius, cy - radius, cx + radius, cy + radius],
                fill="black",
            )
    return canvas


def _effect_edge(image: Image.Image) -> Image.Image:
    edges = ImageOps.grayscale(image).filter(ImageFilter.FIND_EDGES)
    edges = ImageOps.autocontrast(edges)
    return ImageOps.invert(edges).convert("RGB")


def _effect_spooky(image: Image.Image) -> Image.Image:
    result = ImageEnhance.Color(image).enhance(0.65)
    result = ImageEnhance.Contrast(result).enhance(1.18)
    result = _blend_tint(result, (40, 90, 110), 0.22)
    result = _apply_vignette(result, 0.52)
    return _add_grain(result, 0.025)


def _effect_dreamy(image: Image.Image) -> Image.Image:
    blurred = image.filter(ImageFilter.GaussianBlur(radius=10))
    result = Image.blend(image, blurred, 0.38)
    result = _blend_tint(result, (255, 210, 235), 0.12)
    result = ImageEnhance.Brightness(result).enhance(1.05)
    return ImageEnhance.Color(result).enhance(1.1)


def _effect_warm(image: Image.Image) -> Image.Image:
    result = ImageOps.autocontrast(image)
    result = _blend_tint(result, (255, 184, 120), 0.18)
    result = ImageEnhance.Color(result).enhance(1.12)
    result = ImageEnhance.Contrast(result).enhance(1.06)
    return _add_grain(result, 0.018)


def _effect_cyberpunk(image: Image.Image) -> Image.Image:
    result = ImageEnhance.Contrast(image).enhance(1.22)
    result = ImageEnhance.Color(result).enhance(1.35)
    data = _to_array(result).astype(np.float32)
    data[:, :, 0] *= 1.18
    data[:, :, 1] *= 0.92
    data[:, :, 2] *= 1.22
    result = _from_array(data)
    return _blend_tint(result, (25, 0, 55), 0.08)


def _effect_glitch(image: Image.Image) -> Image.Image:
    data = _to_array(image)
    data = _shift_channel(data, 0, 6, 0)
    data = _shift_channel(data, 2, -6, 0)
    height, width, _ = data.shape
    output = data.copy()
    for y in range(0, height, 28):
        if (y // 28) % 2 == 0:
            shift = int(((y / max(1, height)) * 11) % 9) - 4
            output[y:min(y + 6, height), :, :] = np.roll(
                output[y:min(y + 6, height), :, :],
                shift,
                axis=1,
            )
    output[::3, :, :] = np.clip(output[::3, :, :] * 0.88, 0, 255)
    return _from_array(output)


def _effect_vhs(image: Image.Image) -> Image.Image:
    result = image.filter(ImageFilter.GaussianBlur(radius=1.2))
    result = _blend_tint(result, (195, 205, 255), 0.05)
    data = _to_array(result)
    data = _shift_channel(data, 0, 3, 0)
    data[::4, :, :] = np.clip(data[::4, :, :] * 0.82, 0, 255)
    result = _from_array(data)
    return _add_grain(result, 0.045)


def _effect_auto_contrast(image: Image.Image) -> Image.Image:
    return ImageOps.autocontrast(image)


def _effect_colors_pop(image: Image.Image) -> Image.Image:
    result = ImageEnhance.Color(image).enhance(1.55)
    result = ImageEnhance.Contrast(result).enhance(1.12)
    return ImageEnhance.Sharpness(result).enhance(1.18)


EFFECT_HANDLERS: dict[str, Callable[[Image.Image], Image.Image]] = {
    "retro": _effect_retro,
    "comic": _effect_comic,
    "sketch": _effect_sketch,
    "pixelate": _effect_pixelate,
    "halftone": _effect_halftone,
    "edge": _effect_edge,
    "spooky": _effect_spooky,
    "dreamy": _effect_dreamy,
    "warm": _effect_warm,
    "cyberpunk": _effect_cyberpunk,
    "glitch": _effect_glitch,
    "vhs": _effect_vhs,
    "auto-contrast": _effect_auto_contrast,
    "colors-pop": _effect_colors_pop,
}


def apply_effect(input_path: str, output_path: str, effect: str) -> str:
    if effect not in EFFECT_HANDLERS:
        raise ValueError(f"Unsupported image effect: {effect}")
    if not os.path.exists(input_path):
        raise FileNotFoundError(f"Input image does not exist: {input_path}")

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    source = Image.open(input_path).convert("RGB")
    result = EFFECT_HANDLERS[effect](source)
    result.save(output_path)
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Apply deterministic local image effects.")
    parser.add_argument("--input", required=True, help="Input image path")
    parser.add_argument("--output", required=True, help="Output image path")
    parser.add_argument("--effect", required=True, choices=EFFECT_CHOICES, help="Effect to apply")
    args = parser.parse_args()

    try:
        output_path = apply_effect(args.input, args.output, args.effect)
        print(json.dumps({"ok": True, "output": output_path, "effect": args.effect}))
        return 0
    except Exception as error:
        print(json.dumps({"ok": False, "error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
