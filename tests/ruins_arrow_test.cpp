// The ruins arrow (RuinsArrow, Slider 7) on the exported Village: rods, runes and Artifact corners
// lit by assembled pairs, the arrow appearing (and pointing from tile 2 to tile 5) when 3-1 sit on
// 6-2, the buried Slider and its trail, CheckActiveRods, and digging up Slider 7.
#include <cmath>
#include <string>

#include "game/events/conditions.h"
#include "game/events/events.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

// Tiles 1-6 collected (as Sliders 2-6), the grid as `ids` (grid-string order), the scene reloaded.
World ruinsWorld(const char* ids) {
    World world(villageArea(), DEFAULT_SETTINGS);
    for (int id = 2; id <= 6; id++) {
        world.setTileActive(id, true);
        world.state.setCollectible("Slider " + std::to_string(id), "Village", true);
    }
    world.artifact.setGrid(world.grid, ids);
    world.reloadObjects();
    return world;
}

// Updates until the Artifact's moves are done, plus `extra` frames.
void settle(World& world, int extra = 1) {
    for (int f = 0; f < 600 && !world.artifact.queueEmpty(); f++) world.update(DT, 0, 0, false);
    for (int f = 0; f < extra; f++) world.update(DT, 0, 0, false);
}

const RuinsRodDef& rod(const Area& area, int island) {
    for (const RuinsRodDef& r : area.ruinsArrows[0].rods)
        if (r.island == island) return r;
    return area.ruinsArrows[0].rods.at(0);
}

bool rodLit(const World& world, int island) {
    const RuinsRodDef& r = rod(world.area, island);
    return world.objects.sprite(r.object) == r.on && world.objects.rendererEnabled(r.runes);
}

std::string litRods(const World& world) {
    std::string s;
    for (int island : {1, 2, 3, 6}) s += rodLit(world, island) ? std::to_string(island) : "-";
    return s;
}

std::string icons(const World& world) {
    std::string s;
    for (int island : {1, 2, 3, 6}) s += world.artifactButtons.iconShown(island, 0) ? std::to_string(island) : "-";
    return s;
}

const ConditionDef* rodsSpec(const Area& area) {
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (const ConditionDef& d : c.conditions)
                if (d.type == ConditionDef::Type::Spec && d.spec == ConditionDef::Spec::RuinsRodsOn) return &d;
    return nullptr;
}

int trailParticles(const World& world) {
    const ParticlesDef* d = &world.area.particles[world.area.ruinsArrows[0].trail.particles];
    int n = 0;
    for (const Particles::Particle& p : world.particles.particles()) n += p.def == d;
    return n;
}

} // namespace

TEST(ruins_arrow_export) {
    const Area& area = villageArea();
    CHECK(area.ruinsArrows.size() == 1);
    const RuinsArrowDef& r = area.ruinsArrows[0];
    CHECK(area.objects[r.object].owner == 2 && r.renderer == r.object && r.complete == -1);
    for (int s : r.arrows) CHECK(s >= 0);
    CHECK(area.pathOf(r.buriedSlider).find("Burried Slider") != std::string::npos && !area.objects[r.buriedSlider].activeSelf);
    std::string islands;
    for (const RuinsRodDef& d : r.rods) islands += std::to_string(d.island);
    CHECK(islands == "1236");
    for (const RuinsRodDef& d : r.rods) CHECK(d.off != d.on && d.flashTime == 0.25f && area.particles[d.particles].loop);
    CHECK(r.flashColor == 0xe6e6e6ffu);
    CHECK(r.trail.target == r.buriedSlider && r.trail.start == r.object && r.trail.distance == 2 && r.trail.repeat == 8);
    CHECK(r.mapIcons.size() == 4 && r.bob && r.bobLength == 3);
    // Its renderer is a world sprite that starts off; the corner icons start hidden.
    bool off = false;
    for (const WorldSprite& ws : area.worldSprites) off |= ws.object == r.renderer && !ws.enabled;
    CHECK(off);
    for (const RuinsArrowDef::MapIcon& m : r.mapIcons) CHECK(!area.artifact.button(m.button)->iconsEnabled[m.icon]);
    const ConditionDef* spec = rodsSpec(area);
    CHECK(spec && spec->data == 0 && spec->specTarget == r.object);
}

