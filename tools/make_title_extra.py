"""The title screen's layout for the 3DS top screen (a port addition: the original lays its stars out
for a 320x180 view). Generated, seeded and reproducible: don't hand-edit its output, re-run this.
Writes assets/ui/title_extra.json = {"spread": [x, y] (the exported stars and images are spread by
this, filling 400x240; the logo, player and prompt stay put), "logoDown": px the logo sits lower
(at rest and risen), "playerRises": whether the player rises with the menu (the original lifts it
to make room for its buttons, which the port has on the bottom screen: it stays centered),
"stars": extra stars in the gaps
(best-candidate sampling, at the original's density and size mix, kept clear of the images, the
logo, the player and the prompt over their whole motion)}, and a 2x preview in preview/title_spread.png.
Everything it avoids is read from the exported data (title.json, title_sheet.json, the menu font).

Usage: python tools/make_title_extra.py   (needs Pillow; run make_ui.py first if title.json changed)
"""
import collections
import json
import math
import os
import random

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCREEN_W, SCREEN_H = 400, 240  # the 3DS top screen
VIEW_W, VIEW_H = 320, 180      # the original's view
MARGIN = 4                     # px kept between a new star and anything it avoids, and the screen's edge
LOGO_DOWN = 8                  # px the logo sits lower than the original's (the user's layout)
PLAYER_RISES = False           # the player stays centered when the menu opens


def load(*path):
    return json.load(open(os.path.join(REPO, *path), encoding="utf-8"))


def main():
    d = load("assets", "ui", "title.json")
    sheet = load("assets", "ui", "title_sheet.json")["sprites"]
    font = {g["c"]: g["adv"] for g in load("assets", "fonts", "menu.json")["glyphs"]}
    img = Image.open(os.path.join(REPO, "assets", "sheets", "ui_title.png")).convert("RGBA")
    sx, sy = SCREEN_W / VIEW_W, SCREEN_H / VIEW_H
    # The port's layout, as TitleArt::load applies it.
    d["logo"]["restY"] -= LOGO_DOWN
    for k in d["logo"]["riseY"]:
        k[1] -= LOGO_DOWN
    if not PLAYER_RISES:
        for k in d["player"]["riseY"]:
            k[1] = 0

    def box(name, x, y):
        """A sprite's [left, bottom, right, top] drawn with its pivot at (x, y), y up."""
        s = sheet[name]
        w, h = s["rect"][2:]
        px, py = s["pivot"]
        return [x - px, y - py, x - px + w, y - py + h]

    def union(boxes):
        return [min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes)]

    def keys_range(keys):
        values = [k[1] for k in keys]
        return min(values), max(values)

    stars = [((s["pos"][0] * sx, s["pos"][1] * sy), s["on"], s["off"]) for s in d["stars"]]
    images = [((i["pos"][0] * sx, i["pos"][1] * sy), i["frames"][0]) for i in d["images"]]
    avoid = [box(name, x, y) for (x, y), name in images]
    # The logo over its whole motion: at rest, risen (TitleRiseUp's y keys), bobbing (each letter's sine).
    logo = d["logo"]
    lo, hi = keys_range(logo["riseY"] + [[0, logo["restY"], 0, 0]])
    for l in logo["letters"]:
        bob = abs(l["sin"][0]) + abs(l["sin"][3])
        for y in (lo - bob, hi + bob):
            avoid.append(box(l["sprite"], logo["origin"][0], logo["origin"][1] + y))
    # The player, at rest and risen; the prompt's text (its width in the menu font, its cap height).
    p = d["player"]
    lo, hi = keys_range(p["riseY"])
    avoid.append(union([box(f, p["pos"][0], p["pos"][1] + y) for f in p["frames"] for y in (lo, hi)]))
    pr = d["prompt"]
    half = sum(font.get(ord(c), 0) for c in pr["text"]) / 2
    avoid.append([pr["pos"][0] - half, pr["pos"][1] - 4, pr["pos"][0] + half, pr["pos"][1] + 4])

    def clear(x, y, half):
        """A star `half` px from its center to its edge fits at (x, y)."""
        m = MARGIN + half
        if abs(x) > SCREEN_W / 2 - m or abs(y) > SCREEN_H / 2 - m:
            return False
        return all(not (l - m < x < r + m and b - m < y < t + m) for l, b, r, t in avoid)

    # Sizes first, in the exported stars' mix (each size's on / off sprites, as often as the original
    # has it), so each new star keeps clear by its own size.
    random.seed(3)
    mix = collections.Counter((on, off) for _, on, off in stars)
    kinds = [kind for kind, n in sorted(mix.items()) for _ in range(n)]
    random.shuffle(kinds)
    count = round(len(stars) * (sx * sy - 1))  # the original's density over the bigger screen
    # Best-candidate sampling: each new star as far as possible from every other one.
    points = [pt for pt, _, _ in stars]
    extra_stars = []
    for i in range(count):
        on, off = kinds[i % len(kinds)]
        half = max(sheet[on]["rect"][2:]) / 2
        best, best_dist = None, -1
        for _ in range(60):
            x, y = random.uniform(-SCREEN_W / 2, SCREEN_W / 2), random.uniform(-SCREEN_H / 2, SCREEN_H / 2)
            if not clear(x, y, half):
                continue
            dist = min(math.hypot(x - px, y - py) for px, py in points)
            if dist > best_dist:
                best, best_dist = (round(x), round(y)), dist
        points.append(best)
        extra_stars.append({"pos": list(best), "on": on, "off": off})
    with open(os.path.join(REPO, "assets", "ui", "title_extra.json"), "w", newline="\n") as f:
        f.write('{\n  "spread": [%r, %r],\n  "logoDown": %d,\n  "playerRises": %s,\n  "stars": [\n    '
                % (sx, sy, LOGO_DOWN, json.dumps(PLAYER_RISES)))
        f.write(",\n    ".join(json.dumps(s, separators=(", ", ": ")) for s in extra_stars))
        f.write("\n  ]\n}\n")

    # Preview: 400x240 (y up from the center) at 2x, the logo risen (its highest).
    canvas = Image.new("RGBA", (SCREEN_W, SCREEN_H), (26, 26, 26, 255))

    def put(name, x, y):
        s = sheet[name]
        rx, ry, w, h = s["rect"]
        px, py = s["pivot"]
        canvas.alpha_composite(img.crop((rx, ry, rx + w, ry + h)), (round(SCREEN_W / 2 + x - px), round(SCREEN_H / 2 - y - (h - py))))

    for (x, y), on, _ in stars:
        put(on, x, y)
    for s in extra_stars:
        put(s["on"], *s["pos"])
    for (x, y), name in images:
        put(name, x, y)
    put(p["frames"][0], *p["pos"])
    for l in logo["letters"]:
        put(l["sprite"], logo["origin"][0], logo["origin"][1] + keys_range(logo["riseY"])[1])
    os.makedirs(os.path.join(REPO, "preview"), exist_ok=True)
    canvas.resize((SCREEN_W * 2, SCREEN_H * 2), Image.NEAREST).save(os.path.join(REPO, "preview", "title_spread.png"))
    print(f"{len(extra_stars)} extra stars; preview/title_spread.png")


if __name__ == "__main__":
    main()
