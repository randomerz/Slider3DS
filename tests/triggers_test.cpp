// Trigger contacts and what they run, on a small synthetic area (loaded through loadArea, so the
// derived component index is real) and on the Village.
#include <algorithm>
#include <set>
#include <string>

#include "game/area.h"
#include "game/physics.h"
#include "game/world.h"
#include "platform/platform.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

// One STile; five trigger objects along y = 0 (circles of radius 1 at x = -6, -3, 0, 3, 6):
//   0 "Event"     OnTriggerEnter: onEnter flag "entered", onExit flag "exited"
//   1 "Contact"   PlayerConditionals without addToOnAction: flag "contact" on entering
//   2 "NoBoat"    interaction excluded on water: flag "noBoat"
//   3 "NoLand"    interaction excluded on land: flag "noLand"
//   4 "Carry"     interaction that needs a carried item: flag "carry"
// plus "Other" (no trigger) holding two interactions: priority 0 (an unsupported call, then flag
// "low") and priority 1 (flag "high").
const char* TRIGGER_AREA = R"json({
  "ppu": 16,
  "grid": {"width": 1, "height": 1, "targetGrid": "1", "housingOffset": -150},
  "stiles": [{"islandId": 1, "name": "STile (1)", "x": 0, "y": 0, "width": 17, "isTileActive": true}],
  "objects": [{"path": "Event", "parent": -1, "active": true, "owner": 1}, {"path": "Contact", "parent": -1, "active": true, "owner": 1},
              {"path": "NoBoat", "parent": -1, "active": true, "owner": 1}, {"path": "NoLand", "parent": -1, "active": true, "owner": 1},
              {"path": "Carry", "parent": -1, "active": true, "owner": 1}, {"path": "Other", "parent": -1, "active": true, "owner": 1}],
  "triggerColliders": [
    {"object": 0, "owner": 1, "kind": "circle", "center": [-6, 0], "radius": 1},
    {"object": 1, "owner": 1, "kind": "circle", "center": [-3, 0], "radius": 1},
    {"object": 2, "owner": 1, "kind": "circle", "center": [0, 0], "radius": 1},
    {"object": 3, "owner": 1, "kind": "circle", "center": [3, 0], "radius": 1},
    {"object": 4, "owner": 1, "kind": "circle", "center": [6, 0], "radius": 1}],
  "triggerEvents": [{"object": 0, "onPlayerEnter": true,
                     "onEnter": [{"method": "VarManager.SetBoolOn", "string": "entered"}],
                     "onExit": [{"method": "VarManager.SetBoolOn", "string": "exited"}]}],
  "interactions": [
    {"object": 1, "addToOnAction": false, "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "contact"}]},
    {"object": 2, "addToOnAction": true, "excludeBoat": true, "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "noBoat"}]},
    {"object": 3, "addToOnAction": true, "excludeLand": true, "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "noLand"}]},
    {"object": 4, "addToOnAction": true, "isCarryingItem": true, "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "carry"}]},
    {"object": 5, "addToOnAction": true, "onSuccess": [{"method": "CowProp.Moo", "target": 5},
                                                       {"method": "VarManager.SetBoolOn", "string": "low"}]},
    {"object": 5, "addToOnAction": true, "priority": 1, "onSuccess": [{"method": "VarManager.SetBoolOn", "string": "high"}]}],
  "player": {"footBox": {"offset": [0, 0], "size": [0.5, 0.5]}, "boatBox": {"offset": [0, 0], "size": [1, 0.5]}}
})json";

const Area& triggerArea() {
    static Area area;
    static bool loaded = false;
    if (!loaded) {
        testSetFile("synthetic/triggers.json", TRIGGER_AREA);
        std::string error;
        loaded = loadArea("synthetic/triggers.json", area, &error);
        testClearFiles();
        check(loaded, "synthetic trigger area loads: " + error);
    }
    return area;
}

void standAt(World& world, float x) {
    world.player.x = x;
    world.player.y = 0;
    world.update(DT, 0, 0, true); // noclip: the synthetic area has no solid colliders anyway
}

bool offered(const World& world, int interaction) {
    return std::any_of(world.interactables.begin(), world.interactables.end(),
                       [&](const Interactable& it) { return it.index == interaction; });
}

} // namespace

TEST(triggers_enter_and_exit_run_calls) {
    World world(triggerArea());
    standAt(world, -6);
    check(world.state.flag("entered") && !world.state.flag("exited"), "entering runs onEnter only");
    standAt(world, -6);
    world.state.setFlag("entered", false);
    standAt(world, -6);
    check(!world.state.flag("entered"), "staying inside doesn't re-enter");
    standAt(world, 20);
    check(world.state.flag("exited"), "leaving runs onExit");
}

TEST(triggers_hiding_the_object_counts_as_exit) {
    World world(triggerArea());
    standAt(world, -6);
    check(world.triggers.touching(0), "touching the event trigger");
    world.objects.setActive(world.area, 0, false);
    standAt(world, -6);
    check(world.state.flag("exited") && !world.triggers.touching(0), "hidden while inside: exit");
}

TEST(triggers_contact_interaction_runs_on_entering) {
    World world(triggerArea());
    standAt(world, -3);
    check(world.state.flag("contact"), "PlayerConditionals without addToOnAction runs on contact");
    check(!offered(world, 0), "and isn't offered");
}

TEST(triggers_exclusions_follow_the_players_collider) {
    // excludeBoat / excludeLand are judged on entering; switching foot <-> boat swaps the player's
    // collider, which re-sends exit + enter (Player.SetIsOnWater), so the offer is re-judged.
    World world(triggerArea());
    standAt(world, 0);
    check(offered(world, 1), "NoBoat offered on land");
    world.player.onWater = true;
    standAt(world, 0);
    check(!offered(world, 1), "withdrawn once on water");
    world.player.onWater = false;
    standAt(world, 0);
    check(offered(world, 1), "offered again back on land");

    standAt(world, 3);
    check(!offered(world, 2), "NoLand not offered on land");
    world.player.onWater = true;
    standAt(world, 3);
    check(offered(world, 2), "offered on water");
}

TEST(triggers_collider_swap_resends_enter) {
    World world(triggerArea());
    standAt(world, -6);
    world.state.setFlag("entered", false);
    world.player.onWater = true;
    standAt(world, -6);
    check(world.state.flag("exited") && world.state.flag("entered"), "foot -> boat: exit, then enter again");
}

TEST(triggers_carried_item_interactions_wait_for_an_item) {
    World world(triggerArea());
    standAt(world, 6);
    check(!offered(world, 3), "needs a carried item: not offered (no items yet)");
    world.offer({Interactable::Kind::Interaction, 3});
    world.interact();
    check(!world.state.flag("carry"), "and doesn't run without one");
}

TEST(triggers_interact_runs_the_highest_priority_only) {
    // PlayerAction.Interact: only the offered interactables with the highest priority run; an
    // unsupported call (CowProp.Moo) does nothing.
    World world(triggerArea());
    world.offer({Interactable::Kind::Interaction, 4});
    world.offer({Interactable::Kind::Interaction, 5});
    world.offer({Interactable::Kind::Interaction, 5}); // offered twice: listed once
    check(world.interactables.size() == 2, "two on offer");
    world.interact();
    check(world.state.flag("high") && !world.state.flag("low"), "only the higher priority runs");
    world.withdraw({Interactable::Kind::Interaction, 5});
    world.interact();
    check(world.state.flag("low"), "alone, the lower one runs (its unsupported call quietly does nothing)");
}

TEST(triggers_tiles_sliding_without_the_player_turn_their_triggers_off) {
    // SGridAnimator: while the player rides a tile (here tile 1), the other tiles in a move have
    // their colliders, triggers included, off until it ends. Tile 7 (Nemsy's talk trigger) slides
    // into the empty middle slot.
    const Area& area = village();
    World world(area);
    for (int id = 1; id <= 8; id++) world.setTileActive(id, true);
    world.artifact.init(world.grid);
    const ObjectCollider* trigger = nullptr;
    for (const ObjectCollider& c : area.triggerColliders)
        if (c.owner == 7 && area.pathOf(c.object).find("Nemsy") != std::string::npos) trigger = &c;
    check(trigger != nullptr, "tile 7 has Nemsy's trigger");
    if (!trigger) return;
    auto overlapsCenter = [&]() {
        float bx, by;
        world.objectBase(trigger->object, bx, by);
        const float x = bx + (trigger->minX + trigger->maxX) * 0.5f, y = by + (trigger->minY + trigger->maxY) * 0.5f;
        return colliderOverlaps(world, *trigger, {x - 0.01f, y - 0.01f, x + 0.01f, y + 0.01f});
    };
    check(overlapsCenter(), "on before the slide");
    check(world.artifact.tryMove(world.grid, 2, 1, 1, 1, 1), "tile 7 left into the middle, the player on tile 1");
    world.artifact.update(world.grid, DT, 1);
    check(!overlapsCenter(), "off while it slides");
    for (int i = 0; i < 600 && !world.artifact.queueEmpty(); i++) world.artifact.update(world.grid, DT, 1);
    check(overlapsCenter(), "on again once the slide ends");
}

TEST(triggers_collected_collectibles_stay_hidden_on_reload) {
    // SGrid.Start: a collectible already in the inventory is hidden when the scene (re)loads.
    const Area& area = village();
    World world(area);
    const int slider2 = area.findObject("Map/World Grid/Objects/Slider 2 Collectible");
    world.reloadObjects();
    check(world.objects.activeInHierarchy(slider2), "not collected: out");
    world.state.setCollectible("Slider 2", "Village", true);
    world.reloadObjects();
    check(!world.objects.activeInHierarchy(slider2), "collected: hidden on reload");
}

TEST(physics_resting_player_is_pushed_out) {
    // Box2D pushes a resting body out too: a player left overlapping the well moves out without input.
    const Area& area = village();
    World world(area);
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    const int well = area.findObject("Map/Village 8Puzzle/STile (9)/All Tile Maps/Tile Maps/Decorations/Well");
    for (const ObjectCollider& c : area.objectColliders)
        if (c.object == well) {
            float bx, by;
            world.grid.ownerBase(c.owner, bx, by);
            world.player.x = bx + c.minX + 0.1f;
            world.player.y = by + (c.minY + c.maxY) * 0.5f;
        }
    const Hitbox& hb = area.player.footBox;
    check(boxBlocked(world, hitboxAt(hb, world.player.x, world.player.y), false), "starts overlapping the well");
    world.update(DT, 0, 0, false);
    check(!boxBlocked(world, hitboxAt(hb, world.player.x, world.player.y), false), "pushed out without input");
}

TEST(objects_runtime_placement_moves_colliders) {
    // ObjectStates placement: moving an object moves its colliders (NPC walking, carried items).
    const Area& area = village();
    World world(area);
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    const int well = area.findObject("Map/Village 8Puzzle/STile (9)/All Tile Maps/Tile Maps/Decorations/Well");
    float cx = 0, cy = 0;
    for (const ObjectCollider& c : area.objectColliders)
        if (c.object == well) {
            float bx, by;
            world.grid.ownerBase(c.owner, bx, by);
            cx = bx + (c.minX + c.maxX) * 0.5f;
            cy = by + (c.minY + c.maxY) * 0.5f;
        }
    auto blockedAt = [&](float x, float y) { return boxBlocker(world, {x - 0.01f, y - 0.01f, x + 0.01f, y + 0.01f}, false); };
    check(blockedAt(cx, cy).kind == Blocker::Object && !blockedAt(cx + 5, cy), "the well where it was exported");
    world.objects.setPlacement(area, well, -1, 5, 0);
    check(!blockedAt(cx, cy) && blockedAt(cx + 5, cy).kind == Blocker::Object, "moved 5 units right");
}

TEST(objects_placement_follows_the_hierarchy_and_owner) {
    // Moving a parent moves its children's colliders; an owner override makes an object follow
    // another tile (its exported position then counts from that tile's center).
    const Area& area = village();
    World world(area);
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    const int house = area.findObject("Map/Village 8Puzzle/STile (9)/All Tile Maps/Tile Maps/Decorations/House5");
    const int decorations = area.objects[house].parent;
    const ObjectCollider* c = nullptr;
    for (const ObjectCollider& oc : area.objectColliders)
        if (oc.object == house) c = &oc;
    check(c != nullptr && decorations >= 0, "House5 has a collider and a parent");
    if (!c || decorations < 0) return;
    auto blockedAt = [&](float x, float y) {
        return boxBlocker(world, {x - 0.01f, y - 0.01f, x + 0.01f, y + 0.01f}, false).kind == Blocker::Object;
    };
    float bx, by;
    world.grid.ownerBase(9, bx, by);
    const float lx = (c->minX + c->maxX) * 0.5f, ly = (c->minY + c->maxY) * 0.5f; // local to tile 9
    check(blockedAt(bx + lx, by + ly), "House5 where exported");
    world.objects.setPlacement(area, decorations, -1, 0, 3);
    check(!blockedAt(bx + lx, by + ly) && blockedAt(bx + lx, by + ly + 3), "moving its parent moved it up 3");
    world.objects.setPlacement(area, decorations, -1, 0, 0);
    world.objects.setPlacement(area, house, 1, 0, 0); // now counted from tile 1
    float ax, ay;
    world.grid.ownerBase(1, ax, ay);
    check(!blockedAt(bx + lx, by + ly) && blockedAt(ax + lx, ay + ly), "an owner override puts it on tile 1");
}

TEST(calls_unsupported_in_the_village_are_pinned) {
    // Every call method the Village uses that the port doesn't run yet. The exporter's PortMethods
    // and SLIDER_CALL_METHODS must agree: a method added on one side only shows up here.
    const Area& area = village();
    std::set<std::string> unsupported;
    auto collect = [&](const std::vector<CallDef>& calls) {
        for (const CallDef& c : calls)
            if (c.method == CallDef::Method::Unsupported) unsupported.insert(c.name);
    };
    for (const ConditionCheckerDef& c : area.conditionCheckers) {
        collect(c.conditionals.onSuccess);
        collect(c.conditionals.onFail);
    }
    for (const InteractionDef& i : area.interactions) collect(i.onSuccess);
    for (const TriggerEventDef& t : area.triggerEvents) {
        collect(t.onEnter);
        collect(t.onExit);
    }
    for (const CollectibleDef& c : area.collectibles) collect(c.onCollect);
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds) {
            collect(c.onEnter);
            collect(c.onExhausted);
            for (const DialogueDef& d : c.dialogue) {
                collect(d.onStart);
                collect(d.onEnd);
            }
        }
    const std::set<std::string> expected = {
        "Animator.set_runtimeAnimatorController", "ArtifactScreenHider.AddScreensAndShow",
        "ArtifactWorldMapAffector.UpdateStatus", "AudioManager.Play", "BoxCollider2D.set_enabled", "Breadge.Pickup",
        "ChadJump.JumpStarter", "ConditionChecker.PlayErrorSound", "CowProp.Moo", "DogProp.Bark",
        "KnotBox.SetToCorrectPositions", "NPC.StartValidWalk", "NPC.StartWalkAtIndex",
        "NPCAnimatorController.SetBoolToFalse", "NPCAnimatorController.SetBoolToTrue", "ParticleSystem.Play",
        "PlaySound.PlayString", "PlayerAction.SetItemSortingOrderDec", "PlayerAction.SetItemSortingOrderInc",
        "PlayerActionHints.DisableHint", "PlayerActionHints.EnableDisabling", "PlayerActionHints.TriggerHint",
        "UIArtifactWorldMap.SetAreaStatusSilhouette", "UIClick.Click", "VillageGrid.ActivateCollectible",
        "VillageGrid.ActivateSliderCollectible", "VillageGrid.Explode", "VillageGrid.FillInHole",
        "VillageGrid.OnHouseExit", "VillageGrid.OnWaterfallEntry", "VillageGrid.PlayDigSound",
        "VillageGrid.RemoveSlider2Tracker", "VillageGrid.ShufflePuzzle", "VillageGrid.TurnFishOn",
    };
    std::string got;
    for (const std::string& n : unsupported) got += n + ", ";
    check(unsupported == expected, "unsupported call methods: " + got);

    // Drift check both ways: the exporter's report lists the methods its PortMethods doesn't
    // cover; the port's SLIDER_CALL_METHODS decides what loads as Unsupported. They must agree.
    std::string report;
    check(plat::readFile("village/export_report.txt", report), "read the export report");
    std::set<std::string> reported;
    const std::string prefix = "Calls to ", suffix = " are not run by the port yet";
    for (size_t at = report.find(prefix); at != std::string::npos; at = report.find(prefix, at + 1)) {
        const size_t end = report.find(suffix, at);
        if (end != std::string::npos) reported.insert(report.substr(at + prefix.size(), end - at - prefix.size()));
    }
    std::string rep;
    for (const std::string& n : reported) rep += n + ", ";
    check(reported == unsupported, "exporter report agrees with the port (report: " + rep + ")");
}
