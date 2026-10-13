#include "game/area.h"

#include <algorithm>
#include <cmath>

#include "game/json.h"
#include "platform/platform.h"

const STileDef* Area::stile(int islandId) const {
    for (const STileDef& s : stiles)
        if (s.islandId == islandId) return &s;
    return nullptr;
}

const Spawn* Area::spawn(const char* name) const {
    for (const Spawn& s : spawns)
        if (s.name == name) return &s;
    return nullptr;
}

int Area::findObject(const std::string& path) const {
    for (size_t i = 0; i < objects.size(); i++)
        if (objects[i].path == path) return (int)i;
    return -1;
}

int Anim::frameAt(float t) const {
    if (frames.empty()) return -1;
    if (length > 0) t = loop ? std::fmod(t, length) : std::min(t, length);
    if (t < 0) t += length;
    size_t i = 0;
    while (i + 1 < times.size() && times[i + 1] <= t) i++;
    return frames[i];
}

static bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
}

// A sprite placement: "sprite", "pos", "sortY", "sortingLayerValue", "order", optional "flip", "anim".
static SpritePlacement readPlacement(const Json& j) {
    SpritePlacement p;
    p.sprite = j["sprite"].asInt(-1);
    p.anim = j["anim"].asInt(-1);
    p.x = j["pos"].at(0).asFloat();
    p.y = j["pos"].at(1).asFloat();
    p.sortY = j["sortY"].asFloat(p.y);
    p.sortLayer = j["sortingLayerValue"].asInt();
    p.order = j["order"].asInt();
    p.flags = (uint32_t)j["flip"].asInt() & 3u; // bit0 X, bit1 Y = plat::FLIP_X | FLIP_Y
    return p;
}

static void readCalls(const Json& j, std::vector<SetActiveCall>& out) {
    for (const Json& c : j.items())
        if (c.has("target")) out.push_back({c["target"].asInt(-1), c["active"].asBool()});
}

static ConditionDef readCondition(const Json& c) {
    using T = ConditionDef::Type;
    ConditionDef d;
    const std::string& type = c["type"].asString();
    if (type == "item") {
        d.type = T::Item;
        d.name = c["item"].asString();
        d.itemArea = c["itemArea"].asString();
    } else if (type == "grid" || type == "noGrid" || type == "gridStationary") {
        d.type = type == "grid" ? T::Grid : type == "noGrid" ? T::NoGrid : T::GridStationary;
        d.pattern = c["pattern"].asString();
        for (const Json& t : c["stationaryTiles"].items()) d.stationaryTiles.push_back(t.asInt());
    } else if (type == "flag" || type == "noFlag") {
        d.type = type == "flag" ? T::Flag : T::NoFlag;
        d.name = c["flag"].asString();
    } else if (type == "playerCarryingItem") {
        d.type = T::PlayerCarryingItem;
        d.name = c["playerItem"].asString();
    }
    return d;
}

static void readStartCheck(const Json& j, StartCheck& sc) {
    sc.object = j["object"].asInt(-1);
    sc.enabled = j["enabled"].asBool(true);
    for (const Json& c : j["conditions"].items()) sc.conditionals.conditions.push_back(readCondition(c));
    readCalls(j["onSuccess"], sc.conditionals.onSuccess);
    readCalls(j["onFail"], sc.conditionals.onFail);
}

static Hitbox readHitbox(const Json& j) {
    return {j["offset"].at(0).asFloat(), j["offset"].at(1).asFloat(), j["size"].at(0).asFloat(), j["size"].at(1).asFloat()};
}

