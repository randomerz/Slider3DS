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

The ending's top screen (game/ui/end_screen): the demo's EndOfDemo.unity texts baked into one image over
the user's end art, tools/end_ui/end_cutscene_full.png (the original's end_cutscene art extended to the
3DS's 400x240 top screen; end_cutscene_sliced.png beside it is the same art in its parallax layers, for the
full game's EndOfGame), each text outlined, with TextMeshPro's line layout worked out here (END_TEXTS; the
last text line is the port's). No DarkPanel (the user's call): the texts sit where the art leaves room —
the title in the dark sky between the rocket and the cat, the text along the bottom over the clouds — at
1x (the user's rule: Unity's own scaler would scale the canvas 1.25x). Baked with Pillow 12.2.0's
FreeType: after changing the text, its place or Pillow, look at the result (preview/end_screen.png; the
render golden hashes one sprite, not the text).
  assets/sheets/ui_end.png    the sheet (.fmt by its colors)
  assets/ui/end.json          {"sheet", "sprites": {"art": {"rect"}}}

The intro (IntroCutscene.unity's CutsceneManager, game/ui/intro_cutscene): its seven images, and their
captions as the English localization has them (IntroCutscene_scene.csv: Canvas/Texts/Text (n)), with the
canvas' layout (INTRO_LAYOUT: from the scene's RectTransforms, at 1x on the top screen like the ending's):
  assets/sheets/ui_intro.png  the sheet (.fmt by its colors)
  assets/ui/intro.json        {"sheet", "slides": [{"rect", "text"}], "imageY" (px the images' centers sit
                               above the screen's middle), "textBox": [w, h, y] (the captions' box, centered,
                               its middle y px above the bottom; text centered, bottom-aligned), "wordSpacing"
                               (px added to each space; captions are ASCII)}

Usage: python tools/make_ui.py [path to Slider/Assets]   (needs Pillow)
"""
import csv
import json
import math
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
    "cardBackCheated": ("Sprites/UI/Button24_Backgrounds.png", "Button24_Backgrounds_1"),
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


FONTS = os.path.join(ASSETS, "Fonts", "originalFonts")
LIGHT = (230, 230, 230, 255)  # the texts' m_fontColor 0.902
DARK = (26, 26, 26, 255)      # their outline copies' 0.102
# EndOfDemo's TMP texts: font, size, lines (centered; "" = a blank line), the middle of the box they're
# centered in (px from the canvas's (middle, bottom) anchor: x right, y up; VerticalAlignment Middle),
# the font asset's face info (pointSize, lineHeight, ascent, descent) for TMP's line layout, lineSpacing,
# and the box's top margin (m_margin.y: Middle alignment moves down by half of it).
# Title Text: the original's PixelTitanOne at 20 (wrapped by its 200 px box to "Thanks for" / "playing
# Slider!"); here at 16, the font's own pixel size (64/1024 em: at 20 a font pixel is 1.25 px, lumpy at
# 1x), on the original's two lines, in the sky above the planet: centered 20 px left of the middle
# (between the rocket and the cat), 24 px from the top.
# Text: the original's first line and the port's last ("Please wishlist Slider on Steam!" -> "More areas
# coming soon"), without the blank line between them, centered 18 px above the bottom.
END_TEXTS = [
    ("PixelTitanOne.ttf", 16, ["Thanks for", "playing Slider!"], ("middle", -20, 96), (151, 155.1289, 113.25, -28.3125), 0, 0),
    ("PixelTitanZero.ttf", 16, ["This is the end of the demo.", "More areas coming soon"], ("bottom", 0, 18),
     (172, 176.70312, 129, -32.25), -15, 0),
]
END_CANVAS = os.path.join(REPO, "tools", "end_ui", "end_cutscene_full.png")


def half_up(v):
    return math.floor(v + 0.5)  # (Python's round() takes halves to even)


def draw_tmp_text(img, ttf, size, lines, anchor, face, line_spacing, margin_top):
    """Lines as TMP lays them out (centered, the block's ascender-to-descender middle on the box's), each
    outlined like EndOfDemo's texts: every line's dark copies 1 px around first, then the light text."""
    from PIL import ImageDraw, ImageFont
    where, ox, oy = anchor
    cx = img.width / 2 + ox
    cy = (img.height / 2 if where == "middle" else img.height) - oy + margin_top / 2
    point, line_height, ascent, descent = face
    scale = size / point
    advance = line_height * scale + line_spacing * size / 100  # (TMP: lineSpacing in em / 100)
    height = ascent * scale + (len(lines) - 1) * advance - descent * scale
    top = cy - height / 2
    font = ImageFont.truetype(os.path.join(FONTS, ttf), size)
    draw = ImageDraw.Draw(img)
    draw.fontmode = "1"  # (pixel fonts: no antialiasing)
    placed = [(half_up(cx - draw.textlength(line, font=font) / 2), half_up(top + ascent * scale + i * advance), line)
              for i, line in enumerate(lines) if line]
    for x, baseline, line in placed:
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            draw.text((x + dx, baseline + dy), line, font=font, fill=DARK, anchor="ls")
    for x, baseline, line in placed:
        draw.text((x, baseline), line, font=font, fill=LIGHT, anchor="ls")


def end():
    img = Image.open(END_CANVAS).convert("RGBA")
    if img.size != (400, 240):
        sys.exit(f"{END_CANVAS}: {img.width}x{img.height}; the top screen's 400x240 (the texts' places are picked on it)")
    for t in END_TEXTS:
        draw_tmp_text(img, *t)
    os.makedirs(os.path.join(REPO, "preview"), exist_ok=True)
    img.resize((img.width * 2, img.height * 2), Image.NEAREST).save(os.path.join(REPO, "preview", "end_screen.png"))
    sheet, placed = pack({"art": img}, 512)
    write_sheet(sheet, "ui_end")
    write_json(os.path.join(REPO, "assets", "ui", "end.json"), "ui_end",
               {"art": {"rect": [*placed["art"], img.width, img.height]}})
    print(f"end: {img.width}x{img.height}, sheet {sheet.width}x{sheet.height}")


# The title art's textures (assets/ui/title.json names their sprites: Slider3DSTitleExporter).
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


# CutsceneManager.images, in order (an Image's sprite: its texture, and a sprite name for a multi-sprite one).
INTRO_IMAGES = [("Sprites/UI/intro_cutscene/intro_cutscene.png", "intro_cutscene_0"),
                ("Sprites/UI/intro_cutscene/intro_cutscene.png", "intro_cutscene_1"),
                ("Sprites/UI/intro_cutscene/intro_cutscene.png", "intro_cutscene_2"),
                ("Sprites/UI/intro_cutscene/cg 4.PNG", None), ("Sprites/UI/intro_cutscene/cg 5.png", None),
                ("Sprites/UI/intro_cutscene/cg 6.png", None), ("Sprites/UI/intro_cutscene/cg7 160x120.png", None)]
# Images: anchored to the middle, at (0, 20), native size. Texts (1)-(7): 300x45, anchored bottom-middle at
# (0, 32.5): PixelTitanZero 16, centered, bottom-aligned; m_wordSpacing -10 (TMP: em / 100, so -1.6 px a space).
INTRO_LAYOUT = {"imageY": 20, "textBox": [300, 45, 32.5], "wordSpacing": round(-10 * 16 * 0.01, 4)}


def intro():
    rows = csv.reader(open(os.path.join(ASSETS, "StreamingAssets", "Localizations", "English", "IntroCutscene_scene.csv"),
                           encoding="utf-8-sig", newline=""))
    texts = {r[0]: r[2] for r in rows if len(r) > 2}
    cut, captions = {}, []
    for i, (texture, name) in enumerate(INTRO_IMAGES):
        sprites = texture_sprites(texture)
        img, _ = sprites[name] if name else next(iter(sprites.values()))
        cut[f"slide{i}"] = img
        text = texts.get(f"Canvas/Texts/Text ({i + 1})")
        if text is None:
            sys.exit(f"IntroCutscene_scene.csv: no Canvas/Texts/Text ({i + 1})")
        caption = "\n".join(line.strip() for line in text.replace("\r\n", "\n").split("\n"))
        if not caption.isascii():
            sys.exit(f"Text ({i + 1}): not ASCII (the port's fonts and typing are byte-wise): {caption!r}")
        captions.append(caption)
    sheet, placed = pack(cut, 512)
    write_sheet(sheet, "ui_intro")
    slides = [{"rect": [*placed[f"slide{i}"], img.width, img.height], "text": captions[i]} for i, img in enumerate(cut.values())]
    path = os.path.join(REPO, "assets", "ui", "intro.json")
    with open(path, "w", newline="\n") as f:
        f.write('{\n  "sheet": "ui_intro",\n  "slides": [\n')
        f.write(",\n".join("    " + json.dumps(sl, separators=(",", ":")) for sl in slides))
        f.write('\n  ],\n  "imageY": ' + json.dumps(INTRO_LAYOUT["imageY"]) + ',\n  "textBox": ' +
                json.dumps(INTRO_LAYOUT["textBox"]) + ',\n  "wordSpacing": ' + json.dumps(INTRO_LAYOUT["wordSpacing"]) + '\n}\n')
    print(f"intro: {len(slides)} slides, sheet {sheet.width}x{sheet.height}")


if __name__ == "__main__":
    menu()
    title()
    end()
    intro()
