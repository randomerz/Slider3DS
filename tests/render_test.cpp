// Rendering goldens: AreaView draws fixed scenes through the recording test platform, and each
// scene's draw list (sheet, source rect, pixel position, flags; in order) is hashed and compared
// with tests/golden/render.txt. No GPU or window needed, so it's deterministic.
//
// After an intended rendering or export change, regenerate and review the diff:
//   SLIDER3DS_UPDATE_GOLDENS=1 build-pc/Debug/slider3ds_tests.exe render
// On a mismatch the actual draw list is written to <build>/test-out/<scene>.txt for inspection.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>

#include "game/render/area_view.h"
#include "game/ui/artifact_ui.h"
#include "game/ui/hud.h"
#include "game/ui/end_screen.h"
#include "game/ui/menu_skin.h"
#include "game/ui/pause_menu.h"
#include "game/ui/title_menu.h"
#include "game/ui/title_screen.h"
#include "game/save/profile.h"
#include "game/village/knot_box.h"
#include "game/village/village_grid.h"
#include "game/grid/artifact.h"
#include "game/grid/grid.h"
#include "game/world/cheats.h"
#include "game/world/world.h"
#include "platform/platform.h"
#include "test.h"
#include "test_platform.h"

namespace {


std::string drawListText(const std::vector<DrawCall>& calls) {
    std::ostringstream out;
    for (const DrawCall& c : calls) {
        out << c.sheet << ' ' << c.sx << ' ' << c.sy << ' ' << c.sw << ' ' << c.sh << ' ' << c.x << ' ' << c.y << ' '
            << c.flags;
        if (!c.sheet.empty() && c.w != (float)c.sw) { // sprite scale, if not 1; stretched: its size
            const int k = (int)(c.w / c.sw);
            if (c.w == (float)(c.sw * k) && c.h == (float)(c.sh * k)) out << " x" << k;
            else out << " s" << c.w << 'x' << c.h;
        } else if (!c.sheet.empty() && c.h != (float)c.sh) {
            out << " s" << c.w << 'x' << c.h;
        }
        if (c.fill) out << " f" << c.fill;                                                // solid fill, if any
        if (c.tint) out << " r" << std::lround(c.angle * 5729.578f) << " t" << c.tint;   // turned (1/100 deg), tinted
        out << '\n';
    }
    return out.str();
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : s) h = (h ^ ch) * 1099511628211ull;
    return h;
}

// A golden file of draw-list hashes (tests/golden/<file>): compare() checks the draw calls
// recorded since the last testResetDrawCalls() against the named entry; with
// SLIDER3DS_UPDATE_GOLDENS set, the file is rewritten instead when the set is destroyed.
class GoldenSet {
public:
    explicit GoldenSet(const std::string& file) : path_(std::string(TESTS_DIR) + "/golden/" + file) {
        update_ = std::getenv("SLIDER3DS_UPDATE_GOLDENS") != nullptr;
        std::ifstream in(path_);
        std::string name, value, line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            if (ls >> name >> value) goldens_[name] = value;
        }
        fresh_ << "# Draw-list hashes per scene (tests/render_test.cpp): name count:fnv1a64\n";
    }
    ~GoldenSet() {
        if (!update_) {
            for (const auto& g : goldens_)
                if (!compared_.count(g.first))
                    check(false, g.first + ": golden entry in " + path_ + " no test compares (re-bless to drop it)");
            return;
        }
        std::ofstream(path_, std::ios::binary) << fresh_.str();
        printf("    wrote %s\n", path_.c_str());
    }

    void compare(const std::string& name, size_t minDraws) {
        const std::vector<DrawCall>& calls = testDrawCalls();
        const std::string text = drawListText(calls);
        char value[64];
        snprintf(value, sizeof value, "%zu:%016llx", calls.size(), (unsigned long long)fnv1a(text));
        fresh_ << name << ' ' << value << '\n';
        compared_.insert(name);
        check(calls.size() >= minDraws, name + " draws something (" + std::to_string(calls.size()) + ")");
        if (update_) return;
        auto it = goldens_.find(name);
        if (it != goldens_.end() && it->second == value) return;
        std::string out = std::string(TEST_OUT_DIR) + "/" + name + ".txt";
        std::ofstream(out) << text;
        check(false, name + ": draw list " + value + " != golden " +
                         (it == goldens_.end() ? "(missing)" : it->second) + "; actual list in " + out);
    }

private:
    std::string path_;
    bool update_ = false;
    std::map<std::string, std::string> goldens_;
    std::set<std::string> compared_;
    std::ostringstream fresh_;
};

struct Scene {
    const char* name;
    float camX, camY, time;
    bool showInactive;
    std::function<void(World&)> setup;
};

void allButNine(World& world) {
    for (int id = 1; id <= 8; id++) world.setTileActive(id, true);
    world.artifact.init(world.grid);
}

// A new-game World with the player parked far off screen (scenes that want it place it).
World sceneWorld() {
    World world(villageArea(), DEFAULT_SETTINGS);
    world.player.x = world.player.y = 10000;
    return world;
}

} // namespace

// Tiles 1-8, the knot solved and its sparkles played `frames` frames ago.
static void solvedKnotSparkles(World& w, int frames) {
    allButNine(w);
    village::knotSetToCorrectPositions(w, 0);
    w.knots.update(w);
    village::knotCheckParticles(w, 0);
    for (int i = 0; i < frames; i++) w.particles.update(w, 1.0f / 60.0f);
}

// Tiles 1-6, the ruins assembled by sliding 2 into place, `frames` frames after it arrived: the
// arrow, all four rods lit (runes 1 and 2 flashing white early on), the trail toward tile 5.
static void ruinsLit(World& w, int frames) {
    for (int id = 2; id <= 6; id++) w.setTileActive(id, true);
    w.artifact.setGrid(w.grid, "314_692_578");
    w.reloadObjects();
    w.artifact.tryMove(w.grid, 2, 1, 1, 1, 0);
    for (int i = 0; i < 600 && !w.artifact.queueEmpty(); i++) w.update(1.0f / 60.0f, 0, 0, false);
    for (int i = 0; i < frames; i++) w.update(1.0f / 60.0f, 0, 0, false);
}