static void readObjectCollider(const Json& j, ObjectCollider& c) {
    c.object = j["object"].asInt(-1);
    c.owner = j["owner"].asInt();
    c.enabled = j["enabled"].asBool(true);
    const std::string& kind = j["kind"].asString();
    auto readPoints = [](const Json& pts) {
        Polygon p;
        for (const Json& v : pts.items()) p.push_back(v.asFloat());
        return p;
    };
    if (kind == "circle") {
        c.kind = ObjectCollider::Kind::Circle;
        c.cx = j["center"].at(0).asFloat();
        c.cy = j["center"].at(1).asFloat();
        c.radius = j["radius"].asFloat();
        c.minX = c.cx - c.radius;
        c.maxX = c.cx + c.radius;
        c.minY = c.cy - c.radius;
        c.maxY = c.cy + c.radius;
        return;
    }
    if (kind == "edge") {
        c.kind = ObjectCollider::Kind::Edge;
        c.paths.push_back(readPoints(j["points"]));
    } else {
        c.kind = ObjectCollider::Kind::Poly; // "none" (unsupported shape) loads as no paths
        for (const Json& path : j["paths"].items()) c.paths.push_back(readPoints(path));
    }
    bool first = true;
    for (const Polygon& p : c.paths) {
        for (size_t i = 0; i + 1 < p.size(); i += 2) {
            c.minX = first ? p[i] : std::min(c.minX, p[i]);
            c.maxX = first ? p[i] : std::max(c.maxX, p[i]);
            c.minY = first ? p[i + 1] : std::min(c.minY, p[i + 1]);
            c.maxY = first ? p[i + 1] : std::max(c.maxY, p[i + 1]);
            first = false;
        }
    }
}

static UiPart readPart(const Json& j) {
    UiPart p;
    p.sprite = j["sprite"].asInt(-1);
    p.x = j["rect"].at(0).asInt();
    p.y = j["rect"].at(1).asInt();
    p.w = j["rect"].at(2).asInt();
    p.h = j["rect"].at(3).asInt();
    return p;
}

static void readArtifact(const Json& j, ArtifactDef& a) {
    a.present = !j.isNull();
    if (!a.present) return;
    a.buttonSpacing = j["buttonSpacing"].asInt(37);
    a.frame = readPart(j["frame"]);
    a.background = readPart(j["background"]);
    for (const Json& u : j["underlays"].items()) a.underlays.push_back(readPart(u));
    a.pressed = readPart(j["parts"]["pressed"]);
    a.highlight = readPart(j["parts"]["highlight"]);
    a.hover = readPart(j["parts"]["hover"]);
    a.empty = j["empty"].asInt(-1);
    a.blank = j["blank"].asInt(-1);
    a.hoverIcon = j["hoverIcon"].asInt(-1);
    a.pressedDrop = j["pressedDrop"].asInt(1);
    a.nudgeDistance = j["nudge"]["distance"].asFloat(2);
    a.nudgeDuration = j["nudge"]["duration"].asFloat(0.2f);
    for (const Json& b : j["buttons"].items()) {
        ArtifactButtonDef d;
        d.islandId = b["islandId"].asInt();
        d.island = b["island"].asInt(-1);
        d.completed = b["completed"].asInt(-1);
        for (const Json& icon : b["icons"].items()) d.icons.push_back(readPart(icon));
        a.buttons.push_back(std::move(d));
    }
}

