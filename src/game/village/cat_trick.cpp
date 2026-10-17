#include "game/village/cat_trick.h"

#include "game/physics/physics.h"
#include "game/world/world.h"

namespace village {

void CatTricks::update(World& world) {
    const Area& area = world.area;
    for (const CatTrickDef& c : area.catTricks) {
        if (!world.objects.activeInHierarchy(c.object)) continue; // (no Update while inactive)
        float bx, by;
        world.objectBase(c.object, bx, by);
        const Box zone = {bx + c.zoneMinX, by + c.zoneMinY, bx + c.zoneMaxX, by + c.zoneMaxY};
        for (int i = 0; i < CatTrickDef::CATS; i++) {
            if (world.state.flag(CAT_FLAGS[i])) continue; // (once back, for good)
            const ObjectCollider& collider = area.objectColliders[area.items[c.cats[i]].colliderSlot];
            if (colliderOverlaps(world, collider, zone)) world.state.setFlag(CAT_FLAGS[i], true);
        }
        const int n = count(world);
        for (int object : c.textObjects) world.objects.setActive(area, object, n > 0 && n < CatTrickDef::CATS);
    }
}

int CatTricks::count(const World& world) {
    int n = 0;
    for (const char* flag : CAT_FLAGS) n += world.state.flag(flag) ? 1 : 0;
    return n;
}

const char* CatTricks::text(int count) {
    static const char* const TEXTS[] = {"0/3", "1/3", "2/3", "3/3"};
    return TEXTS[count < 0 ? 0 : count > CatTrickDef::CATS ? CatTrickDef::CATS : count];
}

} // namespace village