TEST(render_goldens) {
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");

    const Scene scenes[] = {
        {"start_tile1", 0, 0, 0, false, nullptr},
        {"start_tile1_inactive_shown", 0, 0, 0, true, nullptr},
        {"default_house", 0.5f, -148.25f, 0, false, nullptr},
        {"all_grid_center", 17, 17, 0, false, allButNine},
        {"all_grid_center_animated", 17, 17, 1.37f, false, allButNine},
        {"all_beach", 52, 20, 0.5f, false, allButNine},
        {"all_top_left", -2, 36, 0, false, allButNine},
        {"mid_slide",
         17, 17, 0, false,
         [](World& w) {
             allButNine(w);
             w.artifact.tryMove(w.grid, 2, 1, 1, 1, 0); // tile 7 left into the empty middle
             for (int i = 0; i < 40; i++) w.artifact.update(w.grid, 1.0f / 60.0f, 0);
         }},
        {"shuffled",
         17, 17, 0, false,
         [](World& w) {
             allButNine(w);
             w.artifact.setGrid(w.grid, "182_043_765");
         }},
        // The solved knot (good lines) and its sparkles flying out, then fading.
        {"knot_sparkles", 32, 33, 0, false, [](World& w) { solvedKnotSparkles(w, 30); }},
        {"knot_sparkles_fading", 32, 33, 0, false, [](World& w) { solvedKnotSparkles(w, 114); }},
        {"ruins_lit_flash", 8, 24, 0, false, [](World& w) { ruinsLit(w, 6); }},
        {"ruins_lit", 8, 24, 0, false, [](World& w) { ruinsLit(w, 60); }},
    };

    GoldenSet goldens("render.txt");
    for (const Scene& sc : scenes) {
        World world = sceneWorld();
        if (sc.setup) sc.setup(world);
        world.knots.update(world); // (the setups change the world outside World::update)
        testResetDrawCalls();
        view.begin(area, sheets, sc.camX, sc.camY, plat::TOP_W, plat::TOP_H, sc.time, sc.showInactive);
        view.draw(world);
        goldens.compare(sc.name, 50);
    }
}

// AreaView culls tile cells by each layer's sprite extents and draws runs of solid-color cells as
// rectangles: against a model that places every cell the plain way (no culling, a sprite each) and
// keeps those touching the screen, from many cameras (fractional, far, mid-slide), every visible
// cell must be drawn — its sprite, or (a solid one) a rectangle of its color covering its square —
// and the view never counts more tile draws than visible cells.
TEST(render_tile_culling_draws_exactly_the_visible_cells) {
    const Area& area = villageArea();
    SpriteSheets sheets;
    check(sheets.load(area), "sheets load");
    AreaView view;
    World still = sceneWorld();
    allButNine(still);
    World sliding = sceneWorld();
    allButNine(sliding);
    sliding.artifact.tryMove(sliding.grid, 2, 1, 1, 1, 0); // (tile 7 left into the middle)
    for (int i = 0; i < 23; i++) sliding.artifact.update(sliding.grid, 1.0f / 60.0f, 0);
    using Key = std::tuple<std::string, int, int, int, int, float, float, uint32_t>;
    int cameras = 0, missing = 0, extra = 0;
    uint32_t seed = 12345;
    const auto rnd = [&seed](float lo, float hi) {
        seed = seed * 1664525u + 1013904223u;
        return lo + (hi - lo) * (float)(seed >> 8) / (float)(1u << 24);
    };
    for (const World* world : {&still, &sliding}) {
        for (int c = 0; c < 40; c++, cameras++) {
            const float camX = rnd(-12, 64), camY = rnd(-12, 64), time = rnd(0, 3);
            testResetDrawCalls();
            view.begin(area, sheets, camX, camY, plat::TOP_W, plat::TOP_H, time, false);
            view.draw(*world);
            std::multiset<Key> drawn;
            std::vector<DrawCall> rects; // (sheet "": a rect; its size in sw / sh, its color in flags)
            for (const DrawCall& d : testDrawCalls()) {
                drawn.insert({d.sheet, d.sx, d.sy, d.sw, d.sh, d.x, d.y, d.flags});
                if (d.sheet.empty()) rects.push_back(d);
            }
            const auto covered = [&](uint32_t color, float x, float y, float w, float h) {
                for (const DrawCall& r : rects)
                    if (r.flags == color && r.x <= x && r.y <= y && r.x + r.sw >= x + w && r.y + r.sh >= y + h) return true;
                return false;
            };
            // A solid cell: a rect of its color over its square; an edge-lined one: its base's, and
            // each line's on the screen side its flags (rotate 90 CCW, then flips) put it.
            const auto coveredByRect = [&](const SpriteDef& S, float px, float py, uint32_t flags) {
                if (S.solid) return covered(S.solid, px, py, (float)S.w, (float)S.h);
                if (!S.edgedBase || !covered(S.edgedBase, px, py, (float)S.w, (float)S.h)) return false;
                for (int side = 0; side < 4; side++) {
                    const float lw = S.edgeWidth[side];
                    if (!lw) continue;
                    int on = side;
                    if (flags & plat::ROT90) on = on == 0 ? 2 : on == 1 ? 3 : on == 2 ? 1 : 0;
                    if (flags & plat::FLIP_X) on = on == 2 ? 3 : on == 3 ? 2 : on;
                    if (flags & plat::FLIP_Y) on = on == 0 ? 1 : on == 1 ? 0 : on;
                    const float x = on == 3 ? px + S.w - lw : px, y = on == 1 ? py + S.h - lw : py;
                    const float w = on >= 2 ? lw : (float)S.w, h = on >= 2 ? (float)S.h : lw;
                    const bool onScreen = x < plat::TOP_W && y < plat::TOP_H && x + w > 0 && y + h > 0;
                    if (onScreen && !covered(S.edgeColor[side], x, y, w, h)) return false; // (an unseen line isn't drawn)
                }
                return true;
            };
            // The model: every cell of every shown layer, placed as the view's worldToScreen would.
            int expected = 0;
            for (const TileLayer& L : area.layers) {
                if (!L.active || (L.owner != 0 && !world->grid.tileActive(L.owner))) continue;
                float bx, by;
                world->grid.ownerBase(L.owner, bx, by);
                for (int cy = 0; cy < L.h; cy++)
                    for (int cx = 0; cx < L.w; cx++) {
                        const int v = L.cell(cx, cy);
                        if (v < 0 || area.tiles[v].frames.empty()) continue;
                        const TileDef& T = area.tiles[v];
                        const int s = T.frames.size() > 1 ? T.frames[(size_t)(time * T.fps) % T.frames.size()] : T.frames[0];
                        const SpriteDef& S = area.sprites[s];
                        float px, py;
                        view.worldToScreen(bx + L.originX + L.minX + cx + L.anchorX - S.pivotX / S.ppu,
                                           by + L.originY + L.minY + cy + L.anchorY + (S.h - S.pivotY) / S.ppu, px, py);
                        px = std::round(px);
                        py = std::round(py);
                        if (px >= plat::TOP_W || py >= plat::TOP_H || px + S.w <= 0 || py + S.h <= 0) continue;
                        expected++;
                        const Key k{area.sheetNames[S.sheet], S.x, S.y, S.w, S.h, px, py,
                                    L.xform.empty() ? 0u : (uint32_t)L.xform[cy * L.w + cx]};
                        auto it = drawn.find(k);
                        if (it != drawn.end()) drawn.erase(it);
                        else if (!coveredByRect(S, px, py, std::get<7>(k))) missing++;
                    }
            }
            if (view.lastTileCount() > expected) extra++;
        }
    }
    check(missing == 0, "every visible tile cell drawn (" + std::to_string(missing) + " missing over " +
                            std::to_string(cameras) + " cameras)");
    check(extra == 0, "no other tile cells drawn (" + std::to_string(extra) + " cameras off)");
}

