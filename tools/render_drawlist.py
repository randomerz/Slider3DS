"""Render a recorded draw list (build-pc/test-out/<scene>.txt, written by the render goldens on a
mismatch) to a PNG using the sheet images in assets/sheets, to see what a golden scene looks like.

    python tools/render_drawlist.py build-pc/test-out/artifact_selected.txt [out.png] [--size 320x240] [--scale 3]

Each line: sheet sx sy sw sh x y flags [xN]  (xN = scale; an empty sheet name = a rect, whose flags
is its RGBA color).
"""
import argparse
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLIP_X, FLIP_Y, ROT90 = 1, 2, 4


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("drawlist")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--size", default="400x240")
    ap.add_argument("--scale", type=int, default=3)
    args = ap.parse_args()
    w, h = (int(v) for v in args.size.split("x"))
    canvas = Image.new("RGBA", (w, h), (255, 0, 255, 255))
    sheets = {}
    with open(args.drawlist) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 7:  # rect: empty sheet name
                sx, sy, sw, sh, x, y, color = parts
                c = int(color)
                rect = Image.new("RGBA", (int(sw), int(sh)), (c >> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255))
                canvas.alpha_composite(rect, (round(float(x)), round(float(y))))
                continue
            scale = int(parts.pop()[1:]) if parts[-1].startswith("x") else 1
            name, sx, sy, sw, sh, x, y, flags = parts
            if name not in sheets:
                sheets[name] = Image.open(os.path.join(ROOT, "assets", "sheets", name + ".png")).convert("RGBA")
            sx, sy, sw, sh, flags = int(sx), int(sy), int(sw), int(sh), int(flags)
            img = sheets[name].crop((sx, sy, sx + sw, sy + sh))
            if scale != 1:
                img = img.resize((sw * scale, sh * scale), Image.NEAREST)
            if flags & ROT90:
                img = img.transpose(Image.Transpose.ROTATE_90)
            if flags & FLIP_X:
                img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if flags & FLIP_Y:
                img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            px, py = round(float(x)), round(float(y))
            if px >= w or py >= h or px + img.width <= 0 or py + img.height <= 0:
                continue
            layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            layer.paste(img, (px, py))
            canvas.alpha_composite(layer)
    out = args.out or os.path.splitext(args.drawlist)[0] + ".png"
    canvas.resize((w * args.scale, h * args.scale), Image.NEAREST).save(out)
    print(out)


if __name__ == "__main__":
    main()