TEST(ruins_rods_start_off) {
    // RuinsArrowRod.Awake: off (its runes hidden), as is the arrow, on a new game.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.update(DT, 0, 0, false);
    const RuinsRodDef& r1 = rod(area, 1);
    CHECK(world.objects.sprite(r1.object) == r1.off && !world.objects.rendererEnabled(r1.runes));
    CHECK(!world.objects.rendererEnabled(area.ruinsArrows[0].renderer));
    CHECK(icons(world) == "----" && !checkCondition(*rodsSpec(area), world));
}

TEST(ruins_pairs_light_rods_and_corners) {
    // 3 next to 1 lights rods 1 and 3, 3 above 6 lights 3 and 6 (and their Artifact corners);
    // CheckActiveRods is then true.
    World world = ruinsWorld("314_692_578");
    world.update(DT, 0, 0, false);
    check(litRods(world) == "1-36" && icons(world) == "1-36", "31, 3/6: rods " + litRods(world) + ", icons " + icons(world));
    CHECK(checkCondition(*rodsSpec(world.area), world));
    CHECK(!world.objects.rendererEnabled(world.area.ruinsArrows[0].renderer)); // (not assembled)

    // 1 above 2 ("1...2" across the row break) lights 1 and 2.
    World world2 = ruinsWorld("613_524_978");
    world2.update(DT, 0, 0, false);
    check(litRods(world2) == "12--", "1 over 2: rods " + litRods(world2));
    World none = ruinsWorld("143_692_578");
    none.update(DT, 0, 0, false);
    CHECK(litRods(none) == "----" && !checkCondition(*rodsSpec(none.area), none));
}

TEST(ruins_assembled_by_a_move_shows_the_arrow) {
    // 3 1 / 6 _ 2: sliding 2 into the gap assembles the ruins. As the move starts the arrow stays off
    // (it wasn't assembled before) and the corners update; when the tile arrives the arrow shows,
    // pointing from tile 2 to tile 5, the buried Slider appears, the trail starts, all four rods light.
    const Area& area = villageArea();
    const RuinsArrowDef& r = area.ruinsArrows[0];
    World world = ruinsWorld("314_692_578");
    world.update(DT, 0, 0, false);
    CHECK(!world.objects.activeInHierarchy(r.buriedSlider));
    check(world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0), "2 slides left");
    world.update(DT, 0, 0, false);
    check(icons(world) == "1236" && litRods(world) == "1-36", "moving: corners " + icons(world) + ", rods " + litRods(world));
    CHECK(!world.objects.rendererEnabled(r.renderer));
    settle(world);
    check(litRods(world) == "1236", "arrived: rods " + litRods(world));
    CHECK(world.objects.rendererEnabled(r.renderer));
    // Tile 2 at (1, 1), tile 5 at (0, 0): one left, one down = SSE mirrored.
    CHECK(world.objects.sprite(r.renderer) == r.arrows[5] && world.objects.flipX(r.renderer) == 1);
    CHECK(world.objects.activeInHierarchy(r.buriedSlider));
    for (int f = 0; f < 30; f++) world.update(DT, 0, 0, false);
    check(trailParticles(world) > 0, "trail sparkles: " + std::to_string(trailParticles(world)));
    CHECK(world.particles.playing(r.particles));

    // Sliding 2 away again: the arrow goes off as the move starts, the rods when it ends.
    check(world.artifact.tryMove(world.grid, 1, 1, 2, 1, 0), "2 slides back");
    world.update(DT, 0, 0, false);
    CHECK(!world.objects.rendererEnabled(r.renderer) && !world.particles.playing(r.particles));
    settle(world);
    CHECK(litRods(world) == "1-36");
    CHECK(world.objects.activeInHierarchy(r.buriedSlider)); // (it stays)
}

