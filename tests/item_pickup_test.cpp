// ItemPickupEffect (game/fx/item_pickup): a collectible's pick-up cutscene on the real Village —
// its timeline (circle, item, text, DoOnCollect, the hold) and where AreaView sorts it.
#include <cmath>
#include <string>

#include "game/render/area_view.h"
#include "game/render/sprite_sheets.h"
#include "game/world/world.h"
#include "platform/platform.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

// A World with the player standing on Slider 2 (touched on the next update).
World onSlider2(int& index) {
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.state.setFlag("villageIntroCutscene", true);
    index = area.collectible("Slider 2");
    if (index < 0) return world;
    float x, y;
    world.objectPosition(area.collectibles[index].object, x, y);
    world.placeNear(x, y);
    return world;
}

} // namespace

TEST(item_pickup_runs_its_timeline) {
    int index = -1;
    World world = onSlider2(index);
    CHECK(index >= 0);
    if (index < 0) return;
    const CollectibleDef& c = world.area.collectibles[index];
    world.update(DT, 0, 0, false); // touched: the cutscene starts
    CHECK(world.pickup.active() && world.playerLocked() && !world.objects.activeInHierarchy(c.object));
    CHECK(!world.npcs.dialogueEnabled()); // (dialogueEnabledAllNPC)
    CHECK(world.pickup.circleDiameter() == 0 && !world.pickup.textShown());
    CHECK(!world.state.hasCollectible("Slider 2", "Village") && !world.saveRequested);
    CHECK(world.pickup.itemSprite(world.area) == c.sprite);
    // The circle grows over 0.5 s, eased (half way in time: half the size), then stays at 40 units.
    for (int f = 0; f < 15; f++) world.update(DT, 0, 0, false);
    CHECK(std::fabs(world.pickup.circleDiameter() - ItemPickup::CIRCLE_SIZE / 2) < 0.5f);
    for (int f = 0; f < 15; f++) world.update(DT, 0, 0, false);
    CHECK(world.pickup.circleDiameter() == ItemPickup::CIRCLE_SIZE);
    // The player is held: walking doesn't move it.
    const float x = world.player.x, y = world.player.y;
    for (int f = 0; f < 14; f++) world.update(DT, 1, 0, false); // (frame 44)
    CHECK(world.player.x == x && world.player.y == y);
    CHECK(!world.pickup.textShown() && !world.state.hasCollectible("Slider 2", "Village"));
    // 0.75 s: the text, and DoOnCollect (inventory, onCollect: tile 2 joins, a save).
    world.update(DT, 0, 0, false);
    world.update(DT, 0, 0, false);
    CHECK(world.pickup.textShown() && world.pickup.text() == "Slider 2 Acquired!");
    CHECK(world.state.hasCollectible("Slider 2", "Village") && world.grid.tileActive(2) && world.saveRequested);
    // 2 s: the text goes and the circle shrinks over 0.5 s; 2.5 s: over, the player let go.
    int f = 46;
    while (world.pickup.textShown() && f < 200) world.update(DT, 0, 0, false), f++;
    check(f >= 119 && f <= 122, "text off at frame " + std::to_string(f));
    CHECK(world.pickup.circleDiameter() > ItemPickup::CIRCLE_SIZE * 0.95f && world.playerLocked());
    while (world.pickup.active() && f < 300) world.update(DT, 0, 0, false), f++;
    check(f >= 149 && f <= 152, "over at frame " + std::to_string(f));
    CHECK(!world.playerLocked() && world.pickup.circleDiameter() == 0 && world.pickup.itemSprite(world.area) < 0);
    CHECK(world.npcs.dialogueEnabled());
    world.update(DT, 1, 0, false);
    CHECK(world.player.x > x); // (walks again)
}

TEST(item_pickup_a_scene_load_drops_it) {
    int index = -1;
    World world = onSlider2(index);
    if (index < 0) return;
    world.update(DT, 0, 0, false);
    CHECK(world.pickup.active());
    world.reloadObjects();
    CHECK(!world.pickup.active() && !world.playerLocked() && world.npcs.dialogueEnabled());
}

TEST(item_pickup_draws_under_the_player) {
    // AreaView: the circle (centered on the player) and, once the circle covers it, the item 1 unit
    // above, both before the player's body (ScreenEffects, order -1 vs the body's 0).
    int index = -1;
    World world = onSlider2(index);
    if (index < 0) return;
    const Area& area = world.area;
    SpriteSheets sheets;
    CHECK(sheets.load(area));
    AreaView view;
    world.update(DT, 0, 0, false);
    for (int f = 0; f < 30; f++) world.update(DT, 0, 0, false);
    float camX, camY;
    world.camera(camX, camY);
    testResetDrawCalls();
    view.begin(area, sheets, camX, camY, plat::TOP_W, plat::TOP_H, 0, false);
    view.draw(world);
    const std::vector<DrawCall>& calls = testDrawCalls();
    int circle = -1, item = -1, body = -1;
    const SpriteDef& itemDef = area.sprites[area.collectibles[index].sprite];
    for (int i = 0; i < (int)calls.size(); i++) {
        if (calls[i].sheet == "circle") circle = i;
        else if (circle >= 0 && item < 0 && calls[i].sx == itemDef.x && calls[i].sy == itemDef.y) item = i;
        else if (item >= 0 && body < 0 && !calls[i].sheet.empty() && calls[i].sheet != "line") body = i;
    }
    CHECK(circle >= 0 && item > circle && body > item);
    if (circle < 0 || item < 0) return;
    CHECK(std::fabs(calls[circle].x - plat::TOP_W / 2.0f) < 2 && calls[circle].sw == (int)(ItemPickup::CIRCLE_SIZE / 2 * 16 * 100));
    // The item: its 16 px rect centered 16 px above the player (screen y down).
    CHECK(std::fabs(calls[item].x + itemDef.w / 2.0f - calls[circle].x) < 1);
    CHECK(std::fabs(calls[item].y + itemDef.h / 2.0f - (calls[circle].y - 16)) < 1);
}