static bool validate(const Area& a, std::string* error) {
    if (a.gridW <= 0 || a.gridH <= 0 || (int)a.stiles.size() != a.gridW * a.gridH)
        return fail(error, "grid: expected " + std::to_string(a.gridW * a.gridH) + " stiles");
    std::vector<bool> slotUsed(a.gridW * a.gridH, false);
    for (const STileDef& s : a.stiles) {
        if (s.islandId <= 0 || s.width <= 0 || s.x < 0 || s.x >= a.gridW || s.y < 0 || s.y >= a.gridH ||
            slotUsed[s.y * a.gridW + s.x])
            return fail(error, "stile " + std::to_string(s.islandId) + ": bad id, width or slot");
        slotUsed[s.y * a.gridW + s.x] = true;
    }
    auto ownerOk = [&](int owner) { return owner == 0 || a.stile(owner) != nullptr; };
    for (const TileLayer& L : a.layers)
        if (!ownerOk(L.owner)) return fail(error, "tilemap " + L.path + ": unknown owner STile");
    for (const CollisionLayer& C : a.collision)
        if (!ownerOk(C.owner)) return fail(error, "tilemap " + C.path + ": unknown owner STile");
    for (const Spawn& sp : a.spawns)
        if (!ownerOk(sp.owner)) return fail(error, "spawn " + sp.name + ": unknown owner STile");
    auto spriteIndexOk = [&](int s) { return s >= 0 && s < (int)a.sprites.size(); };
    auto animOk = [&](int i) { return i == -1 || (i >= 0 && i < (int)a.anims.size()); };
    auto objectOk = [&](int i) { return i >= 0 && i < (int)a.objects.size(); };
    // World sprites are drawn 1:1 (SpriteSheets draws native pixels), so they must match the area's ppu.
    auto placementOk = [&](const SpritePlacement& p) {
        return spriteIndexOk(p.sprite) && animOk(p.anim) && a.sprites[p.sprite].ppu == (float)a.ppu;
    };
    for (size_t i = 0; i < a.objects.size(); i++)
        if (a.objects[i].parent < -1 || a.objects[i].parent >= (int)i)
            return fail(error, "object " + a.objects[i].path + ": parent must come before it");
    for (const Anim& an : a.anims) {
        if (an.frames.empty() || an.frames.size() != an.times.size())
            return fail(error, "anim " + an.name + ": frames/times mismatch");
        for (int f : an.frames)
            if (!spriteIndexOk(f) || a.sprites[f].ppu != (float)a.ppu) return fail(error, "anim " + an.name + ": bad sprite");
    }
    for (const WorldSprite& ws : a.worldSprites) {
        if (!objectOk(ws.object)) return fail(error, "world sprite: bad object index");
        if (!ownerOk(ws.owner)) return fail(error, "world sprite " + a.pathOf(ws.object) + ": unknown owner STile");
        if (!placementOk(ws.place))
            return fail(error, "world sprite " + a.pathOf(ws.object) + ": bad sprite, anim or ppu");
    }
    for (const ObjectCollider& c : a.objectColliders) {
        if (!objectOk(c.object)) return fail(error, "collider: bad object index");
        if (!ownerOk(c.owner)) return fail(error, "collider " + a.pathOf(c.object) + ": unknown owner STile");
        for (const Polygon& p : c.paths)
            if (p.size() % 2 != 0) return fail(error, "collider " + a.pathOf(c.object) + ": odd point list");
    }
    for (const StartCheck& sc : a.startChecks) {
        if (!objectOk(sc.object)) return fail(error, "start check: bad object index");
        for (const auto* calls : {&sc.conditionals.onSuccess, &sc.conditionals.onFail})
            for (const SetActiveCall& call : *calls)
                if (!objectOk(call.target)) return fail(error, "start check " + a.pathOf(sc.object) + ": bad SetActive target");
    }
    for (const SpritePlacement* p : {&a.player.body, &a.player.boat})
        if (p->sprite != -1 && !placementOk(*p)) return fail(error, "player: bad sprite, anim or ppu");
    for (const TileDef& t : a.tiles)
        for (int f : t.frames)
            if (spriteIndexOk(f) && a.sprites[f].ppu != (float)a.ppu) return fail(error, "tile: sprite ppu differs from the area's");
    if (a.artifact.present) {
        const ArtifactDef& d = a.artifact;
        auto spriteOk = [&](int s) { return s >= 0 && s < (int)a.sprites.size(); };
        bool ok = spriteOk(d.frame.sprite) && spriteOk(d.background.sprite) && spriteOk(d.pressed.sprite) &&
                  spriteOk(d.highlight.sprite) && spriteOk(d.hover.sprite) && spriteOk(d.empty) &&
                  spriteOk(d.blank) && spriteOk(d.hoverIcon) && d.buttonSpacing > 0;
        for (const UiPart& u : d.underlays) ok = ok && spriteOk(u.sprite);
        for (const ArtifactButtonDef& b : d.buttons) {
            ok = ok && a.stile(b.islandId) && spriteOk(b.island) && spriteOk(b.completed);
            for (const UiPart& icon : b.icons) ok = ok && spriteOk(icon.sprite);
        }
        for (const STileDef& s : a.stiles) ok = ok && d.button(s.islandId);
        if (!ok) return fail(error, "artifact: bad sprite index or button set");
    }
    for (const SpriteDef& sp : a.sprites)
        if (sp.sheet < 0 || sp.sheet >= (int)a.sheetNames.size()) return fail(error, "sprite: bad sheet index");
    for (const TileDef& t : a.tiles)
        for (int f : t.frames)
            if (f < 0 || f >= (int)a.sprites.size()) return fail(error, "tile: bad sprite index");
    for (const TileLayer& L : a.layers) {
        for (int16_t c : L.cells)
            if (c < -1 || c >= (int)a.tiles.size()) return fail(error, "tilemap " + L.path + ": bad tile index");
        if (!L.xform.empty() && L.xform.size() != L.cells.size())
            return fail(error, "tilemap " + L.path + ": xform size mismatch");
    }
    for (const CollisionLayer& C : a.collision)
        for (int16_t c : C.cells)
            if (c < -1 || c >= (int)a.shapes.size()) return fail(error, "tilemap " + C.path + ": bad shape index");
    return true;
}

