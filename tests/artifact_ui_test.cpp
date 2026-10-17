// The Artifact screen's stylus handling (ArtifactUI), driven with synthetic touch input.
#include <cmath>
#include <string>

#include "game/ui/artifact_ui.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

// A world with no player on a tile (stile 0), so no move keeps walls for it.
struct Fixture {
    Settings settings;
    World world{villageArea(), settings};
    Grid& grid = world.grid;
    Artifact& artifact = world.artifact;
    ArtifactUI ui;
    explicit Fixture(bool allButNine = false) {
        if (allButNine)
            for (int id = 1; id <= 8; id++) world.setTileActive(id, true);
        artifact.init(grid);
        ui.init(villageArea(), artifact);
    }
    void frame(bool touching, float x = 0, float y = 0) {
        plat::Input in;
        in.touching = touching;
        in.touchX = (int)x;
        in.touchY = (int)y;
        ui.update(in, world, DT);
        artifact.update(grid, DT, 0);
    }
    void tap(int sx, int sy) {
        float x, y;
        ui.slotCenter(villageArea(), sx, sy, x, y);
        frame(true, x, y);
        frame(true, x, y);
        frame(false); // the 3DS reports no position on release
    }
    // Drags from slot a to slot b in small steps, releasing over b (or off the panel if b < 0).
    void drag(int ax, int ay, int bx, int by) {
        float x0, y0, x1 = 5, y1 = 5;
        ui.slotCenter(villageArea(), ax, ay, x0, y0);
        if (bx >= 0) ui.slotCenter(villageArea(), bx, by, x1, y1);
        for (int i = 0; i <= 10; i++) frame(true, x0 + (x1 - x0) * i / 10, y0 + (y1 - y0) * i / 10);
        frame(false);
    }
    void settle() {
        for (int i = 0; i < 600 && !artifact.queueEmpty(); i++) frame(false);
    }
};

} // namespace

TEST(artifact_ui_tap_select_and_move) {
    Fixture f;
    f.tap(0, 0); // tile 1, bottom-left
    CHECK(f.ui.selected() == 1);
    CHECK(f.ui.isOption(1, 0) && f.ui.isOption(2, 0) && f.ui.isOption(0, 1) && f.ui.isOption(0, 2));
    CHECK(!f.ui.isOption(1, 1));
    f.tap(2, 0); // an option: slide there; the tile stays selected
    check(f.artifact.gridString(f.grid) == "###_###_##1", "queued to (2,0): " + f.artifact.gridString(f.grid));
    CHECK(f.ui.selected() == 1 && f.ui.isOption(0, 0) && f.ui.isOption(2, 1));
    f.tap(1, 1); // not an option: deselect
    CHECK(f.ui.selected() == 0 && !f.ui.isOption(0, 0));
    f.tap(1, 1); // an empty slot with nothing selected: nothing happens
    CHECK(f.ui.selected() == 0);
    f.settle();
    CHECK(f.grid.gridString() == "###_###_##1");
}

TEST(artifact_ui_tap_same_tile_toggles) {
    Fixture f;
    f.tap(0, 0);
    CHECK(f.ui.selected() == 1);
    f.tap(0, 0);
    CHECK(f.ui.selected() == 0);
}

TEST(artifact_ui_drag) {
    Fixture f;
    f.drag(0, 0, 0, 2); // drag tile 1 up two slots
    check(f.artifact.gridString(f.grid) == "1##_###_###", "dragged to (0,2): " + f.artifact.gridString(f.grid));
    CHECK(f.ui.selected() == 0); // a drag deselects when done
    f.settle();
    f.drag(0, 2, -1, -1); // released off the buttons: cancelled
    CHECK(f.artifact.queueEmpty() && f.grid.gridString() == "1##_###_###" && f.ui.selected() == 0);
    f.drag(0, 2, 1, 1); // released on a non-option (diagonal): cancelled
    CHECK(f.artifact.queueEmpty() && f.grid.gridString() == "1##_###_###" && f.ui.selected() == 0);
}

