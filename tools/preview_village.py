"""Render an exported area (village.json + atlas pages) to a PNG, to sanity-check the Unity export.

Usage: python tools/preview_village.py [assets/village/village.json] [out.png] [--houses]
Requires Pillow. Draws every visible tilemap (skipping collider/material layers) with STiles at
their authored grid positions, sorted by (sorting layer, order). Animated tiles show frame 0.
--houses renders the interior tilemaps (placed at housingOffset) instead of the overworld.
"""
import json
import os
import sys

from PIL import Image

HOUSE_Y = -100  # interiors are tilemaps placed at housingOffset (-150); origin below this = "houses"


def visible(tm):
    r = tm.get("render")
    return (tm["active"] and r and r["enabled"] and "collider" not in tm
            and "Materials" not in tm["path"] and tm["size"][0] > 0)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    houses = "--houses" in sys.argv
    src = args[0] if args else os.path.join("assets", "village", "village.json")
    out = args[1] if len(args) > 1 else ("preview_houses.png" if houses else "preview_world.png")

    d = json.load(open(src, encoding="utf-8"))
    ppu = d["ppu"]
    base_dir = os.path.dirname(src)
    pages = [Image.open(os.path.join(base_dir, p["file"])).convert("RGBA") for p in d["pages"]]
    stile_pos = {s["islandId"]: (s["x"] * s["width"], s["y"] * s["width"]) for s in d["stiles"]}

    # Collect draw ops in world units: (sortKey, wx, wy_bottom, spriteImage)
    sprite_cache = {}

    def sprite_img(i, xf):
        key = (i, xf)
        if key not in sprite_cache:
            s = d["sprites"][i]
            x, y, w, h = s["rect"]
            img = pages[s["page"]].crop((x, y, x + w, y + h))
            if xf & 4:
                img = img.transpose(Image.Transpose.ROTATE_90)
            if xf & 1:
                img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if xf & 2:
                img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            sprite_cache[key] = img
        return sprite_cache[key]

    ops = []
    for tm in d["tilemaps"]:
        if not visible(tm):
            continue
        bx, by = stile_pos.get(tm["owner"], (0, 0))
        ox, oy = tm["origin"]
        ax, ay = tm["tileAnchor"]
        minx, miny = tm["cellMin"]
        w, h = tm["size"]
        xform = tm.get("xform")
        if (by + oy < HOUSE_Y) != houses:
            continue
        r = tm["render"]
        key = (r["sortingLayerValue"], r["order"])
        for cy in range(h):
            for cx in range(w):
                v = tm["cells"][cy * w + cx]
                if v < 0 or not d["tiles"][v]["frames"]:
                    continue
                si = d["tiles"][v]["frames"][0]
                s = d["sprites"][si]
                sw, sh = s["rect"][2], s["rect"][3]
                px, py = s["pivot"]
                wx = bx + ox + minx + cx + ax - px / s["ppu"]
                wy = by + oy + miny + cy + ay - py / s["ppu"]
                xf = xform[cy * w + cx] if xform else 0
                ops.append((key, wx, wy, sw / s["ppu"], sh / s["ppu"], si, xf))

    if not ops:
        sys.exit("nothing to draw")
    min_x = min(o[1] for o in ops)
    min_y = min(o[2] for o in ops)
    max_x = max(o[1] + o[3] for o in ops)
    max_y = max(o[2] + o[4] for o in ops)
    W, H = round((max_x - min_x) * ppu), round((max_y - min_y) * ppu)
    canvas = Image.new("RGBA", (W, H), (20, 20, 28, 255))

    ops.sort(key=lambda o: o[0])  # stable: preserves tilemap order within a sort key
    for key, wx, wy, ww, wh, si, xf in ops:
        img = sprite_img(si, xf)
        X = round((wx - min_x) * ppu)
        Y = round((max_y - (wy + wh)) * ppu)
        canvas.alpha_composite(img, (X, Y))

    canvas.save(out)
    print(f"{out}: {W}x{H}px, {len(ops)} sprites, world x[{min_x:.1f},{max_x:.1f}] y[{min_y:.1f},{max_y:.1f}]")


if __name__ == "__main__":
    main()
