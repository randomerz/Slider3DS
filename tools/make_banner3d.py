"""The CIA's 3D HOME Menu banner (`make RELEASE=1 cia`): a voxel cat and the SLIDER logo, spinning.

  tools/cia/banner.cgfx  the model bannertool packs (committed; `make RELEASE=1 FLAT_BANNER=1 cia` uses the
                         flat banner.png instead: what to install over a 3D banner that crashes the HOME Menu)
  preview/banner.glb     the same scene as glTF, to look at in Blender

The cat is the village's sleeping cat (Sprites/Characters/Village/cats.png), one sprite pixel thick. The logo is the high-res wordmark (Sprites/Misc/Slider_Logo_White_7988x1590.png) as thick as the cat: a
cut-out texture on its front and back (the back reads mirrored, like any real object), side walls traced from
its outline. Behind them a dark oval stays still, fading out to its edge (a blended texture: the one
translucent material).

Units are the banner camera's (pycgfx's banner-camera.gltf): at (0, 1, 44.786) looking down -z with a 30 degree
vertical fov, so z = 0 shows x -20..20 and y -11..13, and one unit is 10 screen px. Larger z pops out of the
screen with the 3D slider up, and the near plane is at z = 18.3.

What the HOME Menu needs, from 3dsDL's hardware notes (resources/banner/README.md and docs/HARDWARE.md in
github.com/ankushKun/3dsDL), all enforced here: rigid node animation only (skinning has frozen it), a model
named COMMON (pycgfx does that), each mesh naming its node (pycgfx leaves them blank: a crash), name lookups
that resolve, no near-zero scales, power-of-two textures up to 256,
under 2000 triangles or so, a CGFX under 512 KiB.

pycgfx goes through Euler angles and would tumble a full turn at +-90 degrees, so the spin's keys are written
straight into the converted curves.

On a console anything off the scene's center orbits it although it has no spin of its own (cause unknown; tracks for
the still bones didn't help, and a held rotation track crashes the HOME Menu). So the circle's bone sits on the spin
axis, a Y billboard (with a position-only track, as 3dsDL's) facing the viewer, and doesn't move; its quad is offset
behind the bone, so the billboard keeps it behind the art at every angle.

Usage: python tools/make_banner3d.py <pycgfx dir | --preview> [path to the Slider repo]
  --preview writes only preview/banner.glb, and needs just Pillow.
  pycgfx: github.com/skyfloogle/pycgfx at 1f78850086f3a77c41e07162e842f97a5bf3c18a (no licence file: not kept
  in this repo). Needs Python 3.12+ and, as its Pipfile.lock: dataclasses-json==0.6.7 gltflib==1.0.13
  marshmallow==3.26.1 mypy-extensions==1.1.0 packaging==25.0 pillow==11.2.1 typing-extensions==4.13.2
  typing-inspect==0.9.0
"""
import io
import json
import math
import os
import struct
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLIDER = os.path.join(os.path.dirname(REPO), "Slider")
SPRITES = os.path.join("Slider", "Assets", "Sprites")
CATS = os.path.join(SPRITES, "Characters", "Village", "cats.png")
LOGO = os.path.join(SPRITES, "Misc", "Slider_Logo_White_7988x1590.png")
OUT = os.path.join(REPO, "tools", "cia", "banner.cgfx")
PREVIEW = os.path.join(REPO, "preview", "banner.glb")

CAT_CELL = (0, 0, 16, 16)  # cats_0, a frame of Cat1IdleAsleep
CAT_VOXEL = 0.675
CAT_Y = -2.0
THICKNESS = CAT_VOXEL  # the cat's and the logo's
LOGO_WIDTH = 16.875
LOGO_Y = 4.5
LOGO_COLOR = (230, 230, 230)
LOGO_FACE = (255, 255, 255)  # multiplies the texture, which holds LOGO_COLOR
LOGO_TEXTURE = (256, 64)
LOGO_CONTOUR_WIDTH = 512  # the outline is traced at this size...
LOGO_CONTOUR_TOLERANCE = 1.5  # ...and simplified to this many of its pixels

