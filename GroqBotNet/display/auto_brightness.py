from __future__ import annotations

import json
import math
import os
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps, ImageStat


def get_mean_luma(image: Image.Image) -> float:
    return ImageStat.Stat(image.convert("L")).mean[0]


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def choose_target(mean_luma: float) -> float:
    if mean_luma < 18:
        return 82.0
    if mean_luma < 35:
        return 92.0
    if mean_luma > 210:
        return 176.0
    if mean_luma > 185:
        return 168.0
    return mean_luma


def summarize(before: float, after: float, changed: bool) -> str:
    if not changed:
        return "auto brightness: no adjustment"
    delta = after - before
    direction = "brightened" if delta >= 0 else "dimmed"
    return f"auto brightness: {direction} ({before:.1f} -> {after:.1f})"


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: auto_brightness.py <image-path>", file=sys.stderr)
        return 1

    image_path = Path(sys.argv[1]).resolve()
    if not image_path.is_file():
        print(f"Image not found: {image_path}", file=sys.stderr)
        return 1

    with Image.open(image_path) as source:
        image = source.convert("RGB")

    before_luma = get_mean_luma(image)
    target_luma = choose_target(before_luma)
    changed = not math.isclose(target_luma, before_luma, rel_tol=0.02, abs_tol=4.0)

    if changed:
        working = ImageOps.autocontrast(image, cutoff=0.5)
        auto_luma = max(1.0, get_mean_luma(working))
        brightness_factor = clamp(target_luma / auto_luma, 0.72, 2.35)
        contrast_factor = 1.02 if brightness_factor >= 1.0 else 0.96
        working = ImageEnhance.Brightness(working).enhance(brightness_factor)
        working = ImageEnhance.Contrast(working).enhance(contrast_factor)
    else:
        working = image

    after_luma = get_mean_luma(working)
    suffix = image_path.suffix or ".jpg"
    with tempfile.NamedTemporaryFile(
        delete=False,
        dir=str(image_path.parent),
        prefix=f"{image_path.stem}-autobrightness-",
        suffix=suffix,
    ) as handle:
        temp_path = Path(handle.name)

    try:
        save_kwargs = {}
        if image_path.suffix.lower() in {".jpg", ".jpeg"}:
            save_kwargs = {"quality": 90}
        working.save(temp_path, **save_kwargs)
        os.replace(temp_path, image_path)
    finally:
        if temp_path.exists():
            temp_path.unlink(missing_ok=True)

    print(
        json.dumps(
            {
                "ok": True,
                "changed": changed,
                "before_luma": round(before_luma, 2),
                "after_luma": round(after_luma, 2),
                "target_luma": round(target_luma, 2),
                "summary": summarize(before_luma, after_luma, changed),
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
