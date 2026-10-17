"""Bakes the original's TextMeshPro pixel fonts into bitmap atlases for the port.

Each font is rendered once per glyph at its px per em without antialiasing; pixel fonts at their
native sizes come out as in the game:

  dialogue       NPC dialogue: TinyUnicode (light text) over PixelUnicodeFill (a dark backing behind
                 each letter), 1 em = 1 world unit = 16 px (NPC.prefab: "Dialogue" and "Dialogue BG
                 Font").
  menu           the menus' text: TinyUnicode (a port choice: the original's PixelTitanZero /
                 PixelUnicode read worse on the 3DS), drawn at the menu's 2x like its sprites, in
                 the color each use needs (game/ui/menu_skin; baked light).

For each <name>:
  assets/sheets/font_<name>.png   the atlas (converted for the 3DS like the exported sheets; .fmt: la8)
  assets/fonts/<name>.json        {"sheet", "ppem", "descent" (px below the baseline: TMP puts the
                                   last line's descender on the box bottom), "glyphs": [{"c": code
                                   point, "adv": advance px,
                                   "text"/"fill": [x, y, w, h, ox, oy]}]}: the glyph's rect in the
                                   atlas (top-left origin) and its top-left relative to the pen on
                                   the baseline (px, y down); "fill" (the backing, if the font has
                                   one) is drawn first, at the same pen.

Usage: python tools/make_font.py [path to Slider/Assets/Fonts/originalFonts]   (needs Pillow)
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    REPO, "..", "Slider", "Slider", "Assets", "Fonts", "originalFonts")
CHARS = [chr(c) for c in range(32, 127)]  # the port's text is ASCII (dialogue: once ' and ... are replaced)
LIGHT = (230, 230, 230, 255)  # Dialogue m_fontColor 0.902
DARK = (26, 26, 26, 255)      # Dialogue BG Font m_fontColor 0.102
ATLAS_W = 128

# name: (ppem, (ttf, color) of the letters, (ttf, color) of the backing or None)
FONTS = {
    "dialogue": (16, ("TinyUnicode.ttf", LIGHT), ("PixelUnicodeFill.ttf", DARK)),
    "menu": (16, ("TinyUnicode.ttf", LIGHT), None),
}


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


def bake(name, ppem, text, fill):
    text_font = ImageFont.truetype(os.path.join(FONT_DIR, text[0]), ppem)
    layers = [("text", text_font, text[1])]
    fill_font = None
    if fill:
        fill_font = ImageFont.truetype(os.path.join(FONT_DIR, fill[0]), ppem)
        layers.append(("fill", fill_font, fill[1]))

    pieces = []  # (glyph index, key, image, ox, oy)
    glyphs = []
    for ch in CHARS:
        adv = text_font.getlength(ch)
        if adv != int(adv):
            sys.exit(f"{name} {ch!r}: advance {adv} is not whole pixels")
        g = {"c": ord(ch), "adv": int(adv)}
        glyphs.append(g)
        for key, font, color in layers:
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

    sheet = "font_" + name
    atlas.save(os.path.join(REPO, "assets", "sheets", sheet + ".png"))
    with open(os.path.join(REPO, "assets", "sheets", sheet + ".fmt"), "w", newline="\n") as f:
        f.write("la8\n")  # gray glyphs: lossless as luminance + alpha (see the Makefile)
    os.makedirs(os.path.join(REPO, "assets", "fonts"), exist_ok=True)
    with open(os.path.join(REPO, "assets", "fonts", name + ".json"), "w", newline="\n") as f:
        f.write('{\n  "sheet": "%s",\n  "ppem": %d,\n  "descent": %d,\n  "glyphs": [\n'
                % (sheet, ppem, text_font.getmetrics()[1]))
        f.write(",\n".join("    " + json.dumps(g, separators=(",", ":")) for g in glyphs))
        f.write("\n  ]\n}\n")
    print(f"{name}: {len(glyphs)} glyphs, atlas {ATLAS_W}x{height}")
    if fill_font:
        # The original lays the backing out as its own TMP text (its own advances, wordSpacing +5.6
        # vs the letters' -10); the port draws each backing glyph at its letter's pen instead, which
        # is what that spacing approximates. Report where the two fonts' advances differ.
        diff = [(ch, int(text_font.getlength(ch)), int(fill_font.getlength(ch))) for ch in CHARS
                if text_font.getlength(ch) != fill_font.getlength(ch)]
        if diff:
            print("  advances differ (text vs fill; backings follow the text's): " +
                  ", ".join(f"{ch!r} {a} vs {b}" for ch, a, b in diff))


def main():
    for name, (ppem, text, fill) in FONTS.items():
        bake(name, ppem, text, fill)


if __name__ == "__main__":
    main()