TEST(ruins_arrow_points_from_tile_2_to_tile_5) {
    // UpdateArrowDirection by where 5 is relative to 2 (dx, dy in slots): sprite index, flipped when
    // 5 is to the left. Each grid has 3 1 / 6 2 assembled (Start shows it).
    const struct { const char* grid; int sprite; bool flip; } cases[] = {
        {"314_629_578", 5, true},  // dx -1, dy -1: SSE, mirrored
        {"314_627_895", 4, false}, // dx +1, dy -1: SE
        {"314_625_798", 3, false}, // dx +1, dy  0: ESE
        {"315_624_798", 2, false}, // dx +1, dy +1: ENE
        {"579_831_462", 1, true},  // dx -2, dy +2: NE, mirrored
        {"759_831_462", 0, true},  // dx -1, dy +2: NNE, mirrored
    };
    for (const auto& c : cases) {
        World world = ruinsWorld(c.grid);
        const RuinsArrowDef& r = world.area.ruinsArrows[0];
        world.update(DT, 0, 0, false);
        check(world.objects.rendererEnabled(r.renderer) && world.objects.sprite(r.renderer) == r.arrows[c.sprite] &&
                  (world.objects.flipX(r.renderer) == 1) == c.flip,
              std::string(c.grid) + ": arrow " + std::to_string(c.sprite) + (c.flip ? " mirrored" : ""));
    }
}

TEST(ruins_runes_flash_white_when_lit) {
    // FlashWhiteSprite.Flash(1) as runes light: white for 0.25 s, then normal; not again while lit.
    const Area& area = villageArea();
    const RuinsArrowDef& r = area.ruinsArrows[0];
    World world = ruinsWorld("314_692_578");
    world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0);
    settle(world, 0);
    const RuinsRodDef& r2 = rod(area, 2);
    CHECK(world.objects.fill(r2.runes) == r.flashColor && world.objects.fill(rod(area, 6).runes) == 0);
    for (int f = 0; f < 20; f++) world.update(DT, 0, 0, false);
    CHECK(world.objects.fill(r2.runes) == 0);
    // Rod 6 was lit already (3 above 6): re-lit as the move ended, without a flash.
    CHECK(world.objects.fill(rod(area, 1).runes) == 0);
}

TEST(ruins_archeologist_sees_the_rods) {
    // Her conditional "ahA! it looks like those ruins pieces might connect!!" asks CheckActiveRods
    // (and Slider 6); once the ruins are assembled her "arrow" one takes over.
    const Area& area = villageArea();
    int arch = -1;
    for (int i = 0; i < (int)area.npcs.size(); i++)
        if (area.npcs[i].name == "Archeologist") arch = i;
    CHECK(arch >= 0);
    World world = ruinsWorld("314_692_578");
    world.update(DT, 0, 0, false);
    check(world.npcs.currentCond(arch) == 3, "rods lit: cond " + std::to_string(world.npcs.currentCond(arch)));
    world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0);
    settle(world);
    check(world.npcs.currentCond(arch) == 4, "assembled: cond " + std::to_string(world.npcs.currentCond(arch)));
}

TEST(ruins_dig_up_slider_7) {
    // At the buried Slider the action button digs it up; picking it up collects tile 7: the arrow
    // shows its complete sprite (none here) and the trail stops repeating.
    const Area& area = villageArea();
    const RuinsArrowDef& r = area.ruinsArrows[0];
    World world = ruinsWorld("314_629_578");
    world.update(DT, 0, 0, false);
    CHECK(world.objects.activeInHierarchy(r.buriedSlider));
    const int slider7 = area.collectible("Slider 7");
    CHECK(slider7 >= 0 && !world.objects.activeInHierarchy(area.collectibles[slider7].object)); // (its Start hid it)
    float x, y;
    world.objectPosition(r.buriedSlider, x, y);
    world.placeNear(x, y);
    world.update(DT, 0, 0, false);
    CHECK(world.promptShown());
    world.action(); // dig
    check(world.objects.activeInHierarchy(area.collectibles[slider7].object), "Slider 7 dug up");
    for (int f = 0; f < 5; f++) world.update(DT, 0, 0, false); // (standing on it: picked up)
    check(world.state.hasCollectible("Slider 7", "Village") && world.grid.tileActive(7), "collected: tile 7 joins");
    CHECK(!world.objects.activeInHierarchy(r.buriedSlider));
    CHECK(world.objects.rendererEnabled(r.renderer) && world.objects.sprite(r.renderer) == ObjectStates::NO_SPRITE);
    CHECK(!world.particles.playing(r.particles)); // (a port choice: its sparkles go with it)
    for (int f = 0; f < 60 * 12; f++) world.update(DT, 0, 0, false);
    check(trailParticles(world) == 0, "no more trails: " + std::to_string(trailParticles(world)));
}