bool loadArea(const char* path, Area& out, std::string* error) {
    std::string text;
    if (!plat::readFile(path, text))
        return fail(error, std::string("can't read ") + path);

    Json doc;
    std::string parseError;
    if (!Json::parse(text, doc, &parseError))
        return fail(error, std::string(path) + ": " + parseError);
    text.clear();
    text.shrink_to_fit();

    out = Area();
    out.ppu = doc["ppu"].asInt(16);
    out.gridW = doc["grid"]["width"].asInt();
    out.gridH = doc["grid"]["height"].asInt();
    out.targetGrid = doc["grid"]["targetGrid"].asString();
    out.housingOffset = doc["grid"]["housingOffset"].asInt();

    for (const Json& s : doc["stiles"].items())
        out.stiles.push_back({s["islandId"].asInt(), s["name"].asString(), s["x"].asInt(), s["y"].asInt(),
                              s["width"].asInt(), s["isTileActive"].asBool()});

    for (int i = 0; i < 3; i++) out.background[i] = (uint8_t)doc["background"].at(i).asInt();

    for (const Json& o : doc["objects"].items())
        out.objects.push_back({o["path"].asString(), o["parent"].asInt(-1), o["active"].asBool(true)});

    for (const Json& s : doc["worldSprites"].items()) {
        WorldSprite ws;
        ws.object = s["object"].asInt(-1);
        ws.owner = s["owner"].asInt();
        ws.place = readPlacement(s);
        ws.animStart = s["animStart"].asFloat();
        ws.animRandom = s["animRandom"].asFloat();
        out.worldSprites.push_back(std::move(ws));
    }

    for (const Json& a : doc["anims"].items()) {
        Anim an;
        an.name = a["name"].asString();
        for (const Json& f : a["frames"].items()) an.frames.push_back(f.asInt());
        for (const Json& t : a["times"].items()) an.times.push_back(t.asFloat());
        an.length = a["length"].asFloat();
        an.loop = a["loop"].asBool(true);
        out.anims.push_back(std::move(an));
    }

    for (const Json& c : doc["objectColliders"].items()) {
        ObjectCollider oc;
        readObjectCollider(c, oc);
        out.objectColliders.push_back(std::move(oc));
    }

    for (const Json& c : doc["startChecks"].items()) {
        StartCheck sc;
        readStartCheck(c, sc);
        out.startChecks.push_back(std::move(sc));
    }

    if (doc.has("player")) {
        if (doc["player"].has("body")) out.player.body = readPlacement(doc["player"]["body"]);
        if (doc["player"].has("boat")) out.player.boat = readPlacement(doc["player"]["boat"]);
        out.player.footBox = readHitbox(doc["player"]["footBox"]);
        out.player.boatBox = readHitbox(doc["player"]["boatBox"]);
    }

    readArtifact(doc["artifact"], out.artifact);

    for (const Json& s : doc["spawns"].items())
        out.spawns.push_back({s["name"].asString(), s["owner"].asInt(), s["pos"].at(0).asFloat(),
                              s["pos"].at(1).asFloat(), s["inBoat"].asBool()});

    for (const Json& shape : doc["collisionShapes"].items()) {
        std::vector<Polygon> polys;
        for (const Json& poly : shape.items()) {
            Polygon p;
            for (const Json& v : poly.items()) p.push_back(v.asFloat());
            polys.push_back(std::move(p));
        }
        out.shapes.push_back(std::move(polys));
    }

    for (const Json& sh : doc["sheets"].items())
        out.sheetNames.push_back(sh["name"].asString());

    for (const Json& s : doc["sprites"].items()) {
        const Json& r = s["rect"];
        out.sprites.push_back({s["sheet"].asInt(), r.at(0).asInt(), r.at(1).asInt(), r.at(2).asInt(), r.at(3).asInt(),
                               s["pivot"].at(0).asFloat(), s["pivot"].at(1).asFloat(), s["ppu"].asFloat(16)});
    }

    for (const Json& t : doc["tiles"].items()) {
        TileDef def;
        for (const Json& f : t["frames"].items()) def.frames.push_back(f.asInt());
        def.fps = t["fps"].asFloat();
        out.tiles.push_back(std::move(def));
    }

    for (const Json& t : doc["tilemaps"].items()) {
        if (t["kind"].asString() != "collision" || !t["enabled"].asBool() || t["trigger"].asBool() ||
            t["size"].at(0).asInt() == 0)
            continue;
        CollisionLayer C;
        C.path = t["path"].asString();
        C.owner = t["owner"].asInt();
        C.active = t["active"].asBool(true);
        const std::string& mode = t["mode"].asString();
        C.mode = mode == "onFoot" ? CollisionMode::OnFoot : mode == "onWater" ? CollisionMode::OnWater : CollisionMode::Always;
        C.originX = t["origin"].at(0).asFloat();
        C.originY = t["origin"].at(1).asFloat();
        C.minX = t["cellMin"].at(0).asInt();
        C.minY = t["cellMin"].at(1).asInt();
        C.w = t["size"].at(0).asInt();
        C.h = t["size"].at(1).asInt();
        C.cells.reserve(C.w * C.h);
        for (const Json& c : t["cells"].items()) C.cells.push_back((int16_t)c.asInt());
        if ((int)C.cells.size() != C.w * C.h)
            return fail(error, "tilemap " + C.path + ": cell count mismatch");
        out.collision.push_back(std::move(C));
    }

    for (const Json& t : doc["tilemaps"].items()) {
        if (t["kind"].asString() != "visual" || !t["render"]["enabled"].asBool() || t["size"].at(0).asInt() == 0)
            continue;
        TileLayer L;
        L.path = t["path"].asString();
        L.owner = t["owner"].asInt();
        L.active = t["active"].asBool(true);
        L.originX = t["origin"].at(0).asFloat();
        L.originY = t["origin"].at(1).asFloat();
        L.anchorX = t["tileAnchor"].at(0).asFloat(0.5f);
        L.anchorY = t["tileAnchor"].at(1).asFloat(0.5f);
        L.sortLayer = t["render"]["sortingLayerValue"].asInt();
        L.order = t["render"]["order"].asInt();
        L.minX = t["cellMin"].at(0).asInt();
        L.minY = t["cellMin"].at(1).asInt();
        L.w = t["size"].at(0).asInt();
        L.h = t["size"].at(1).asInt();
        L.cells.reserve(L.w * L.h);
        for (const Json& c : t["cells"].items()) L.cells.push_back((int16_t)c.asInt());
        if (t.has("xform"))
            for (const Json& x : t["xform"].items()) L.xform.push_back((uint8_t)x.asInt());
        if ((int)L.cells.size() != L.w * L.h)
            return fail(error, "tilemap " + L.path + ": cell count mismatch");
        out.layers.push_back(std::move(L));
    }

    if (!validate(out, error)) return false;

    // Unity draws by (sorting layer, order); stable keeps file order within equal keys.
    std::stable_sort(out.layers.begin(), out.layers.end(), [](const TileLayer& a, const TileLayer& b) {
        return a.sortLayer != b.sortLayer ? a.sortLayer < b.sortLayer : a.order < b.order;
    });
    return true;
}
