#include "game/world/cheats.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

#include "game/village/village_grid.h"
#include "game/world/world.h"

namespace cheats {

void markUsed(World& world) { world.state.setFlag(USED_CHEATS_FLAG, true); }

std::vector<Cheats::Give> giveList(const Area& area) {
    std::vector<Cheats::Give> list;
    for (int i = 0; i < (int)area.collectibles.size(); i++) list.push_back({area.collectibles[i].name, i});
    auto slider = [](const std::string& name) { return name.rfind("Slider ", 0) == 0 ? std::atoi(name.c_str() + 7) : INT_MAX; };
    std::sort(list.begin(), list.end(), [&](const Cheats::Give& a, const Cheats::Give& b) {
        const int sa = slider(a.name), sb = slider(b.name);
        return sa != sb ? sa < sb : a.name < b.name;
    });
    return list;
}

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