TEST(artifact_ui_release_stylus_drops_a_drag) {
    // The pause menu opening mid-drag: the release that follows doesn't drop the tile.
    Fixture f;
    float x0, y0, x1, y1;
    f.ui.slotCenter(villageArea(), 0, 0, x0, y0);
    f.ui.slotCenter(villageArea(), 0, 2, x1, y1);
    for (int i = 0; i <= 10; i++) f.frame(true, x0 + (x1 - x0) * i / 10, y0 + (y1 - y0) * i / 10);
    CHECK(f.ui.selected() == 1);
    f.ui.releaseStylus();
    f.frame(false);
    CHECK(f.artifact.queueEmpty() && f.ui.selected() == 1);
    // Resuming with the stylus still down (closed with B): lifting it isn't a tap.
    f.tap(0, 0); // (deselects tile 1)
    CHECK(f.ui.selected() == 0);
    f.ui.releaseStylus();
    f.frame(true, x0, y0);
    f.frame(true, x0, y0);
    f.frame(false);
    CHECK(f.ui.selected() == 0 && f.artifact.queueEmpty());
    f.tap(0, 0); // the next real tap works
    CHECK(f.ui.selected() == 1);
}

TEST(artifact_ui_drag_back_onto_itself_selects) {
    Fixture f;
    float x, y;
    f.ui.slotCenter(villageArea(), 0, 0, x, y);
    for (int i = 0; i <= 10; i++) f.frame(true, x + (i <= 5 ? i : 10 - i) * 4, y); // out and back
    f.frame(false);
    CHECK(f.ui.selected() == 1 && f.ui.isOption(1, 0) && f.artifact.queueEmpty());
    f.tap(1, 0); // and the selection works like a tap's
    check(f.artifact.gridString(f.grid) == "###_###_#1#", "moved: " + f.artifact.gridString(f.grid));
}

TEST(artifact_ui_respects_rules) {
    Fixture f;
    f.tap(1, 0); // empty slot: can't select
    CHECK(f.ui.selected() == 0);
    f.artifact.canAddMoves = false; // UIArtifact.DisableMovement
    f.tap(0, 0);
    CHECK(f.ui.selected() == 0 && f.artifact.queueEmpty());
    f.artifact.canAddMoves = true;
    f.artifact.canQueue = false; // queueing off: selects, but no move
    f.tap(0, 0);
    f.tap(1, 0);
    CHECK(f.ui.selected() == 1 && f.artifact.queueEmpty());
}

TEST(artifact_ui_set_grid_drops_the_selection) {
    // SGrid.SetGrid deselects the selected button: a tap on one of its old options then does nothing.
    Fixture f;
    f.tap(0, 0);
    CHECK(f.ui.selected() == 1 && f.ui.isOption(1, 0));
    CHECK(f.world.setGrid("624_897_153")); // (the same layout: still a SetGrid)
    f.frame(false);
    CHECK(f.ui.selected() == 0 && !f.ui.isOption(1, 0));
    f.tap(1, 0);
    CHECK(f.ui.selected() == 0 && f.artifact.queueEmpty());
}

TEST(artifact_ui_auto_move_setting) {
    // Settings.AutoMove does the same at any time: tile 5's single option, on a tap.
    Fixture f(true);
    f.settings.autoMove = true;
    f.tap(1, 0);
    CHECK(f.ui.selected() == 0);
    check(f.artifact.gridString(f.grid) == "624_857_1#3", "auto-moved: " + f.artifact.gridString(f.grid));
    f.settle();
    f.drag(1, 1, 1, 0); // a drag still picks its own target (here: back down)
    f.settle();
    check(f.artifact.gridString(f.grid) == "624_8#7_153", "dragged back: " + f.artifact.gridString(f.grid));
    f.settings.autoMove = false;
    f.tap(1, 0); // off: a tap only selects
    CHECK(f.ui.selected() == 5 && f.artifact.queueEmpty());
}

TEST(artifact_ui_auto_move) {
    // After the final shuffle a tile with a single option moves on tap (forceAutoMoveVillage).
    Fixture f(true); // all but 9 collected: the only empty slot is the middle
    f.world.state.setFlag("forceAutoMoveVillage", true);
    f.tap(1, 0); // tile 5 (bottom middle) has one option: up into the middle
    CHECK(f.ui.selected() == 0);
    check(f.artifact.gridString(f.grid) == "624_857_1#3", "auto-moved: " + f.artifact.gridString(f.grid));
    f.world.state.setFlag("forceAutoMoveVillage", false);
    f.settle();
    f.tap(0, 0); // tile 1 now has one option (right), but without autoMove a tap only selects
    CHECK(f.ui.selected() == 1 && f.artifact.queueEmpty());
}
