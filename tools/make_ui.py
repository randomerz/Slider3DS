"""Cuts the original's menu UI sprites into a sheet for the port's menus (game/ui/menu_skin), and the title art's.

The sprites, as the prefabs use them (UICanvas PausePanel, GeneralOptionsPanel):
  Menu Button (the main menu's; Image sliced, Button sprite swap): normal Button24_0 (a light face
      over a 2 px shaded bottom edge), pressed _1 (the face 1 px lower); 9-sliced, 4 px borders.
  Selected: brackets beside the button (the user's art, tools/ui_art/: bracket.png for a button,
      bracket_pressed.png for a pressed one, 1 px shorter like its sunk face), cut as the left
      one and its mirror; 9-sliced vertically, 3 px ends.
  UI Toggle: background ui_toggle_0, selected background ui_toggle_0_selected, checkmark ui_toggle_1.
  UI Slider: handle ui_slider_1 (the Handle Image's m_Sprite), selected handle ui_slider_0 (the
      Slider's m_SpriteState). Names mislead: resolve fileIDs through the .meta's internalIDs.
  The title menu (MainMenu.unity): profile cards (Button24_BottomEmpty: normal _0, pressed _1 (1 px
      lower), selected _2 (outlined); on Button24_Backgrounds_0), bracket frames (MainMenuButton20:
      light _0: the name field, a selected key; dark _2: keys; the mid _1, the main menu's, is unused), icons (back_button, trash icon: normal
      _0, pressed _1 (1 px lower), selected _2 (light)).

  assets/sheets/ui_menu.png   the sheet (.fmt: la8, all gray)
  assets/ui/menu.json         {"sheet", "sprites": {name: {"rect": [x, y, w, h] (top-left origin),
                               "border": [left, top, right, bottom] (9-slice, px)}}}

The title screen's art: the sprites assets/ui/title.json names (written by the Unity exporter's
Slider3DSTitleExporter: the layout), cut from TITLE_TEXTURES:
  assets/sheets/ui_title.png  the sheet (.fmt by its colors)
  assets/ui/title_sheet.json  {"sheet", "sprites": {name: {"rect", "pivot": [x, y] (px from the bottom-left)}}}

The ending's art (the demo's EndOfDemo.unity Background: end_cutscene_parallax_0, 320x180; game/ui/end_screen):
  assets/sheets/ui_end.png    the sheet (.fmt by its colors)
  assets/ui/end.json          {"sheet", "sprites": {"art": {"rect"}}}

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
    # The title menu (MainMenu.unity's SavesPanel / NewSavePanel):
    "card": ("Sprites/UI/Button24_BottomEmpty.png", "Button24_0_Empty"),
    "cardPressed": ("Sprites/UI/Button24_BottomEmpty.png", "Button24_1_Empty"),
    "cardSelected": ("Sprites/UI/Button24_BottomEmpty.png", "Button24_2_Empty"),
    "cardBack": ("Sprites/UI/Button24_Backgrounds.png", "Button24_Backgrounds_0"),
    "frameLight": ("Sprites/UI/MainMenuButton20.png", "MainMenuButton20_0"),
    "frameDark": ("Sprites/UI/MainMenuButton20.png", "MainMenuButton20_2"),
    "back": ("Sprites/UI/artifact/back_button.png", "back_button_0"),
    "backPressed": ("Sprites/UI/artifact/back_button.png", "back_button_1"),
    "backSelected": ("Sprites/UI/artifact/back_button.png", "back_button_2"),
    "trash": ("Sprites/UI/menus/trash icon.png", "trash icon_0"),
    "trashPressed": ("Sprites/UI/menus/trash icon.png", "trash icon_1"),
    "trashSelected": ("Sprites/UI/menus/trash icon.png", "trash icon_2"),
}
SHEET_W = 128


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


ART = os.path.join(REPO, "tools", "ui_art")
BRACKET_ENDS = 3  # px at each end of a bracket that don't stretch


def port_art(name):
    """A sprite drawn for the port (tools/ui_art/<name>.png), with its 9-slice border."""
    img = Image.open(os.path.join(ART, name + ".png")).convert("RGBA")
    return img, [0, BRACKET_ENDS, 0, BRACKET_ENDS]


def mirrored(sprite):
    img, border = sprite
    left, top, right, bottom = border
    return img.transpose(Image.Transpose.FLIP_LEFT_RIGHT), [right, top, left, bottom]


def pack(cut, width):
    """Shelf-packs {name: image} in order, 1 px gaps: the sheet and each image's top-left corner."""
    x = y = shelf = 0
    placed = {}
    for name, img in cut.items():
        if x + img.width > width:
            x, y, shelf = 0, y + shelf + 1, 0
        placed[name] = (x, y)
        x += img.width + 1
        shelf = max(shelf, img.height)
    height = 8
    while height < y + shelf:
        height *= 2
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for name, img in cut.items():
        sheet.paste(img, placed[name])
    return sheet, placed


def write_sheet(sheet, name):
    sheet.save(os.path.join(REPO, "assets", "sheets", name + ".png"))
    px = sheet.tobytes()  # RGBA
    gray = all(px[i] == px[i + 1] == px[i + 2] for i in range(0, len(px), 4) if px[i + 3])
    with open(os.path.join(REPO, "assets", "sheets", name + ".fmt"), "w", newline="\n") as f:
        f.write("la8\n" if gray else "rgba8\n")  # gray sprites: lossless as luminance + alpha (see the Makefile)


