// The save profile (game/save/profile): its file format, and World::save / World::load.
#include <algorithm>
#include <cmath>
#include <string>

#include "game/save/profile.h"
#include "game/village/cat_trick.h"
#include "game/world/world.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

void frames(World& w, int n) {
    for (int i = 0; i < n; i++) w.update(DT, 0, 0, true);
}

const CollectibleDef* collectible(const Area& area, const char* name) {
    for (const CollectibleDef& c : area.collectibles)
        if (c.name == name) return &c;
    return nullptr;
}

} // namespace

TEST(profile_round_trips_through_its_text) {
    SaveProfile p = SaveProfile::make("Tab\there\\ new\nline");
    p.playTime = 1234.5678f;
    p.state.setFlag("villageIntroCutscene", true);
    p.state.setFlag("a flag with spaces", true);
    p.state.setStr("key", "value\twith\\escapes\r\n");
    p.state.setCollectible("Slider 2", "Village", true);
    p.grid = "123_456_789";
    p.activeTiles = {1, 2, 9};
    p.hasPlayer = true;
    p.playerX = -3.1415927f;
    p.playerY = 1e-7f;
    p.onWater = true;
    SaveProfile q;
    CHECK(SaveProfile::parse(p.serialize(), q));
    CHECK(q.name == p.name && q.playTime == p.playTime && q.grid == p.grid && q.activeTiles == p.activeTiles);
    CHECK(q.hasPlayer && q.playerX == p.playerX && q.playerY == p.playerY && q.onWater && !q.inHouse);
    CHECK(q.state.flag("villageIntroCutscene") && q.state.flag("a flag with spaces") && !q.state.flag("other"));
    CHECK(q.state.str("key") == "value\twith\\escapes\r\n" && q.state.str("Cat") == p.name);
    CHECK(q.state.str("CatUpper") == "TAB\tHERE\\ NEW\nLINE");
    CHECK(q.state.hasCollectible("Slider 2", "Village") && !q.state.hasCollectible("Slider 3", "Village"));

    // Never saved: no grid or player lines, and none read back.
    SaveProfile fresh = SaveProfile::make("Cat"), r;
    CHECK(SaveProfile::parse(fresh.serialize(), r) && r.grid.empty() && r.activeTiles.empty() && !r.hasPlayer);
}

TEST(profile_parse_rejects_bad_files) {
    const std::string good = SaveProfile::make("Cat").serialize();
    SaveProfile p;
    CHECK(SaveProfile::parse(good, p));
    CHECK(SaveProfile::parse(good + "someLaterKind\tx\ty\n", p)); // (a later version's: skipped)
    CHECK(SaveProfile::parse(good + "\n\n", p));
    const char* bad[] = {
        "",                                   // no header
        "slider3ds-profile 2\nname\tCat\n",   // another format version
        "name\tCat\n",                        // no header line
    };
    for (const char* text : bad) check(!SaveProfile::parse(text, p), std::string("rejects: ") + text);
    const char* badLines[] = {
        "name\tCat\textra\n",      // wrong field count
        "flag\n",                  // missing field
        "playTime\tabc\n",         // not a number
        "playTime\t-1\n",          // negative
        "playTime\tinf\n",         // not finite
        "flag\tbad\\escape\n",     // unknown escape
        "flag\tends with\\\n",     // dangling backslash
        "grid\t\n",                // empty grid
        "activeTiles\t1 x\n",      // not ids
        "player\t1\t2\t3\t0\n",    // a bool that isn't 0/1
        "player\t1\t2\t0\n",       // wrong field count
    };
    for (const char* ln : badLines) check(!SaveProfile::parse(good + ln, p), std::string("rejects line: ") + ln);
}

TEST(profile_load_and_save_use_the_slot_file) {
    testUserFiles().clear();
    SaveProfile p;
    CHECK(SaveProfile::load(1, p) == SaveProfile::Read::None);
    SaveProfile q = SaveProfile::make("Mittens");
    CHECK(q.save(1) && testUserFiles().count(SaveProfile::fileName(1)) == 1);
    CHECK(SaveProfile::load(1, p) == SaveProfile::Read::Ok && p.name == "Mittens");
    CHECK(SaveProfile::load(0, p) == SaveProfile::Read::None && SaveProfile::load(2, p) == SaveProfile::Read::None);
    testUserFiles()[SaveProfile::fileName(1)] = "garbage";
    CHECK(SaveProfile::load(1, p) == SaveProfile::Read::Bad);
    testUserFiles().clear();
}

