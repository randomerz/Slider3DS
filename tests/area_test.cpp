#include <string>

#include "game/data/area.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr size_t VILLAGE_LAYERS = 76, VILLAGE_COLLISION = 35, VILLAGE_SHAPES = 9, VILLAGE_TILES = 168,
                 VILLAGE_SPRITES = 707, VILLAGE_FILLED_CELLS = 27554;

// A minimal valid area: 1x1 grid, one sheet/sprite/tile, one visual and one collision tilemap,
// scene objects, a world sprite with a one-frame anim, two object colliders, a start check and the
// player.
// Each `replace` pair breaks one thing for the validation tests.
std::string fixture(const std::string& from = "", const std::string& to = "") {
    std::string s = R"json({
  "ppu": 16,
  "grid": {"width": 1, "height": 1, "targetGrid": "1", "housingOffset": -150},
  "stiles": [{"islandId": 1, "name": "STile (1)", "x": 0, "y": 0, "width": 17, "isTileActive": true}],
  "spawns": [{"name": "Default", "owner": 1, "pos": [0, 0], "inBoat": false}],
  "collisionShapes": [[[0, 0, 1, 0, 1, 1, 0, 1]]],
  "sheets": [{"name": "sheetA"}],
  "sprites": [{"sheet": 0, "rect": [0, 0, 16, 16], "pivot": [8, 8], "ppu": 16},
              {"sheet": 0, "rect": [0, 0, 32, 32], "pivot": [16, 16], "ppu": 32}],
  "tiles": [{"frames": [0], "fps": 0}],
  "anims": [{"name": "Idle", "frames": [0], "times": [0], "length": 1, "loop": true}],
  "objects": [{"path": "Root", "parent": -1, "active": true, "owner": 1}, {"path": "Root/WS", "parent": 0, "active": false, "owner": 1},
              {"path": "Root/OC", "parent": 0, "active": true, "owner": 1}],
  "worldSprites": [{"object": 1, "owner": 1, "sprite": 0, "pos": [1, 2], "sortY": 2, "sortingLayerValue": 7,
                    "order": 0, "flip": 1, "anim": 0}],
  "objectColliders": [{"object": 2, "owner": 1, "kind": "poly", "paths": [[0, 0, 1, 0, 1, 1, 0, 1]]},
                      {"object": 2, "owner": 1, "enabled": false, "kind": "circle", "center": [3, 4], "radius": 0.5}],
  "conditionCheckers": [{"object": 0, "checkOnStart": true, "conditions": [{"type": "flag", "flag": "f"}],
                         "onSuccess": [{"method": "GameObject.SetActive", "target": 1, "bool": true}],
                         "onFail": [{"method": "KnotBox.Reset", "target": 0}]}],
  "triggerColliders": [{"object": 2, "owner": 1, "kind": "circle", "center": [0, 0], "radius": 1}],
  "interactions": [{"object": 2, "addToOnAction": true, "excludeBoat": true, "onSuccess": [
      {"method": "PlayerPositionChanger.UPPRelativeMe", "target": 2, "owner": 1, "pos": [1, 2], "dPos": [0, -3],
       "tpToHouse": true},
      {"method": "ConditionChecker.CheckConditions", "target": 0, "checker": 0}]}],
  "triggerEvents": [{"object": 2, "onPlayerEnter": true, "onEnter": [{"method": "VarManager.SetBoolOn", "target": 0,
                     "string": "touched"}], "onExit": []}],
  "collectibles": [{"object": 1, "name": "Slider 2", "area": "Village", "disableAtStart": true, "sprite": 0,
                    "onCollect": [{"method": "Collectible.ActivateSTile", "target": 1, "int": 2}]}],
  "fadeDuration": 0.5,
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
    CHECK(a.objects.size() == 3 && a.objects[1].parent == 0 && !a.objects[1].activeSelf && a.findObject("Root/OC") == 2);
    CHECK(a.worldSprites.size() == 1 && a.worldSprites[0].object == 1 && a.worldSprites[0].place.anim == 0 &&
          a.worldSprites[0].place.flags == 1 && a.worldSprites[0].place.sortY == 2);
    CHECK(a.objectColliders.size() == 2 && a.objectColliders[0].maxX == 1 && a.objectColliders[1].object == 2 &&
          !a.objectColliders[1].enabled && a.objectColliders[1].kind == ObjectCollider::Kind::Circle &&
          a.objectColliders[1].minY == 3.5f);
    using M = CallDef::Method;
    CHECK(a.conditionCheckers.size() == 1 && a.conditionCheckers[0].checkOnStart &&
          a.conditionCheckers[0].conditionals.conditions.size() == 1);
    const ConditionalsDef& cc = a.conditionCheckers[0].conditionals;
    CHECK(cc.onSuccess.size() == 1 && cc.onSuccess[0].method == M::SetActive && cc.onSuccess[0].target == 1 &&
          cc.onSuccess[0].boolArg);
    CHECK(cc.onFail.size() == 1 && cc.onFail[0].method == M::Unsupported && cc.onFail[0].name == "KnotBox.Reset");
    CHECK(a.triggerColliders.size() == 1 && a.triggerColliders[0].kind == ObjectCollider::Kind::Circle);
    CHECK(a.interactions.size() == 1 && a.interactions[0].addToOnAction && a.interactions[0].excludeBoat &&
          !a.interactions[0].excludeLand && a.interactions[0].onSuccess.size() == 2);
    const CallDef& tp = a.interactions[0].onSuccess[0];
    CHECK(tp.method == M::TeleportRelativeMe && tp.data >= 0 && tp.data < (int)a.teleports.size());
    if (tp.data >= 0 && tp.data < (int)a.teleports.size()) {
        const TeleportDef& t = a.teleports[tp.data];
        CHECK(t.owner == 1 && t.x == 1 && t.y == 2 && t.dy == -3 && t.tpToHouse && !t.hasGoTo);
    }
    CHECK(a.interactions[0].onSuccess[1].method == M::CheckConditions && a.interactions[0].onSuccess[1].data == 0);
    // Each object's components: Root/OC (2) has the trigger event, then the interaction.
    CHECK(a.componentStart.size() == a.objects.size() + 1);
    auto refs = a.components(2);
    CHECK(refs.end() - refs.begin() == 2 && refs.begin()[0].kind == ComponentRef::Kind::TriggerEvent &&
          refs.begin()[1].kind == ComponentRef::Kind::Interaction);
    CHECK(a.triggerEvents.size() == 1 && a.triggerEvents[0].onEnter.size() == 1 &&
          a.triggerEvents[0].onEnter[0].method == M::SetFlagOn && a.triggerEvents[0].onEnter[0].stringArg == "touched");
    CHECK(a.collectibles.size() == 1 && a.collectibles[0].name == "Slider 2" && a.collectibles[0].disableAtStart &&
          a.collectibles[0].onCollect[0].method == M::ActivateSTile && a.collectibles[0].onCollect[0].intArg == 2);
    CHECK(a.fadeDuration == 0.5f);
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
        {R"("object": 1, "owner": 1, "sprite": 0)", R"("object": 1, "owner": 1, "sprite": 3)", "world sprite -> missing sprite"},
        {R"("object": 1, "owner": 1, "sprite": 0)", R"("object": 1, "owner": 1, "sprite": 1)", "world sprite ppu != area ppu"},
        {R"("order": 0, "flip": 1, "anim": 0})", R"("order": 0, "flip": 1, "anim": 2})", "world sprite -> missing anim"},
        {R"("object": 1, "owner": 1)", R"("object": 1, "owner": 5)", "world sprite owner is not an STile"},
        {R"("object": 1, "owner": 1)", R"("object": 3, "owner": 1)", "world sprite -> missing object"},
        {R"("object": 2, "owner": 1)", R"("object": 2, "owner": 5)", "collider owner is not an STile"},
        {R"("object": 2, "owner": 1)", R"("object": -1, "owner": 1)", "collider -> missing object"},
        {R"("path": "Root/WS", "parent": 0)", R"("path": "Root/WS", "parent": 2)", "object parent after it"},
        {R"("target": 1, "bool": true)", R"("target": 7, "bool": true)", "call -> missing target"},
        {R"("target": 1, "bool": true)", R"("bool": true)", "SetActive call without a target"},
        {R"("conditionCheckers": [{"object": 0)", R"("conditionCheckers": [{"object": 9)", "checker -> missing object"},
        {R"("target": 0, "checker": 0)", R"("target": 0, "checker": 3)", "CheckConditions -> missing checker"},
        {R"("owner": 1, "pos": [1, 2])", R"("owner": 7, "pos": [1, 2])", "teleport owner is not an STile"},
        {R"("interactions": [{"object": 2)", R"("interactions": [{"object": 8)", "interaction -> missing object"},
        {R"("triggerEvents": [{"object": 2)", R"("triggerEvents": [{"object": 8)", "trigger event -> missing object"},
        {R"("collectibles": [{"object": 1)", R"("collectibles": [{"object": 8)", "collectible -> missing object"},
        {R"("triggerColliders": [{"object": 2)", R"("triggerColliders": [{"object": 8)", "trigger collider -> missing object"},
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

TEST(area_validation_rejects_bad_cat_tricks) {
    // The fixture plus an item (collider 0: solid) and a CatTrick whose cats are it.
    const std::string item = R"json("items": [{"object": 2, "type": "Item", "name": "Cat", "radius": 0.5, "spriteObject": 1,
               "collider": 0, "pickUpDuration": 0.5, "xCurve": [0, 1], "yCurve": [0, 1], "enableOnDrop": [],
               "onPickUp": [], "onDrop": []}],
  "catTricks": [{"object": 0, "zone": [-1, -1, 1, 1], "cats": [0, 0, 0], "textObjects": [1, 1],
                 "text": {"box": [0, 0, 2, 1], "lineAdvance": 0.43, "sortingLayerValue": 9, "order": 0}}],
  "fadeDuration": 0.5,)json";
    const std::string good = fixture(R"("fadeDuration": 0.5,)", item);
    Area a;
    std::string err;
    check(loadFixture(good, a, err), "a good cat trick loads: " + err);
    CHECK(a.catTricks.size() == 1 && a.catTricks[0].cats[2] == 0);
    const char* cases[][3] = {
        {R"("cats": [0, 0, 0])", R"("cats": [0, 3, 0])", "cat -> missing item"},
        {R"("collider": 0, "pickUpDuration")", R"("collider": -1, "pickUpDuration")", "cat without a collider"},
        {R"("collider": 0, "pickUpDuration")", R"("collider": 0, "colliderTrigger": true, "pickUpDuration")",
         "cat with a trigger collider"},
        {R"("zone": [-1, -1, 1, 1])", R"("zone": [1, -1, -1, 1])", "empty zone"},
        {R"("textObjects": [1, 1])", R"("textObjects": [1, 9])", "text object -> missing object"},
        {R"("catTricks": [{"object": 0)", R"("catTricks": [{"object": 7)", "cat trick -> missing object"},
    };
    for (const auto& c : cases) {
        Area bad;
        const size_t at = good.find(c[0]);
        check(at != std::string::npos, std::string("pattern for ") + c[2]);
        if (at == std::string::npos) continue;
        std::string json = good;
        json.replace(at, std::string(c[0]).size(), c[1]);
        check(!loadFixture(json, bad, err), std::string("rejects ") + c[2]);
    }
}

