#include "game/anim/momentum_gizmos.h"

#include "game/world/world.h"

void MomentumGizmos::reset(const Area& area) { enabled_.assign(area.momentumGizmos.size(), 0); }

void MomentumGizmos::enable(World& world) {
    const Area& area = world.area;
    for (size_t i = 0; i < area.momentumGizmos.size(); i++) {
        const MomentumGizmoDef& g = area.momentumGizmos[i];
        const bool on = world.objects.activeInHierarchy(area.animators[g.animator].object);
        if (on && !enabled_[i]) world.animators.setParam(world, g.animator, g.random, rng_.unit());
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
            world.animators.setParam(world, g.animator, g.dx, start ? (float)e.dx : 0);
            world.animators.setParam(world, g.animator, g.dy, start ? (float)e.dy : 0);
            if (!start) world.animators.setParam(world, g.animator, g.finishedMoving, 1);
        }
    }
}
