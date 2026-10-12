#include <string>

#include "game/area.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr size_t VILLAGE_LAYERS = 76, VILLAGE_COLLISION = 35, VILLAGE_SHAPES = 9, VILLAGE_TILES = 168,
                 VILLAGE_SPRITES = 514, VILLAGE_FILLED_CELLS = 27554;

// A minimal valid area: 1x1 grid, one sheet/sprite/tile, one visual and one collision tilemap, a
// world sprite with a one-frame anim, an object collider and the player.
// Each `replace` pair breaks one thing for the validation tests.
std::string fixture(const std::string& from = "", const std::string& to = "") {
    std::string s = R"json({
  "ppu": 16,
  "grid": {"width": 1, "height": 1, "targetGrid": "1", "housingOffset": -150},
  "stiles": [{"islandId": 1, "name": "STile (1)", "x": 0, "y": 0, "width": 17, "isTileActive": true}],
  "spawns": [{"name": "Default", "owner": 1, "pos": [0, 0], "inBoat": false}],
  "collisionShapes": [[[0, 0, 1, 0, 1, 1, 0, 1]]],
  "sheets": [{"name": "sheetA"}],
  "sprites": [{"sheet": 0, "rect": [0, 0, 16, 16], "pivot": [8, 8], "ppu": 16}],
  "tiles": [{"frames": [0], "fps": 0}],
  "anims": [{"name": "Idle", "frames": [0], "times": [0], "length": 1, "loop": true}],
  "worldSprites": [{"path": "WS", "owner": 1, "sprite": 0, "pos": [1, 2], "sortY": 2, "sortingLayerValue": 7,
                    "order": 0, "flip": 1, "active": true, "anim": 0}],
  "objectColliders": [{"path": "OC", "owner": 1, "active": true, "kind": "poly", "paths": [[0, 0, 1, 0, 1, 1, 0, 1]]},
                      {"path": "CI", "owner": 0, "active": true, "kind": "circle", "center": [3, 4], "radius": 0.5}],
  "player": {"body": {"sprite": 0, "pos": [0, 0], "sortY": 0.5, "sortingLayerValue": 7, "order": 0, "anim": 0}},
  "tilemaps": [
    {"kind": "visual", "path": "VIS", "owner": 1, "active": true, "origin": [0, 0], "tileAnchor": [0.5, 0.5],
     "render": {"enabled": true, "sortingLayerValue": 0, "order": 0}, "cellMin": [0, 0], "size": [2, 1],
     "cells": [0, -1], "xform": [0, 0]},
    {"kind": "collision", "path": "COL", "owner": 1, "active": true, "enabled": true, "trigger": false,
     "mode": "always", "origin": [0, 0], "cellMin": [0, 0], "size": [1, 1], "cells": [0]}
  ]
})json";
    if (!from.empty()) {
        size_t at = s.find(from);
        if (at == std::string::npos) return "fixture: pattern not found: " + from;
        s.replace(at, from.size(), to);
    }
    return s;
}

bool loadFixture(const std::string& json, Area& out, std::string& err) {
    testSetFile("fixture/fixture.json", json);
    bool ok = loadArea("fixture/fixture.json", out, &err);
    testClearFiles();
    return ok;
}

} // namespace

TEST(area_fixture_loads) {
    Area a;
    std::string err;
    check(loadFixture(fixture(), a, err), "valid fixture loads: " + err);
    CHECK(a.stiles.size() == 1 && a.layers.size() == 1 && a.collision.size() == 1 && a.shapes.size() == 1);
    CHECK(a.layers.size() == 1 && a.layers[0].cell(0, 0) == 0 && a.layers[0].cell(1, 0) == -1);
    CHECK(a.worldSprites.size() == 1 && a.worldSprites[0].anim == 0 && a.worldSprites[0].flags == 1 &&
          a.worldSprites[0].sortY == 2);
    CHECK(a.objectColliders.size() == 2 && a.objectColliders[0].maxX == 1 &&
          a.objectColliders[1].kind == ObjectCollider::Kind::Circle && a.objectColliders[1].minY == 3.5f);
    CHECK(a.player.body.sprite == 0 && a.player.body.sortY == 0.5f && a.player.boat.sprite == -1);
}

TEST(anim_frame_at) {
    Anim an;
    an.frames = {10, 11, 12};
    an.times = {0, 0.25f, 0.5f};
    an.length = 1;
    CHECK(an.frameAt(0) == 10 && an.frameAt(0.24f) == 10 && an.frameAt(0.25f) == 11 && an.frameAt(0.9f) == 12);
    CHECK(an.frameAt(1.1f) == 10); // loops
    an.loop = false;
    CHECK(an.frameAt(5) == 12); // holds the last frame
}