// Animator transform curves: one controller shared by the player's Animator (its item holder) and an
// NPC's (its sprite); each curve resolves to the target its own Animator has, -1 on the other.
std::string animatorFixture(const std::string& from = "", const std::string& to = "") {
    std::string s = fixture(R"("fadeDuration": 0.5,)", R"("fadeDuration": 0.5,
  "animatorControllers": [{"name": "P", "params": [], "default": 0, "any": [], "states": [
    {"name": "Idle", "anim": 0, "speed": 1, "transitions": [], "length": 1, "loop": true,
     "moves": [{"path": "Holder", "y": [0, 0.75, 0, 0]}, {"path": "Sprite", "x": [0, 0.25, 0, 0]}]}]}],
  "npcs": [{"object": 0, "name": "N", "facingLeft": false, "pingObject": -1,
    "animator": {"object": 0, "controller": 0, "keepState": false, "controllerObject": 0,
                 "targets": [{"path": "Sprite", "object": 1, "local": [0, 0]}]},
    "text": {"box": [-1, 0, 1, 1], "fontSize": 1, "lineAdvance": 0.4, "wordSpacing": 0, "charSpacing": 0,
             "sortingLayerValue": 9, "order": 0}, "conds": []}],
  "playerAction": {"carryPos": [0, 0.8125], "carryPath": "Holder", "boatDropBase": [0, 0]},)");
    const std::string player = R"("player": {"body":)";
    const size_t at = s.find(player);
    if (at == std::string::npos) return "fixture: no player";
    s.replace(at, player.size(), R"("player": {"animator": 0, "animatorTargets": {"targets": [{"path": "Holder", "object": -1, "local": [0, 0.8125]}]}, "body":)");
    if (!from.empty()) {
        const size_t f = s.find(from);
        if (f == std::string::npos) return "fixture: pattern not found: " + from;
        s.replace(f, from.size(), to);
    }
    return s;
}

