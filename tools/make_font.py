"""Bakes the NPC dialogue font into a bitmap atlas for the port.

The original types dialogue with TextMeshPro: TinyUnicode (light text) over PixelUnicodeFill (a dark
backing behind each letter), both pixel fonts at 1 em = 1 world unit = 16 px (NPC.prefab: "Dialogue"
and "Dialogue BG Font"). Rendered at 16 px per em without antialiasing they come out exactly as
in the game, so this renders each glyph of both once, in the prefab's colors:

  assets/sheets/font_dialogue.png   the atlas (converted for the 3DS like the exported sheets)
  assets/fonts/dialogue.json        {"sheet", "ppem", "descent" (px below the baseline: TMP puts the
                                     last line's descender on the box bottom), "glyphs": [{"c": code
                                     point, "adv": advance px,
                                     "text"/"fill": [x, y, w, h, ox, oy]}]}: the glyph's rect in the
                                     atlas (top-left origin) and its top-left relative to the pen on
                                     the baseline (px, y down); "fill" is drawn first, at the same pen.

Usage: python tools/make_font.py [path to Slider/Assets/Fonts/originalFonts]   (needs Pillow)
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    REPO, "..", "Slider", "Slider", "Assets", "Fonts", "originalFonts")
PPEM = 16
CHARS = [chr(c) for c in range(32, 127)]  # the Village's dialogue is ASCII once ' and ... are replaced
TEXT_COLOR = (230, 230, 230, 255)  # NPC.prefab Dialogue m_fontColor 0.902
FILL_COLOR = (26, 26, 26, 255)     # Dialogue BG Font m_fontColor 0.102
ATLAS_W = 128


def render(font, ch, color):
    """The glyph's pixels (cropped) and its top-left relative to the pen on the baseline."""
    ascent, _ = font.getmetrics()
    left, top, right, bottom = font.getbbox(ch)
    if right <= left or bottom <= top:
        return None, 0, 0
    w, h = right - left, bottom - top
    mask = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(mask)
    d.fontmode = "1"  # no antialiasing: pixel fonts at their native size
    d.text((-left, -top), ch, font=font, fill=255)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    img.paste(Image.new("RGBA", (w, h), color), (0, 0), mask)
    return img, left, top - ascent


def main():
    text_font = ImageFont.truetype(os.path.join(FONT_DIR, "TinyUnicode.ttf"), PPEM)
    fill_font = ImageFont.truetype(os.path.join(FONT_DIR, "PixelUnicodeFill.ttf"), PPEM)

    pieces = []  # (glyph index, key, image, ox, oy)
    glyphs = []
    for ch in CHARS:
        adv = text_font.getlength(ch)
        if adv != int(adv):
            sys.exit(f"{ch!r}: advance {adv} is not whole pixels")
        g = {"c": ord(ch), "adv": int(adv)}
        glyphs.append(g)
        for key, font, color in (("text", text_font, TEXT_COLOR), ("fill", fill_font, FILL_COLOR)):
            img, ox, oy = render(font, ch, color)
            if img is not None:
                pieces.append((len(glyphs) - 1, key, img, ox, oy))

    # Shelf packing, tallest first; 1 px gaps so filtering never bleeds (draws are point-sampled anyway).
    pieces.sort(key=lambda p: (-p[2].height, glyphs[p[0]]["c"], p[1]))
    x = y = shelf = 0
    placed = []
    for gi, key, img, ox, oy in pieces:
        if x + img.width > ATLAS_W:
            x, y, shelf = 0, y + shelf + 1, 0
        placed.append((gi, key, img, x, y, ox, oy))
        x += img.width + 1
        shelf = max(shelf, img.height)
    height = 8
    while height < y + shelf:
        height *= 2
    atlas = Image.new("RGBA", (ATLAS_W, height), (0, 0, 0, 0))
    for gi, key, img, px, py, ox, oy in placed:
        atlas.paste(img, (px, py))
        glyphs[gi][key] = [px, py, img.width, img.height, ox, oy]

    atlas.save(os.path.join(REPO, "assets", "sheets", "font_dialogue.png"))
    os.makedirs(os.path.join(REPO, "assets", "fonts"), exist_ok=True)
    with open(os.path.join(REPO, "assets", "fonts", "dialogue.json"), "w", newline="\n") as f:
        f.write('{\n  "sheet": "font_dialogue",\n  "ppem": %d,\n  "descent": %d,\n  "glyphs": [\n'
                % (PPEM, text_font.getmetrics()[1]))
        f.write(",\n".join("    " + json.dumps(g, separators=(",", ":")) for g in glyphs))
        f.write("\n  ]\n}\n")
    print(f"{len(glyphs)} glyphs, atlas {ATLAS_W}x{height}")


if __name__ == "__main__":
    main()
