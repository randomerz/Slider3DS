// Grid model, grid strings, CheckGrid patterns and the slide curve.
#include <string>

#include "game/grid/grid.h"
#include "test.h"

namespace {
const std::string SOLVED = "624_8#7_153"; // VillageGrid.CheckFinalPlacements
}

TEST(checkgrid_patterns) {
    CHECK(CheckGrid::contains(SOLVED, "8#7") && !CheckGrid::contains(SOLVED, "87"));
    CHECK(CheckGrid::contains(SOLVED, "^6.{2}_8") && !CheckGrid::contains(SOLVED, "^2"));
    check(CheckGrid::contains("123_456_789", "3...6") && !CheckGrid::contains("123_465_789", "3...6"),
          "contains '3...6' (3 directly above 6)");
    CHECK(CheckGrid::row(SOLVED, "153") && CheckGrid::row(SOLVED, "8#7") && !CheckGrid::row(SOLVED, "687"));
    CHECK(CheckGrid::column(SOLVED, "681") && CheckGrid::column(SOLVED, "2#5") && !CheckGrid::column(SOLVED, "671"));
    CHECK(CheckGrid::subgrid(SOLVED, "628#") && CheckGrid::subgrid(SOLVED, "#753") && !CheckGrid::subgrid(SOLVED, "6215"));
    check(CheckGrid::contains(SOLVED, "6.._..._...") && !CheckGrid::contains("264_8#7_153", "^6.._..._..."),
          "Village condition pattern 6.._..._...");
    CHECK(gridMatches("a1b", "a[0-9]b") && !gridMatches("axb", "a[0-9]b") && gridMatches("axb", "a[^0-9]b"));
    CHECK(gridMatches("aaab", "^a+b$") && gridMatches("b", "^a*b$") && !gridMatches("aab", "^a?b$"));
}

TEST(grid_string_conversions) {
    CHECK(idToChar(1) == '1' && idToChar(9) == '9' && idToChar(10) == 'A');
    CHECK(charToId('1') == 1 && charToId('A') == 10);
}

TEST(slide_curve) {
    bool monotonic = true;
    for (int i = 1; i <= 100; i++) monotonic = monotonic && slideCurve(i / 100.0f) >= slideCurve((i - 1) / 100.0f);
    check(slideCurve(0) == 0 && slideCurve(1) == 1 && monotonic, "slideCurve 0 -> 1, monotonic");
    check(slideCurve(0.5f) < 0.2f && slideCurve(0.9f) > 0.3f,
          "slideCurve eases in (0.5 -> " + std::to_string(slideCurve(0.5f)) + ")");
}

TEST(grid_start_state) {
    Grid grid;
    grid.init(villageArea());
    check(grid.gridString(true) == "624_897_153", "start layout " + grid.gridString(true));
    check(grid.gridString() == "###_###_1##", "start grid string " + grid.gridString());
    CHECK(grid.checkTarget());
    for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);
    check(grid.gridString() == SOLVED, "all but 9 collected -> final check string " + grid.gridString());
}

TEST(grid_set_grid) {
    Grid grid;
    grid.init(villageArea());
    // VillageGrid.DoShuffle: new int[3,3] {{7,0,1},{6,4,8},{5,3,2}} indexed puzzle[x,y], 0 = tile 9.
    CHECK(grid.setGrid("182_043_765"));
    check(grid.gridString(true) == "182_943_765" && !grid.checkTarget(), "shuffle layout " + grid.gridString(true));
    const TileState* t7 = grid.tile(7);
    CHECK(t7 && t7->x == 0 && t7->y == 0 && t7->posX == 0.0f);
    for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);
    check(grid.gridString() == "182_#43_765", "shuffled grid string " + grid.gridString());
    const std::string before = grid.gridString(true);
    check(!grid.setGrid("12_34") && !grid.setGrid("624_8X7_153") && !grid.setGrid("111_111_111") &&
              !grid.setGrid("624_8977_153"),
          "setGrid rejects malformed strings");
    check(grid.gridString(true) == before, "a rejected setGrid changes nothing");
}

TEST(grid_move_without_active_tile_ends) {
    // A move of two empty slots animates nothing; it must still end and drop its fences.
    Grid grid;
    grid.init(villageArea());
    grid.startMove(SMove::swap(1, 1, 2, 1, 9, 7), 0, 42);
    CHECK(grid.moving() && !grid.walls().empty());
    std::vector<int> done = grid.update(1.0f / 60.0f);
    CHECK(done.size() == 1 && done[0] == 42);
    CHECK(!grid.moving() && grid.walls().empty());
    check(grid.gridString(true) == "624_879_153", "empty slots swapped: " + grid.gridString(true));
}

TEST(grid_stile_under) {
    Grid grid;
    grid.init(villageArea());
    CHECK(grid.stileUnder(0, 3) == 1);
    CHECK(grid.stileUnder(0, 3.0f + grid.housingOffset()) == 1); // tile 1's house
    CHECK(grid.stileUnder(17, 17) == 0);                      // tile 9: not collected
    CHECK(grid.stileUnder(-20, 0) == 0);                      // off the grid
}
