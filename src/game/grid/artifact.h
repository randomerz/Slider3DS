// The Artifact's move logic (UIArtifact.cs), without any drawing: its own view of the grid
// (buttons move the moment a move is queued), a queue of up to 3 moves, and the rule that
// non-overlapping moves run at the same time. Chained moves speed up, down to half duration.
#pragma once

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "game/grid/grid.h"

class Artifact {
public:
    void init(const Grid& grid); // buttons match the grid; clears queues

    int width() const { return w_; }
    int height() const { return h_; }

    // Button (tile id) at slot (x, y) as the Artifact shows it.
    int buttonAt(int x, int y) const { return buttons_[y * w_ + x]; }
    bool buttonActive(const Grid& grid, int x, int y) const { return grid.tileActive(buttonAt(x, y)); }
    // Slot of a tile's button; false if the tile isn't on the Artifact.
    bool findButton(int islandId, int& x, int& y) const;

    // Empty slots the tile at (x, y) can slide to: each direction, through a line of empties.
    std::vector<std::pair<int, int>> moveOptions(const Grid& grid, int x, int y) const;

    // Queues sliding the tile at `from` into the empty slot `to` (UIArtifact.TryQueueMoveFromButtonPair).
    // False if it isn't one of moveOptions, the queue is full, or queueing is disabled.
    bool tryMove(Grid& grid, int fromX, int fromY, int toX, int toY, int playerStile);

    // Advances the grid's animations and starts queued moves as they free up. Call once per frame.
    void update(Grid& grid, float dt, int playerStile);

    // Rearranges the grid at once (SGrid.SetGrid) and resets the Artifact to match it: queued and
    // running moves are dropped. False, and no change, if `ids` is malformed.
    bool setGrid(Grid& grid, const std::string& ids);

    // Grid string of the buttons (UIArtifact.GetGridString), used by the final check.
    std::string gridString(const Grid& grid) const;
    bool queueEmpty() const { return queue_.empty() && active_.empty(); }
    bool tileInActiveMove(int islandId) const; // part of a move that is animating now

    bool canQueue = true;
    static constexpr int MAX_QUEUE = 3;

private:
    struct Queued {
        SMove move;
        int id;
    };
    void processQueue(Grid& grid, int playerStile);

    int w_ = 0, h_ = 0;
    std::vector<int> buttons_;
    std::deque<Queued> queue_;
    std::vector<Queued> active_;
    int nextId_ = 1;
    int moveCounter_ = 0;
    float consecutiveTimer_ = 0;
};
