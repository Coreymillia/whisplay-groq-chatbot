#!/usr/bin/env python3
import argparse
import sys
from io import BytesIO

from PIL import Image


def render_companion_image(input_path: str, width: int, height: int, quality: int) -> bytes:
    with Image.open(input_path) as source:
        source = source.convert("RGBA")
        source.thumbnail((width, height), Image.LANCZOS)

        canvas = Image.new("RGBA", (width, height), (0, 0, 0, 255))
        offset_x = (width - source.width) // 2
        offset_y = (height - source.height) // 2
        canvas.alpha_composite(source, (offset_x, offset_y))

        output = BytesIO()
        canvas.convert("RGB").save(output, format="JPEG", quality=quality, optimize=True)
        return output.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description="Render a fitted companion JPEG.")
    parser.add_argument("--input", required=True, help="Source image path")
    parser.add_argument("--width", required=True, type=int, help="Output width")
    parser.add_argument("--height", required=True, type=int, help="Output height")
    parser.add_argument("--quality", type=int, default=85, help="JPEG quality")
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        raise ValueError("Width and height must be positive.")

    sys.stdout.buffer.write(
        render_companion_image(args.input, args.width, args.height, args.quality)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
