// Triggers, interactions, collectibles and the calls they run, on the real Village: the player is
// placed in a trigger and the World updates, like a frame of play.
#include <string>

#include "game/area.h"
#include "game/physics.h"
#include "game/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

// Puts the player at the center of the first trigger collider of the object at `path`; false if
// there is none.
bool standIn(World& world, const std::string& path) {
    const Area& area = world.area;
    const int object = area.findObject(path);
    for (const ObjectCollider& c : area.triggerColliders) {
        if (c.object != object) continue;
        float bx, by;
        world.grid.ownerBase(c.owner, bx, by);
        world.player.x = bx + (c.minX + c.maxX) * 0.5f;
        world.player.y = by + (c.minY + c.maxY) * 0.5f - world.player.hitbox(area).offY;
        world.player.stile = world.grid.stileUnder(world.player.x, world.player.y);
        return true;
    }
    return false;
}

int interactionOn(const Area& area, const std::string& path) {
    const int object = area.findObject(path);
    for (size_t i = 0; i < area.interactions.size(); i++)
        if (area.interactions[i].object == object) return (int)i;
    return -1;
}

const std::string HOUSE1_EXIT = "Map/Village 8Puzzle/STile (1)/All Tile Maps/House Tile Maps/Decorations/HouseTPAction";
const std::string HOUSE1_DOOR = "Map/Village 8Puzzle/STile (1)/Objects/House2/HouseTPDown";

} // namespace

TEST(events_house_door_round_trip) {
    // Tile 1's house: the exit inside is offered while the player stands on it; A (interact)
    // teleports the player outside with a fade from black. The door outside brings it back in.
    const Area& area = village();
    World world(area);
    check(standIn(world, HOUSE1_EXIT), "standing on tile 1's house exit");
    world.update(DT, 0, 0, false);
    const int exitIndex = interactionOn(area, HOUSE1_EXIT);
    check(world.interactables.size() == 1 && world.interactables[0].index == exitIndex, "the exit is on offer");
    check(world.player.y < -100, "inside the house (y " + std::to_string(world.player.y) + ")");

    world.interact();
    check(world.player.y > -20 && !world.player.inHouse, "outside after the exit (y " + std::to_string(world.player.y) + ")");
    check(world.player.stile == 1, "on tile 1");
    check(world.effects.fade == 1, "faded to black");
    check(!boxBlocked(world, hitboxAt(world.player.hitbox(area), world.player.x, world.player.y), false),
          "landed somewhere free");
    world.update(DT, 0, 0, false);
    {
        std::string offered;
        for (const Interactable& it : world.interactables) offered += area.pathOf(area.interactions[it.index].object) + "; ";
        check(world.interactables.size() == 1 && world.interactables[0].index == interactionOn(area, HOUSE1_DOOR),
              "outside, the house door is on offer instead of the exit (" + offered + ")");
    }
    check(world.effects.fade < 1 && world.effects.fade > 0.9f, "fading back in");
    for (int i = 0; i < 120; i++) world.update(DT, 0, 0, false);
    check(world.effects.fade == 0, "faded in within the fade duration");

    check(standIn(world, HOUSE1_DOOR), "standing at the house's door outside");
    world.update(DT, 0, 0, false);
    check(world.interactables.size() == 1, "the door is on offer");
    world.interact();
    check(world.player.y < -100 && world.player.inHouse, "back inside (y " + std::to_string(world.player.y) + ")");
}

TEST(events_doors_are_not_offered_without_their_trigger) {
    // Control: the Default spawn (inside, by the bed) touches no interaction, and A does nothing.
    const Area& area = village();
    World world(area);
    world.placeAt(*area.spawn("Default"));
    const float x = world.player.x, y = world.player.y;
    world.update(DT, 0, 0, false);
    check(world.interactables.empty(), "nothing on offer by the bed");
    world.interact();
    check(world.player.x == x && world.player.y == y, "interact does nothing");
}

TEST(events_collectible_pickup_collects_its_tile) {
    // Slider 2 lies in the world: touching it collects it (inventory), hides it, and its onCollect
    // (Collectible.ActivateSTile(2)) adds tile 2 to the grid.
    const Area& area = village();
    World world(area);
    const std::string path = "Map/World Grid/Objects/Slider 2 Collectible";
    const int object = area.findObject(path);
    check(world.objects.activeInHierarchy(object), "Slider 2 is out at the start");
    check(!world.grid.tileActive(2) && !world.state.hasCollectible("Slider 2", "Village"), "not collected yet");
    check(standIn(world, path), "standing on Slider 2");
    world.update(DT, 0, 0, false);
    check(world.state.hasCollectible("Slider 2", "Village"), "in the inventory");
    check(world.grid.tileActive(2), "tile 2 collected");
    check(!world.objects.activeInHierarchy(object), "the collectible is gone");
}

TEST(events_disabled_at_start_collectibles_are_hidden) {
    // Collectibles with shouldDisableAtStart (Sliders handed out by NPCs) start hidden, so walking
    // over where they sit collects nothing.
    const Area& area = village();
    World world(area);
    const std::string path = "Map/Village 8Puzzle/STile (2)/Objects/Slider 3 Collectible";
    check(!world.objects.activeInHierarchy(area.findObject(path)), "Slider 3 starts hidden");
    world.setTileActive(2, true); // its tile, so the trigger could collide
    check(standIn(world, path), "standing where Slider 3 would be");
    world.update(DT, 0, 0, false);
    check(!world.state.hasCollectible("Slider 3", "Village") && !world.grid.tileActive(3), "nothing collected");
}

TEST(events_trigger_enter_sets_a_flag) {
    // The Artifact prompt trigger on tile 1 sets villageTouchedGrass on entering.
    const Area& area = village();
    World world(area);
    check(!world.state.flag("villageTouchedGrass"), "flag starts off");
    check(standIn(world, "Map/Village 8Puzzle/STile (1)/Objects/Artifact Prompt Trigger"), "standing in the prompt trigger");
    world.update(DT, 0, 0, false);
    check(world.state.flag("villageTouchedGrass"), "flag set on entering");
}
