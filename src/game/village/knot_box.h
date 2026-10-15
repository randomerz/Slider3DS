// KnotBox (Kevin's knot puzzle on STile 4, Slider 5): the knot's nodes are items; line i joins node
// i and node i + 1, the last back to the first. CheckLines marks a line bad when its raycast hits
// another node's ray collider, or it crosses a line that isn't next to it; the puzzle is solved
// with no bad lines (CheckPuzzle; CheckPuzzlePartial: at most 3).
// Not yet: the "Hat Click" sound with the sparkles.
#pragma once

#include <cstdint>

class World;

namespace village {

constexpr int MAX_KNOT_LINES = 64;

// KnotBox.CheckLines for knotBoxes[box]: fills bad[line] (if given) and returns how many are bad.
// While the box is inactive its lines sit at the origin (OnDisable: RemoveLines; before its first
// Update they're there too), where nothing is bad.
int knotBadLines(const World& world, int box, bool* bad = nullptr);
// Where line `line` runs now (KnotBox.MakeLines: node to next node), world units.
void knotLine(const World& world, int box, int line, float& x0, float& y0, float& x1, float& y1);
// KnotBox.CheckParticles: solved -> every node's sparkles play.
void knotCheckParticles(World& world, int box);
// KnotBox.SetToCorrectPositions: each node's parent (the item) moves to its solved spot.
void knotSetToCorrectPositions(World& world, int box);

} // namespace village
