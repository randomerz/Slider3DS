#include "game/grid/artifact.h"

#include <algorithm>

namespace {
constexpr float CONSECUTIVE_BUFFER = 0.15f; // seconds idle before chained moves stop speeding up
}

void Artifact::init(const Grid& grid) {
    w_ = grid.width();
    h_ = grid.height();
    buttons_.resize(w_ * h_);
    for (int y = 0; y < h_; y++)
        for (int x = 0; x < w_; x++) buttons_[y * w_ + x] = grid.idAt(x, y);
    queue_.clear();
    active_.clear();
    moveCounter_ = 0;
    resets_++;
    consecutiveTimer_ = 0;
}

bool Artifact::findButton(int islandId, int& x, int& y) const {
    for (int i = 0; i < w_ * h_; i++)
        if (buttons_[i] == islandId) {
            x = i % w_;
            y = i / w_;
            return true;
        }
    return false;
}

std::vector<std::pair<int, int>> Artifact::moveOptions(const Grid& grid, int x, int y) const {
    std::vector<std::pair<int, int>> out;
    if (x < 0 || x >= w_ || y < 0 || y >= h_ || !buttonActive(grid, x, y)) return out;
    const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    for (int d = 0; d < 4; d++) {
        for (int i = 1;; i++) {
            int nx = x + dx[d] * i, ny = y + dy[d] * i;
            if (nx < 0 || nx >= w_ || ny < 0 || ny >= h_ || buttonActive(grid, nx, ny)) break;
            out.push_back({nx, ny});
        }
    }
    return out;
}

bool Artifact::tryMove(Grid& grid, int fromX, int fromY, int toX, int toY, int playerStile) {
    if (!canQueue || (int)queue_.size() >= MAX_QUEUE) return false;
    const auto options = moveOptions(grid, fromX, fromY);
    if (std::find(options.begin(), options.end(), std::make_pair(toX, toY)) == options.end()) return false;

    int idFrom = buttonAt(fromX, fromY), idTo = buttonAt(toX, toY);
    queue_.push_back({SMove::swap(fromX, fromY, toX, toY, idFrom, idTo), nextId_++});
    std::swap(buttons_[fromY * w_ + fromX], buttons_[toY * w_ + toX]);
    processQueue(grid, playerStile);
    return true;
}

void Artifact::processQueue(Grid& grid, int playerStile) {
    while (!queue_.empty()) {
        for (const Queued& a : active_)
            if (a.move.overlaps(queue_.front().move)) return;
        Queued q = queue_.front();
        queue_.pop_front();
        bool hasActive = false;
        for (const Movement& m : q.move.moves) hasActive = hasActive || grid.tileActive(grid.idAt(m.sx, m.sy));
        if (!hasActive) continue; // only empty slots: UIArtifact drops these too
        q.move.duration = std::max(q.move.duration - moveCounter_ / 10.0f, q.move.duration / 2);
        consecutiveTimer_ = 0;
        moveCounter_++;
        active_.push_back(q);
        grid.startMove(q.move, playerStile, q.id);
    }
}

void Artifact::update(Grid& grid, float dt, int playerStile) {
    for (int id : grid.update(dt))
        active_.erase(std::remove_if(active_.begin(), active_.end(), [&](const Queued& q) { return q.id == id; }),
                      active_.end());
    processQueue(grid, playerStile);

    if (moveCounter_ > 0 && consecutiveTimer_ < CONSECUTIVE_BUFFER && queueEmpty()) {
        consecutiveTimer_ += dt;
        if (consecutiveTimer_ >= CONSECUTIVE_BUFFER) moveCounter_ = 0;
    }
}

bool Artifact::tileInActiveMove(int islandId) const {
    for (const Queued& q : active_)
        for (const Movement& m : q.move.moves)
            if (m.islandId == islandId) return true;
    return false;
}

bool Artifact::setGrid(Grid& grid, const std::string& ids) {
    if (!grid.setGrid(ids)) return false;
    init(grid);
    return true;
}

std::string Artifact::gridString(const Grid& grid) const {
    return buildGridString(
        w_, h_, [&](int x, int y) { return buttonAt(x, y); }, [&](int id) { return grid.tileActive(id); });
}
