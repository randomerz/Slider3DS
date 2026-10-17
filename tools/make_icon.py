"""Makes the 3DS app icon (slider3ds.png, 48x48; the Makefile embeds it in the SMDH) from the
original's SliderCat128 (Slider/production/misc: 16x16 pixel art at 8x).

The art is reduced to its 16x16 pixels, scaled 3x (48 px, whole pixels) and laid on the menus'
dark gray (#1a1a1a): SMDH icons are RGB565, with no transparency.

Usage: python tools/make_icon.py [path to SliderCat128.png]   (needs Pillow)
"""
import os
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    REPO, "..", "Slider", "production", "misc", "SliderCat128.png")
ART = 16           # the source's pixel art size
SIZE = 48          # the SMDH large icon
BACKGROUND = (26, 26, 26, 255)


def main():
    src = Image.open(SOURCE).convert("RGBA")
    k = src.width // ART
    if src.size != (ART * k, ART * k):
        sys.exit(f"{SOURCE}: expected {ART}x{ART} pixel art at a whole scale, got {src.size}")
    art = Image.new("RGBA", (ART, ART))
    for y in range(ART):
        for x in range(ART):
            art.putpixel((x, y), src.getpixel((x * k + k // 2, y * k + k // 2)))  # each block's middle
    icon = Image.new("RGBA", (SIZE, SIZE), BACKGROUND)
    icon.alpha_composite(art.resize((SIZE, SIZE), Image.NEAREST))
    out = os.path.join(REPO, "slider3ds.png")
    icon.convert("RGB").save(out)
    print(out)


if __name__ == "__main__":
    main()