CIRCLE_COLOR = (26, 26, 26)  # the game's #1a1a1a
CIRCLE_ALPHA = 0.85  # at its middle
CIRCLE_SOLID = 0.7  # how far out, as a share of its radius, the fade starts
CIRCLE_TEXTURE = 32  # small: stretched, its filtering hides the texture format's 16 alpha levels
CIRCLE_HALF = (19.6, 12.0)  # its quad; on a console a top edge at y 11.8 on screen was cut off, 10.2 now
CIRCLE_BEHIND = 10.0  # its quad behind its bone, past all that spins (the art's reach: LOGO_WIDTH / 2)
CIRCLE_Y = 0.27
SPINNING = ("Cat", "Logo")

SPIN_SECONDS = 6.0
SPIN_KEYS = 24
AMBIENT = 0.45
FULL_AMBIENT = ("Logo_ffffff", "Circle_ffffff")  # these show their own colors at any angle
LIGHT = (0.35, -0.5, -0.8)  # the way it travels: down, right, into the screen

MAX_TRIANGLES = 2000
MAX_CGFX = 0x80000


def pixels(path, box):
    """The opaque pixels in box, cropped to them: {(x, y): (r, g, b)}, y down, and the size."""
    image = Image.open(path).convert("RGBA").crop(box)
    left, top, right, bottom = image.getbbox()
    cells = {}
    for y in range(top, bottom):
        for x in range(left, right):
            r, g, b, a = image.getpixel((x, y))
            if a:
                cells[(x - left, y - top)] = (r, g, b)
    return cells, right - left, bottom - top


def merged_rects(cells):
    """Greedy rectangles covering a set of (x, y): [(x, y, width, height)]."""
    left = set(cells)
    rects = []
    for x, y in sorted(cells, key=lambda c: (c[1], c[0])):
        if (x, y) not in left:
            continue
        w = 1
        while (x + w, y) in left:
            w += 1
        h = 1
        while all((x + i, y + h) in left for i in range(w)):
            h += 1
        for j in range(h):
            for i in range(w):
                left.discard((x + i, y + j))
        rects.append((x, y, w, h))
    return rects


def merged_strips(cells, vertical):
    """Runs one cell wide: side faces only merge along their edge."""
    strips = []
    lines = {}
    for x, y in cells:
        lines.setdefault(x if vertical else y, []).append(y if vertical else x)
    for line, values in lines.items():
        values.sort()
        start = previous = values[0]
        for value in values[1:] + [None]:
            if value is None or value != previous + 1:
                run = previous - start + 1
                strips.append((line, start, 1, run) if vertical else (start, line, run, 1))
                start = value
            previous = value
    return strips