TEST(area_animator_curves_resolve_per_animator) {
    Area a;
    std::string err;
    check(loadFixture(animatorFixture(), a, err), "animator fixture loads: " + err);
    CHECK(a.player.animator >= 0 && a.npcs.size() == 1 && a.npcs[0].animator >= 0);
    const AnimatorStateDef& st = a.animatorControllers[0].states[0];
    const AnimatorDef& player = a.animators[a.player.animator];
    const AnimatorDef& npc = a.animators[a.npcs[0].animator];
    CHECK(st.moves.size() == 2 && st.moves[0].path == "Holder" && st.moves[1].path == "Sprite");
    CHECK(player.curveTarget[st.moves[0].id] == 0 && player.curveTarget[st.moves[1].id] == -1);
    CHECK(npc.curveTarget[st.moves[0].id] == -1 && npc.curveTarget[st.moves[1].id] == 0);
    CHECK(a.playerAction.carryTarget == 0);
    // The loader's rules for the player's targets.
    const char* cases[][3] = {
        {R"({"targets": [{"path": "Holder", "object": -1, "local": [0, 0.8125]}]})", R"({"targets": []})",
         "carryPath that isn't a target of the player's Animator"},
        {R"("local": [0, 0.8125]}]})", R"("local": [0, 0.8125]}, {"path": "Hat", "object": -1, "local": [0, 1]}]})",
         "a second target inside the player (the port moves only the item holder)"},
    };
    for (const auto& c : cases) {
        Area bad;
        const std::string json = animatorFixture(c[0], c[1]);
        check(json.rfind("fixture:", 0) != 0, json);
        check(!loadFixture(json, bad, err), std::string("rejects ") + c[2]);
    }
}

