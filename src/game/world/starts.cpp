#include "game/world/starts.h"

#include "game/events/conditions.h"
#include "game/world/world.h"

void Starts::reset(const Area& area) {
    started_.assign(area.componentRefs.size(), 0);
    fresh_ = true;
}

bool Starts::started(const Area& area, ComponentRef::Kind kind, int index) const {
    for (size_t i = 0; i < area.componentRefs.size(); i++)
        if (area.componentRefs[i].kind == kind && area.componentRefs[i].index == index) return started_[i] != 0;
    return false;
}

void Starts::update(World& world) {
    if (!fresh_ && world.objects.activeVersion() == seenVersion_) return;
    fresh_ = false;
    const Area& area = world.area;
    using K = ComponentRef::Kind;
    for (bool ranAny = true; ranAny;) {
        ranAny = false;
        for (int o = 0; o < (int)area.objects.size(); o++) {
            if (!world.objects.activeInHierarchy(o)) continue;
            for (int r = area.componentStart[o]; r < area.componentStart[o + 1]; r++) {
                const ComponentRef& ref = area.componentRefs[r];
                if (started_[r] || (ref.kind != K::ConditionChecker && ref.kind != K::Collectible && ref.kind != K::Npc))
                    continue;
                if (ref.kind == K::Npc) {
                    started_[r] = 1;
                    world.npcs.start(world, ref.index);
                } else if (ref.kind == K::ConditionChecker) {
                    const ConditionCheckerDef& cc = area.conditionCheckers[ref.index];
                    if (!cc.enabled) continue;
                    started_[r] = 1;
                    if (cc.checkOnStart) runConditionals(world, cc.conditionals);
                } else {
                    started_[r] = 1;
                    if (area.collectibles[ref.index].disableAtStart) world.objects.setActive(area, o, false);
                }
                ranAny = true;
                if (!world.objects.activeInHierarchy(o)) break; // hid itself (or was hidden)
            }
        }
    }
    seenVersion_ = world.objects.activeVersion();
}