def write_json(path, sheet_name, sprites):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        f.write('{\n  "sheet": "' + sheet_name + '",\n  "sprites": {\n')
        f.write(",\n".join(f'    "{n}": ' + json.dumps(s, separators=(",", ":")) for n, s in sprites.items()))
        f.write("\n  }\n}\n")


def menu():
    cut = {name: unity_sprite(tex, sprite) for name, (tex, sprite) in SPRITES.items()}
    for name, art in (("bracket", "bracket"), ("bracketPressed", "bracket_pressed")):
        cut[name + "Left"] = port_art(art)
        cut[name + "Right"] = mirrored(cut[name + "Left"])
    sheet, placed = pack({n: img for n, (img, border) in cut.items()}, SHEET_W)
    write_sheet(sheet, "ui_menu")
    sprites = {n: {"rect": [*placed[n], img.width, img.height], "border": border} for n, (img, border) in cut.items()}
    write_json(os.path.join(REPO, "assets", "ui", "menu.json"), "ui_menu", sprites)
    print(f"menu: {len(sprites)} sprites, sheet {sheet.width}x{sheet.height}")


# The title art's textures (assets/ui/title.json names their sprites: Slider3DSTitleExporter).
def end():
    img, _ = unity_sprite("Sprites/UI/end_cutscene/end_cutscene_parallax.png", "end_cutscene_parallax_0")
    sheet, placed = pack({"art": img}, 512)
    write_sheet(sheet, "ui_end")
    write_json(os.path.join(REPO, "assets", "ui", "end.json"), "ui_end",
               {"art": {"rect": [*placed["art"], img.width, img.height]}})
    print(f"end: sheet {sheet.width}x{sheet.height}")


TITLE_TEXTURES = ["Sprites/UI/menus/pixel_logo.png", "Sprites/UI/menus/mainmenu_bg_stars.png",
                  "Sprites/UI/menus/mainmenu_giant_star.png", "Sprites/UI/menus/mainmenu_meteor.png",
                  "Sprites/UI/menus/mainmenu_plannet.png", "Sprites/Characters/player/playersprite.png"]
ALIGNMENT_PIVOTS = {0: (0.5, 0.5), 1: (0, 1), 2: (0.5, 1), 3: (1, 1), 4: (0, 0.5), 5: (1, 0.5), 6: (0, 0),
                    7: (0.5, 0), 8: (1, 0)}  # (9: custom, the sprite's own pivot)


def texture_sprites(texture):
    """{name: (image, pivot (px from the bottom-left))} for a texture's sprites (or the whole texture)."""
    meta = open(os.path.join(ASSETS, texture + ".meta"), encoding="utf-8").read()
    img = Image.open(os.path.join(ASSETS, texture)).convert("RGBA")
    out = {}
    if re.search(r"spriteMode: 1\n", meta):
        align = int(re.search(r"\n  alignment: (\d+)", meta).group(1))
        pivot = ALIGNMENT_PIVOTS.get(align) or tuple(map(float, re.search(r"spritePivot: \{x: ([\d.]+), y: ([\d.]+)\}", meta).groups()))
        out[os.path.splitext(os.path.basename(texture))[0]] = (img, [pivot[0] * img.width, pivot[1] * img.height])
        return out
    for m in re.finditer(r"name: (.+)\n\s+rect:\n\s+serializedVersion: \d+\n\s+x: (\d+)\n\s+y: (\d+)\n\s+width: (\d+)\n"
                         r"\s+height: (\d+)\n\s+alignment: (\d+)\n\s+pivot: \{x: ([\d.]+), y: ([\d.]+)\}", meta):
        name = m.group(1)
        x, y, w, h, align = map(int, m.groups()[1:6])
        pivot = ALIGNMENT_PIVOTS.get(align) or (float(m.group(7)), float(m.group(8)))
        top = img.height - y - h
        out[name] = (img.crop((x, top, x + w, top + h)), [pivot[0] * w, pivot[1] * h])
    return out


def title():
    layout = json.load(open(os.path.join(REPO, "assets", "ui", "title.json"), encoding="utf-8"))
    names = []
    for s in layout["stars"]:
        names += [s["on"], s["off"]]
    for i in layout["images"]:
        names += i["frames"]
    names += [l["sprite"] for l in layout["logo"]["letters"]] + layout["player"]["frames"]
    available = {}
    for t in TITLE_TEXTURES:
        available.update(texture_sprites(t))
    cut = {}
    for n in dict.fromkeys(names):
        if n not in available:
            sys.exit(f"title.json: no sprite {n} in TITLE_TEXTURES")
        img, (px, py) = available[n]
        # Cropped to its visible pixels (each logo letter sits in a full-logo frame), the pivot kept
        # on the same pixel.
        box = img.getbbox()
        if box:
            left, top, right, bottom = box
            px, py = px - left, py - (img.height - bottom)
            img = img.crop(box)
        cut[n] = (img, [px, py])
    sheet, placed = pack({n: img for n, (img, pivot) in cut.items()}, 256)
    write_sheet(sheet, "ui_title")
    sprites = {n: {"rect": [*placed[n], img.width, img.height], "pivot": pivot} for n, (img, pivot) in cut.items()}
    write_json(os.path.join(REPO, "assets", "ui", "title_sheet.json"), "ui_title", sprites)
    print(f"title: {len(sprites)} sprites, sheet {sheet.width}x{sheet.height}")


if __name__ == "__main__":
    menu()
    title()
    end()