class Mesh:
    """Flat-shaded quads per color: {color: (vertices, indices)}, a vertex being position, normal, uv.
    A part of the color `textured` has uvs and shows `texture` (PNG bytes), cut out or `blended`."""

    def __init__(self):
        self.parts = {}
        self.texture = None
        self.textured = None
        self.blended = False

    def quad(self, color, corner, u, v, uvs=(None, None, None, None)):
        """Corner, then two edges; cross(u, v) is the way it faces. uvs go around from the corner."""
        normal = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        length = math.sqrt(sum(c * c for c in normal))
        normal = tuple(c / length for c in normal)
        vertices, indices = self.parts.setdefault(color, ([], []))
        base = len(vertices)
        for (a, b), uv in zip(((0, 0), (1, 0), (1, 1), (0, 1)), uvs):
            vertices.append((tuple(corner[i] + a * u[i] + b * v[i] for i in range(3)), normal, uv))
        indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))

    def triangles(self):
        return sum(len(indices) // 3 for _, indices in self.parts.values())


def extrude(cells, width, height, voxel):
    """A slab one voxel thick: cells {(x, y): color}, on both faces; centered on its origin, y up. Runs of faces
    that match are merged."""
    mesh = Mesh()
    X = lambda x: (x - width / 2) * voxel
    Y = lambda y: (height / 2 - y) * voxel
    z = voxel / 2

    groups = {}
    for c, color in cells.items():
        groups.setdefault(color, set()).add(c)
    for color, group in groups.items():
        for x, y, w, h in merged_rects(group):
            mesh.quad(color, (X(x), Y(y + h), z), (w * voxel, 0, 0), (0, h * voxel, 0))
            mesh.quad(color, (X(x + w), Y(y + h), -z), (-w * voxel, 0, 0), (0, h * voxel, 0))

    groups = {}
    for (x, y), color in cells.items():
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (x + dx, y + dy) not in cells:
                groups.setdefault((dx, dy, color), set()).add((x, y))
    for (dx, dy, color), group in groups.items():
        for x, y, w, h in merged_strips(group, dx != 0):
            if dx == 1:
                mesh.quad(color, (X(x + 1), Y(y + h), -z), (0, h * voxel, 0), (0, 0, 2 * z))
            elif dx == -1:
                mesh.quad(color, (X(x), Y(y), -z), (0, -h * voxel, 0), (0, 0, 2 * z))
            elif dy == 1:
                mesh.quad(color, (X(x), Y(y + 1), -z), (w * voxel, 0, 0), (0, 0, 2 * z))
            else:
                mesh.quad(color, (X(x + w), Y(y), -z), (-w * voxel, 0, 0), (0, 0, 2 * z))
    return mesh


def outline_loops(mask, width, height):
    """The closed outlines of a set of (x, y) pixels, as corner lists with the pixels on the left when walking
    them with y up (so the outward normal is to the right)."""
    following = {}
    for x, y in mask:
        if (x, y + 1) not in mask:
            following.setdefault((x, y + 1), []).append((x + 1, y + 1))
        if (x + 1, y) not in mask:
            following.setdefault((x + 1, y + 1), []).append((x + 1, y))
        if (x, y - 1) not in mask:
            following.setdefault((x + 1, y), []).append((x, y))
        if (x - 1, y) not in mask:
            following.setdefault((x, y), []).append((x, y + 1))
    loops = []
    while following:
        start = next(iter(following))
        loop = [start]
        at = start
        while True:
            ways = following[at]
            to = ways.pop()
            if not ways:
                del following[at]
            if to == start:
                break
            loop.append(to)
            at = to
        loops.append(loop)
    return loops


def simplified(loop, tolerance):
    """Douglas-Peucker over a closed loop, split at its first corner and the corner furthest from it."""
    far = max(range(len(loop)), key=lambda i: (loop[i][0] - loop[0][0]) ** 2 + (loop[i][1] - loop[0][1]) ** 2)
    keep = {0, far}
    stack = [(0, far), (far, len(loop))]
    while stack:
        a, b = stack.pop()
        ax, ay = loop[a]
        bx, by = loop[b % len(loop)]
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy) or 1.0
        worst, worst_at = 0.0, None
        for i in range(a + 1, b):
            off = abs((loop[i][0] - ax) * dy - (loop[i][1] - ay) * dx) / length
            if off > worst:
                worst, worst_at = off, i
        if worst > tolerance:
            keep.add(worst_at)
            stack += [(a, worst_at), (worst_at, b)]
    return [loop[i] for i in sorted(keep)]


def logo_texture(image):
    """Full logo -> PNG bytes for the texture, and how far down it the logo reaches (v)."""
    w, h = LOGO_TEXTURE
    logo_h = round(w * image.height / image.width)
    alpha = image.getchannel("A").resize((w, logo_h), Image.LANCZOS)
    mask = Image.new("L", (w, h), 0)
    mask.paste(alpha, (0, 0))
    # One color everywhere, so filtering at the cut-out's edge can't bring in a dark fringe.
    texture = Image.new("RGBA", (w, h), LOGO_COLOR + (0,))
    texture.putalpha(mask)
    png = io.BytesIO()
    texture.save(png, "PNG")
    return png.getvalue(), logo_h / h


def circle_texture():
    size = CIRCLE_TEXTURE
    texture = Image.new("RGBA", (size, size))
    for y in range(size):
        for x in range(size):
            out = math.hypot((x + 0.5) / size * 2 - 1, (y + 0.5) / size * 2 - 1)
            t = min(max((out - CIRCLE_SOLID) / (1 - CIRCLE_SOLID), 0.0), 1.0)
            fade = 1 - t * t * (3 - 2 * t)
            texture.putpixel((x, y), CIRCLE_COLOR + (round(255 * CIRCLE_ALPHA * fade),))
    png = io.BytesIO()
    texture.save(png, "PNG")
    return png.getvalue()


