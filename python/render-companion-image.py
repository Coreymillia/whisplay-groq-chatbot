#!/usr/bin/env python3
import argparse
import sys
from io import BytesIO

from PIL import Image, ImageOps


def render_companion_image(input_path: str, width: int, height: int, quality: int) -> bytes:
    with Image.open(input_path) as source:
        source = source.convert("RGBA")
        canvas = ImageOps.fit(
            source,
            (width, height),
            method=Image.LANCZOS,
            centering=(0.5, 0.5),
        )

        output = BytesIO()
        canvas.convert("RGB").save(
            output,
            format="JPEG",
            quality=quality,
            optimize=False,
            progressive=False,
            subsampling=2,
        )
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
