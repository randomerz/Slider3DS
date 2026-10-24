// MomentumGizmo (game/anim/momentum_gizmos): trees and bushes sway through their Animators as their
// tile slides; an Animator's int / float parameters and conditions (game/anim/animators).
#include <set>
#include <string>

#include "game/world/world.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

void frames(World& world, int n) {
    for (int i = 0; i < n; i++) world.update(DT, 0, 0, false);
}

// The first gizmo on `stile` with the Tree controller (Drag / Impact states), -1 if none.
int treeOn(const Area& area, int stile) {
    for (size_t i = 0; i < area.momentumGizmos.size(); i++) {
        const MomentumGizmoDef& g = area.momentumGizmos[i];
        if (g.stile == stile && area.animatorControllers[area.animators[g.animator].controller].stateIndex("TreeDragLeft") >= 0)
            return (int)i;
    }
    return -1;
}

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

} // namespace

TEST(momentum_gizmos_sway_with_their_tile) {
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    frames(world, 1);
    const int tree = treeOn(area, 1), other = treeOn(area, 0); // (on tile 1; off the tiles: never moves)
    check(tree >= 0 && other >= 0, "a tree on tile 1 and one off the tiles");
    const int a = area.momentumGizmos[tree].animator, b = area.momentumGizmos[other].animator;

    // OnEnable: each its own random in [0, 1) (those on tiles not collected yet: not enabled, none).
    std::set<float> randoms;
    int enabled = 0;
    for (const MomentumGizmoDef& g : area.momentumGizmos) {
        const float r = world.animators.paramValue(area, g.animator, "random");
        CHECK(r >= 0 && r < 1);
        if (!world.objects.activeInHierarchy(area.animators[g.animator].object)) continue;
        enabled++;
        randoms.insert(r);
    }
    CHECK(enabled > 10 && (int)randoms.size() == enabled);
    CHECK(world.animators.stateName(area, a) == "TreeIdle");

    // Tile 1 slides: dx / dy = the move; the tree drags while it moves, then plays an impact.
    auto slide = [&](int toX, int toY, int& dx, int& dy) {
        const TileState* t = world.grid.tile(1);
        dx = toX - t->x;
        dy = toY - t->y;
        CHECK(world.artifact.tryMove(world.grid, t->x, t->y, toX, toY, world.player.stile));
        frames(world, 2);
    };
    const int homeX = world.grid.tile(1)->x, homeY = world.grid.tile(1)->y;
    const auto options = world.artifact.moveOptions(world.grid, homeX, homeY);
    check(!options.empty(), "tile 1 can move");
    int dx = 0, dy = 0;
    slide(options[0].first, options[0].second, dx, dy);
    CHECK(world.animators.paramValue(area, a, "dx") == dx && world.animators.paramValue(area, a, "dy") == dy);
    const std::string drag = world.animators.stateName(area, a);
    check(startsWith(drag, "TreeDrag"), "dragging: " + drag);
    CHECK(world.animators.stateName(area, b) == "TreeIdle"); // (another tile's tree)
    int frame = 0;
    while (world.grid.tile(1)->posX != world.grid.tile(1)->x && frame++ < 120) frames(world, 1);
    frames(world, 2);
    CHECK(world.animators.paramValue(area, a, "dx") == 0 && world.animators.paramValue(area, a, "dy") == 0);
    check(startsWith(world.animators.stateName(area, a), "TreeImpact"), "impact: " + world.animators.stateName(area, a));
    CHECK(!world.animators.param(area, a, "finishedMoving")); // (used up by the transition)
    frames(world, 120);
    CHECK(world.animators.stateName(area, a) == "TreeIdle");

    // Back the other way: the opposite drag.
    int dx2 = 0, dy2 = 0;
    slide(homeX, homeY, dx2, dy2);
    CHECK(dx2 == -dx && dy2 == -dy);
    check(startsWith(world.animators.stateName(area, a), "TreeDrag") && world.animators.stateName(area, a) != drag,
          "the other drag: " + world.animators.stateName(area, a));
}

TEST(animators_int_and_float_conditions) {
    // SimpleDynamicGizmo's Start goes to Idle on random > 0.5 or < 0.5 (a float); Tree's Idle to a drag
    // on dx / dy (ints) other than 0.
    using Mode = AnimatorConditionDef::Mode;
    const AnimatorConditionDef gt{0, Mode::Greater, 0.5f}, lt{0, Mode::Less, 0}, eq{0, Mode::Equals, 2},
        ne{0, Mode::NotEqual, 0}, on{0, Mode::If, 0}, off{0, Mode::IfNot, 0};
    CHECK(gt.holds(0.6f) && !gt.holds(0.5f) && lt.holds(-1) && !lt.holds(0) && eq.holds(2) && !eq.holds(1));
    CHECK(ne.holds(1) && !ne.holds(0) && on.holds(1) && !on.holds(0) && off.holds(0) && !off.holds(1));
    bool numeric = false;
    for (const AnimatorControllerDef& c : villageArea().animatorControllers)
        for (const AnimatorStateDef& s : c.states)
            for (const AnimatorTransitionDef& t : s.transitions)
                for (const AnimatorConditionDef& k : t.conds) numeric = numeric || (k.mode != Mode::If && k.mode != Mode::IfNot);
    CHECK(numeric); // (the export carries them)
}