TEST(ruins_queued_moves_see_their_own_grid) {
    // Two moves queued back to back: 2 in (assembles the ruins) then 2 out. The second starts the
    // moment the first ends, before the events are handed out; each event still reads the grid as it
    // was when it fired, so the arrow shows as the first ends (revealing the buried Slider for good)
    // and goes off as the second starts (in the same frame, as in Unity: never drawn in between).
    const Area& area = villageArea();
    const RuinsArrowDef& r = area.ruinsArrows[0];
    World world = ruinsWorld("314_692_578");
    world.update(DT, 0, 0, false);
    CHECK(world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0) && world.artifact.tryMove(world.grid, 1, 1, 2, 1, 0));
    settle(world);
    CHECK(!world.objects.rendererEnabled(r.renderer) && !world.particles.playing(r.particles));
    CHECK(world.objects.activeInHierarchy(r.buriedSlider));
    CHECK(litRods(world) == "1-36");
}

TEST(ruins_move_events_see_tiles_at_rest) {
    // 3 1 / 6 2 stay assembled while 5 slides right (into 7's empty slot): OnGridMove re-aims the
    // trail at the buried Slider before tile 5 has moved, even when the move starts between frames
    // (the stylus) and the next frame is long.
    const RuinsArrowDef& r = villageArea().ruinsArrows[0];
    World world = ruinsWorld("314_629_578");
    world.update(DT, 0, 0, false);
    float sx, sy, tx, ty;
    world.objectPosition(r.trail.start, sx, sy);
    world.objectPosition(r.trail.target, tx, ty);
    CHECK(world.artifact.tryMove(world.grid, 0, 0, 1, 0, 0));
    world.update(0.1f, 0, 0, false);
    const village::RuinsArrows::Points& p = world.ruins.trailPoints(0);
    CHECK(p.size() >= 2);
    if (p.size() < 2) return;
    const float len = std::hypot(tx - sx, ty - sy);
    const float ux = (p[1].first - p[0].first) / r.trail.distance, uy = (p[1].second - p[0].second) / r.trail.distance;
    check(std::fabs(ux - (tx - sx) / len) < 1e-4f && std::fabs(uy - (ty - sy) / len) < 1e-4f,
          "trail direction " + std::to_string(ux) + ", " + std::to_string(uy));
}

TEST(ruins_trail_repeats_every_8_seconds) {
    // The trail runs at once and again every 8 s while the arrow shows; each run's sparkles live <= 5 s.
    World world = ruinsWorld("314_629_578");
    int runsSeen = 0;
    bool was = false;
    for (int f = 0; f < 60 * 20; f++) {
        world.update(DT, 0, 0, false);
        const bool any = trailParticles(world) > 0;
        if (any && !was) runsSeen++;
        was = any;
    }
    check(runsSeen == 3, "runs at 0, 8 and 16 s: " + std::to_string(runsSeen));
}

TEST(ruins_arrow_bobs_a_pixel) {
    // BobAnimation (3 s cycle): level until 0.5 s, a pixel down until 1.5 s, level until 2 s, a pixel up.
    const Area& area = villageArea();
    const int renderer = area.ruinsArrows[0].renderer;
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(2, true);
    const struct { float t, dy; } at[] = {{0.3f, 0}, {1.0f, -1}, {1.7f, 0}, {2.5f, 1}, {3.3f, 0}};
    float t = 0;
    for (const auto& s : at) {
        while (t < s.t) {
            world.update(DT, 0, 0, false);
            t += DT;
        }
        check(world.objects.offsetY(renderer) * area.ppu == s.dy, "at " + std::to_string(s.t) + " s: " +
                                                                   std::to_string(world.objects.offsetY(renderer) * area.ppu) + " px");
    }
}

TEST(ruins_unlit_rods_stop_sparkling) {
    const Area& area = villageArea();
    World world = ruinsWorld("314_692_578");
    world.update(DT, 0, 0, false);
    CHECK(world.particles.playing(rod(area, 1).particles) && !world.particles.playing(rod(area, 2).particles));
    // 1 slides out of the corner: 3 1 breaks (3 over 6 stays), rod 1 goes out.
    CHECK(world.artifact.tryMove(world.grid, 1, 2, 1, 1, 0));
    settle(world);
    check(litRods(world) == "--36", "rods " + litRods(world));
    CHECK(!world.particles.playing(rod(area, 1).particles) && world.particles.playing(rod(area, 3).particles));
}
