#include "game/artifact.h"

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
    consecutiveTimer_ = 0;
}

bool Artifact::buttonActive(const Area& area, int x, int y) const {
    const STileDef* s = area.stile(buttonAt(x, y));
    return s && s->active;
}

std::vector<std::pair<int, int>> Artifact::moveOptions(const Area& area, int x, int y) const {
    std::vector<std::pair<int, int>> out;
    if (!buttonActive(area, x, y)) return out;
    const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    for (int d = 0; d < 4; d++) {
        for (int i = 1;; i++) {
            int nx = x + dx[d] * i, ny = y + dy[d] * i;
            if (nx < 0 || nx >= w_ || ny < 0 || ny >= h_ || buttonActive(area, nx, ny)) break;
            out.push_back({nx, ny});
        }
    }
    return out;
}

bool Artifact::tryMove(Grid& grid, int fromX, int fromY, int toX, int toY, int playerStile) {
    if (!canQueue || (int)queue_.size() >= MAX_QUEUE) return false;
    const STileDef* from = grid.stile(buttonAt(fromX, fromY));
    if (!from || !from->active) return false;
    const int dx = toX - fromX, dy = toY - fromY;
    if ((dx != 0) == (dy != 0)) return false; // straight lines only
    const int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
    for (int x = fromX + sx, y = fromY + sy;; x += sx, y += sy) {
        if (x < 0 || x >= w_ || y < 0 || y >= h_) return false;
        const STileDef* t = grid.stile(buttonAt(x, y));
        if (!t || t->active) return false;
        if (x == toX && y == toY) break;
    }

    int idFrom = buttonAt(fromX, fromY), idTo = buttonAt(toX, toY);
    queue_.push_back({SMove::swap(fromX, fromY, toX, toY, idFrom, idTo), nextId_++});
    std::swap(buttons_[fromY * w_ + fromX], buttons_[toY * w_ + toX]);
    processQueue(grid, playerStile);
    return true;
}

void Artifact::processQueue(Grid& grid, int playerStile) {
    while (!queue_.empty()) {
        const Queued& next = queue_.front();
        for (const Queued& a : active_)
            if (a.move.overlaps(next.move)) return;
        Queued q = queue_.front();
        queue_.pop_front();
        bool hasActive = false;
        for (const Movement& m : q.move.moves) {
            const STileDef* s = grid.stile(grid.idAt(m.sx, m.sy));
            if (s && s->active) hasActive = true;
        }
        if (!hasActive) continue; // only empty slots: nothing would ever finish it
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

std::string Artifact::gridString(const Area& area) const {
    std::string s;
    for (int y = h_ - 1; y >= 0; y--) {
        for (int x = 0; x < w_; x++) {
            int id = buttonAt(x, y);
            s += buttonActive(area, x, y) ? (char)(id > 9 ? 'A' - 10 + id : '0' + id) : '#';
        }
        if (y != 0) s += '_';
    }
    return s;
}
