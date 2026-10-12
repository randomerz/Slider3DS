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

void Area::ownerBase(int owner, float& x, float& y) const {
    const STileDef* s = owner != 0 ? stile(owner) : nullptr;
    x = s ? s->posX * s->width : 0.0f;
    y = s ? s->posY * s->width : 0.0f;
}

static bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
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

    for (const Json& s : doc["stiles"].items()) {
        STileDef d{s["islandId"].asInt(), s["name"].asString(), s["x"].asInt(), s["y"].asInt(),
                   s["width"].asInt(), s["isTileActive"].asBool()};
        d.startX = d.x;
        d.startY = d.y;
        d.startActive = d.active;
        d.posX = (float)d.x;
        d.posY = (float)d.y;
        out.stiles.push_back(std::move(d));
    }

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

    // Unity draws by (sorting layer, order); stable keeps file order within equal keys.
    std::stable_sort(out.layers.begin(), out.layers.end(), [](const TileLayer& a, const TileLayer& b) {
        return a.sortLayer != b.sortLayer ? a.sortLayer < b.sortLayer : a.order < b.order;
    });
    return true;
}
