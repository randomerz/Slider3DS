// KnotBox (Kevin's knot puzzle on STile 4, Slider 5): the knot's nodes are items; line i joins node
// i and node i + 1, the last back to the first. CheckLines marks a line bad when its raycast hits
// another node's ray collider, or it crosses a line that isn't next to it; the puzzle is solved
// with no bad lines (CheckPuzzle; CheckPuzzlePartial: at most 3).
// Not yet: the "Hat Click" sound with the sparkles.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

class World;

namespace village {

constexpr int MAX_KNOT_LINES = KnotBoxDef::MAX_NODES;

// The knot boxes as KnotBox.Update / LateUpdate leave them each frame (MakeLines, then CheckLines):
// the World refreshes them at the end of every update (and after Starts), and the CheckPuzzle specs,
// CheckParticles and the drawing read them, as Unity's read the lines' last positions.
class KnotBoxes {
public:
    void update(const World& world);
    int badLines(int box) const { return box >= 0 && box < (int)boxes_.size() ? boxes_[box].badCount : 0; }
    bool lineBad(int box, int line) const { return boxes_[box].bad[line] != 0; }
    // Line `line`'s ends (world units); at the origin while the box is inactive.
    void line(int box, int line, float& x0, float& y0, float& x1, float& y1) const;

private:
    struct Box {
        int badCount = 0;
        std::vector<uint8_t> bad;
        std::vector<float> ends; // x0, y0, x1, y1 per line
    };
    std::vector<Box> boxes_;
};

// KnotBox.CheckLines for knotBoxes[box] with the nodes where they are now: fills bad[line] (if
// given) and returns how many are bad. While the box is inactive its lines sit at the origin
// (OnDisable: RemoveLines; before its first Update they're there too), where nothing is bad.
int knotBadLines(const World& world, int box, bool* bad = nullptr);
// Where line `line` runs now (KnotBox.MakeLines: node to next node), world units.
void knotLine(const World& world, int box, int line, float& x0, float& y0, float& x1, float& y1);
// KnotBox.CheckParticles (World::knots as of the last update): solved -> every node's sparkles play.
void knotCheckParticles(World& world, int box);
// KnotBox.SetToCorrectPositions: each node's parent (the item) moves to its solved spot.
void knotSetToCorrectPositions(World& world, int box);

} // namespace village