TEST(game_state_assign_moves_the_version_past_both) {
    GameState a, b;
    b.setFlag("x", true);
    b.setFlag("y", true);
    a.assign(b);
    CHECK(a.flag("x") && a.flag("y") && a.version() > b.version());
    GameState c;
    const uint32_t before = a.version();
    a.assign(c); // (an emptier state with a lower version)
    CHECK(!a.flag("x") && a.version() > before);
}

TEST(world_save_then_load_restores_the_game) {
    const Area& area = villageArea();
    World w(area, DEFAULT_SETTINGS);
    w.state.newProfile("Boomo");
    w.setTileActive(2, true);
    w.setTileActive(9, true);
    CHECK(w.setGrid("312_456_789"));
    w.state.setFlag("villageIntroCutscene", true);
    const CollectibleDef* slider2 = collectible(area, "Slider 2");
    CHECK(slider2 != nullptr);
    if (!slider2) return;
    w.state.setCollectible(slider2->name, slider2->area, true);
    float sx, sy;
    w.grid.ownerBase(2, sx, sy);
    w.placeNear(sx + 0.5f, sy - 0.5f);
    frames(w, 30);
    SaveProfile p = SaveProfile::make("Boomo");
    w.save(p);
    CHECK(p.grid == "312_456_789" && p.activeTiles == std::vector<int>({1, 2, 9}));
    CHECK(std::fabs(p.playTime - 30 * DT) < 1e-4);

    SaveProfile read;
    CHECK(SaveProfile::parse(p.serialize(), read));
    World l(area, DEFAULT_SETTINGS);
    l.load(read);
    check(l.grid.gridString(true) == "312_456_789", l.grid.gridString(true));
    check(l.grid.gridString() == "#12_###_##9", l.grid.gridString()); // (only 1, 2 and 9 collected)
    CHECK(l.grid.tileActive(2) && l.grid.tileActive(9) && !l.grid.tileActive(5));
    int bx, by;
    CHECK(l.artifact.findButton(3, bx, by) && bx == 0 && by == 2); // (the Artifact matches)
    CHECK(l.player.x == w.player.x && l.player.y == w.player.y && l.player.stile == 2);
    CHECK(l.state.flag("villageIntroCutscene") && l.state.str("Cat") == "Boomo");
    CHECK(!l.objects.activeInHierarchy(slider2->object)); // (collected: SGrid.Start hides it)
    // (CatAbductionController.Start saw the intro flag: the cat waits on the beach)
    const CatAbductionDef& abduction = area.areaGrid.catAbduction;
    float cx, cy;
    l.objectPosition(area.items[abduction.item].object, cx, cy);
    CHECK(l.state.flag("villageExploresseSequenceDone") && cx == abduction.landingX && cy == abduction.landingY);
    CHECK(l.playTime == read.playTime);
}

TEST(world_load_of_a_new_profile_is_a_new_game) {
    // After a session that went elsewhere (in the boat, on another tile), a never-saved profile
    // starts at the new game's spawn, as a fresh scene load would.
    const Area& area = villageArea();
    World w(area, DEFAULT_SETTINGS);
    w.setTileActive(2, true);
    w.state.setFlag("x", true);
    w.placeNear(3, 4);
    w.player.onWater = true;
    frames(w, 10);
    w.load(SaveProfile::make("Tofu"));
    CHECK(!w.grid.tileActive(2) && !w.state.flag("x") && w.state.str("Cat") == "Tofu" && w.playTime == 0);
    World fresh(area, DEFAULT_SETTINGS);
    const int start = fresh.placeAtStart();
    CHECK(start >= 0 && area.spawns[start].name == "Default");
    CHECK(w.player.x == fresh.player.x && w.player.y == fresh.player.y && !w.player.onWater);
    CHECK(w.player.inHouse && w.player.stile == 1); // (the Default spawn is in tile 1's house)
}

TEST(world_save_then_load_keeps_the_boat_and_the_house) {
    const Area& area = villageArea();
    World w(area, DEFAULT_SETTINGS);
    w.placeAtStart();
    CHECK(w.player.inHouse);
    SaveProfile p;
    w.save(p);
    CHECK(p.inHouse && !p.onWater);
    float bx, by;
    w.grid.ownerBase(1, bx, by);
    w.placeNear(bx, by); // (outdoors)
    CHECK(!w.player.inHouse);
    w.player.onWater = true;
    w.save(p);
    SaveProfile read;
    CHECK(SaveProfile::parse(p.serialize(), read));
    World l(area, DEFAULT_SETTINGS);
    l.load(read);
    CHECK(l.player.onWater && !l.player.inHouse && &l.player.hitbox(area) == &area.player.boatBox);
}

