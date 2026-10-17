// Items (game/items): picking up with the action button, carrying, dropping, and what reads the
// carried item (conditions, item-gated interactions), on a small synthetic area and the Village.
#include <cmath>
#include <string>

#include "game/events/conditions.h"
#include "game/items/items.h"
#include "game/physics/physics.h"
#include "game/world/world.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

// One STile; the player starts at (0, 0).
//   0 "Knot"         an Item named "Knot" at (1, 0): sprite object 1 "Knot/Sprite", a solid circle
//                    collider (radius 0.25, layer Item); OnPickUp sets flag "picked", OnDrop "dropped"
//   2 "Wall"         a solid box x 0.4..0.6, y -2..2 (inactive at first): between the player and the knot
//   3 "Pedestal"     an interaction (on action) that needs the "Knot" carried, trigger around (-3, 0):
//                    flag "placed"
//   4 "Gate"         a condition checker: conditions "carrying Knot"
const char* ITEM_AREA = R"json({
  "ppu": 16,
  "grid": {"width": 1, "height": 1, "targetGrid": "1", "housingOffset": -150},
  "stiles": [{"islandId": 1, "name": "STile (1)", "x": 0, "y": 0, "width": 17, "isTileActive": true}],
  "objects": [{"path": "Knot", "parent": -1, "active": true, "owner": 1, "pos": [1, 0]},
              {"path": "Knot/Sprite", "parent": 0, "active": true, "owner": 1, "pos": [1, 0.25]},
              {"path": "Wall", "parent": -1, "active": false, "owner": 1, "pos": [0.5, 0]},
              {"path": "Pedestal", "parent": -1, "active": true, "owner": 1, "pos": [-3, 0]},
              {"path": "Gate", "parent": -1, "active": true, "owner": 1, "pos": [0, 5]}],
  "objectColliders": [
    {"object": 0, "owner": 1, "layer": "Item", "kind": "circle", "center": [1, 0], "radius": 0.25},
    {"object": 2, "owner": 1, "layer": "Default", "kind": "poly", "paths": [[0.4, -2, 0.6, -2, 0.6, 2, 0.4, 2]]}],
  "triggerColliders": [{"object": 3, "owner": 1, "layer": "Default", "kind": "circle", "center": [-3, 0], "radius": 1}],
  "interactions": [{"object": 3, "addToOnAction": true, "isCarryingItem": true, "itemName": "Knot",
                    "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "placed"}]}],
  "conditionCheckers": [{"object": 4, "checkOnStart": false, "conditions": [{"type": "playerCarryingItem", "playerItem": "Knot"}],
                         "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "gateOpen"}], "onFail": []}],
  "items": [{"object": 0, "type": "Item", "name": "Knot", "canKeep": false, "disableAtStart": false, "radius": 0.5,
             "spriteObject": 1, "collider": 0, "pickUpDuration": 0.5,
             "xCurve": [0, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1],
             "yCurve": [0, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1],
             "enableOnDrop": [], "onPickUp": [{"method": "VarManager.SetBoolOn", "string": "picked"}],
             "onDrop": [{"method": "VarManager.SetBoolOn", "string": "dropped"}]}],
  "playerAction": {"carryPos": [0, 0.8125], "boatDropBase": [0, -0.5], "itemLayers": ["Item"],
                   "dropLayers": ["Default", "Item"]},
  "player": {"footBox": {"offset": [0, 0], "size": [0.5, 0.5]}, "boatBox": {"offset": [0, 0], "size": [1, 0.5]}}
})json";

const Area& itemArea() {
    static Area area;
    static bool loaded = false;
    if (!loaded) {
        testSetFile("synthetic/items.json", ITEM_AREA);
        std::string error;
        loaded = loadArea("synthetic/items.json", area, &error);
        testClearFiles();
        check(loaded, "synthetic item area loads: " + error);
    }
    return area;
}

void frames(World& world, int n, float moveX = 0, float moveY = 0) {
    for (int i = 0; i < n; i++) world.update(DT, moveX, moveY, true); // noclip: walls only matter to items
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

} // namespace

TEST(items_pick_up_animates_then_carries) {
    World world(itemArea(), DEFAULT_SETTINGS);
    const Area& a = world.area;
    frames(world, 1);
    world.action();
    check(world.items.carried() == 0 && world.items.busy(), "A next to the knot: picking it up");
    float x, y;
    world.objectPosition(0, x, y);
    check(near(x, 1) && near(y, 0), "during the animation the knot itself stays put");
    check(world.objects.active(a.objectColliders[0]) && boxBlocked(world, {0.9f, -0.1f, 1.1f, 0.1f}, false),
          "its collider too, until the animation ends (AnimatePickUpEnd turns it off)");
    frames(world, 12); // 0.2 s: the sprite is on its way
    world.objectPosition(1, x, y);
    check(x < 1 && x > 0 && y > 0.25f && y < 1.0625f, "its sprite flies up toward the carry point");
    frames(world, 20); // past 0.5 s
    world.objectPosition(0, x, y);
    check(!world.items.busy() && near(x, 0) && near(y, 0.8125f), "carried over the player's head");
    check(!world.objects.active(a.objectColliders[0]) && world.state.flag("picked"), "collider off, OnPickUp ran");
    check(!boxBlocked(world, {0.9f, -0.1f, 1.1f, 0.1f}, false), "nothing solid left where it lay");
    frames(world, 30, 1, 0); // walk right
    world.objectPosition(0, x, y);
    check(near(x, world.player.x) && near(y, world.player.y + 0.8125f), "it follows the player");
}

TEST(items_need_a_clear_line_and_range) {
    World world(itemArea(), DEFAULT_SETTINGS);
    world.objects.setActive(world.area, 2, true); // the wall between
    frames(world, 1);
    world.action();
    check(world.items.carried() < 0, "a wall in the way: no pick-up");
    world.objects.setActive(world.area, 2, false);
    world.player.x = -1; // 1.75 from the knot's collider edge
    frames(world, 1);
    world.action();
    check(world.items.carried() < 0, "out of reach (1 unit)");
    world.player.x = 0;
    frames(world, 1);
    world.action();
    check(world.items.carried() == 0, "in reach, clear: picked up");
}

TEST(items_drop_lands_ahead) {
    World world(itemArea(), DEFAULT_SETTINGS);
    const Area& a = world.area;
    frames(world, 1);
    world.action();
    frames(world, 40);
    world.player.x = 4; // (clear of the pedestal, which would take the knot instead)
    frames(world, 1, -1, 0); // walking left: drops go left
    float x, y;
    check(world.items.dropIndicator(x, y) && near(x, world.player.x - 1) && near(y, world.player.y),
          "the drop indicator: 1 unit ahead");
    const float landX = x;
    world.action();
    check(world.items.carried() < 0 && world.items.busy(), "A with nothing else to do: dropping");
    world.objectPosition(0, x, y);
    check(near(x, landX) && world.objects.active(a.objectColliders[0]), "the knot lands at the indicator, collider on");
    float sx, sy;
    world.objectPosition(1, sx, sy);
    check(sy > y + 0.25f + 0.1f, "its sprite starts from the carry point and falls");
    frames(world, 40);
    world.objectPosition(1, sx, sy);
    check(!world.items.busy() && world.state.flag("dropped") && near(sx, landX) && near(sy, y + 0.25f), "landed: OnDrop ran");
}

TEST(items_drop_needs_room) {
    World world(itemArea(), DEFAULT_SETTINGS);
    frames(world, 1);
    world.action();
    frames(world, 40);
    world.objects.setActive(world.area, 2, true); // a wall 0.4..0.6 to the right
    frames(world, 1, 1, 0);
    world.player.x = 0;
    frames(world, 1);
    float x, y;
    check(!world.items.dropIndicator(x, y), "facing the wall up close: no room (the drop would be under 0.35 away)");
    world.action();
    check(world.items.carried() == 0, "A doesn't drop it");
    world.player.x = -0.5; // (the drop box is 1 long, centered on the player: it reaches behind too)
    frames(world, 1, -1, 0);
    check(world.items.dropIndicator(x, y), "a step away, facing away: room again");
}

TEST(items_carried_item_gates_conditions_and_interactions) {
    World world(itemArea(), DEFAULT_SETTINGS);
    const Area& a = world.area;
    check(!runConditionals(world, a.conditionCheckers[0].conditionals), "not carrying: the gate stays shut");
    world.player.x = -3; // into the pedestal's trigger without the knot: not offered
    frames(world, 1);
    check(world.interactables.empty(), "the pedestal wants the knot");
    world.player.x = 0;
    frames(world, 1);
    world.action();
    frames(world, 40);
    check(runConditionals(world, a.conditionCheckers[0].conditionals) && world.state.flag("gateOpen"),
          "carrying the knot: playerCarryingItem holds");
    world.player.x = -3;
    frames(world, 1);
    check(world.interactables.size() == 1, "entering with the knot: offered");
    world.action();
    check(world.state.flag("placed") && world.items.carried() == 0, "A uses the pedestal (and doesn't drop the knot)");
}

TEST(items_village_knot_node_round_trip) {
    // Kevin's knot nodes (tile 4) are Items: pick one up and drop it on the tile.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(4, true);
    int knot = -1;
    for (size_t i = 0; i < area.items.size(); i++)
        if (area.pathOf(area.items[i].object).find("Knot/Knot Node (2)") != std::string::npos) knot = (int)i;
    check(knot >= 0, "the village has Knot Node (2)");
    if (knot < 0) return;
    float x, y;
    world.objectPosition(area.items[knot].object, x, y);
    world.placeNear(x, y - 0.8f);
    world.update(DT, 0, 0, false);
    world.action();
    for (int i = 0; i < 40; i++) world.update(DT, 0, 0, false);
    check(world.items.carried() == knot, "picked up");
    for (int i = 0; i < 20; i++) world.update(DT, 0, -1, false); // walk down a bit, facing down
    world.action();
    for (int i = 0; i < 40; i++) world.update(DT, 0, 0, false);
    world.objectPosition(area.items[knot].object, x, y);
    check(world.items.carried() < 0 && world.objects.owner(area.items[knot].object) == 4 && y < world.player.y,
          "dropped below the player, onto tile 4");
}

TEST(items_sorting_order_while_carried) {
    // PlayerAction.SetItemSortingOrderInc / Dec set the order carried items' sprites get (at pick-up,
    // and the carried one's at once); a landed drop resets it (as exported).
    World world(itemArea(), DEFAULT_SETTINGS);
    frames(world, 1);
    world.items.sortingOrderInc(world, 5);
    world.action();
    CHECK(world.objects.sortingOrder(1) == 5);
    frames(world, 40);
    world.items.sortingOrderInc(world, 7);
    CHECK(world.objects.sortingOrder(1) == 7);
    world.items.sortingOrderDec(world, 3); // (two calls in: no change)
    CHECK(world.objects.sortingOrder(1) == 7);
    world.items.sortingOrderDec(world, 3);
    CHECK(world.objects.sortingOrder(1) == 3);
    world.player.x = 4;
    frames(world, 1, -1, 0);
    world.action();
    CHECK(world.objects.sortingOrder(1) == 3); // falling: still the carried order
    frames(world, 40);
    CHECK(world.objects.sortingOrder(1) == ObjectStates::EXPORTED_ORDER);
}
