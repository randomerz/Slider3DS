#include "game/starts.h"

#include "game/conditions.h"
#include "game/world.h"

void Starts::reset(const Area& area) {
    checkers_.assign(area.conditionCheckers.size(), 0);
    collectibles_.assign(area.collectibles.size(), 0);
}

void Starts::update(World& world) {
    const Area& area = world.area;
    for (bool ranAny = true; ranAny;) {
        ranAny = false;
        for (size_t i = 0; i < area.conditionCheckers.size(); i++) {
            const ConditionCheckerDef& cc = area.conditionCheckers[i];
            if (checkers_[i] || !cc.enabled || !world.objects.activeInHierarchy(cc.object)) continue;
            checkers_[i] = 1;
            ranAny = true;
            if (cc.checkOnStart) runConditionals(world, cc.conditionals);
        }
        for (size_t i = 0; i < area.collectibles.size(); i++) {
            const CollectibleDef& c = area.collectibles[i];
            if (collectibles_[i] || !world.objects.activeInHierarchy(c.object)) continue;
            collectibles_[i] = 1;
            ranAny = true;
            if (c.disableAtStart) world.objects.setActive(area, c.object, false);
        }
    }
}
