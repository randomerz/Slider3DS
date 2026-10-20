// The Artifact's pins (game/ui/ui_trackers + ArtifactUI's drawing): UITracker's blinking, showing
// with their target, VillageGrid's Slider 2 pin, and where they land on the Artifact.
#include <cmath>

#include "game/events/events.h"
#include "game/ui/artifact_ui.h"
#include "game/ui/ui_trackers.h"
#include "game/world/world.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

const UiTracker* find(const World& w, int object) {
    for (const UiTracker& t : w.trackers.trackers())
        if (t.object == object) return &t;
    return nullptr;
}

} // namespace

TEST(ui_trackers_player_and_slider2_pins_at_the_start) {
    World w(villageArea(), DEFAULT_SETTINGS);
    const int slider2 = villageArea().areaGrid.slider2Collectible;
    CHECK(slider2 >= 0 && w.trackers.trackers().size() == 2);
    CHECK(find(w, UiTracker::PLAYER) != nullptr && find(w, slider2) != nullptr);
    // Collected: VillageGrid.Start adds no Slider 2 pin.
    w.state.setCollectible("Slider 2", "Village", true);
    w.reloadObjects();
    CHECK(w.trackers.trackers().size() == 1 && find(w, slider2) == nullptr);
}

TEST(ui_trackers_player_pin_blinks_3_s_every_10_s) {
    World w(villageArea(), DEFAULT_SETTINGS);
    int blinkFirst = 0, blinkBetween = 0, blinkRepeat = 0;
    for (int f = 0; f < 13 * 60; f++) {
        w.trackers.update(w, DT);
        const bool b = find(w, UiTracker::PLAYER)->blinking;
        const float t = f * DT;
        if (t < 3) blinkFirst += b;
        else if (t > 3.1f && t < 9.9f) blinkBetween += b;
        else if (t > 10.1f) blinkRepeat += b;
    }
    // Half of each second while blinking (the blink sprite while 0 <= toSwap <= 0.5), none between.
    CHECK(std::abs(blinkFirst - 90) <= 3 && blinkBetween == 0 && std::abs(blinkRepeat - 90) <= 3);
}

TEST(ui_trackers_pin_follows_its_target_and_restarts_its_blink) {
    World w(villageArea(), DEFAULT_SETTINGS);
    const int slider2 = villageArea().areaGrid.slider2Collectible;
    for (int f = 0; f < 4 * 60; f++) w.trackers.update(w, DT); // (its one blink is over)
    CHECK(find(w, slider2)->shown && find(w, slider2)->blinkLeft < 0);
    w.objects.setActive(villageArea(), slider2, false);
    w.trackers.update(w, DT);
    CHECK(!find(w, slider2)->shown);
    w.objects.setActive(villageArea(), slider2, true); // (OnEnable: blinks again)
    w.trackers.update(w, DT);
    CHECK(find(w, slider2)->shown && find(w, slider2)->blinkLeft > 2.9f);
}

TEST(ui_trackers_remove_slider2_tracker_call) {
    World w(villageArea(), DEFAULT_SETTINGS);
    CallDef remove;
    remove.method = CallDef::Method::VillageRemoveSlider2Tracker;
    runCall(w, remove);
    CHECK(w.trackers.trackers().size() == 1 && find(w, UiTracker::PLAYER) != nullptr);
}

TEST(ui_trackers_pins_land_on_their_tiles_button_or_clamped_off_the_grid) {
    const Area& area = villageArea();
    SpriteSheets sheets;
    check(sheets.load(area), "sheets load");
    World w(area, DEFAULT_SETTINGS);
    w.artifact.init(w.grid);
    ArtifactUI ui;
    ui.init(area, w.artifact);
    w.trackers.remove(area.areaGrid.slider2Collectible); // (just the player's)
    const auto pinCenter = [&](float& x, float& y, int& sprite) {
        testResetDrawCalls();
        ui.draw(w, sheets);
        const DrawCall& d = testDrawCalls().back(); // (pins come last)
        x = d.x + d.sw * ui.scale * 0.5f;
        y = d.y + d.sh * ui.scale * 0.5f;
        sprite = d.sw;
    };
    // Outdoors on tile 1, 1 unit right and 2 up of its center: its button + 27/17 UI px per unit.
    float tx, ty;
    w.grid.ownerBase(1, tx, ty);
    w.player.x = tx + 1;
    w.player.y = ty + 2;
    int bx, by;
    CHECK(w.artifact.findButton(1, bx, by));
    float cx, cy, x, y;
    int size;
    ui.slotCenter(area, bx, by, cx, cy);
    pinCenter(x, y, size);
    CHECK(std::abs(x - (cx + 27.0f / 17.0f * ui.scale)) <= 1 && std::abs(y - (cy - 2 * 27.0f / 17.0f * ui.scale)) <= 1);
    CHECK(size == area.sprites[area.uiTrackers.playerFullBlackCircle].w);
    // In its house (below y -75): the button's center.
    w.player.y = ty + area.housingOffset;
    pinCenter(x, y, size);
    CHECK(std::abs(x - cx) <= 1 && std::abs(y - cy) <= 1);
    // Off the grid, far up and right: clamped on the panel, whole on screen, the white off-map pin.
    w.player.x = 80;
    w.player.y = 90;
    pinCenter(x, y, size);
    CHECK(size == area.sprites[area.uiTrackers.playerFullWhiteCircle].w);
    CHECK(std::abs(x - (ui.centerX + 70 * ui.scale)) <= 1 && y - size * ui.scale * 0.5f >= 0);
}
