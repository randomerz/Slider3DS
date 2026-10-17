// Kevin's knot box (KnotBox) on the exported Village: the lines' good/bad check, CheckPuzzle,
// SetToCorrectPositions (also via its start check with Slider 5) and the sparkles on a drop.
#include <cmath>
#include <set>
#include <string>

#include "game/events/conditions.h"
#include "game/events/events.h"
#include "game/village/knot_box.h"
#include "game/world/world.h"
#include "test.h"

namespace {

// Kevin's conditional that asks KnotBox.CheckPuzzle.
const ConditionDef* knotSpec(const Area& area, ConditionDef::Spec spec) {
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (const ConditionDef& d : c.conditions)
                if (d.type == ConditionDef::Type::Spec && d.spec == spec) return &d;
    return nullptr;
}

// Moves knot node `i` (its item) to world (x, y).
void placeNode(World& world, int i, float x, float y) {
    const int item = world.objects.parent(world.area.knotBoxes[0].nodes[i]);
    world.placeObject(item, world.objects.owner(item), x, y);
}

void correctPos(const World& world, int i, float& x, float& y) {
    const KnotBoxDef::Pos& p = world.area.knotBoxes[0].correct[i];
    world.grid.ownerBase(p.owner, x, y);
    x += p.x;
    y += p.y;
}

} // namespace

TEST(knot_box_export) {
    const Area& area = villageArea();
    check(area.knotBoxes.size() == 1, "one knot box");
    const KnotBoxDef& k = area.knotBoxes[0];
    CHECK(area.objects[k.object].path.find("STile (4)/Objects/Knot") != std::string::npos);
    CHECK(k.nodes.size() == 7 && k.lines.size() == 7 && k.correct.size() == 7 && k.particles.size() == 7);
    CHECK(k.good == 0xe6e6e6ffu && k.bad == 0x666666ffu);
    // The ray hits the nodes' own trigger circles, nothing else.
    std::set<int> rayObjects;
    for (const ObjectCollider& c : k.rayColliders) {
        CHECK(c.kind == ObjectCollider::Kind::Circle && std::fabs(c.radius - 0.4f) < 1e-4f);
        rayObjects.insert(c.object);
    }
    CHECK(rayObjects == std::set<int>(k.nodes.begin(), k.nodes.end()));
    for (const KnotLineDef& l : k.lines) CHECK(std::fabs(l.width - 0.1075f) < 1e-3f);
    // Each node's drop checks for sparkles; Kevin's conditional asks CheckPuzzle of this box.
    int drops = 0;
    for (const ItemDef& it : area.items)
        for (const CallDef& c : it.onDrop)
            drops += c.method == CallDef::Method::KnotCheckParticles && c.data == 0;
    CHECK(drops == 7);
    const ConditionDef* spec = knotSpec(area, ConditionDef::Spec::KnotSolved);
    check(spec && spec->specTarget == k.object, "Kevin's CheckPuzzle targets the knot box");
}

TEST(knot_box_starts_tangled_and_solves_at_the_correct_positions) {
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    check(village::knotBadLines(world, 0) == 0, "tile 4 not collected: the box is off, its lines at the origin");
    world.setTileActive(4, true);
    bool bad[village::MAX_KNOT_LINES];
    const int start = village::knotBadLines(world, 0, bad);
    check(start == 7, "every line starts bad (" + std::to_string(start) + ")");
    const ConditionDef& solved = *knotSpec(area, ConditionDef::Spec::KnotSolved);
    CHECK(!checkCondition(solved, world));

    village::knotSetToCorrectPositions(world, 0);
    for (int i = 0; i < 7; i++) {
        float x, y, cx, cy;
        world.objectPosition(area.knotBoxes[0].nodes[i], x, y);
        correctPos(world, i, cx, cy);
        check(std::fabs(x - cx) < 1e-4f && std::fabs(y - cy) < 1e-4f, "node " + std::to_string(i) + " at its spot");
        CHECK(world.objects.owner(world.objects.parent(area.knotBoxes[0].nodes[i])) == 4); // still on tile 4
    }
    check(village::knotBadLines(world, 0) == 0, "solved");
    CHECK(!checkCondition(solved, world)); // the spec reads the knot as of the last update...
    world.update(1.0f / 60, 0, 0, false);
    CHECK(checkCondition(solved, world)); // ...and sees it solved after the next one
}

TEST(knot_box_start_check_solves_it_with_slider_5) {
    // The Knot's ConditionChecker (checkOnStart: Slider 5 collected) runs SetToCorrectPositions.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.state.setCollectible("Slider 5", "Village", true);
    world.setTileActive(4, true);
    CHECK(village::knotBadLines(world, 0) == 0);
}

