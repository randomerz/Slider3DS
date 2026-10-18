"""The title screen's layout for the 3DS top screen (a port addition: the original lays its stars out
for a 320x180 view): writes assets/ui/title_extra.json = {"spread": [x, y] (the exported stars and
images are spread by this, filling 400x240; the logo, player and prompt stay put), "stars": extra
stars placed in the gaps (best-candidate sampling, kept clear of the logo, player, prompt and
images, at the original's density and size mix)}, and a 2x preview in preview/title_spread.png.

Usage: python tools/make_title_extra.py   (needs Pillow; run make_ui.py first if title.json changed)
"""
import json
import math
import os
import random

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SX, SY = 400 / 320, 240 / 180  # the original's 320x180 view spread over the top screen


def main():
    d = json.load(open(os.path.join(REPO, "assets", "ui", "title.json"), encoding="utf-8"))
    sheet = json.load(open(os.path.join(REPO, "assets", "ui", "title_sheet.json"), encoding="utf-8"))
    img = Image.open(os.path.join(REPO, "assets", "sheets", "ui_title.png")).convert("RGBA")
    stars = [((s["pos"][0] * SX, s["pos"][1] * SY), s["on"]) for s in d["stars"]]
    images = [((i["pos"][0] * SX, i["pos"][1] * SY), i["frames"][0]) for i in d["images"]]

    def clear(x, y):
        """Away from the images (by their size), the logo (and its rise), the player and the prompt."""
        for (ix, iy), name in images:
            w, h = sheet["sprites"][name]["rect"][2:]
            if abs(x - ix) < w / 2 + 6 and abs(y - iy) < h / 2 + 6:
                return False
        if abs(x) < 100 and 18 < y < 64:
            return False
        if abs(x) < 14 and -12 < y < 30:
            return False
        if abs(x) < 64 and -72 < y < -56:
            return False
        return abs(x) < 196 and abs(y) < 116

    # Best-candidate sampling: each new star as far as possible from every other one.
    random.seed(3)
    points = [p for p, _ in stars]
    extra = []
    for _ in range(round(len(stars) * (SX * SY - 1))):  # the original's density over the bigger screen
        best, best_dist = None, -1
        for _ in range(60):
            x, y = random.uniform(-196, 196), random.uniform(-116, 116)
            if not clear(x, y):
                continue
            dist = min(math.hypot(x - px, y - py) for px, py in points)
            if dist > best_dist:
                best, best_dist = (round(x), round(y)), dist
        points.append(best)
        extra.append(best)
    # Sizes in the original's mix (31 small, 17 medium, 10 big of 58).
    kinds = ([("mainmenu_bg_stars_0", "mainmenu_bg_stars_1")] * 31 + [("mainmenu_bg_stars_2", "mainmenu_bg_stars_3")] * 17
             + [("mainmenu_bg_stars_4", "mainmenu_bg_stars_5")] * 10)
    random.shuffle(kinds)
    extra_stars = [{"pos": list(p), "on": kinds[i % len(kinds)][0], "off": kinds[i % len(kinds)][1]} for i, p in enumerate(extra)]
    with open(os.path.join(REPO, "assets", "ui", "title_extra.json"), "w", newline="\n") as f:
        f.write('{\n  "spread": [%r, %r],\n  "stars": [\n    ' % (SX, SY))
        f.write(",\n    ".join(json.dumps(s, separators=(", ", ": ")) for s in extra_stars))
        f.write("\n  ]\n}\n")

    # Preview: 400x240 (y up from the center) at 2x.
    canvas = Image.new("RGBA", (400, 240), (26, 26, 26, 255))

    def put(name, x, y):
        s = sheet["sprites"][name]
        rx, ry, w, h = s["rect"]
        px, py = s["pivot"]
        canvas.alpha_composite(img.crop((rx, ry, rx + w, ry + h)), (round(200 + x - px), round(120 - y - (h - py))))

    for (x, y), on in stars:
        put(on, x, y)
    for s in extra_stars:
        put(s["on"], *s["pos"])
    for (x, y), name in images:
        put(name, x, y)
    put(d["player"]["frames"][0], *d["player"]["pos"])
    for l in d["logo"]["letters"]:
        put(l["sprite"], d["logo"]["origin"][0], d["logo"]["origin"][1] + d["logo"]["restY"])
    os.makedirs(os.path.join(REPO, "preview"), exist_ok=True)
    canvas.resize((800, 480), Image.NEAREST).save(os.path.join(REPO, "preview", "title_spread.png"))
    print(f"{len(extra_stars)} extra stars; preview/title_spread.png")


if __name__ == "__main__":
    main()
