#include "game/world/cheats.h"

#include "game/village/village_grid.h"
#include "game/world/world.h"

namespace cheats {

void markUsed(World& world) { world.state.setFlag(USED_CHEATS_FLAG, true); }

void giveCollectible(World& world, int index) {
    const CollectibleDef& c = world.area.collectibles[index];
    village::activateCollectible(world, c.name, true); // (SGrid.ActivateCollectible)
    world.objects.setParent(world.area, c.object, -1);
    world.placeObject(c.object, 0, world.player.x, world.player.y);
    markUsed(world);
}

void respawnPlayer(World& world) {
    world.placeAtStart();
    markUsed(world);
}

} // namespace cheats
