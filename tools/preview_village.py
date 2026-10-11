"""Render an exported area (<area>.json + shared sheets) to a PNG, to sanity-check the Unity export.

Usage: python tools/preview_village.py [assets/village/village.json] [out.png] [--houses] [--collision] [--materials]
Requires Pillow. Draws every visible "visual" tilemap with STiles at their authored grid positions,
sorted by (sorting layer, order). Animated tiles show frame 0.
--houses     render the interior tilemaps (placed at housingOffset) instead of the overworld
--collision  overlay collision polygons (red)
--materials  overlay footstep materials (one color per material)
"""
import json
import os
import sys

from PIL import Image, ImageDraw

HOUSE_Y = -100  # interiors are tilemaps placed at housingOffset (-150); origin below this = "houses"
MATERIAL_COLORS = [(66, 135, 245), (245, 66, 230), (245, 200, 66), (66, 245, 120), (180, 110, 60),
                   (160, 160, 160), (245, 120, 66), (120, 66, 245)]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    houses = "--houses" in sys.argv
    src = args[0] if args else os.path.join("assets", "village", "village.json")
    out = args[1] if len(args) > 1 else ("preview_houses.png" if houses else "preview_world.png")

    d = json.load(open(src, encoding="utf-8"))
    ppu = d["ppu"]
    base_dir = os.path.dirname(src)
    sheets = [Image.open(os.path.join(base_dir, s["file"])).convert("RGBA") for s in d["sheets"]]
    stile_pos = {s["islandId"]: (s["x"] * s["width"], s["y"] * s["width"]) for s in d["stiles"]}

    def placed(tm):
        """Yields (cellIndex, cellWorldX, cellWorldY) for occupied cells of a tilemap in this view."""
        if not tm["active"] or tm["size"][0] == 0:
            return
        bx, by = stile_pos.get(tm["owner"], (0, 0))
        ox, oy = tm["origin"]
        if (by + oy < HOUSE_Y) != houses:
            return
        minx, miny = tm["cellMin"]
        w, h = tm["size"]
        for cy in range(h):
            for cx in range(w):
                i = cy * w + cx
                if tm["cells"][i] >= 0:
                    yield i, bx + ox + minx + cx, by + oy + miny + cy

    sprite_cache = {}

    def sprite_img(i, xf):
        if (i, xf) not in sprite_cache:
            s = d["sprites"][i]
            x, y, w, h = s["rect"]
            img = sheets[s["sheet"]].crop((x, y, x + w, y + h))
            if xf & 4:
                img = img.transpose(Image.Transpose.ROTATE_90)
            if xf & 1:
                img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if xf & 2:
                img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            sprite_cache[(i, xf)] = img
        return sprite_cache[(i, xf)]

    # Draw ops in world units: (sortKey, x, yBottom, w, h, sprite, xform)
    ops = []
    for tm in d["tilemaps"]:
        if tm["kind"] != "visual" or not tm["render"]["enabled"]:
            continue
        ax, ay = tm["tileAnchor"]
        key = (tm["render"]["sortingLayerValue"], tm["render"]["order"])
        xform = tm.get("xform")
        for i, wx, wy in placed(tm):
            frames = d["tiles"][tm["cells"][i]]["frames"]
            if not frames:
                continue
            s = d["sprites"][frames[0]]
            px, py = s["pivot"]
            ops.append((key, wx + ax - px / s["ppu"], wy + ay - py / s["ppu"],
                        s["rect"][2] / s["ppu"], s["rect"][3] / s["ppu"], frames[0], xform[i] if xform else 0))

    if not ops:
        sys.exit("nothing to draw")
    min_x = min(o[1] for o in ops)
    min_y = min(o[2] for o in ops)
    max_x = max(o[1] + o[3] for o in ops)
    max_y = max(o[2] + o[4] for o in ops)
    W, H = round((max_x - min_x) * ppu), round((max_y - min_y) * ppu)
    canvas = Image.new("RGBA", (W, H), (20, 20, 28, 255))

    def to_px(wx, wy):
        return (wx - min_x) * ppu, (max_y - wy) * ppu

    ops.sort(key=lambda o: o[0])  # stable: preserves tilemap order within a sort key
    for key, wx, wy, ww, wh, si, xf in ops:
        X, Y = to_px(wx, wy + wh)
        canvas.alpha_composite(sprite_img(si, xf), (round(X), round(Y)))

    overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    if "--materials" in sys.argv:
        for tm in d["tilemaps"]:
            if tm["kind"] == "material":
                for i, wx, wy in placed(tm):
                    c = MATERIAL_COLORS[tm["cells"][i] % len(MATERIAL_COLORS)]
                    x0, y0 = to_px(wx, wy + 1)
                    draw.rectangle((x0, y0, x0 + ppu - 1, y0 + ppu - 1), fill=c + (110,))
        print("materials:", ", ".join(f"{m['footstep']}={MATERIAL_COLORS[i % len(MATERIAL_COLORS)]}"
                                     for i, m in enumerate(d["materials"])))
    if "--collision" in sys.argv:
        for tm in d["tilemaps"]:
            if tm["kind"] == "collision" and tm["enabled"] and not tm["trigger"]:
                for i, wx, wy in placed(tm):
                    for poly in d["collisionShapes"][tm["cells"][i]]:
                        pts = [to_px(wx + poly[k], wy + poly[k + 1]) for k in range(0, len(poly), 2)]
                        draw.polygon(pts, fill=(255, 40, 40, 90))
    canvas.alpha_composite(overlay)

    canvas.save(out)
    print(f"{out}: {W}x{H}px, {len(ops)} sprites, world x[{min_x:.1f},{max_x:.1f}] y[{min_y:.1f},{max_y:.1f}]")


if __name__ == "__main__":
    main()
