#include "game/grid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <tuple>

namespace {

char idToChar(int id) { return id > 9 ? (char)('A' - 10 + id) : (char)('0' + id); }
int charToId(char c) { return c > '9' ? c - 'A' + 10 : c - '0'; }

// --- Minimal regex (backtracking), enough for the grid patterns the game uses. ---

struct Atom {
    bool any = false, negate = false;
    std::string set; // literal chars; for a class, expanded members
    bool matches(char c) const {
        if (any) return true;
        bool in = set.find(c) != std::string::npos;
        return negate ? !in : in;
    }
};

struct Piece {
    Atom atom;
    int min = 1, max = 1; // max -1 = unbounded
};

bool parsePattern(const std::string& p, std::vector<Piece>& out, bool& anchorStart, bool& anchorEnd) {
    size_t i = 0;
    anchorStart = anchorEnd = false;
    if (i < p.size() && p[i] == '^') { anchorStart = true; i++; }
    while (i < p.size()) {
        char c = p[i];
        if (c == '$' && i + 1 == p.size()) { anchorEnd = true; i++; break; }
        Piece piece;
        if (c == '.') {
            piece.atom.any = true;
            i++;
        } else if (c == '[') {
            i++;
            if (i < p.size() && p[i] == '^') { piece.atom.negate = true; i++; }
            while (i < p.size() && p[i] != ']') {
                if (i + 2 < p.size() && p[i + 1] == '-' && p[i + 2] != ']') {
                    for (char ch = p[i]; ch <= p[i + 2]; ch++) piece.atom.set += ch;
                    i += 3;
                } else {
                    piece.atom.set += p[i++];
                }
            }
            if (i >= p.size()) return false;
            i++; // ']'
        } else {
            if (c == '\\' && i + 1 < p.size()) c = p[++i];
            piece.atom.set = std::string(1, c);
            i++;
        }
        if (i < p.size()) {
            char q = p[i];
            if (q == '*') { piece.min = 0; piece.max = -1; i++; }
            else if (q == '+') { piece.min = 1; piece.max = -1; i++; }
            else if (q == '?') { piece.min = 0; piece.max = 1; i++; }
            else if (q == '{') {
                size_t close = p.find('}', i);
                if (close == std::string::npos) return false;
                std::string body = p.substr(i + 1, close - i - 1);
                size_t comma = body.find(',');
                piece.min = std::atoi(body.substr(0, comma).c_str());
                piece.max = comma == std::string::npos ? piece.min
                          : comma + 1 == body.size()   ? -1
                                                       : std::atoi(body.substr(comma + 1).c_str());
                i = close + 1;
            }
        }
        out.push_back(piece);
    }
    return true;
}

bool matchHere(const std::vector<Piece>& ps, size_t pi, const std::string& s, size_t si, bool anchorEnd) {
    if (pi == ps.size()) return !anchorEnd || si == s.size();
    const Piece& p = ps[pi];
    size_t n = 0;
    while (n < (size_t)p.min) {
        if (si + n >= s.size() || !p.atom.matches(s[si + n])) return false;
        n++;
    }
    // Greedy: take as many as allowed, then back off.
    size_t most = n;
    while ((p.max < 0 || most < (size_t)p.max) && si + most < s.size() && p.atom.matches(s[si + most])) most++;
    for (size_t k = most + 1; k-- > n;)
        if (matchHere(ps, pi + 1, s, si + k, anchorEnd)) return true;
    return false;
}

// Village SGridAnimator.movementCurve: key (0,0) outSlope 0.2914 outWeight 0.8417,
// key (1,1) inSlope 9.4735 inWeight 0.04655 -> cubic Bezier control points:
const float CP1X = 0.84171325f, CP1Y = 0.84171325f * 0.29140925f;
const float CP2X = 1 - 0.046554923f, CP2Y = 1 - 0.046554923f * 9.473493f;

float bezier(float a, float b, float u) { // P0 = 0, P3 = 1
    float v = 1 - u;
    return 3 * v * v * u * a + 3 * v * u * u * b + u * u * u;
}

} // namespace

float slideCurve(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    float lo = 0, hi = 1; // x(u) is monotonic: solve x(u) = t
    for (int i = 0; i < 24; i++) {
        float mid = (lo + hi) * 0.5f;
        if (bezier(CP1X, CP2X, mid) < t) lo = mid; else hi = mid;
    }
    return bezier(CP1Y, CP2Y, (lo + hi) * 0.5f);
}

bool gridMatches(const std::string& s, const std::string& pattern) {
    std::vector<Piece> ps;
    bool anchorStart, anchorEnd;
    if (!parsePattern(pattern, ps, anchorStart, anchorEnd)) return false;
    for (size_t start = 0; start <= s.size(); start++) {
        if (matchHere(ps, 0, s, start, anchorEnd)) return true;
        if (anchorStart) break;
    }
    return false;
}