TEST(area_validation_rejects_bad_indices) {
    const char* cases[][3] = {
        {R"("frames": [0])", R"("frames": [5])", "tile frame -> missing sprite"},
        {R"("sheet": 0)", R"("sheet": 3)", "sprite -> missing sheet"},
        {R"("cells": [0, -1])", R"("cells": [0, 9])", "visual cell -> missing tile"},
        {R"("cells": [0]})", R"("cells": [4]})", "collision cell -> missing shape"},
        {R"("xform": [0, 0])", R"("xform": [0])", "xform size mismatch"},
        {R"("cells": [0, -1])", R"("cells": [0])", "cell count mismatch"},
        {R"("owner": 1, "pos")", R"("owner": 4, "pos")", "spawn owner is not an STile"},
        {R"("path": "VIS", "owner": 1)", R"("path": "VIS", "owner": 2)", "tilemap owner is not an STile"},
        {R"("x": 0, "y": 0, "width")", R"("x": 1, "y": 0, "width")", "STile slot outside the grid"},
        {R"("width": 1, "height": 1)", R"("width": 2, "height": 1)", "fewer STiles than slots"},
        {R"("frames": [0], "times")", R"("frames": [7], "times")", "anim frame -> missing sprite"},
        {R"("times": [0])", R"("times": [0, 1])", "anim frames/times mismatch"},
        {R"("path": "WS", "owner": 1, "sprite": 0)", R"("path": "WS", "owner": 1, "sprite": 3)", "world sprite -> missing sprite"},
        {R"("active": true, "anim": 0}],)", R"("active": true, "anim": 2}],)", "world sprite -> missing anim"},
        {R"("path": "WS", "owner": 1)", R"("path": "WS", "owner": 5)", "world sprite owner is not an STile"},
        {R"("path": "OC", "owner": 1)", R"("path": "OC", "owner": 5)", "collider owner is not an STile"},
        {R"([[0, 0, 1, 0, 1, 1, 0, 1]]},)", R"([[0, 0, 1, 0, 1, 1, 0]]},)", "collider odd point list"},
        {R"("body": {"sprite": 0)", R"("body": {"sprite": 9)", "player -> missing sprite"},
    };
    for (const auto& c : cases) {
        Area a;
        std::string err;
        std::string json = fixture(c[0], c[1]);
        check(json.rfind("fixture:", 0) != 0, json);
        check(!loadFixture(json, a, err), std::string("rejects ") + c[2]);
    }
}

// Pinned shape of the committed export. A re-export that changes these is a real content
// change: check the previews, then update the numbers here on purpose.
TEST(area_village_export_stats) {
    const Area& a = village();
    check(a.gridW == 3 && a.gridH == 3 && a.targetGrid == "624897153" && a.housingOffset == -150, "grid config");
    check(a.stiles.size() == 9, "stiles " + std::to_string(a.stiles.size()));
    check(a.spawns.size() == 4, "spawns " + std::to_string(a.spawns.size()));
    check(a.layers.size() == VILLAGE_LAYERS, "visual layers " + std::to_string(a.layers.size()));
    check(a.collision.size() == VILLAGE_COLLISION, "collision layers " + std::to_string(a.collision.size()));
    check(a.shapes.size() == VILLAGE_SHAPES, "collision shapes " + std::to_string(a.shapes.size()));
    check(a.tiles.size() == VILLAGE_TILES, "tiles " + std::to_string(a.tiles.size()));
    check(a.sprites.size() == VILLAGE_SPRITES, "sprites " + std::to_string(a.sprites.size()));
    check(a.sheetNames.size() == 26, "sheets " + std::to_string(a.sheetNames.size()));
    check(a.worldSprites.size() == 329, "world sprites " + std::to_string(a.worldSprites.size()));
    check(a.anims.size() == 36, "anims " + std::to_string(a.anims.size()));
    check(a.objectColliders.size() == 218, "object colliders " + std::to_string(a.objectColliders.size()));
    check(a.player.footBox.w > 0 && a.player.boatBox.w > 0, "player hitboxes");
    check(a.player.body.sprite >= 0 && a.player.boat.sprite >= 0, "player body + boat sprites");
    size_t cells = 0;
    for (const TileLayer& L : a.layers)
        for (int16_t c : L.cells) cells += c >= 0;
    check(cells == VILLAGE_FILLED_CELLS, "filled visual cells " + std::to_string(cells));
}