// Pinned shape of the committed export. A re-export that changes these is a real content
// change: check the previews, then update the numbers here on purpose.
TEST(area_village_export_stats) {
    const Area& a = villageArea();
    check(a.gridW == 3 && a.gridH == 3 && a.targetGrid == "624897153" && a.housingOffset == -150, "grid config");
    check(a.stiles.size() == 9, "stiles " + std::to_string(a.stiles.size()));
    check(a.spawns.size() == 4, "spawns " + std::to_string(a.spawns.size()));
    check(a.layers.size() == VILLAGE_LAYERS, "visual layers " + std::to_string(a.layers.size()));
    check(a.collision.size() == VILLAGE_COLLISION, "collision layers " + std::to_string(a.collision.size()));
    check(a.shapes.size() == VILLAGE_SHAPES, "collision shapes " + std::to_string(a.shapes.size()));
    check(a.tiles.size() == VILLAGE_TILES, "tiles " + std::to_string(a.tiles.size()));
    check(a.sprites.size() == VILLAGE_SPRITES, "sprites " + std::to_string(a.sprites.size()));
    check(a.sheetNames.size() == 25, "sheets " + std::to_string(a.sheetNames.size()));
    check(a.worldSprites.size() == 333, "world sprites " + std::to_string(a.worldSprites.size()));
    check(a.knotBoxes.size() == 1 && a.ruinsArrows.size() == 1 && a.chadJumps.size() == 1 && a.particles.size() == 28,
          "knot boxes / ruins arrows / particle systems " + std::to_string(a.knotBoxes.size()) + "/" +
              std::to_string(a.ruinsArrows.size()) + "/" + std::to_string(a.particles.size()));
    check(a.anims.size() == 86, "anims " + std::to_string(a.anims.size()));
    check(a.objectColliders.size() == 218, "object colliders " + std::to_string(a.objectColliders.size()));
    check(a.player.footBox.w > 0 && a.player.boatBox.w > 0, "player hitboxes");
    check(a.triggerColliders.size() == 56 && a.conditionCheckers.size() == 7 && a.interactions.size() == 18 &&
              a.triggerEvents.size() == 25 && a.collectibles.size() == 13,
          "triggers/checkers/interactions/events/collectibles " + std::to_string(a.triggerColliders.size()) + "/" +
              std::to_string(a.conditionCheckers.size()) + "/" + std::to_string(a.interactions.size()) + "/" +
              std::to_string(a.triggerEvents.size()) + "/" + std::to_string(a.collectibles.size()));
    check(a.npcs.size() == 18 && a.items.size() == 15 && a.playerAction.present, "npcs / items " +
          std::to_string(a.npcs.size()) + "/" + std::to_string(a.items.size()));
    check(a.animatorControllers.size() == 20 && a.animators.size() == 35,
          "animator controllers / animators " + std::to_string(a.animatorControllers.size()) + "/" +
              std::to_string(a.animators.size()));
    check(a.player.actionIndicator.sprite >= 0, "player action indicator");
    check(a.player.body.sprite >= 0 && a.player.boat.sprite >= 0, "player body + boat sprites");
    size_t cells = 0;
    for (const TileLayer& L : a.layers)
        for (int16_t c : L.cells) cells += c >= 0;
    check(cells == VILLAGE_FILLED_CELLS, "filled visual cells " + std::to_string(cells));
}