TEST(render_artifact_goldens) {
    const Area& area = villageArea();
    SpriteSheets sheets;
    check(sheets.load(area), "sheets load");
    GoldenSet goldens("artifact.txt");

    struct Setup {
        World world{villageArea(), DEFAULT_SETTINGS};
        ArtifactUI ui;
        explicit Setup(bool allButNine) {
            if (allButNine)
                for (int id = 1; id <= 8; id++) world.setTileActive(id, true);
            world.artifact.init(world.grid);
            ui.init(villageArea(), world.artifact);
        }
        void touch(bool down, int x, int y, int frames = 1) {
            for (int i = 0; i < frames; i++) {
                plat::Input in;
                in.touching = down;
                float cx, cy;
                ui.slotCenter(villageArea(), x, y, cx, cy);
                in.touchX = (int)cx + (down && i > 0 ? i : 0); // wiggle to start drags
                in.touchY = (int)cy;
                ui.update(in, world, 1.0f / 60.0f);
                world.artifact.update(world.grid, 1.0f / 60.0f, 0);
            }
        }
        void render(const SpriteSheets& sheets) {
            testResetDrawCalls();
            ui.draw(world, sheets);
        }
    };

    {
        Setup s(false);
        s.render(sheets);
        goldens.compare("artifact_start", 11);
    }
    {
        Setup s(false);
        s.touch(true, 0, 0); // tap tile 1: selected, 4 options highlighted
        s.touch(false, 0, 0);
        s.render(sheets);
        goldens.compare("artifact_selected", 11);
    }
    {
        // The D-pad cursor's hover frame: on the selected tile 1, then on an option.
        Setup s(false);
        auto press = [&](uint32_t b) {
            plat::Input in;
            in.pressed = in.held = b;
            s.ui.update(in, s.world, 1.0f / 60.0f);
        };
        press(plat::BTN_DOWN); // (shown: the middle)
        press(plat::BTN_DOWN);
        press(plat::BTN_LEFT);
        press(plat::BTN_X);
        check(s.ui.selected() == 1, "tile 1 selected by the cursor");
        s.render(sheets);
        goldens.compare("artifact_cursor_selected", 11);
        press(plat::BTN_UP);
        s.render(sheets);
        goldens.compare("artifact_cursor_option", 11);
    }
    {
        Setup s(true);
        s.touch(true, 2, 1); // tile 7, then the middle slot: moving (pressed, nudged)
        s.touch(false, 2, 1);
        s.touch(true, 1, 1);
        s.touch(false, 1, 1);
        s.touch(false, 0, 0, 4);
        s.render(sheets);
        goldens.compare("artifact_moving", 11);
    }
    {
        // A SetGrid during World::update (after the UI's update, before its draw) already hides the
        // selection: the draw matches an untouched Artifact's.
        Setup idle(false);
        idle.render(sheets);
        const std::string untouched = drawListText(testDrawCalls());
        Setup s(false);
        s.touch(true, 0, 0);
        s.touch(false, 0, 0);
        s.render(sheets);
        check(drawListText(testDrawCalls()) != untouched, "the selection shows");
        check(s.world.setGrid("624_897_153"), "setGrid");
        s.render(sheets);
        check(drawListText(testDrawCalls()) == untouched, "no stale selection drawn after a setGrid");
    }
    {
        Setup s(true);
        s.touch(true, 1, 0, 6); // drag tile 5 ...
        s.touch(true, 1, 1, 2); // ... hovering over the empty middle
        s.render(sheets);
        goldens.compare("artifact_drag_hover", 11);
    }
}

TEST(render_layer_ranges_compose) {
    // Drawing sorting layer by sorting layer gives exactly the same
    // draw list as drawing everything at once.
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    World world = sceneWorld();
    allButNine(world);
    world.player.x = 17; // in view, so its sorting layer is covered too
    world.player.y = 17;

    testResetDrawCalls();
    view.begin(area, sheets, 17, 17, plat::TOP_W, plat::TOP_H, 0, false);
    view.draw(world);
    const std::string all = drawListText(testDrawCalls());
    const int allCount = view.lastSpriteCount();

    testResetDrawCalls();
    view.begin(area, sheets, 17, 17, plat::TOP_W, plat::TOP_H, 0, false);
    std::set<int> sortLayers;
    for (const TileLayer& L : area.layers) sortLayers.insert(L.sortLayer);
    for (const WorldSprite& ws : area.worldSprites) sortLayers.insert(ws.place.sortLayer);
    sortLayers.insert(area.player.body.sortLayer);
    int layersSeen = 0;
    for (int sl : sortLayers) {
        view.draw(world, sl, sl);
        layersSeen++;
    }
    check(layersSeen > 1, "the export uses several sorting layers (" + std::to_string(layersSeen) + ")");
    check(drawListText(testDrawCalls()) == all && view.lastSpriteCount() == allCount,
          "per-sorting-layer draws == one full draw");
}

