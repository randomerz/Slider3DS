// The Artifact's pins (game/ui/ui_trackers + ArtifactUI's drawing): UITracker's blinking, showing
// with their target, VillageGrid's Slider 2 pin, and where they land on the Artifact.
#include <cmath>

#include "game/events/events.h"
#include "game/ui/artifact_ui.h"
#include "game/grid/ui_trackers.h"
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

namespace {

// An Artifact on the Village (tiles 1-8 collected or just tile 1), drawn into the test platform.
struct Board {
    World w{villageArea(), DEFAULT_SETTINGS};
    ArtifactUI ui;
    SpriteSheets sheets;
    explicit Board(bool allButNine = false) {
        check(sheets.load(villageArea()), "sheets load");
        if (allButNine)
            for (int id = 1; id <= 8; id++) w.setTileActive(id, true);
        w.artifact.init(w.grid);
        ui.init(villageArea(), w.artifact);
    }
    void touch(bool down, int x, int y, int frames = 1) {
        for (int i = 0; i < frames; i++) {
            plat::Input in;
            in.touching = down;
            float cx, cy;
            ui.slotCenter(villageArea(), x, y, cx, cy);
            in.touchX = (int)cx;
            in.touchY = (int)cy;
            ui.update(in, w, DT);
            w.artifact.update(w.grid, DT, 0);
        }
    }
    void draw() {
        testResetDrawCalls();
        ui.draw(w, sheets);
    }
};

bool isSprite(const DrawCall& d, int sprite) {
    const SpriteDef& S = villageArea().sprites[sprite];
    return sprite >= 0 && d.sheet == villageArea().sheetNames[S.sheet] && d.sx == S.x && d.sy == S.y && d.sw == S.w &&
           d.sh == S.h;
}

// The draw of `sprite` (index in the draw list; -1: none).
int drawOf(int sprite) {
    const std::vector<DrawCall>& calls = testDrawCalls();
    for (int i = 0; i < (int)calls.size(); i++)
        if (isSprite(calls[i], sprite)) return i;
    return -1;
}

// The pins drawn (the ui sheet's).
std::vector<DrawCall> pinDraws() {
    std::vector<DrawCall> out;
    for (const DrawCall& d : testDrawCalls())
        if (d.sheet == villageArea().sheetNames[villageArea().sprites[villageArea().uiTrackers.circle3].sheet]) out.push_back(d);
    return out;
}

} // namespace

TEST(ui_trackers_pins_land_on_their_tiles_button_or_clamped_off_the_grid) {
    const Area& area = villageArea();
    const UiTrackersDef& u = area.uiTrackers;
    Board b;
    b.w.trackers.remove(area.areaGrid.slider2Collectible); // (just the player's)
    const int scale = b.ui.scale;
    // Outdoors on tile 1, 1 unit right and 2 up of its center: its button + 27/17 UI px per unit,
    // on whole UI px from the button image's 2x grid.
    float tx, ty;
    b.w.grid.ownerBase(1, tx, ty);
    b.w.player.x = tx + 1;
    b.w.player.y = ty + 2;
    int bx, by;
    CHECK(b.w.artifact.findButton(1, bx, by));
    float cx, cy;
    b.ui.slotCenter(area, bx, by, cx, cy);
    b.draw();
    const int image = drawOf(area.artifact.buttons[0].island);
    CHECK(image >= 0 && pinDraws().size() == 1);
    if (image < 0 || pinDraws().size() != 1) return;
    DrawCall pin = pinDraws()[0];
    const DrawCall& img = testDrawCalls()[image];
    const float px = pin.x + pin.sw * scale * 0.5f, py = pin.y + pin.sh * scale * 0.5f;
    CHECK(std::abs(px - (cx + 27.0f / 17.0f * scale)) <= scale && std::abs(py - (cy - 2 * 27.0f / 17.0f * scale)) <= scale);
    CHECK((int)(pin.x - img.x) % scale == 0 && (int)(pin.y - img.y) % scale == 0); // (the button's pixel grid)
    CHECK(isSprite(pin, u.playerFullBlackCircle));
    // In its house (below y -75): the button's center.
    b.w.player.y = ty + area.housingOffset;
    b.draw();
    pin = pinDraws()[0];
    CHECK(std::abs(pin.x + pin.sw * scale * 0.5f - cx) <= scale && std::abs(pin.y + pin.sh * scale * 0.5f - cy) <= scale);
    // Off the grid, far up and right: clamped on the panel, whole on screen, the white off-map pin.
    b.w.player.x = 80;
    b.w.player.y = 90;
    b.draw();
    pin = pinDraws()[0];
    CHECK(isSprite(pin, u.playerFullWhiteCircle) && pin.y >= 0 && (int)(pin.x - b.ui.centerX) % scale == 0);
    CHECK(std::abs(pin.x + pin.sw * scale * 0.5f - (b.ui.centerX + 70 * scale)) <= scale);
}

