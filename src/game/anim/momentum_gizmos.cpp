#include "game/anim/momentum_gizmos.h"

#include "game/world/world.h"

void MomentumGizmos::reset(const Area& area) { enabled_.assign(area.momentumGizmos.size(), 0); }

float MomentumGizmos::random() {
    rng_ ^= rng_ << 13; // xorshift32
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float)(rng_ >> 8) / (float)(1u << 24);
}

void MomentumGizmos::enable(World& world) {
    const Area& area = world.area;
    for (size_t i = 0; i < area.momentumGizmos.size(); i++) {
        const int animator = area.momentumGizmos[i].animator;
        const bool on = world.objects.activeInHierarchy(area.animators[animator].object);
        if (on && !enabled_[i]) world.animators.setFloat(world, animator, "random", random());
        enabled_[i] = on;
    }
}

void MomentumGizmos::gridEvents(World& world, const std::vector<GridEvent>& events) {
    const Area& area = world.area;
    for (const GridEvent& e : events) {
        const bool start = e.kind == GridEvent::Kind::TileMoveStart;
        if (!start && e.kind != GridEvent::Kind::TileMoveEnd) continue;
        for (const MomentumGizmoDef& g : area.momentumGizmos) {
            if (g.stile != e.islandId || !world.objects.activeInHierarchy(area.animators[g.animator].object)) continue;
            // OnStileChangeDir: moveDir is start - end, so -moveDir is the move itself.
            world.animators.setInteger(world, g.animator, "dx", start ? e.dx : 0);
            world.animators.setInteger(world, g.animator, "dy", start ? e.dy : 0);
            if (!start) world.animators.setTrigger(world, g.animator, "finishedMoving");
        }
    }
}