def build_circle():
    mesh = Mesh()
    mesh.texture = circle_texture()
    mesh.textured = (255, 255, 255)
    mesh.blended = True
    w, h = CIRCLE_HALF
    mesh.quad(mesh.textured, (-w, -h, -CIRCLE_BEHIND), (2 * w, 0, 0), (0, 2 * h, 0), ((0, 1), (1, 1), (1, 0), (0, 0)))
    return mesh


def build_cat(slider):
    cells, width, height = pixels(os.path.join(slider, CATS), CAT_CELL)
    return extrude(cells, width, height, CAT_VOXEL)


def build_logo(slider):
    image = Image.open(os.path.join(slider, LOGO)).convert("RGBA")
    image = image.crop(image.getbbox())
    half_w = LOGO_WIDTH / 2
    half_h = half_w * image.height / image.width
    z = THICKNESS / 2

    mesh = Mesh()
    mesh.texture, v = logo_texture(image)
    mesh.textured = LOGO_FACE
    mesh.quad(LOGO_FACE, (-half_w, -half_h, z), (2 * half_w, 0, 0), (0, 2 * half_h, 0),
              ((0, v), (1, v), (1, 0), (0, 0)))
    mesh.quad(LOGO_FACE, (half_w, -half_h, -z), (-2 * half_w, 0, 0), (0, 2 * half_h, 0),
              ((1, v), (0, v), (0, 0), (1, 0)))

    contour_h = round(LOGO_CONTOUR_WIDTH * image.height / image.width)
    alpha = image.getchannel("A").resize((LOGO_CONTOUR_WIDTH, contour_h), Image.LANCZOS)
    mask = {(x, y) for y in range(contour_h) for x in range(LOGO_CONTOUR_WIDTH) if alpha.getpixel((x, y)) >= 128}
    X = lambda x: (x / LOGO_CONTOUR_WIDTH * 2 - 1) * half_w
    Y = lambda y: (1 - y / contour_h * 2) * half_h
    for loop in outline_loops(mask, LOGO_CONTOUR_WIDTH, contour_h):
        corners = simplified(loop, LOGO_CONTOUR_TOLERANCE)
        for (ax, ay), (bx, by) in zip(corners, corners[1:] + corners[:1]):
            mesh.quad(LOGO_COLOR, (X(ax), Y(ay), -z), (X(bx) - X(ax), Y(by) - Y(ay), 0), (0, 0, 2 * z))
    return mesh


