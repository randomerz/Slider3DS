// The Village intro (VillageGrid.OnHouseExit + StartVillageCutscene) and CameraDolly on the exported
// Village: the camera rolls along the dolly's track while the player is held, Romeo looks around and
// walks in, the fade out at the end hands the camera back; holding the action button skips it.
#include <cmath>
#include <string>

#include "game/village/village_grid.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

void frames(World& world, int n, float moveX = 0, float moveY = 0) {
    for (int i = 0; i < n; i++) world.update(DT, moveX, moveY, false);
}

int romeo(const Area& area) { return area.areaGrid.romeo; }

bool romeoFlipped(const World& world) { return world.objects.flipX(world.area.npcs[romeo(world.area)].sprite); }

bool romeoPing(const World& world) {
    return world.objects.activeInHierarchy(world.area.npcs[romeo(world.area)].pingObject);
}

bool cameraAt(const World& world, float t) {
    float x, y, ex, ey;
    world.camera(x, y);
    world.area.cameraDollies[world.area.areaGrid.introDolly].at(t, ex, ey);
    return std::fabs(x - ex) < 1e-3f && std::fabs(y - ey) < 1e-3f;
}

} // namespace

TEST(intro_export) {
    const Area& area = villageArea();
    CHECK(area.cameraDollies.size() == 1 && area.areaGrid.introDolly == 0);
    const CameraDollyDef& d = area.cameraDollies[0];
    CHECK(d.duration == 6 && d.rate == 60 && d.track.size() == 2 * 361 && d.holdToSkip == 1);
    CHECK(d.holdCurve.eval(0) == 0 && d.holdCurve.eval(1) == 1);
    // From above the grid's top-right (Romeo's corner) down to the starting house on tile 1.
    float x, y;
    d.at(0, x, y);
    CHECK(x == 35 && y == 46);
    d.at(6, x, y);
    CHECK(std::fabs(x - 1.5f) < 1e-3f && std::fabs(y - 0.5f) < 1e-3f);
    CHECK(romeo(area) >= 0 && area.npcs[romeo(area)].name == "Romeo");
    CHECK(area.npcs[romeo(area)].conds[0].walks.size() == 1); // (the walk the intro starts)
}

TEST(intro_plays_once_on_the_first_house_exit) {
    World world(villageArea(), DEFAULT_SETTINGS);
    world.placeAt(world.area.spawns[0]);
    const float px = world.player.x, py = world.player.y;
    village::houseExit(world);
    CHECK(world.state.flag(village::INTRO_CUTSCENE_FLAG) && world.dollies.running() && world.playerLocked());
    CHECK(world.effects.black() == 1 && cameraAt(world, 0)); // fading in on the track's start
    CHECK(romeoFlipped(world) && !romeoPing(world));
    frames(world, 1, 1, 0); // (the frame it started in: the camera stays)
    CHECK(cameraAt(world, 0));
    frames(world, 30, 1, 0);
    // Rolling, the player held where it was; Romeo looks the other way at 0.5 s.
    CHECK(cameraAt(world, 30 * DT) && world.player.x == px && world.player.y == py && !romeoFlipped(world));
    frames(world, 30);
    CHECK(romeoFlipped(world) && !world.walks.walking(romeo(world.area)));
    frames(world, 63); // 2 s (each chained wait counts from the frame after): he walks
    CHECK(world.walks.walking(romeo(world.area)));
    CHECK(cameraAt(world, (1 + 30 + 30 + 63 - 1) * DT)); // (mid-track, where it's fast: rolling from the frame after the start)
    // 5.75 s: the fade to black; once black (+1 s) the camera's back on the player, fading in.
    int f = 1 + 30 + 30 + 63;
    for (; f < 340; f++) world.update(DT, 0, 0, false);
    CHECK(world.effects.black() == 0 && world.dollies.running());
    for (; f < 380; f++) world.update(DT, 0, 0, false);
    CHECK(world.effects.black() > 0.4f && world.dollies.running());
    for (; f < 600 && world.dollies.running(); f++) world.update(DT, 0, 0, false);
    check(f >= 400 && f <= 412, "ends after 6.75 s: " + std::to_string(f) + " frames");
    CHECK(!world.playerLocked() && world.effects.black() > 0.9f);
    float cx, cy;
    world.camera(cx, cy);
    CHECK(cx == world.player.x && cy == world.player.y);
    CHECK(romeoPing(world)); // (back at 5 s)
    frames(world, 1, 1, 0);
    CHECK(world.player.x > px); // the player moves again
    // Only the first time.
    village::houseExit(world);
    CHECK(!world.dollies.running());
}

TEST(intro_hold_action_to_skip) {
    World world(villageArea(), DEFAULT_SETTINGS);
    world.placeNear(0, 0);
    world.items.drop(world, world.area.areaGrid.ruinsFragment, world.player.x + 1, world.player.y); // (in reach)
    frames(world, 40);
    village::houseExit(world);
    frames(world, 10);
    CHECK(!world.dollies.promptShown());
    bool held = false; // the action button
    auto run = [&](int n) {
        for (int i = 0; i < n; i++) world.update(DT, FrameInput{0, 0, held});
    };
    // A press shows the prompt and starts the hold; letting go early empties the bar (the prompt stays).
    held = true;
    world.action();
    CHECK(world.items.carried() == -1); // (the player can't act: no pick-up)
    run(30);
    CHECK(world.dollies.promptShown() && world.dollies.promptFill() > 0.3f && world.dollies.promptFill() < 0.7f);
    held = false;
    run(1);
    CHECK(world.dollies.promptShown() && world.dollies.promptFill() == 0);
    run(30); // (the opening fade from black is over)
    CHECK(world.effects.black() == 0);
    // A full second held: fade to black, the camera stops; then it ends.
    held = true;
    world.action();
    run(62);
    CHECK(world.effects.black() > 0 && world.dollies.running());
    float x0, y0, x1, y1;
    world.camera(x0, y0);
    run(20);
    world.camera(x1, y1);
    CHECK(x0 == x1 && y0 == y1);
    run(45);
    CHECK(!world.dollies.running() && !world.dollies.promptShown() && !world.playerLocked());
}

TEST(intro_fade_to_black_runs_its_callback_unless_replaced) {
    // UIEffects: a fade to black ends with its callback; a fade started meanwhile cancels it.
    const Area& area = villageArea();
    ScreenEffects fx;
    fx.reset();
    uint32_t frame = 0; // (World::frame: a fade counts from the frame after it starts)
    fx.fadeToBlack(EffectDone::DollyEnd, frame);
    CHECK(fx.black() == 0);
    CHECK(fx.update(area, DT, 0, frame++).fadeEnd == EffectDone::None && fx.black() == 0); // (its start frame)
    EffectDone done = EffectDone::None;
    int n = 0;
    for (; n < 600 && done == EffectDone::None; n++) done = fx.update(area, DT, 0, frame++).fadeEnd;
    CHECK(done == EffectDone::DollyEnd && std::abs(n - (int)std::lround(area.fadeDuration / DT)) <= 1);
    CHECK(fx.black() == 0); // (the panel turns off at the end)
    fx.fadeToBlack(EffectDone::DollyEnd, frame);
    fx.update(area, DT, 0, frame++);
    fx.fadeFromBlack(frame);
    for (n = 0; n < 600; n++) check(fx.update(area, DT, 0, frame++).fadeEnd == EffectDone::None, "cancelled: no callback");
}