TEST(render_player_y_sorts_with_world_sprites) {
    // Unity's custom sort axis (0,1,0): in the same sorting layer and order, the sprite with the
    // lower sort point draws later (in front). The player just below tile 1's tree is drawn after
    // it, just above it before it.
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    World world = sceneWorld();
    size_t treeIndex = area.worldSprites.size();
    for (size_t i = 0; i < area.worldSprites.size(); i++)
        if (area.worldSprites[i].owner == 1 &&
            area.pathOf(area.worldSprites[i].object).find("Decorations/Tree1") != std::string::npos)
            treeIndex = i;
    check(treeIndex < area.worldSprites.size(), "tile 1 has a Tree1");
    if (treeIndex == area.worldSprites.size()) return;
    const WorldSprite& tree = area.worldSprites[treeIndex];
    const SpritePlacement& body = area.player.body;
    check(body.sortLayer == tree.place.sortLayer && body.order == tree.place.order, "player and tree share layer + order");

    float bx, by;
    world.grid.ownerBase(1, bx, by);
    auto drawIndex = [&](const std::vector<DrawCall>& calls, int sprite) {
        const SpriteDef& S = area.sprites[sprite];
        for (size_t i = 0; i < calls.size(); i++)
            if (calls[i].sheet == area.sheetNames[S.sheet] && calls[i].sx == S.x && calls[i].sy == S.y) return (int)i;
        return -1;
    };
    for (float dy : {-0.5f, 0.5f}) {
        world.player.x = bx + tree.place.x + 0.5f;
        world.player.y = by + tree.place.sortY + dy - body.sortY;
        testResetDrawCalls();
        view.begin(area, sheets, world.player.x, world.player.y, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        const SortedSprite treeSprite = placeSprite(area, tree.place, bx, by, AreaView::animTime(area, tree, treeIndex, 0));
        const SortedSprite playerSprite = placeSprite(area, body, world.player.x, world.player.y, 0);
        const int ti = drawIndex(testDrawCalls(), treeSprite.sprite);
        const int pi = drawIndex(testDrawCalls(), playerSprite.sprite);
        check(ti >= 0 && pi >= 0, "tree and player both drawn");
        check(dy < 0 ? pi > ti : pi < ti,
              dy < 0 ? "player below the tree is drawn in front" : "player above the tree is drawn behind");
    }
}

TEST(render_start_check_goldens) {
    // The cave door after the new-game start checks (rocks, no dither shadow) and once the door
    // has exploded (shadowed doorway, no rocks).
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    GoldenSet goldens("objects.txt");
    for (bool exploded : {false, true}) {
        World world = sceneWorld();
        world.state.setFlag("caveDoorExploded", exploded);
        world.reloadObjects();
        testResetDrawCalls();
        view.begin(area, sheets, 36.5f, 50.0f, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        goldens.compare(exploded ? "cave_door_exploded" : "cave_door_new_game", 50);
    }
}

TEST(render_action_indicator_while_an_interaction_is_offered) {
    // PlayerAction's indicator above the player shows while an offered interactable asks for it: a
    // door always, an NPC once its line waiting for the player is typed.
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    const SpriteDef& indicator = area.sprites[area.player.actionIndicator.sprite];
    auto indicatorDrawn = [&](World& world) {
        testResetDrawCalls();
        view.begin(area, sheets, world.player.x, world.player.y, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        // Where it goes above the player (NPCs' message pings use the same sprite).
        const SortedSprite sp = placeSprite(area, area.player.actionIndicator, world.player.x, world.player.y, 0);
        float x, y;
        view.worldToScreen(sp.x - indicator.pivotX / indicator.ppu, sp.y + (indicator.h - indicator.pivotY) / indicator.ppu, x, y);
        for (const DrawCall& c : testDrawCalls())
            if (c.sheet == area.sheetNames[indicator.sheet] && c.sx == indicator.x && c.sy == indicator.y &&
                c.x == std::round(x) && c.y == std::round(y))
                return true;
        return false;
    };
    // Stands the player on the center of `object`'s trigger.
    auto standOnTrigger = [&](World& world, int object) {
        for (const ObjectCollider& c : area.triggerColliders)
            if (c.object == object) {
                float bx, by;
                world.objectBase(c.object, bx, by);
                world.player.x = bx + (c.minX + c.maxX) * 0.5f;
                world.player.y = by + (c.minY + c.maxY) * 0.5f - area.player.footBox.offY;
            }
        world.update(1.0f / 60.0f, 0, 0, false);
    };
    World world(area, DEFAULT_SETTINGS);
    world.placeAt(*area.spawn("Default"));
    world.update(1.0f / 60.0f, 0, 0, false);
    check(world.interactables.empty() && !indicatorDrawn(world), "nothing on offer by the bed: no indicator");
    const int exploresseObject = area.findObject("Map/Village 8Puzzle/STile (1)/All Tile Maps/House Tile Maps/Decorations/Exploresse");
    const int exploresse = area.npcOn(exploresseObject);
    standOnTrigger(world, exploresseObject);
    check(exploresse >= 0 && world.npcs.typer(exploresse).typing(), "next to Exploresse: she starts talking");
    check(!world.interactables.empty() && !indicatorDrawn(world), "she's on offer, but no indicator while typing");
    for (int i = 0; i < 600 && world.npcs.typer(exploresse).typing(); i++) world.update(1.0f / 60.0f, 0, 0, false);
    world.update(1.0f / 60.0f, 0, 0, false);
    check(indicatorDrawn(world), "her line is typed and waits for the player: the indicator shows");
    standOnTrigger(world, area.findObject("Map/Village 8Puzzle/STile (1)/All Tile Maps/House Tile Maps/Decorations/HouseTPAction"));
    check(!world.interactables.empty(), "on tile 1's house exit: it's on offer");
    check(indicatorDrawn(world), "the indicator shows");
}

TEST(render_pause_menu_goldens) {
    // The Paused page as it opens; the Options page (slider selected, toggle on); Quit asking again;
    // Resume held down by the stylus (pressed: its label 2 px lower; the question is dropped).
    MenuSkin skin;
    std::string error;
    check(skin.load(&error), "menu skin loads " + error);
    GoldenSet goldens("menu.txt");
    PauseMenu menu;
    Settings s;
    auto shoot = [&](const char* name) {
        testResetDrawCalls();
        menu.draw(s, skin);
        goldens.compare(name, 20);
    };
    auto press = [&](uint32_t button) {
        plat::Input in;
        in.pressed = button;
        menu.update(in, s);
    };
    menu.open();
    shoot("pause_menu_paused");
    for (int i = 0; i < 31; i++) menu.update(plat::Input{}, s); // the brackets bounced 1 px out
    shoot("pause_menu_bounced");
    press(plat::BTN_DOWN);
    press(plat::BTN_A);
    shoot("pause_menu_options"); // General / Sound / Back
    press(plat::BTN_DOWN);
    press(plat::BTN_A);
    shoot("pause_menu_sound");
    press(plat::BTN_B);
    press(plat::BTN_UP);
    press(plat::BTN_A);
    s.autoMove = true;
    shoot("pause_menu_general");
    press(plat::BTN_DOWN);
    press(plat::BTN_DOWN); // the toggle selected (light label, its selected background), the sliders not
    s.setScreenShake(0);
    shoot("pause_menu_general_min");
    s.setScreenShake(1);
    shoot("pause_menu_general_max");
    press(plat::BTN_B);
    press(plat::BTN_B);
    press(plat::BTN_DOWN);
    press(plat::BTN_A); // Quit: asks again
    CHECK(menu.confirmingQuit());
    shoot("pause_menu_quit_confirm");
    menu.update(plat::Input{}, s);
    plat::Input in;
    in.touching = true;
    const PauseMenu::Box resume = PauseMenu::itemBox(PauseMenu::RESUME);
    in.touchX = resume.x + 8;
    in.touchY = resume.y + 8;
    menu.update(in, s);
    CHECK(menu.isOpen() && menu.cursor() == PauseMenu::RESUME);
    shoot("pause_menu_pressed");

    // In the game: Options with Cheats usable; the Cheats page grayed out, then enabled (Give's
    // second collectible marked, NoClip on, Respawn selected).
    Cheats cheats;
    cheats.collectibles = {"Artifact", "Slider 3", "Boating License"};
    menu.open(PauseMenu::Page::Paused, &cheats);
    menu.update(plat::Input{}, s);
    press(plat::BTN_DOWN);
    press(plat::BTN_A);
    press(plat::BTN_DOWN);
    press(plat::BTN_DOWN);
    shoot("pause_menu_options_in_game");
    press(plat::BTN_A);
    shoot("pause_menu_cheats_off");
    press(plat::BTN_A); // Enable Cheats
    press(plat::BTN_DOWN);
    press(plat::BTN_RIGHT);
    press(plat::BTN_A); // Give: Slider 3 marked
    press(plat::BTN_DOWN);
    press(plat::BTN_A); // NoClip
    press(plat::BTN_DOWN);
    CHECK(s.cheats && cheats.noclip && cheats.toGive == std::vector<int>{1} && menu.cursor() == PauseMenu::RESPAWN);
    shoot("pause_menu_cheats_on");
}

TEST(render_title_screen_goldens) {
    // The top screen's intro: the empty background (the logo off to the left), the logo landing (the
    // shake rising), at rest with the prompt half faded in, half a second after a key (risen part way,
    // the prompt fading out, the stars fading in) and once the stars are in.
    TitleArt art;
    std::string error;
    check(art.load(&error), "title art loads " + error);
    MenuSkin skin;
    check(skin.load(&error), "menu skin loads " + error);
    GoldenSet goldens("title_screen.txt");
    TitleScreen screen;
    screen.start(art);
    auto run = [&](float until, const plat::Input& in = {}) {
        while (screen.time() < until - 1e-4f) screen.update(villageArea().cameraShake, in, 1.0f / 60, 0.5f);
    };
    auto shoot = [&](const char* name, int minDraws = 20) {
        testResetDrawCalls();
        screen.draw(art, skin.font());
        goldens.compare(name, minDraws);
    };
    run(1);
    CHECK(screen.backgroundAlpha(art) == 0);
    shoot("title_start", 7); // (the logo's 6 letters and the player)
    run(2.9f);
    shoot("title_logo_landing", 7);
    run(5);
    CHECK(screen.promptAlpha(art) > 0.4f && screen.promptAlpha(art) < 0.6f && !screen.menuShown());
    shoot("title_prompt");
    plat::Input key;
    key.pressed = plat::BTN_A;
    screen.update(villageArea().cameraShake, key, 1.0f / 60, 0.5f);
    CHECK(screen.menuShown());
    run(5.5f);
    CHECK(screen.backgroundAlpha(art) > 0.3f && screen.backgroundAlpha(art) < 0.7f);
    shoot("title_rising");
    run(7);
    CHECK(screen.backgroundAlpha(art) == 1);
    shoot("title_stars_in");
}

TEST(end_art_refuses_sizes_off_the_screen) {
    // The art must be the original canvas (320x180) up to the top screen (400x240), in its sheet.
    std::string real;
    check(plat::readFile("ui/end.json", real), "ui/end.json");
    auto loads = [&](int x, int y, int w, int h) {
        testSetFile("ui/end.json", R"({"sheet": "ui_end", "sprites": {"art": {"rect": [)" + std::to_string(x) + ", " +
                                       std::to_string(y) + ", " + std::to_string(w) + ", " + std::to_string(h) + "]}}}");
        EndArt art;
        std::string error;
        return art.load(&error);
    };
    CHECK(loads(0, 0, 320, 180) && loads(0, 0, 400, 240));
    CHECK(!loads(0, 0, 300, 180) && !loads(0, 0, 320, 170) && !loads(0, 0, 401, 240) && !loads(0, 0, 400, 241));
    CHECK(!loads(-1, 0, 320, 180) && !loads(0, -1, 320, 180));
    testSetFile("ui/end.json", real);
}

TEST(render_ending_goldens) {
    // The port's ending (EndOfDemo fitted): the top screen half faded in, then both screens once it
    // takes input.
    MenuSkin skin;
    EndArt art;
    std::string error;
    check(skin.load(&error), "menu skin loads " + error);
    check(art.load(&error), "end art loads " + error);
    GoldenSet goldens("ending.txt");
    EndScreen end;
    end.start();
    for (int i = 0; i < EndScreen::FADE_FRAMES / 2; i++) end.update({});
    testResetDrawCalls();
    end.drawTop(art);
    goldens.compare("ending_fading_in", 2); // (the baked canvas + the fade)
    testResetDrawCalls();
    end.drawBottom(skin);
    CHECK(testDrawCalls().empty()); // (no button until it takes input)
    for (int i = 0; i < EndScreen::LOCKOUT_FRAMES; i++) end.update({});
    testResetDrawCalls();
    end.drawTop(art);
    goldens.compare("ending_top", 1);
    testResetDrawCalls();
    end.drawBottom(skin);
    goldens.compare("ending_bottom", 10);
}

TEST(render_title_menu_goldens) {
    // Main without profiles (Continue grayed) and with one; Saves: a profile (1/9, 1h32), an
    // unreadable file, one that used cheats (the dark card); delete mode with a card awaiting Confirm; New Save with a name
    // in capitals.
    MenuSkin skin;
    std::string error;
    check(skin.load(&error), "menu skin loads " + error);
    GoldenSet goldens("title.txt");
    TitleMenu menu;
    auto shoot = [&](const char* name) {
        testResetDrawCalls();
        menu.draw(skin);
        goldens.compare(name, 20);
    };
    auto press = [&](uint32_t button) {
        plat::Input in;
        in.pressed = button;
        menu.update(in);
    };
    testUserFiles().clear();
    menu.open();
    menu.update(plat::Input{});
    shoot("title_main_new");
    SaveProfile p = SaveProfile::make("Boomo");
    p.playTime = 3600 + 32 * 60;
    p.state.setCollectible("Slider 9", "Village", true);
    p.save(0);
    testUserFiles()[SaveProfile::fileName(1)] = "garbage";
    SaveProfile cheated = SaveProfile::make("Cheater");
    cheated.state.setFlag(cheats::USED_CHEATS_FLAG, true);
    cheated.save(2);
    menu.open();
    menu.update(plat::Input{});
    shoot("title_main");
    press(plat::BTN_DOWN);
    press(plat::BTN_A);
    shoot("title_saves");
    auto tap = [&](TitleMenu::Widget w) {
        TitleMenu::Box b;
        check(menu.box(w, b), "widget shown");
        plat::Input in;
        in.touching = true;
        in.touchX = b.x + b.w / 2;
        in.touchY = b.y + b.h / 2;
        menu.update(in);
        menu.update(plat::Input{});
    };
    tap(TitleMenu::TRASH); // delete mode
    tap(TitleMenu::CARD);  // the first card awaits Confirm (its text showing: the blink's first half)
    CHECK(menu.deleting() && menu.confirming() == 0);
    shoot("title_saves_delete");
    press(plat::BTN_B);
    press(plat::BTN_A);
    press(plat::BTN_DOWN);
    press(plat::BTN_DOWN);
    press(plat::BTN_DOWN);
    press(plat::BTN_A); // the empty slot: New Save
    press(plat::BTN_DOWN);
    press(plat::BTN_DOWN);
    press(plat::BTN_DOWN);
    press(plat::BTN_A); // Caps
    press(plat::BTN_UP);
    press(plat::BTN_A);
    press(plat::BTN_A);
    shoot("title_new_save");
    testUserFiles().clear();
}

TEST(render_npc_dialogue_goldens) {
    // Exploresse's greeting typed over her head: part-way, and fully typed (the wavy "strange."
    // bobbing, the " ..." waiting for the player). The camera is on her.
    const Area& area = villageArea();
    SpriteSheets sheets;
    Font font;
    AreaView view;
    std::string error;
    check(sheets.load(area) && font.load("fonts/dialogue.json", &error), "sheets and font load " + error);
    view.setFont(&font);
    GoldenSet goldens("dialogue.txt");
    const int npc = area.npcOn(area.findObject("Map/Village 8Puzzle/STile (1)/All Tile Maps/House Tile Maps/Decorations/Exploresse"));
    check(npc >= 0, "Exploresse");
    if (npc < 0) return;
    const NpcDef& n = area.npcs[npc];
    World world = sceneWorld();
    float nx, ny;
    world.objectPosition(n.object, nx, ny);
    world.player.x = nx - 0.5f;
    world.player.y = ny;
    auto shoot = [&](const char* name) {
        testResetDrawCalls();
        view.begin(area, sheets, nx, ny + 1, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        goldens.compare(name, 50);
    };
    for (int i = 0; i < 20; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.npcs.typer(npc).typing(), "still typing after 20 frames");
    shoot("exploresse_typing");
    for (int i = 0; i < 600 && world.npcs.typer(npc).typing(); i++) world.update(1.0f / 60.0f, 0, 0, false);
    world.update(1.0f / 60.0f, 0, 0, false);
    shoot("exploresse_typed");
}

TEST(render_cat_count_golden) {
    // Cathy's house with Mittens returned: CatTrick's "1/3" in the dialogue font.
    const Area& area = villageArea();
    SpriteSheets sheets;
    Font font;
    AreaView view;
    std::string error;
    check(sheets.load(area) && font.load("fonts/dialogue.json", &error), "sheets and font load " + error);
    view.setFont(&font);
    GoldenSet goldens("cats.txt");
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(9, true);
    world.reloadObjects();
    const CatTrickDef& c = area.catTricks[0];
    float bx, by;
    world.objectBase(c.object, bx, by);
    const float x = bx + (c.zoneMinX + c.zoneMaxX) / 2, y = by + (c.zoneMinY + c.zoneMaxY) / 2;
    world.items.drop(world, c.cats[0], x, y);
    for (int i = 0; i < 40; i++) world.update(1.0f / 60.0f, 0, 0, true);
    check(world.objects.activeInHierarchy(c.textObjects[0]), "the count shows");
    testResetDrawCalls();
    view.begin(area, sheets, x, y, plat::TOP_W, plat::TOP_H, 0, false);
    view.draw(world);
    goldens.compare("cat_count_1_of_3", 50);
}

TEST(render_npc_walk_goldens) {
    // NPCs mid-walk, the camera on them: Chad on his way to the rocks (the Village solved), Romeo
    // coming down onto tile 1 (his sprite, ping and dialogue trigger follow the NPC).
    const Area& area = villageArea();
    SpriteSheets sheets;
    Font font;
    AreaView view;
    std::string error;
    check(sheets.load(area) && font.load("fonts/dialogue.json", &error), "sheets and font load " + error);
    view.setFont(&font);
    GoldenSet goldens("walks.txt");
    auto shoot = [&](World& world, const char* npcName, const char* name) {
        int npc = -1;
        for (int i = 0; i < (int)area.npcs.size(); i++)
            if (area.npcs[i].name == npcName) npc = i;
        check(npc >= 0 && world.walks.walking(npc), std::string(npcName) + " walking");
        if (npc < 0) return;
        float x, y;
        world.objectPosition(area.npcs[npc].object, x, y);
        testResetDrawCalls();
        view.begin(area, sheets, x, y, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        goldens.compare(name, 50);
    };
    {
        World world = sceneWorld();
        for (int id = 2; id <= 9; id++) world.setTileActive(id, true);
        check(world.setGrid("624_897_153"), "solved grid");
        for (int i = 0; i < 150; i++) world.update(1.0f / 60.0f, 0, 0, false);
        shoot(world, "Chad", "chad_walking");
    }
    {
        World world = sceneWorld();
        check(world.setGrid("641_892_753"), "tile 1 top-right");
        for (int i = 0; i < 40; i++) world.update(1.0f / 60.0f, 0, 0, false);
        shoot(world, "Romeo", "romeo_walking");
    }
}

TEST(render_cat_abduction_golden) {
    // The cat in its bubble over the water, riding the sine wave east (the camera on it).
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    GoldenSet goldens("abduction.txt");
    World world = sceneWorld();
    world.abduction.introFinished();
    for (int i = 0; i < 60 * 14; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.abduction.started() && world.objects.rendererEnabled(area.areaGrid.catAbduction.bubble), "floating");
    float x, y;
    world.objectPosition(area.items[area.areaGrid.catAbduction.item].object, x, y);
    testResetDrawCalls();
    view.begin(area, sheets, x, y, plat::TOP_W, plat::TOP_H, 0, false);
    view.draw(world);
    goldens.compare("cat_in_bubble", 20);
}

TEST(render_intro_goldens) {
    // The intro's camera dolly mid-roll (Romeo walking in, the player held in the house) with the
    // skip prompt half held, as main draws them: the view at World::camera, the prompt at 1x over it.
    const Area& area = villageArea();
    SpriteSheets sheets;
    Font font;
    MenuSkin skin;
    AreaView view;
    std::string error;
    check(sheets.load(area) && font.load("fonts/dialogue.json", &error) && skin.load(&error), "assets load " + error);
    view.setFont(&font);
    GoldenSet goldens("intro.txt");
    World world = sceneWorld();
    world.placeAt(area.spawns[0]);
    village::houseExit(world);
    for (int i = 0; i < 160; i++) world.update(1.0f / 60.0f, 0, 0, false);
    world.action();
    for (int i = 0; i < 30; i++) world.update(1.0f / 60.0f, FrameInput{0, 0, true});
    check(world.dollies.running() && world.dollies.promptShown(), "rolling, the prompt up");
    float x, y;
    world.camera(x, y);
    testResetDrawCalls();
    view.begin(area, sheets, x, y, plat::TOP_W, plat::TOP_H, 0, false);
    view.draw(world);
    drawTopOverlays(world, skin.font(), skin.captionFont()); // (as main draws them: the prompt, then the panels)
    goldens.compare("intro_rolling_skip_held", 50);
}

TEST(render_white_flash_follows_the_screen_effects_setting) {
    // UIEffects.FlashWhite peaks at the setting's alpha (none at 0: no panel drawn).
    MenuSkin skin;
    std::string error;
    check(skin.load(&error), "menu skin loads " + error);
    for (float amount : {1.0f, 0.5f, 0.0f}) {
        Settings s;
        s.screenEffects = amount;
        World world(villageArea(), s);
        world.effects.flashWhite();
        for (int i = 0; i < 300 && world.effects.whiteAlpha(world.area) < 1; i++) world.update(1.0f / 60.0f, 0, 0, true);
        check(world.effects.whiteAlpha(world.area) == 1, "at the flash's peak");
        testResetDrawCalls();
        drawTopOverlays(world, skin.font(), skin.captionFont());
        int alpha = -1;
        for (const DrawCall& c : testDrawCalls())
            if (c.sheet.empty() && c.w == plat::TOP_W && c.h == plat::TOP_H) alpha = (int)(c.flags & 0xff);
        check(alpha == (amount > 0 ? (int)(amount * 255) : -1), "the panel's alpha at " + std::to_string(amount) + ": " +
                                                                   std::to_string(alpha));
    }
}

TEST(render_item_pickup_golden) {
    // ItemPickupEffect 0.15 s in (the dark circle growing over the world, the item inside it, the
    // player on top) and 1 s in (it covers the view; "Slider 2 Acquired!").
    const Area& area = villageArea();
    SpriteSheets sheets;
    Font font;
    MenuSkin skin;
    AreaView view;
    std::string error;
    check(sheets.load(area) && font.load("fonts/dialogue.json", &error) && skin.load(&error), "assets load " + error);
    view.setFont(&font);
    GoldenSet goldens("pickup.txt");
    World world = sceneWorld();
    float x, y;
    world.objectPosition(area.collectibles[area.collectible("Slider 2")].object, x, y);
    world.placeNear(x, y);
    auto shoot = [&](const char* name) {
        float cx, cy;
        world.camera(cx, cy);
        testResetDrawCalls();
        view.begin(area, sheets, cx, cy, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        drawTopOverlays(world, skin.font(), skin.captionFont());
        goldens.compare(name, 10);
    };
    for (int i = 0; i < 9; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.pickup.circleDiameter() > 2 && world.pickup.circleDiameter() < 10, "the circle growing");
    shoot("pickup_expanding");
    for (int i = 9; i < 61; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.pickup.active() && world.pickup.textShown(), "the cutscene, its text up");
    shoot("pickup_text");
}

TEST(render_item_pickup_wraps_a_long_name_upward) {
    // No Village item's caption is too wide at 2x; later areas' are ("Legendary Cheese Burger
    // Acquired!" is 498 px): words wrap, the last line where one line sits, the rest 32 px (one em
    // at 2x) above it. It wraps at the screen less 8 px each side: "Crystal Chunk #2 Acquired!" (390
    // px) fits the 400 px screen but wraps.
    MenuSkin skin;
    std::string error;
    check(skin.load(&error), "skin loads " + error);
    const Font& font = skin.captionFont();
    using Lines = std::vector<std::string>;
    CHECK(font.wrap("Slider 2 Acquired!", 2, 384) == Lines{"Slider 2 Acquired!"});
    CHECK(font.wrap("Legendary Cheese Burger Acquired!", 2, 384) == (Lines{"Legendary Cheese Burger", "Acquired!"}));
    CHECK(font.wrap("Supercalifragilistic Acquired!", 1, 40) == (Lines{"Supercalifragilistic", "Acquired!"})); // (too wide alone)
    const int exact = font.textWidth("Slider 2 Acquired!", 2);
    CHECK(font.wrap("Slider 2 Acquired!", 2, exact) == Lines{"Slider 2 Acquired!"}); // (exactly as wide: fits)
    CHECK(font.wrap("Slider 2 Acquired!", 2, exact - 1).size() == 2);
    CHECK(font.wrap("Slider  2", 1, 1000) == Lines{"Slider 2"}); // (a run of spaces: one)

    auto pickUp = [&](const char* name, int& aY, int& firstY, char first) {
        Area area = villageArea(); // (a copy: Slider 2 renamed)
        const int index = area.collectible("Slider 2");
        CHECK(index >= 0);
        if (index < 0) return;
        area.collectibles[index].name = name;
        World world(area, DEFAULT_SETTINGS);
        float x, y;
        world.objectPosition(area.collectibles[index].object, x, y);
        world.placeNear(x, y);
        for (int i = 0; i < 61; i++) world.update(1.0f / 60.0f, 0, 0, false);
        CHECK(world.pickup.textShown());
        testResetDrawCalls();
        drawTopOverlays(world, skin.font(), skin.captionFont());
        const GlyphRect& a = font.def().glyph('A').text;
        const GlyphRect& f = font.def().glyph(first).text;
        aY = firstY = -1;
        for (const DrawCall& c : testDrawCalls()) {
            if (c.sx == a.x && c.sy == a.y) aY = (int)c.y; // (the last 'A': "Acquired!")
            if (c.sx == f.x && c.sy == f.y && firstY < 0) firstY = (int)c.y;
        }
    };
    const GlyphRect& capA = font.def().glyph('A').text;
    const int baseline = plat::TOP_H / 2 - 50 - (int)std::floor(capA.oy + capA.h / 2.0f) * 2;
    int aY, lY;
    pickUp("Legendary Cheese Burger", aY, lY, 'L');
    GoldenSet goldens("pickup_wrapped.txt");
    goldens.compare("pickup_wrapped", 10);
    check(aY == baseline + capA.oy * 2, "Acquired! on the one-line spot: " + std::to_string(aY));
    check(lY == baseline - 32 + font.def().glyph('L').text.oy * 2, "Legendary... a line above: " + std::to_string(lY));
    pickUp("Crystal Chunk #2", aY, lY, 'C');
    check(aY == baseline + capA.oy * 2 && lY == baseline - 32 + font.def().glyph('C').text.oy * 2,
          "Crystal Chunk #2 wraps (the margin): " + std::to_string(lY));
}

TEST(render_smoke_golden) {
    // The cave door blown open: five SmokePoofs' turned, tinted frames 0.3 s and 1.5 s in.
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    GoldenSet goldens("smoke.txt");
    World world(area, DEFAULT_SETTINGS);
    float x, y;
    world.objectPosition(area.areaGrid.caveDoorEntrance, x, y);
    world.placeNear(x, y - 3);
    village::explode(world);
    auto shoot = [&](const char* name) {
        float cx, cy;
        world.camera(cx, cy);
        testResetDrawCalls();
        view.begin(area, sheets, cx, cy, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        int tinted = 0;
        for (const DrawCall& c : testDrawCalls()) tinted += c.tint != 0;
        check(tinted == 30, std::string(name) + ": 30 smoke frames, got " + std::to_string(tinted));
        goldens.compare(name, 10);
    };
    for (int i = 0; i < 18; i++) world.update(1.0f / 60.0f, 0, 0, false);
    shoot("smoke_early");
    for (int i = 0; i < 72; i++) world.update(1.0f / 60.0f, 0, 0, false);
    shoot("smoke_late");
}

TEST(render_tree_sway_golden) {
    // Tile 1 sliding with its trees (MomentumGizmo): dragged mid-slide, then the impact as it lands.
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    GoldenSet goldens("trees.txt");
    World world(area, DEFAULT_SETTINGS);
    int tree = -1;
    for (size_t i = 0; i < area.momentumGizmos.size() && tree < 0; i++)
        if (area.momentumGizmos[i].stile == 1) tree = area.momentumGizmos[i].animator;
    check(tree >= 0, "a tree on tile 1");
    float x, y;
    world.objectPosition(area.animators[tree].object, x, y);
    const TileState* t = world.grid.tile(1);
    const auto options = world.artifact.moveOptions(world.grid, t->x, t->y);
    check(!options.empty() && world.artifact.tryMove(world.grid, t->x, t->y, options[0].first, options[0].second, 0),
          "tile 1 slides");
    auto shoot = [&](const char* name) {
        world.objectPosition(area.animators[tree].object, x, y);
        testResetDrawCalls();
        view.begin(area, sheets, x, y, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        goldens.compare(name, 10);
    };
    for (int i = 0; i < 20; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.animators.stateName(area, tree).rfind("TreeDrag", 0) == 0, "dragging");
    shoot("trees_drag");
    while (world.grid.moving()) world.update(1.0f / 60.0f, 0, 0, false);
    for (int i = 0; i < 4; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.animators.stateName(area, tree).rfind("TreeImpact", 0) == 0, "landing");
    shoot("trees_impact");
}

TEST(render_particles_near_the_edge_are_drawn) {
    // Particles whose centers sit just off each edge (swept 0-9 px out) but whose square or turned
    // frame reaches a pixel in: each one is drawn — its own draw, matched by center and turn (counts
    // alone would let extra draws hide a missing one). Twenty poofs a step, for many turns.
    const Area& area = villageArea();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    const float W = plat::TOP_W, H = plat::TOP_H;
    const int dirs[4][2] = {{-1, 0}, {1, 0}, {0, 1}, {0, -1}}; // left, right, top, bottom (world y up)
    int checked = 0, missing = 0;
    for (const auto& dir : dirs)
        for (ParticleType type : {ParticleType::SmokePoof, ParticleType::MiniSparkle})
            for (int off = 0; off <= 36; off++) { // (quarter pixels)
                World world = sceneWorld();
                float cx, cy;
                world.camera(cx, cy);
                const float out = ((dir[0] ? W : H) / 2 + off / 4.0f) / area.ppu;
                for (int k = 0; k < 20; k++) world.particles.spawnPrefab(world, type, cx + dir[0] * out, cy + dir[1] * out, -1);
                world.particles.update(world, 1e-4f); // (the first burst, barely moved)
                testResetDrawCalls();
                view.begin(area, sheets, cx, cy, plat::TOP_W, plat::TOP_H, 0, false);
                view.draw(world);
                for (const Particles::Particle& p : world.particles.particles()) {
                    const float sx = (p.x - cx) * area.ppu + W / 2, sy = (cy - p.y) * area.ppu + H / 2; // its center
                    const float half = p.def->size * area.ppu / 2;
                    const bool frames = !p.def->frames.empty();
                    const float reach = frames ? half * (std::fabs(std::cos(p.rotation)) + std::fabs(std::sin(p.rotation))) : half;
                    // (a pixel center inside: half a pixel past the edge)
                    if (sx + reach <= 0.5f || sx - reach >= W - 0.5f || sy + reach <= 0.5f || sy - reach >= H - 0.5f) continue;
                    checked++;
                    bool drawn = false;
                    for (const DrawCall& c : testDrawCalls()) {
                        // (its own draw: a frame turned as it is, a square its size)
                        const bool kind = frames ? c.tint != 0 && c.angle == p.rotation && std::fabs(c.w - 2 * half) < 1e-3f
                                                 : c.sheet.empty() && std::fabs(c.w - 2 * half) < 1e-3f &&
                                                       std::fabs(c.h - 2 * half) < 1e-3f;
                        drawn = drawn || (kind && std::fabs(c.x + c.w / 2 - sx) < 1e-3f && std::fabs(c.y + c.h / 2 - sy) < 1e-3f);
                    }
                    missing += !drawn;
                }
            }
    check(checked > 100, "particles that reach in: " + std::to_string(checked));
    check(missing == 0, "visible particles culled: " + std::to_string(missing));
}

TEST(render_hints_golden) {
    // The hint box along the top screen's bottom: "move" fading in (4.25 s after the scene starts),
    // fully shown, then its text fading out for the next one's (dismissed with "interact" waiting).
    const Area& area = villageArea();
    SpriteSheets sheets;
    MenuSkin skin;
    AreaView view;
    std::string error;
    check(sheets.load(area) && skin.load(&error), "assets load " + error);
    GoldenSet goldens("hints.txt");
    World world = sceneWorld();
    auto shoot = [&](const char* name) {
        float cx, cy;
        world.camera(cx, cy);
        testResetDrawCalls();
        view.begin(area, sheets, cx, cy, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        drawTopOverlays(world, skin.font(), skin.captionFont());
        goldens.compare(name, 10);
    };
    for (int i = 0; i < 255; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.hints.shownHint() == area.hints.find("move") && world.hints.boxAlpha() > 0.3f &&
              world.hints.boxAlpha() < 0.7f,
          "move fading in");
    shoot("hint_fading_in");
    for (int i = 0; i < 30; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.hints.boxAlpha() == 1, "shown");
    shoot("hint_shown");
    world.hints.trigger(world, area.hints.find("interact"));
    for (int i = 0; i < 5 * 60 + 2; i++) world.update(1.0f / 60.0f, 0, 0, false);
    world.hints.disable(world, area.hints.find("move"));
    for (int i = 0; i < 15; i++) world.update(1.0f / 60.0f, 0, 0, false);
    check(world.hints.textAlpha() > 0.3f && world.hints.textAlpha() < 0.7f && world.hints.boxAlpha() == 1, "swapping");
    shoot("hint_swapping");
}
