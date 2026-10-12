// Headless checks of the sliding grid, the Artifact move queue and slide physics against the
// real exported Village. Called from physics_test.cpp's main.
#include <cmath>
#include <string>

#include "game/area.h"
#include "game/artifact.h"
#include "game/grid.h"
#include "game/physics.h"

void check(bool ok, const std::string& what);

namespace {

// Runs the Artifact until every queued move has finished (or ~10 s pass).
void settle(Artifact& artifact, Grid& grid, int playerStile = 0) {
    for (int i = 0; i < 600 && !artifact.queueEmpty(); i++) artifact.update(grid, 1.0f / 60.0f, playerStile);
}

} // namespace

void gridTests(const Area& loaded) {
    // Pattern matching (CheckGrid).
    const std::string solved = "624_8#7_153";
    check(CheckGrid::contains(solved, "8#7") && !CheckGrid::contains(solved, "87"), "contains literal");
    check(CheckGrid::contains(solved, "^6.{2}_8") && !CheckGrid::contains(solved, "^2"), "contains ^ and .{n}");
    check(CheckGrid::contains("123_456_789", "3...6") && !CheckGrid::contains("123_465_789", "3...6"),
          "contains '3...6' (3 directly above 6)");
    check(CheckGrid::row(solved, "153") && CheckGrid::row(solved, "8#7") && !CheckGrid::row(solved, "687"), "row");
    check(CheckGrid::column(solved, "681") && CheckGrid::column(solved, "2#5") && !CheckGrid::column(solved, "671"),
          "column");
    check(CheckGrid::subgrid(solved, "628#") && CheckGrid::subgrid(solved, "#753") && !CheckGrid::subgrid(solved, "6215"),
          "subgrid");
    check(CheckGrid::contains("624_8#7_153", "6.._..._...") && !CheckGrid::contains("264_8#7_153", "^6.._..._..."),
          "Village condition pattern 6.._..._...");

    // Easing: Unity's weighted curve, a strong ease-in.
    bool monotonic = true;
    for (int i = 1; i <= 100; i++) monotonic = monotonic && slideCurve(i / 100.0f) >= slideCurve((i - 1) / 100.0f);
    check(slideCurve(0) == 0 && slideCurve(1) == 1 && monotonic, "slideCurve 0 -> 1, monotonic");
    check(slideCurve(0.5f) < 0.2f && slideCurve(0.9f) > 0.3f, "slideCurve eases in (0.5 -> " +
                                                                  std::to_string(slideCurve(0.5f)) + ")");

    Area area = loaded;
    Grid grid;
    grid.init(area);
    check(grid.gridString(true) == "624_897_153", "start layout " + grid.gridString(true));
    check(grid.gridString() == "###_###_1##", "start grid string " + grid.gridString());
    check(grid.checkTarget(), "start layout matches targetGrid");
    check(grid.setGrid("7#1_648_532") && grid.gridString(true) == "791_648_532" && !grid.checkTarget(),
          "setGrid (final-puzzle shuffle) round-trips");
    check(!grid.setGrid("12_34") && !grid.setGrid("624_8X7_153"), "setGrid rejects malformed strings");

    // Final check: all but 9 collected, solved layout -> "624_8#7_153".
    grid.init(area);
    for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);
    check(grid.gridString() == solved, "final check string " + grid.gridString());

    // Artifact moves from the real start: only tile 1 (bottom-left) is active.
    grid.init(area);
    Artifact artifact;
    artifact.init(grid);
    auto opts = artifact.moveOptions(area, 0, 0);
    check(opts.size() == 4, "tile 1 can slide to 4 slots (2 right, 2 up), got " + std::to_string(opts.size()));
    check(!artifact.tryMove(grid, 0, 0, 1, 1, 0), "diagonal move refused");
    check(!artifact.tryMove(grid, 1, 0, 2, 0, 0), "moving an empty slot refused");

    const STileDef* t1 = area.stile(1);
    check(artifact.tryMove(grid, 0, 0, 2, 0, 0), "tile 1 queued two slots right");
    check(t1->x == 2 && t1->y == 0 && t1->posX == 0.0f, "logical slot updates at once, drawn position doesn't");
    // Outline of the 3 slots: bottom 3 + sides 2 (outer neighbours are off-grid) + top 3 raised by both
    // the slot and its neighbour above, like Unity's SGridBackground pairs = 11, doubled for houses.
    check(area.borderWalls.size() == 22, "slide's 3 slots are fenced (11 edges + house copies), got " +
                                             std::to_string(area.borderWalls.size()));
    check(segmentBlockedBy(area, 25.0f, -10.0f, 0.0f, 3.0f, false) == "slide border",
          "walking into the sliding row from below is blocked by its fence");
    for (int i = 0; i < 30; i++) artifact.update(grid, 1.0f / 60.0f, 0);
    check(t1->posX > 0.0f && t1->posX < 1.0f, "half way through, tile 1 has eased only a little (" +
                                                  std::to_string(t1->posX) + " slots)");
    settle(artifact, grid);
    check(t1->posX == 2.0f && t1->posY == 0.0f && area.borderWalls.empty(), "tile 1 arrives and fences drop");
    check(grid.gridString() == "###_###_##1" && artifact.gridString(area) == grid.gridString(),
          "grid and artifact agree: " + grid.gridString());

    // Queue: at most 3 waiting; overlapping moves wait for the previous one, then run faster.
    check(artifact.tryMove(grid, 2, 0, 2, 1, 0) && artifact.tryMove(grid, 2, 1, 1, 1, 0) &&
              artifact.tryMove(grid, 1, 1, 1, 2, 0),
          "three chained moves queue");
    check(artifact.gridString(area) == "#1#_###_###" && grid.gridString() == "###_##1_###",
          "artifact shows the queued result; grid only the running move");
    float t = 0;
    while (!artifact.queueEmpty() && t < 10) { artifact.update(grid, 1.0f / 60.0f, 0); t += 1.0f / 60.0f; }
    check(t < 2.85f, "chained moves speed up (3 slides took " + std::to_string(t) + " s)");

    // Riding: the player standing on a sliding tile moves with it and can't walk off it.
    grid.init(area);
    artifact.init(grid);
    float px = 0, py = 3; // on tile 1, outdoors
    int under = grid.stileUnder(px, py);
    check(under == 1, "player stands on tile 1");
    artifact.tryMove(grid, 0, 0, 0, 1, under);
    bool blockedOff = false;
    for (int i = 0; i < 120 && !artifact.queueEmpty(); i++) {
        under = grid.stileUnder(px, py, under);
        float oy = area.stile(under)->posY;
        artifact.update(grid, 1.0f / 60.0f, under);
        py += (area.stile(under)->posY - oy) * 17;
        if (i == 30) blockedOff = segmentBlockedBy(area, px + 8.0f, py, 1.0f, 0, false) == "slide border";
    }
    check(std::fabs(py - 20.0f) < 1e-3f, "player rode tile 1 up one slot (y " + std::to_string(py) + ")");
    check(blockedOff, "player can't step off the right edge mid-slide");
    check(area.borderWalls.empty(), "riding fences drop after the move");
}