TEST(ui_trackers_hidden_and_blinking_pins_draw_as_they_should) {
    const Area& area = villageArea();
    const UiTrackersDef& u = area.uiTrackers;
    const int slider2 = area.areaGrid.slider2Collectible;
    Board b;
    // Slider 2's pin: off the grid (white), blinking for its first 3 s — the empty one half the time.
    bool sawFull = false, sawEmpty = false;
    for (int f = 0; f < 120; f++) {
        b.w.trackers.update(b.w, DT);
        b.draw();
        sawFull = sawFull || drawOf(u.circle1W) >= 0;
        sawEmpty = sawEmpty || drawOf(u.circleEmptyW) >= 0;
        check(!(drawOf(u.circle1W) >= 0 && drawOf(u.circleEmptyW) >= 0), "one sprite at a time");
    }
    CHECK(sawFull && sawEmpty);
    // Its target hidden: no pin.
    b.w.objects.setActive(area, slider2, false);
    b.w.trackers.update(b.w, DT);
    b.draw();
    CHECK(drawOf(u.circle1W) < 0 && drawOf(u.circleEmptyW) < 0 && pinDraws().size() == 1);
    // The player's, on its tile, mid-blink: the empty circle.
    while (!b.w.trackers.trackers()[0].blinking) b.w.trackers.update(b.w, DT);
    b.draw();
    CHECK(drawOf(u.playerFullBlackCircleEmpty) >= 0 && drawOf(u.playerFullBlackCircle) < 0);
}

TEST(ui_trackers_pin_moves_with_its_button_under_its_frames) {
    const Area& area = villageArea();
    Board b(true);
    b.w.trackers.remove(area.areaGrid.slider2Collectible);
    const int pinSprite = area.uiTrackers.playerFullBlackCircle;
    // The player outdoors on tile 7: where its pin sits on the button image, at rest.
    float tx, ty;
    b.w.grid.ownerBase(7, tx, ty);
    b.w.player.x = tx - 2;
    b.w.player.y = ty + 1;
    const int island7 = area.artifact.buttons[6].island;
    b.draw();
    const int restImage = drawOf(island7), restPin = drawOf(pinSprite);
    CHECK(restImage >= 0 && restPin > restImage);
    if (restImage < 0 || restPin < 0) return;
    const DrawCall rest = testDrawCalls()[restImage]; // (copied: the next draw replaces the list)
    const float dx = testDrawCalls()[restPin].x - rest.x, dy = testDrawCalls()[restPin].y - rest.y;
    // Tapped: pressed, its image (and the pin with it) one UI px lower, the pin under the pressed frame.
    b.touch(true, 2, 1);
    b.touch(false, 2, 1);
    b.draw();
    int image = drawOf(island7), pin = drawOf(pinSprite), pressed = drawOf(area.artifact.pressed.sprite);
    CHECK(image >= 0 && pin > image && pressed > pin);
    CHECK(testDrawCalls()[image].y == rest.y + area.artifact.pressedDrop * b.ui.scale);
    CHECK(testDrawCalls()[pin].x - testDrawCalls()[image].x == dx && testDrawCalls()[pin].y - testDrawCalls()[image].y == dy);
    // Moved to the middle slot: once the tile has stopped (the player kept still where it was), the
    // button still nudging into its slot — the pin rides the image's offset for the new spot.
    b.touch(true, 1, 1);
    b.touch(false, 1, 1);
    for (int i = 0; i < 600 && !b.w.artifact.queueEmpty(); i++) b.touch(false, 0, 0);
    b.w.grid.ownerBase(7, tx, ty);
    b.w.player.x = tx - 2;
    b.w.player.y = ty + 1;
    b.draw();
    image = drawOf(island7);
    pin = drawOf(pinSprite);
    CHECK(image >= 0 && pin > image);
    if (image >= 0 && pin >= 0)
        CHECK(testDrawCalls()[pin].x - testDrawCalls()[image].x == dx && testDrawCalls()[pin].y - testDrawCalls()[image].y == dy);
}