namespace CheckGrid {
bool contains(const std::string& g, const std::string& regex) { return gridMatches(g, regex); }
bool row(const std::string& g, const std::string& r) { return gridMatches(g, "_" + r) || gridMatches(g, r + "_"); }
bool column(const std::string& g, const std::string& c) {
    const std::string tail = std::string(1, c[0]) + ".{3}" + c[1] + ".{3}" + c[2];
    return gridMatches(g, "^" + tail) || gridMatches(g, "^." + tail) || gridMatches(g, "^.{2}" + tail);
}
bool subgrid(const std::string& g, const std::string& s) {
    return gridMatches(g, std::string() + s[0] + s[1] + "._" + s[2] + s[3]) ||
           gridMatches(g, std::string() + s[0] + s[1] + "_." + s[2] + s[3]);
}
} // namespace CheckGrid

// --- SMove ---

SMove SMove::swap(int x1, int y1, int x2, int y2, int islandId1, int islandId2) {
    SMove m;
    m.moves.push_back({x1, y1, x2, y2, islandId1});
    m.moves.push_back({x2, y2, x1, y1, islandId2});
    return m;
}

std::vector<std::pair<int, int>> SMove::positions() const {
    std::set<std::pair<int, int>> set;
    for (const Movement& m : moves)
        for (int x = std::min(m.sx, m.ex); x <= std::max(m.sx, m.ex); x++)
            for (int y = std::min(m.sy, m.ey); y <= std::max(m.sy, m.ey); y++)
                set.insert({x, y});
    return {set.begin(), set.end()};
}

bool SMove::overlaps(const SMove& other) const {
    auto a = positions(), b = other.positions();
    for (const auto& p : a)
        if (std::find(b.begin(), b.end(), p) != b.end()) return true;
    return false;
}

std::vector<SMove::Border> SMove::borders() const {
    const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    std::set<std::tuple<int, int, int>> on;
    auto toggle = [&](int x, int y, int side) {
        auto k = std::make_tuple(x, y, side);
        if (!on.erase(k)) on.insert(k);
    };
    for (const auto& p : positions()) {
        for (int side = 0; side < 4; side++) {
            toggle(p.first, p.second, side);
            toggle(p.first + dx[side], p.second + dy[side], (side + 2) % 4);
        }
    }
    std::vector<Border> out;
    for (const auto& k : on) out.push_back({std::get<0>(k), std::get<1>(k), std::get<2>(k)});
    return out;
}

// --- Grid ---

void Grid::init(Area& area) {
    area_ = &area;
    w_ = area.gridW;
    h_ = area.gridH;
    slots_.assign(w_ * h_, 0);
    int maxId = 0;
    for (STileDef& s : area.stiles) {
        s.x = s.startX;
        s.y = s.startY;
        s.active = s.startActive;
        s.posX = (float)s.x;
        s.posY = (float)s.y;
        s.collidersOff = 0;
        if (s.x >= 0 && s.x < w_ && s.y >= 0 && s.y < h_) slots_[s.y * w_ + s.x] = s.islandId;
        maxId = std::max(maxId, s.islandId);
    }
    anims_.clear();
    active_.clear();
    bgBorderCount_.assign(w_ * h_ * 4, 0);
    tileBorderCount_.assign(maxId + 1, 0);
    rebuildWalls();
}

STileDef* Grid::stile(int islandId) {
    for (STileDef& s : area_->stiles)
        if (s.islandId == islandId) return &s;
    return nullptr;
}

std::string Grid::gridString(bool numsOnly) const {
    std::string s;
    for (int y = h_ - 1; y >= 0; y--) {
        for (int x = 0; x < w_; x++) {
            const STileDef* t = area_->stile(idAt(x, y));
            s += (numsOnly || (t && t->active)) ? idToChar(idAt(x, y)) : '#';
        }
        if (y != 0) s += '_';
    }
    return s;
}

bool Grid::setGrid(const std::string& ids) {
    std::vector<int> next;
    for (char c : ids) {
        if (c == '_') continue;
        int id = (c == '0' || c == '#') ? w_ * h_ : charToId(c);
        if (!area_->stile(id)) return false;
        next.push_back(id);
    }
    if ((int)next.size() != w_ * h_) return false;
    anims_.clear();
    active_.clear();
    std::fill(bgBorderCount_.begin(), bgBorderCount_.end(), 0);
    std::fill(tileBorderCount_.begin(), tileBorderCount_.end(), 0);
    for (int i = 0; i < w_ * h_; i++) {
        int x = i % w_, y = h_ - 1 - i / w_;
        slots_[y * w_ + x] = next[i];
        STileDef* s = stile(next[i]);
        s->x = x;
        s->y = y;
        s->posX = (float)x;
        s->posY = (float)y;
        s->collidersOff = 0;
    }
    rebuildWalls();
    return true;
}

bool Grid::checkTarget() const {
    const std::string have = gridString(true);
    std::string want;
    for (char c : area_->targetGrid) if (c != '_') want += c;
    std::string flat;
    for (char c : have) if (c != '_') flat += c;
    if (flat.size() != want.size()) return false;
    for (size_t i = 0; i < want.size(); i++)
        if (want[i] != '*' && want[i] != flat[i]) return false;
    return true;
}

void Grid::setTileActive(int islandId, bool active) {
    if (STileDef* s = stile(islandId)) s->active = active;
}