class Glb:
    def __init__(self):
        self.doc = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"name": "Banner", "nodes": []}], "nodes": [],
                    "meshes": [], "materials": [], "accessors": [], "bufferViews": [], "buffers": []}
        self.data = bytearray()
        self.channels = []
        self.samplers = []

    def view(self, payload, target=None):
        while len(self.data) % 4:
            self.data.append(0)
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(payload)}
        if target:
            view["target"] = target
        self.doc["bufferViews"].append(view)
        self.data += payload
        return len(self.doc["bufferViews"]) - 1

    def accessor(self, payload, component, count, kind, target=None, bounds=None):
        accessor = {"bufferView": self.view(payload, target), "componentType": component, "count": count, "type": kind}
        if bounds:
            accessor["min"], accessor["max"] = bounds
        self.doc["accessors"].append(accessor)
        return len(self.doc["accessors"]) - 1

    def texture(self, name, png):
        self.doc.setdefault("images", []).append({"name": name, "bufferView": self.view(png), "mimeType": "image/png"})
        self.doc.setdefault("samplers", []).append({"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071})
        self.doc.setdefault("textures", []).append({"source": len(self.doc["images"]) - 1,
                                                    "sampler": len(self.doc["samplers"]) - 1})
        return len(self.doc["textures"]) - 1

    def node(self, name, mesh, translation):
        primitives = []
        for color, (vertices, indices) in sorted(mesh.parts.items()):
            material = {"name": "%s_%02x%02x%02x" % (name, *color), "pbrMetallicRoughness": {
                "baseColorFactor": [c / 255 for c in color] + [1.0], "metallicFactor": 0.0, "roughnessFactor": 1.0}}
            positions = [v[0] for v in vertices]
            bounds = ([min(p[i] for p in positions) for i in range(3)], [max(p[i] for p in positions) for i in range(3)])
            attributes = {
                "POSITION": self.accessor(b"".join(struct.pack("<3f", *p) for p in positions), 5126, len(vertices), "VEC3", 34962, bounds),
                "NORMAL": self.accessor(b"".join(struct.pack("<3f", *v[1]) for v in vertices), 5126, len(vertices), "VEC3", 34962),
            }
            if color == mesh.textured:
                material["pbrMetallicRoughness"]["baseColorTexture"] = {"index": self.texture(name, mesh.texture)}
                material["alphaMode"] = "BLEND" if mesh.blended else "MASK"
                if not mesh.blended:
                    material["alphaCutoff"] = 0.5
                attributes["TEXCOORD_0"] = self.accessor(b"".join(struct.pack("<2f", *v[2]) for v in vertices), 5126, len(vertices), "VEC2", 34962)
            self.doc["materials"].append(material)
            index = self.accessor(struct.pack("<%dH" % len(indices), *indices), 5123, len(indices), "SCALAR", 34963)
            primitives.append({"attributes": attributes, "indices": index, "material": len(self.doc["materials"]) - 1})
        self.doc["meshes"].append({"name": name, "primitives": primitives})
        self.doc["nodes"].append({"name": name, "mesh": len(self.doc["meshes"]) - 1, "translation": list(translation)})
        self.doc["scenes"][0]["nodes"].append(len(self.doc["nodes"]) - 1)
        return len(self.doc["nodes"]) - 1

    def hold(self, node, position):
        """A position-only track that doesn't move."""
        times = struct.pack("<2f", 0.0, SPIN_SECONDS)
        time_accessor = self.accessor(times, 5126, 2, "SCALAR", bounds=([0.0], [SPIN_SECONDS]))
        value_accessor = self.accessor(struct.pack("<3f", *position) * 2, 5126, 2, "VEC3")
        self.samplers.append({"input": time_accessor, "output": value_accessor, "interpolation": "LINEAR"})
        self.channels.append({"sampler": len(self.samplers) - 1, "target": {"node": node, "path": "translation"}})

    def spin(self, node):
        times = [SPIN_SECONDS * i / SPIN_KEYS for i in range(SPIN_KEYS + 1)]
        keys = struct.pack("<%df" % len(times), *times)
        half_angles = [math.pi * i / SPIN_KEYS for i in range(SPIN_KEYS + 1)]
        values = b"".join(struct.pack("<4f", 0.0, math.sin(a), 0.0, math.cos(a)) for a in half_angles)
        time_accessor = self.accessor(keys, 5126, len(times), "SCALAR", bounds=([times[0]], [times[-1]]))
        value_accessor = self.accessor(values, 5126, len(half_angles), "VEC4")
        self.samplers.append({"input": time_accessor, "output": value_accessor, "interpolation": "LINEAR"})
        self.channels.append({"sampler": len(self.samplers) - 1, "target": {"node": node, "path": "rotation"}})

    def write(self, path):
        self.doc["animations"] = [{"name": "Banner", "channels": self.channels, "samplers": self.samplers}]
        while len(self.data) % 4:
            self.data.append(0)
        self.doc["buffers"].append({"byteLength": len(self.data)})
        text = json.dumps(self.doc, separators=(",", ":")).encode()
        text += b" " * (-len(text) % 4)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(self.data)))
            f.write(struct.pack("<I4s", len(text), b"JSON") + text)
            f.write(struct.pack("<I4s", len(self.data), b"BIN\0") + self.data)


def unreachable_names(info):
    """Names a console's Patricia walk of this table wouldn't land on."""
    nodes = info.dict.nodes

    def bit_of(name, index):
        raw = name.encode()
        return index // 8 < len(raw) and raw[index // 8] >> (index % 8) & 1

    def ref(i):
        return math.inf if i == 0 else nodes[i].refbit

    missing = []
    for node in nodes[1:]:
        at, to = 0, nodes[0].left_index
        while ref(at) > ref(to):
            at = to
            to = nodes[at].right_index if bit_of(node.name, nodes[at].refbit) else nodes[at].left_index
        if nodes[to] is not node:
            missing.append(node.name)
    return missing


