#!/usr/bin/env python3
"""
Convert an image to RGB888 C header (uint8_t array, R,G,B order).

Usage:
  py -3 png_to_rgb888.py input.png -o image_rgb888.h
  py -3 png_to_rgb888.py input.png -o image_rgb888.h --width 480 --height 320
  py -3 png_to_rgb888.py input.png -o image_rgb888.h --letterbox
"""

import argparse
import os
from PIL import Image


def convert(image_path: str, out_path: str, w: int, h: int, letterbox: bool) -> None:
    if not os.path.exists(image_path):
        raise FileNotFoundError(f"Input image not found: {image_path}")

    img = Image.open(image_path).convert("RGB")

    if letterbox:
        src_w, src_h = img.size
        scale = min(w / src_w, h / src_h)
        new_w = max(1, int(round(src_w * scale)))
        new_h = max(1, int(round(src_h * scale)))
        resized = img.resize((new_w, new_h), Image.LANCZOS)

        canvas = Image.new("RGB", (w, h), (0, 0, 0))
        x0 = (w - new_w) // 2
        y0 = (h - new_h) // 2
        canvas.paste(resized, (x0, y0))
        img = canvas
    else:
        img = img.resize((w, h), Image.LANCZOS)

    # Fast: already in R,G,B byte order
    rgb_bytes = img.tobytes()  # length = w*h*3

    if len(rgb_bytes) != w * h * 3:
        raise RuntimeError(f"Unexpected byte count: {len(rgb_bytes)} vs expected {w*h*3}")

    mn = min(rgb_bytes) if rgb_bytes else 0
    mx = max(rgb_bytes) if rgb_bytes else 0

    print(f"Loaded: {image_path}")
    print(f"Output size: {w}x{h}, bytes: {len(rgb_bytes)} (expected {w*h*3})")
    print(f"Byte min/max: {mn}/{mx}")

    var_name = os.path.splitext(os.path.basename(out_path))[0]
    var_name = "".join(c if (c.isalnum() or c == "_") else "_" for c in var_name)

    with open(out_path, "w", newline="\n") as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define IMG_WIDTH  ({w})\n")
        f.write(f"#define IMG_HEIGHT ({h})\n")
        f.write(f"#define IMG_BPP    (3)\n\n")
        f.write("// RGB888 byte order: R, G, B for each pixel\n")
        f.write(f"static const uint8_t {var_name}[IMG_WIDTH*IMG_HEIGHT*IMG_BPP] = {{\n")

        # 12 bytes per line
        for i, val in enumerate(rgb_bytes):
            if i % 12 == 0:
                f.write("    ")
            f.write(f"0x{val:02X}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")

        if len(rgb_bytes) % 12 != 0:
            f.write("\n")

        f.write("};\n")

    print(f"Wrote: {out_path} (array name: {var_name})")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="Input image (PNG/JPG/etc)")
    ap.add_argument("-o", "--output", default="image_rgb888.h", help="Output .h file")
    ap.add_argument("--width", type=int, default=480, help="Output width")
    ap.add_argument("--height", type=int, default=320, help="Output height")
    ap.add_argument("--letterbox", action="store_true", help="Preserve aspect ratio with black bars")
    args = ap.parse_args()

    convert(args.input, args.output, args.width, args.height, args.letterbox)


if __name__ == "__main__":
    main()