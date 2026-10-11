#include "game/area.h"

#include <algorithm>

#include "game/json.h"
#include "platform/platform.h"

const STileDef* Area::stile(int islandId) const {
    for (const STileDef& s : stiles)
        if (s.islandId == islandId) return &s;
    return nullptr;
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

    for (const Json& s : doc["stiles"].items())
        out.stiles.push_back({s["islandId"].asInt(), s["name"].asString(), s["x"].asInt(), s["y"].asInt(),
                              s["width"].asInt(), s["isTileActive"].asBool()});

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
