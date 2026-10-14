// Sliding-tile grid: the runtime state of an Area's STiles, ported from SGrid / STile /
// SMove / SGridAnimator. Owns each tile's slot, animated position and collider switch, plus the
// temporary border walls that fence a slide. Rendering and physics read it alongside the Area.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "game/data/area.h"

// Grid strings: "123_6##_4#5" = top row first, rows joined by '_', inactive tiles as '#'.
char idToChar(int id); // Converter.IntToChar: 1-9, then A, B, ...
int charToId(char c);
template <class IdAt, class IsActive>
std::string buildGridString(int w, int h, IdAt idAt, IsActive isActive, bool numsOnly = false) {
    std::string s;
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            int id = idAt(x, y);
            s += (numsOnly || isActive(id)) ? idToChar(id) : '#';
        }
        if (y != 0) s += '_';
    }
    return s;
}

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

// Runtime state of one STile.
struct TileState {
    int islandId = 0;
    int x = 0, y = 0;         // logical slot (updates the moment a move starts)
    bool active = false;      // collected
    float posX = 0, posY = 0; // drawn/physical position in slots; differs from x, y while sliding
    int collidersOff = 0;     // > 0: colliders off (STile.SetSliderCollider, during others' moves)
};

// Temporary wall segment (world units): the edge colliders that fence a slide.
struct Wall { float x0, y0, x1, y1; };

class Grid {
public:
    // New-game state from the Area: tiles at their exported slots and active flags, nothing moving.
    void init(const Area& area);

    int width() const { return w_; }
    int height() const { return h_; }
    int tileWidth() const { return tileWidth_; }
    int housingOffset() const { return housingOffset_; }
    // islandId of the tile in slot (x, y); every slot holds a tile (inactive = empty).
    int idAt(int x, int y) const { return slots_[y * w_ + x]; }
    const TileState* tile(int islandId) const;
    const std::vector<TileState>& tiles() const { return tiles_; }
    bool tileActive(int islandId) const;
    // World position of an owner's origin: an STile's animated center, or (0,0) for the world (0).
    void ownerBase(int owner, float& x, float& y) const;
    const std::vector<Wall>& walls() const { return walls_; }

    // Grid string of the slots; inactive tiles as '#' unless numsOnly. (Cached until the grid changes.)
    const std::string& gridString(bool numsOnly = false) const;
    // Bumped whenever slots, collected tiles or which tiles are moving change (not while they glide).
    uint32_t version() const { return version_; }
    // Rearranges every tile at once (SGrid.SetGrid): `ids` in grid-string order (top row first),
    // '_' ignored, '0' = the highest-numbered tile; each id exactly once. Cancels motion (use
    // Artifact::setGrid to keep the Artifact in step). False, and no change, if malformed.
    bool setGrid(const std::string& ids);
    bool checkTarget() const; // gridString(true) matches the area's targetGrid ('*' = any)

    void setTileActive(int islandId, bool active); // collecting a Slider enables its tile

    // Starts animating a move (SGrid.Move + SGridAnimator.Move). Logical slots update at once;
    // `playerStile` (0 = none) is the tile the player stands on, which keeps border walls
    // around it while it moves. `id` tags the move for update()'s finished list.
    void startMove(const SMove& move, int playerStile, int id);
    // Advances animations by dt seconds. Returns the ids of moves that finished this step.
    std::vector<int> update(float dt);
    bool moving() const { return !active_.empty(); }
    bool tileMoving(int islandId) const; // STile.IsMoving: the tile is animating in a move

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

    TileState* tileMut(int islandId);
    void clearMotion();
    void rebuildWalls();

    int w_ = 0, h_ = 0, tileWidth_ = 17, housingOffset_ = 0;
    std::string targetGrid_;
    std::vector<TileState> tiles_;  // in export order
    std::vector<int> tileIndex_;    // islandId -> index in tiles_, -1 = none
    std::vector<int> slots_;
    std::vector<Anim> anims_;
    std::vector<ActiveMove> active_;
    std::vector<int> bgBorderCount_;   // per slot * 4 sides (SGridBackground counters)
    std::vector<int> tileBorderCount_; // per tiles_ index (STile.SetBorderColliders counter)
    uint32_t version_ = 1;
    mutable std::string cached_[2];    // gridString(numsOnly) as of cachedVersion_[numsOnly]
    mutable uint32_t cachedVersion_[2] = {0, 0};
    std::vector<Wall> walls_;
};