def convert(pycgfx_dir, glb_path):
    sys.path.insert(0, pycgfx_dir)
    import gltflib
    import main as pycgfx
    from cgfx.canm import StepLinear64Key
    from cgfx.shared import ColorFloat, Vector3
    from cgfx.sobj import BillboardMode

    cgfx = pycgfx.convert_gltf(gltflib.GLTF.load(glb_path, load_file_resources=True))
    model = cgfx.data.models["COMMON"]
    model.skeleton.bones.dict.regenerate()

    bones = list(model.skeleton.bones)
    materials = list(model.materials)
    for mesh in model.meshes.data.contents:
        shape = model.shapes.data.contents[mesh.shape_index]
        bone = bones[shape.primitive_sets.data.contents[0].related_bones.data.contents[0]]
        mesh.mesh_node_name = bone
        mesh.name = shape.name = "%s_%s" % (bone, materials[mesh.material_index])

    model.skeleton.bones["Circle"].billboard_mode = BillboardMode.YAxial
    model.materials["Circle_ffffff"].transluscency_kind = 1
    for name in model.materials:
        level = 1.0 if name in FULL_AMBIENT else AMBIENT
        model.materials[name].material_color.ambient = ColorFloat(level, level, level, 1)
    length = math.sqrt(sum(c * c for c in LIGHT))
    for name in cgfx.data.lights:
        cgfx.data.lights[name].position_or_direction = Vector3(*(c / length for c in LIGHT))

    for name in SPINNING:
        bone = cgfx.data.skeletal_animations["COMMON"].member_animations_data[name]
        frames = [SPIN_SECONDS * 60 * i / SPIN_KEYS for i in range(SPIN_KEYS + 1)]
        bone.rot_x.segments[0].keys = [StepLinear64Key(f, 0.0) for f in frames]
        bone.rot_y.segments[0].keys = [StepLinear64Key(f, 2 * math.pi * i / SPIN_KEYS) for i, f in enumerate(frames)]
        bone.rot_z.segments[0].keys = [StepLinear64Key(f, 0.0) for f in frames]

    tables = {"models": cgfx.data.models, "textures": cgfx.data.textures, "luts": cgfx.data.lookup_tables,
              "lights": cgfx.data.lights, "scenes": cgfx.data.scenes, "animations": cgfx.data.skeletal_animations,
              "bones": model.skeleton.bones, "materials": model.materials,
              "animation groups": model.animation_group_descriptions,
              "tracks": cgfx.data.skeletal_animations["COMMON"].member_animations_data,
              "bone members": model.animation_group_descriptions["SkeletalAnimation"].members}
    for label, table in tables.items():
        missing = unreachable_names(table)
        if missing:
            sys.exit("%s: a lookup wouldn't find %s" % (label, ", ".join(missing)))
    for mesh in model.meshes.data.contents:
        if mesh.mesh_node_name not in bones:
            sys.exit("mesh %s names no node" % mesh.name)
    return pycgfx.write(cgfx), [mesh.name for mesh in model.meshes.data.contents]


def main(pycgfx_dir, slider):
    circle, cat, logo = build_circle(), build_cat(slider), build_logo(slider)
    triangles = circle.triangles() + cat.triangles() + logo.triangles()
    if triangles > MAX_TRIANGLES:
        sys.exit("%d triangles (the HOME Menu's safe limit is about %d)" % (triangles, MAX_TRIANGLES))

    glb = Glb()
    circle_at = (0, CIRCLE_Y, 0)
    glb.hold(glb.node("Circle", circle, circle_at), circle_at)
    glb.spin(glb.node("Cat", cat, (0, CAT_Y, 0)))
    glb.spin(glb.node("Logo", logo, (0, LOGO_Y, 0)))
    glb.write(PREVIEW)
    if pycgfx_dir == "--preview":
        print("preview/banner.glb: %d triangles (cat %d, logo %d)" % (triangles, cat.triangles(), logo.triangles()))
        return

    data, meshes = convert(pycgfx_dir, PREVIEW)
    expected = len(circle.parts) + len(cat.parts) + len(logo.parts)
    if len(meshes) != expected:
        sys.exit("%d meshes converted, %d built" % (len(meshes), expected))
    if len(data) > MAX_CGFX:
        sys.exit("CGFX is %d bytes (limit %d)" % (len(data), MAX_CGFX))
    with open(OUT, "wb") as f:
        f.write(data)
    print("banner.cgfx: %d bytes, %d triangles (cat %d, logo %d), meshes: %s" % (
        len(data), triangles, cat.triangles(), logo.triangles(), ", ".join(meshes)))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else SLIDER)
