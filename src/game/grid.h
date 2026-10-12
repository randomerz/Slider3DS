// Sliding-tile grid: the runtime state of the Area's STiles, ported from SGrid / STile /
// SMove / SGridAnimator. It writes tile positions into Area::stiles (logical slot x/y,
// animated posX/posY, collider switches) and the temporary border walls into
// Area::borderWalls, so rendering and physics just follow the Area.
#pragma once

#include <string>
#include <vector>

#include "game/area.h"

// One tile (or empty slot) moving from start to end, in grid slots (SMove.cs Movement).
struct Movement {
    int sx, sy, ex, ey;
    int islandId;
};

// A move made on the Artifact (SMove.cs). Village only needs swaps (SMoveSwap): the tile and the
// (inactive) tile in the empty slot trade places, possibly across a line of empty slots.
struct SMove {
    float duration = 1.0f; // normalized; scaled by Grid::durationMultiplier
    std::vector<Movement> moves;

    static SMove swap(int x1, int y1, int x2, int y2, int islandId1, int islandId2);

    // Slots the move covers: every slot between each movement's start and end.
    std::vector<std::pair<int, int>> positions() const;
    bool overlaps(const SMove& other) const;
    // Outline of the covered slots: (slot x, slot y, side) with sides 0..3 = right, up, left, down.
    // Shared edges between covered slots cancel out (SMove.AddBorder toggles).
    struct Border { int x, y, side; };
    std::vector<Border> borders() const;
};

// Unity's movementCurve on the Village SGridAnimator (weighted Bezier ease-in), t in [0,1].
float slideCurve(float t);

// Unanchored match of `pattern` against a grid string, like C# Regex.IsMatch for the subset the
// game uses: literals, '.', '^', '$', [classes], and the quantifiers * + ? {n} {n,m}.
bool gridMatches(const std::string& gridString, const std::string& pattern);

// CheckGrid helpers (Utility/CheckGridState.cs), for 3x3 grids.
namespace CheckGrid {
bool contains(const std::string& gridString, const std::string& regex);
bool row(const std::string& gridString, const std::string& row);
bool column(const std::string& gridString, const std::string& column);
bool subgrid(const std::string& gridString, const std::string& subgrid); // TL TR BL BR
}

class Grid {
public:
    // New-game state: tiles at their exported slots and active flags, nothing moving.
    void init(Area& area);

    int width() const { return w_; }
    int height() const { return h_; }
    // islandId of the tile in slot (x, y); every slot holds a tile (inactive = empty).
    int idAt(int x, int y) const { return slots_[y * w_ + x]; }
    STileDef* stile(int islandId);

    // "123_6##_4#5": top row first, rows joined by '_', inactive tiles as '#' unless numsOnly.
    std::string gridString(bool numsOnly = false) const;
    // Rearranges every tile at once (SGrid.SetGrid): `ids` in grid-string order (top row first),
    // '_' ignored, '0' or '#' = the highest-numbered tile. Cancels motion. False if malformed.
    bool setGrid(const std::string& ids);
    bool checkTarget() const; // gridString(true) matches area.targetGrid ('*' = any)

    void setTileActive(int islandId, bool active); // collecting a Slider enables its tile

    // Starts animating a move (SGrid.Move + SGridAnimator.Move). Logical slots update at once;
    // `playerStile` (0 = none) is the tile the player stands on, which keeps border walls
    // around it while it moves. `id` tags the move for movesFinished().
    void startMove(const SMove& move, int playerStile, int id);
    // Advances animations by dt seconds. Returns the ids of moves that finished this step.
    std::vector<int> update(float dt);
    bool moving() const { return !anims_.empty(); }

    // The tile under a world point (SGrid.GetSTileUnderneath): active tiles whose slot square
    // (or house square below it) contains the point. Prefers `current`, else the lowest id. 0 = none.
    int stileUnder(float x, float y, int current = 0) const;

    float durationMultiplier = 1.0f; // 0.5 once the final puzzle starts

private:
    struct Anim {
        int moveId, islandId;
        float sx, sy, ex, ey, t, duration;
        bool playerOn;
    };
    struct ActiveMove {
        int id;
        std::vector<SMove::Border> borders;
        std::vector<int> collidersOff; // tiles whose colliders are off until the move ends
    };

    void rebuildWalls();

    Area* area_ = nullptr;
    int w_ = 0, h_ = 0;
    std::vector<int> slots_;
    std::vector<Anim> anims_;
    std::vector<ActiveMove> active_;
    std::vector<int> bgBorderCount_;    // per slot * 4 sides (SGridBackground counters)
    std::vector<int> tileBorderCount_;  // per islandId (STile.SetBorderColliders counter)
};
