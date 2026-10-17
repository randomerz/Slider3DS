"""Cuts the original's menu UI sprites into a sheet for the port's pause menu (game/ui/menu_skin).

The sprites, as the prefabs use them (UICanvas PausePanel, GeneralOptionsPanel):
  Menu Button (the main menu's; Image sliced, Button sprite swap): normal Button24_0 (a light face
      over a 2 px shaded bottom edge), pressed _1 (the face 1 px lower); 9-sliced, 4 px borders.
      Selected: a port sprite (user's design), Button24_0 between two brackets in the dark gray of
      the original's outlined selected _2: 2 px thick, 1 px outside the button's sides, the full
      height (past the shaded edge), their ends turning inward. It's 2 px wider than Button24_0;
      the skin draws it 1 px out on each side.
  UI Toggle: background ui_toggle_0, selected background ui_toggle_0_selected, checkmark ui_toggle_1.
  UI Slider: handle ui_slider_1 (the Handle Image's m_Sprite), selected handle ui_slider_0 (the
      Slider's m_SpriteState). Names mislead: resolve fileIDs through the .meta's internalIDs.

  assets/sheets/ui_menu.png   the sheet (.fmt: la8, all gray)
  assets/ui/menu.json         {"sheet", "sprites": {name: {"rect": [x, y, w, h] (top-left origin),
                               "border": [left, top, right, bottom] (9-slice, px)}}}

Usage: python tools/make_ui.py [path to Slider/Assets]   (needs Pillow)
"""
import json
import os
import re
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "..", "Slider", "Slider", "Assets")

# port name: (texture under Assets, Unity sprite name)
SPRITES = {
    "button": ("Sprites/UI/Button24.png", "Button24_0"),
    "buttonPressed": ("Sprites/UI/Button24.png", "Button24_1"),
    "toggle": ("Sprites/UI/ui_toggle.png", "ui_toggle_0"),
    "toggleSelected": ("Sprites/UI/ui_toggle.png", "ui_toggle_0_selected"),
    "toggleCheck": ("Sprites/UI/ui_toggle.png", "ui_toggle_1"),
    "sliderHandle": ("Sprites/UI/menus/ui_slider.png", "ui_slider_1"),
    "sliderHandleSelected": ("Sprites/UI/menus/ui_slider.png", "ui_slider_0"),
}
SHEET_W = 64


def unity_sprite(texture, name):
    """The sprite's pixels and its 9-slice border [left, top, right, bottom] from the texture's .meta."""
    meta = open(os.path.join(ASSETS, texture + ".meta"), encoding="utf-8").read()
    m = re.search(r"name: " + re.escape(name) + r"\n\s+rect:\n\s+serializedVersion: \d+\n\s+x: (\d+)\n\s+y: (\d+)\n"
                  r"\s+width: (\d+)\n\s+height: (\d+)\n(?:.*\n)*?\s+border: \{x: (\d+), y: (\d+), z: (\d+), w: (\d+)\}", meta)
    if not m:
        sys.exit(f"{texture}: no sprite {name}")
    x, y, w, h, bl, bb, br, bt = map(int, m.groups())
    img = Image.open(os.path.join(ASSETS, texture)).convert("RGBA")
    top = img.height - y - h  # Unity rects start at the bottom
    return img.crop((x, top, x + w, top + h)), [bl, bt, br, bb]


# The left bracket, by row: the columns it covers (x 0 = 1 px outside the button). The right one
# mirrors it. Rows between the ends are the straight part.
BRACKET_TOP = [(1, 2), (0, 2)]     # row 0, row 1: turning inward
BRACKET_SIDE = (0, 1)              # rows 2 .. h-3
BRACKET_BOTTOM = [(0, 2), (1, 2)]  # row h-2, row h-1


def selected_button():
    """Button24_0, 1 px in from each side of a sprite 2 px wider, between dark brackets."""
    face, border = unity_sprite("Sprites/UI/Button24.png", "Button24_0")
    outline, _ = unity_sprite("Sprites/UI/Button24.png", "Button24_2")
    dark = outline.getpixel((0, outline.height // 2))  # the outline's gray (0.4)
    w, h = face.width + 2, face.height
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    img.paste(face, (1, 0))
    rows = BRACKET_TOP + [BRACKET_SIDE] * (h - 4) + BRACKET_BOTTOM
    for y, (x0, x1) in enumerate(rows):
        for x in range(x0, x1 + 1):
            img.putpixel((x, y), dark)
            img.putpixel((w - 1 - x, y), dark)
    left, top, right, bottom = border
    return img, [left + 1, top, right + 1, bottom]


def main():
    cut = {name: unity_sprite(tex, sprite) for name, (tex, sprite) in SPRITES.items()}
    cut["buttonSelected"] = selected_button()
    # Shelf packing in table order, 1 px gaps.
    x = y = shelf = 0
    placed = {}
    for name, (img, border) in cut.items():
        if x + img.width > SHEET_W:
            x, y, shelf = 0, y + shelf + 1, 0
        placed[name] = (x, y)
        x += img.width + 1
        shelf = max(shelf, img.height)
    height = 8
    while height < y + shelf:
        height *= 2
    sheet = Image.new("RGBA", (SHEET_W, height), (0, 0, 0, 0))
    sprites = {}
    for name, (img, border) in cut.items():
        px, py = placed[name]
        sheet.paste(img, (px, py))
        sprites[name] = {"rect": [px, py, img.width, img.height], "border": border}

    sheet.save(os.path.join(REPO, "assets", "sheets", "ui_menu.png"))
    with open(os.path.join(REPO, "assets", "sheets", "ui_menu.fmt"), "w", newline="\n") as f:
        f.write("la8\n")  # gray sprites: lossless as luminance + alpha (see the Makefile)
    os.makedirs(os.path.join(REPO, "assets", "ui"), exist_ok=True)
    with open(os.path.join(REPO, "assets", "ui", "menu.json"), "w", newline="\n") as f:
        f.write('{\n  "sheet": "ui_menu",\n  "sprites": {\n')
        f.write(",\n".join(f'    "{n}": ' + json.dumps(s, separators=(",", ":")) for n, s in sprites.items()))
        f.write("\n  }\n}\n")
    print(f"{len(sprites)} sprites, sheet {SHEET_W}x{height}")


if __name__ == "__main__":
    main()