void Grid::startMove(const SMove& move, int playerStile, int id) {
    ActiveMove am{id, move.borders(), {}};

    // SGridAnimator.DisableBordersAndColliders: fence the covered slots, and while the player
    // rides a tile, switch off the colliders of the other tiles in the move (in the old grid).
    for (const SMove::Border& b : am.borders)
        if (b.x >= 0 && b.x < w_ && b.y >= 0 && b.y < h_) bgBorderCount_[(b.y * w_ + b.x) * 4 + b.side]++;
    if (playerStile != 0) {
        for (const auto& p : move.positions()) {
            int tid = idAt(p.first, p.second);
            if (tid != playerStile) {
                stile(tid)->collidersOff++;
                am.collidersOff.push_back(tid);
            }
        }
    }

    const float duration = std::min(durationMultiplier * move.duration, 2.1f);
    std::vector<int> next = slots_;
    for (const Movement& m : move.moves) {
        int tid = idAt(m.sx, m.sy);
        STileDef* s = stile(tid);
        next[m.ey * w_ + m.ex] = tid;
        s->x = m.ex;
        s->y = m.ey;
        if (s->active) {
            bool on = tid == playerStile;
            if (on) tileBorderCount_[tid]++;
            anims_.push_back({id, tid, (float)m.sx, (float)m.sy, (float)m.ex, (float)m.ey, 0.0f, duration, on});
        } else {
            s->posX = (float)m.ex; // empty slots jump straight to their new place
            s->posY = (float)m.ey;
        }
    }
    slots_ = next;
    active_.push_back(std::move(am));
    rebuildWalls();
}

std::vector<int> Grid::update(float dt) {
    std::vector<int> finished;
    if (anims_.empty()) return finished;
    for (Anim& a : anims_) {
        STileDef* s = stile(a.islandId);
        a.t += dt;
        float k = slideCurve(std::min(a.t / a.duration, 1.0f));
        s->posX = a.sx + (a.ex - a.sx) * k;
        s->posY = a.sy + (a.ey - a.sy) * k;
        if (a.t >= a.duration) {
            s->posX = a.ex;
            s->posY = a.ey;
            if (a.playerOn) tileBorderCount_[a.islandId]--;
        }
    }
    anims_.erase(std::remove_if(anims_.begin(), anims_.end(), [](const Anim& a) { return a.t >= a.duration; }),
                 anims_.end());

    // A move ends once all its tiles have arrived: drop its fences, restore colliders.
    for (size_t i = 0; i < active_.size();) {
        const ActiveMove& am = active_[i];
        bool running = std::any_of(anims_.begin(), anims_.end(), [&](const Anim& a) { return a.moveId == am.id; });
        if (running) { i++; continue; }
        for (const SMove::Border& b : am.borders)
            if (b.x >= 0 && b.x < w_ && b.y >= 0 && b.y < h_) {
                int& c = bgBorderCount_[(b.y * w_ + b.x) * 4 + b.side];
                c = std::max(0, c - 1);
            }
        for (int tid : am.collidersOff) stile(tid)->collidersOff--;
        finished.push_back(am.id);
        active_.erase(active_.begin() + i);
    }
    rebuildWalls();
    return finished;
}

int Grid::stileUnder(float x, float y, int current) const {
    int best = 0;
    for (const STileDef& s : area_->stiles) {
        if (!s.active) continue;
        const float cx = s.posX * s.width, cy = s.posY * s.width, half = s.width * 0.5f;
        const float hy = cy + area_->housingOffset;
        bool in = cx - half <= x && x < cx + half &&
                  ((cy - half <= y && y < cy + half) || (hy - half <= y && y < hy + half));
        if (!in) continue;
        if (s.islandId == current) return current;
        if (best == 0 || s.islandId < best) best = s.islandId;
    }
    return best;
}

void Grid::rebuildWalls() {
    std::vector<Wall>& walls = area_->borderWalls;
    walls.clear();
    // Side i of a square centered on (cx, cy), half-size h, in the world and in its house copy.
    auto addSide = [&](float cx, float cy, float h, int side) {
        const float c[4][4] = {{h, -h, h, h}, {-h, h, h, h}, {-h, -h, -h, h}, {-h, -h, h, -h}};
        for (float oy : {0.0f, (float)area_->housingOffset})
            walls.push_back({cx + c[side][0], cy + oy + c[side][1], cx + c[side][2], cy + oy + c[side][3]});
    };
    const float width = area_->stiles.empty() ? 17.0f : (float)area_->stiles[0].width;
    for (int i = 0; i < w_ * h_ * 4; i++)
        if (bgBorderCount_[i] > 0) {
            int slot = i / 4;
            addSide((slot % w_) * width, (slot / w_) * width, width * 0.5f, i % 4);
        }
    for (const STileDef& s : area_->stiles)
        if (s.islandId < (int)tileBorderCount_.size() && tileBorderCount_[s.islandId] > 0)
            for (int side = 0; side < 4; side++) addSide(s.posX * s.width, s.posY * s.width, s.width * 0.5f, side);
}
