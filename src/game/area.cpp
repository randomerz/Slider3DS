#include "game/area.h"

#include <algorithm>

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

static bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
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
    for (const StaticSprite& st : a.statics) {
        if (!ownerOk(st.owner)) return fail(error, "static sprite " + st.path + ": unknown owner STile");
        if (st.sprite < 0 || st.sprite >= (int)a.sprites.size())
            return fail(error, "static sprite " + st.path + ": bad sprite index");
    }
    if (a.artifact.present) {
        const ArtifactDef& d = a.artifact;
        auto spriteOk = [&](int s) { return s >= 0 && s < (int)a.sprites.size(); };
        bool ok = spriteOk(d.frame.sprite) && spriteOk(d.background.sprite) && spriteOk(d.pressed.sprite) &&
                  spriteOk(d.highlight.sprite) && spriteOk(d.hover.sprite) && spriteOk(d.empty) &&
                  spriteOk(d.blank) && spriteOk(d.hoverIcon) && d.buttonSpacing > 0;
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

    for (const Json& s : doc["staticSprites"].items()) {
        StaticSprite st;
        st.path = s["path"].asString();
        st.owner = s["owner"].asInt();
        st.sprite = s["sprite"].asInt(-1);
        st.x = s["pos"].at(0).asFloat();
        st.y = s["pos"].at(1).asFloat();
        st.sortLayer = s["sortingLayerValue"].asInt();
        st.order = s["order"].asInt();
        st.flags = (uint32_t)s["flip"].asInt() & 3u; // bit0 X, bit1 Y = plat::FLIP_X | FLIP_Y
        st.active = s["active"].asBool(true);
        out.statics.push_back(std::move(st));
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

    // Unity's transparency sort for sprites: sorting layer, order, then (sort axis Y) higher y first.
    std::stable_sort(out.statics.begin(), out.statics.end(), [](const StaticSprite& a, const StaticSprite& b) {
        if (a.sortLayer != b.sortLayer) return a.sortLayer < b.sortLayer;
        if (a.order != b.order) return a.order < b.order;
        return a.y > b.y;
    });

    // Unity draws by (sorting layer, order); stable keeps file order within equal keys.
    std::stable_sort(out.layers.begin(), out.layers.end(), [](const TileLayer& a, const TileLayer& b) {
        return a.sortLayer != b.sortLayer ? a.sortLayer < b.sortLayer : a.order < b.order;
    });
    return true;
}