TEST(world_load_keeps_the_authored_grid_if_the_saved_one_doesnt_fit) {
    World w(villageArea(), DEFAULT_SETTINGS);
    const std::string authored = w.grid.gridString(true);
    SaveProfile p = SaveProfile::make("Cat");
    p.grid = "1234_5678"; // (8 tiles: not this grid)
    p.activeTiles = {1, 2, 3};
    w.load(p);
    CHECK(w.grid.gridString(true) == authored && !w.grid.tileActive(2));
}

TEST(world_save_puts_the_player_where_its_sliding_tile_ends) {
    World w(villageArea(), DEFAULT_SETTINGS);
    float bx, by;
    w.grid.ownerBase(1, bx, by);
    w.placeNear(bx, by);
    frames(w, 1);
    CHECK(w.player.stile == 1);
    const TileState* t = w.grid.tile(1);
    const int fromX = t->x, fromY = t->y;
    bool moved = false; // (only tile 1 is collected: any slot beside it is free)
    for (int d = 0; d < 4 && !moved; d++) {
        const int tx = fromX + (d == 0) - (d == 1), ty = fromY + (d == 2) - (d == 3);
        if (tx >= 0 && ty >= 0 && tx < w.grid.width() && ty < w.grid.height())
            moved = w.artifact.tryMove(w.grid, fromX, fromY, tx, ty, w.player.stile);
    }
    CHECK(moved);
    frames(w, 6); // (mid-slide)
    CHECK(w.grid.tileMoving(1));
    const float offX = w.player.x - t->posX * w.grid.tileWidth(), offY = w.player.y - t->posY * w.grid.tileWidth();
    SaveProfile p;
    w.save(p);
    CHECK(std::fabs(p.playerX - (t->x * w.grid.tileWidth() + offX)) < 1e-4f);
    CHECK(std::fabs(p.playerY - (t->y * w.grid.tileWidth() + offY)) < 1e-4f);
    frames(w, 120); // (the slide ends: the player rides it there)
    CHECK(std::fabs(p.playerX - w.player.x) < 1e-3f && std::fabs(p.playerY - w.player.y) < 1e-3f);
}

TEST(collectible_pick_up_asks_for_a_save) {
    const Area& area = villageArea();
    World w(area, DEFAULT_SETTINGS);
    const CollectibleDef* slider2 = collectible(area, "Slider 2");
    CHECK(slider2 && w.objects.activeInHierarchy(slider2->object));
    if (!slider2) return;
    frames(w, 1);
    CHECK(!w.saveRequested);
    float x, y;
    w.objectPosition(slider2->object, x, y);
    w.placeNear(x, y);
    frames(w, 2);
    CHECK(w.state.hasCollectible("Slider 2", "Village") && w.saveRequested);
    w.load(SaveProfile::make("Cat")); // (a load starts without one)
    CHECK(!w.saveRequested);
}

TEST(cat_trick_load_puts_returned_cats_on_their_spawns) {
    const Area& area = villageArea();
    World w(area, DEFAULT_SETTINGS);
    const CatTrickDef& c = area.catTricks[0];
    w.state.setFlag(village::CAT_FLAGS[0], true); // Mittens and Tofu back, Autumn not
    w.state.setFlag(village::CAT_FLAGS[2], true);
    SaveProfile p = SaveProfile::make("Cat");
    w.save(p);
    World l(area, DEFAULT_SETTINGS);
    l.load(p);
    for (int i = 0; i < CatTrickDef::CATS; i++) {
        const int cat = area.items[c.cats[i]].object;
        float x, y, sx, sy, ax, ay;
        l.objectPosition(cat, x, y);
        l.objectPosition(c.spawns[i], sx, sy);
        w.objectPosition(cat, ax, ay); // (authored: the World before the load)
        const bool onSpawn = std::hypot(x - sx, y - sy) < 1e-4f;
        if (i == 1) check(!onSpawn && x == ax && y == ay, "Autumn stays where it was authored");
        else check(onSpawn && l.objects.parent(cat) == c.spawns[i], "a returned cat sits on its spawn");
    }
}
