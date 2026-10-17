#include "game/village/village_intro.h"

#include "game/village/village_grid.h"
#include "game/world/world.h"

namespace village {

namespace {

// Romeo's NPC index, -1 if the area has none.
int romeo(const World& world) {
    const int object = world.area.areaGrid.romeo;
    return object >= 0 ? world.area.npcOn(object) : -1;
}

void faceRight(World& world, int npc, bool flipped) { // romeoNPC.sr.flipX
    if (npc >= 0 && world.area.npcs[npc].sprite >= 0) world.objects.setFlipX(world.area.npcs[npc].sprite, flipped);
}

void showPing(World& world, int npc, bool shown) { // DialogueDisplay.Activate / DeactivateMessagePing
    if (npc >= 0 && world.area.npcs[npc].pingObject >= 0)
        world.objects.setActive(world.area, world.area.npcs[npc].pingObject, shown);
}

} // namespace

void VillageIntro::houseExit(World& world) {
    if (world.state.flag(INTRO_CUTSCENE_FLAG)) return;
    world.state.setFlag(INTRO_CUTSCENE_FLAG, true);
    // StartVillageCutscene, up to its first wait.
    const int npc = romeo(world);
    faceRight(world, npc, true);
    showPing(world, npc, false);
    world.dollies.start(world, world.area.areaGrid.introDolly);
    step_ = 0;
    wait_.start(0.5f, world.frame);
}

void VillageIntro::coroutines(World& world, float dt) {
    if (step_ < 0 || !wait_.done(world.frame, dt)) return;
    const int npc = romeo(world);
    switch (step_++) {
    case 0:
        faceRight(world, npc, false);
        wait_.start(0.5f, world.frame);
        break;
    case 1:
        faceRight(world, npc, true);
        wait_.start(1, world.frame);
        break;
    case 2:
        if (npc >= 0) world.walks.start(world, npc, 0);
        wait_.start(3, world.frame);
        break;
    default:
        showPing(world, npc, true);
        step_ = -1;
        break;
    }
}

} // namespace village