TEST(knot_box_line_is_bad_through_another_node) {
    // Solved, then node 3 moves next to line 0 (node 0 -> 1), on the knot's inside: within its ray
    // collider's radius (0.4) of the line, line 0 is bad; a bit further, not.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(4, true);
    village::knotSetToCorrectPositions(world, 0);
    float x0, y0, x1, y1;
    correctPos(world, 0, x0, y0);
    correctPos(world, 1, x1, y1);
    const float len = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    const float nx = (y1 - y0) / len, ny = -(x1 - x0) / len; // toward the knot's inside (+x here)
    check(nx > 0, "inward normal");
    bool bad[village::MAX_KNOT_LINES];
    for (float d : {0.3f, 0.5f}) {
        placeNode(world, 3, (x0 + x1) * 0.5f + nx * d, (y0 + y1) * 0.5f + ny * d);
        village::knotBadLines(world, 0, bad);
        check(bad[0] == (d < 0.4f), "line 0 with node 3 at " + std::to_string(d));
    }
}

TEST(knot_box_crossing_lines_are_bad) {
    // Swapping two nodes' spots twists the loop: the lines that now cross go bad, and the partial
    // check (at most 3 bad) follows the count.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(4, true);
    village::knotSetToCorrectPositions(world, 0);
    float ax, ay, bx, by;
    correctPos(world, 1, ax, ay);
    correctPos(world, 4, bx, by);
    placeNode(world, 1, bx, by);
    placeNode(world, 4, ax, ay);
    const int n = village::knotBadLines(world, 0);
    check(n > 0, "twisted: " + std::to_string(n) + " bad");
    ConditionDef partial; // CheckPuzzlePartial (the Village doesn't use it)
    partial.type = ConditionDef::Type::Spec;
    partial.spec = ConditionDef::Spec::KnotPartlySolved;
    partial.specTarget = area.knotBoxes[0].object;
    partial.data = 0;
    world.knots.update(world); // (the knot as of the frame's end)
    CHECK(checkCondition(partial, world) == (n <= 3));
    CHECK(!checkCondition(*knotSpec(area, ConditionDef::Spec::KnotSolved), world));
}

TEST(knot_box_drop_sparkles_only_when_solved) {
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(4, true);
    const ItemDef* node = nullptr;
    for (const ItemDef& it : area.items)
        if (!it.onDrop.empty() && it.onDrop[0].method == CallDef::Method::KnotCheckParticles) node = &it;
    check(node != nullptr, "a knot node item");
    // (Only the knot's sparkles count: the collectibles' play on their own.)
    auto knotSparkles = [&] {
        int n = 0;
        for (const Particles::Particle& p : world.particles.particles())
            for (int k : area.knotBoxes[0].particles) n += p.def == &area.particles[k];
        return n;
    };
    runCalls(world, node->onDrop);
    world.update(1.0f / 60, 0, 0, false);
    check(knotSparkles() == 0, "tangled: no sparkles");

    village::knotSetToCorrectPositions(world, 0);
    world.knots.update(world);
    runCalls(world, node->onDrop);
    world.update(1.0f / 60, 0, 0, false);
    check(knotSparkles() == 7 * 3, "solved: each node's first burst (" + std::to_string(knotSparkles()) + ")");
}

TEST(knot_box_first_and_last_lines_are_neighbours) {
    // Lines 0 and 6 share node 0, so CheckLines never tests them against each other. With node 0
    // here (solved otherwise) the shared end turns so IntersectingSegs would call them crossing:
    // its orientations are exactly 0 at the shared node, which Mathf.Sign counts as positive.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(4, true);
    village::knotSetToCorrectPositions(world, 0);
    float bx, by;
    world.grid.ownerBase(4, bx, by);
    placeNode(world, 0, bx - 2, by - 1);
    bool bad[village::MAX_KNOT_LINES];
    const int n = village::knotBadLines(world, 0, bad);
    check(n == 0, "still solved (" + std::to_string(n) + " bad)");
}

TEST(knot_box_solved_kevin_hands_out_slider_5) {
    // The L + B dev setup (tiles 2-4 collected), the knot solved: Kevin's CheckPuzzle conditional
    // takes over, and standing at him plays its line, whose end shows the Slider 5 collectible.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    for (int id = 2; id <= 4; id++) {
        world.setTileActive(id, true);
        world.state.setCollectible("Slider " + std::to_string(id), "Village", true);
    }
    world.reloadObjects();
    int kevin = -1;
    for (int i = 0; i < (int)area.npcs.size(); i++)
        if (area.npcs[i].name == "Kevin") kevin = i;
    check(kevin >= 0, "Kevin");
    const int slider5 = area.collectible("Slider 5");
    check(slider5 >= 0 && !world.objects.activeInHierarchy(area.collectibles[slider5].object), "Slider 5 hidden");
    world.update(1.0f / 60, 0, 0, false);
    check(world.npcs.currentCond(kevin) == 0, "tangled: asks for help");

    village::knotSetToCorrectPositions(world, 0);
    float x, y;
    world.objectPosition(area.npcs[kevin].object, x, y);
    world.placeNear(x, y - 1);
    for (int f = 0; f < 60 * 20 && !world.objects.activeInHierarchy(area.collectibles[slider5].object); f++) {
        world.update(1.0f / 60, 0, 0, false);
        if (f % 30 == 0) world.action(); // (advances a line that waits for the action button)
    }
    check(world.npcs.currentCond(kevin) == 1, "solved: thanks you");
    check(world.objects.activeInHierarchy(area.collectibles[slider5].object), "Slider 5 shows up");
}
