#include "game/physics.h"

#include "game/world.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// Overlaps count only past this depth, so a box resting flush against a collider is free.
constexpr float EPS = 1e-4f;

// The box's interior, shrunk by EPS.
Box inner(const Box& b) { return {b.minX + EPS, b.minY + EPS, b.maxX - EPS, b.maxY - EPS}; }

bool boxesOverlap(const Box& a, const Box& b) {
    return a.minX < b.maxX && b.minX < a.maxX && a.minY < b.maxY && b.minY < a.maxY;
}

// Segment a-b vs box `in` (already shrunk): Liang-Barsky clipping.
bool segmentHitsBox(float ax, float ay, float bx, float by, const Box& in) {
    float t0 = 0, t1 = 1;
    const float d[2] = {bx - ax, by - ay};
    const float p[2] = {ax, ay};
    const float lo[2] = {in.minX, in.minY}, hi[2] = {in.maxX, in.maxY};
    for (int k = 0; k < 2; k++) {
        if (std::fabs(d[k]) < 1e-12f) {
            if (p[k] <= lo[k] || p[k] >= hi[k]) return false;
            continue;
        }
        float ta = (lo[k] - p[k]) / d[k], tb = (hi[k] - p[k]) / d[k];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    return true;
}

bool pointInPolygon(float x, float y, const float* pts, size_t n, float ox, float oy) {
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = pts[2 * i] + ox, yi = pts[2 * i + 1] + oy;
        float xj = pts[2 * j] + ox, yj = pts[2 * j + 1] + oy;
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
    }
    return inside;
}

// Closed polygon (n points, flat x,y pairs, offset by (ox, oy)) vs box interior: an edge crosses
// the box, or the polygon contains it.
bool polygonHitsBox(const float* pts, size_t n, float ox, float oy, const Box& in) {
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
        if (segmentHitsBox(pts[2 * j] + ox, pts[2 * j + 1] + oy, pts[2 * i] + ox, pts[2 * i + 1] + oy, in)) return true;
    return pointInPolygon((in.minX + in.maxX) * 0.5f, (in.minY + in.maxY) * 0.5f, pts, n, ox, oy);
}

bool circleHitsBox(float cx, float cy, float r, const Box& in) {
    const float nx = std::max(in.minX, std::min(cx, in.maxX)), ny = std::max(in.minY, std::min(cy, in.maxY));
    const float ex = nx - cx, ey = ny - cy;
    return ex * ex + ey * ey < r * r;
}

// The colliders of an STile's objects collide like its tilemaps: off while the tile is inactive
// (its slot is solid anyway) or sliding in a move the player isn't riding.
bool ownerCollides(const Grid& grid, int owner) {
    if (owner == 0) return true;
    const TileState* t = grid.tile(owner);
    return t && t->active && t->collidersOff <= 0;
}

bool objectHit(const ObjectCollider& c, float ox, float oy, const Box& in) {
    if (!boxesOverlap(in, {c.minX + ox, c.minY + oy, c.maxX + ox, c.maxY + oy})) return false;
    switch (c.kind) {
    case ObjectCollider::Kind::Circle:
        return circleHitsBox(c.cx + ox, c.cy + oy, c.radius, in);
    case ObjectCollider::Kind::Poly:
        for (const Polygon& p : c.paths)
            if (polygonHitsBox(p.data(), p.size() / 2, ox, oy, in)) return true;
        return false;
    case ObjectCollider::Kind::Edge:
        for (const Polygon& p : c.paths)
            for (size_t i = 0; i + 3 < p.size(); i += 2)
                if (segmentHitsBox(p[i] + ox, p[i + 1] + oy, p[i + 2] + ox, p[i + 3] + oy, in)) return true;
        return false;
    }
    return false;
}

bool layerEnabled(const Grid& grid, const CollisionLayer& L, bool onWater) {
    switch (L.mode) {
    case CollisionMode::OnFoot: if (onWater) return false; break;
    case CollisionMode::OnWater: if (!onWater) return false; break;
    case CollisionMode::Always: if (!L.active) return false; break;
    }
    // An inactive STile's tilemaps (colliders included) are switched off, and so are a tile's
    // colliders while it slides in a move the player isn't riding.
    return ownerCollides(grid, L.owner);
}

} // namespace

Blocker boxBlocker(const World& world, const Box& box, bool onWater) {
    const Area& area = world.area;
    const Grid& grid = world.grid;
    const Box in = inner(box);

    // Edge colliders fencing a slide in progress.
    const std::vector<Wall>& walls = grid.walls();
    for (size_t i = 0; i < walls.size(); i++) {
        const Wall& w = walls[i];
        if (segmentHitsBox(w.x0, w.y0, w.x1, w.y1, in)) return {Blocker::Wall, (int)i};
    }

    // Inactive STiles: STile.SetTileActive makes their slider colliders solid, blocking the slot
    // and the house interior below it.
    const float half = grid.tileWidth() * 0.5f;
    for (const TileState& t : grid.tiles()) {
        if (t.active || t.collidersOff > 0) continue;
        const float cx = t.posX * grid.tileWidth(), cy = t.posY * grid.tileWidth();
        const float hy = cy + grid.housingOffset();
        if (boxesOverlap(in, {cx - half, cy - half, cx + half, cy + half}) ||
            boxesOverlap(in, {cx - half, hy - half, cx + half, hy + half}))
            return {Blocker::InactiveTile, t.islandId};
    }

    for (size_t li = 0; li < area.collision.size(); li++) {
        const CollisionLayer& L = area.collision[li];
        if (!layerEnabled(grid, L, onWater)) continue;
        float bx, by;
        grid.ownerBase(L.owner, bx, by);
        // Cell (cx, cy)'s min corner sits at (ox + cx, oy + cy); a shape may poke into its neighbours.
        const float ox = bx + L.originX + L.minX, oy = by + L.originY + L.minY;
        int x0 = std::max(0, (int)std::floor(in.minX - ox) - 1);
        int x1 = std::min(L.w - 1, (int)std::floor(in.maxX - ox) + 1);
        int y0 = std::max(0, (int)std::floor(in.minY - oy) - 1);
        int y1 = std::min(L.h - 1, (int)std::floor(in.maxY - oy) + 1);
        for (int cy = y0; cy <= y1; cy++) {
            for (int cx = x0; cx <= x1; cx++) {
                int s = L.cell(cx, cy);
                if (s < 0) continue;
                for (const Polygon& poly : area.shapes[s])
                    if (polygonHitsBox(poly.data(), poly.size() / 2, ox + cx, oy + cy, in))
                        return {Blocker::Layer, (int)li};
            }
        }
    }

    for (size_t i = 0; i < area.objectColliders.size(); i++) {
        const ObjectCollider& c = area.objectColliders[i];
        if (!world.objects.active(c) || !ownerCollides(grid, c.owner)) continue;
        float bx, by;
        grid.ownerBase(c.owner, bx, by);
        if (objectHit(c, bx, by, in)) return {Blocker::Object, (int)i};
    }
    return {};
}

std::string describe(const Area& area, const Blocker& b) {
    switch (b.kind) {
    case Blocker::None: return "";
    case Blocker::Wall: return "slide border";
    case Blocker::InactiveTile: return "inactive STile " + std::to_string(b.index);
    case Blocker::Layer: {
        const CollisionLayer& L = area.collision[b.index];
        return (L.owner ? "STile " + std::to_string(L.owner) + " " : std::string()) + L.path;
    }
    case Blocker::Object: return "object " + area.pathOf(area.objectColliders[b.index].object);
    }
    return "";
}

namespace {

// Moves (x, y) by up to (dx, dy), stopping where the box would first overlap something (bisected
// to 1/64 of the step, well under a pixel). Returns the fraction of the step taken.
float sweep(const World& world, const Hitbox& hb, float& x, float& y,
            float dx, float dy, bool onWater) {
    auto freeAt = [&](float t) { return !boxBlocked(world, hitboxAt(hb, x + t * dx, y + t * dy), onWater); };
    float t = 1;
    if (!freeAt(1)) {
        float lo = 0, hi = 1;
        for (int i = 0; i < 6; i++) {
            const float mid = (lo + hi) * 0.5f;
            (freeAt(mid) ? lo : hi) = mid;
        }
        t = lo;
    }
    x += t * dx;
    y += t * dy;
    return t;
}

// Box2D's push-out for a box that overlaps something: the nearest free spot within one unit, in
// 1/16 steps along 8 directions. False if it is buried deeper than that.
bool pushOut(const World& world, const Hitbox& hb, float& x, float& y,
             bool onWater) {
    static const float dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7071f, 0.7071f}, {-0.7071f, 0.7071f},
                                     {0.7071f, -0.7071f}, {-0.7071f, -0.7071f}};
    for (int k = 1; k <= 16; k++) {
        const float r = k / 16.0f;
        for (const auto& d : dirs) {
            if (!boxBlocked(world, hitboxAt(hb, x + d[0] * r, y + d[1] * r), onWater)) {
                x += d[0] * r;
                y += d[1] * r;
                return true;
            }
        }
    }
    return false;
}

// The contact normal at a box resting against something: the middle of the arc of directions it
// can move in (probed 1/32 unit out in 16 directions). False if none or all are free.
bool contactNormal(const World& world, const Hitbox& hb, float x, float y,
                   bool onWater, float& nx, float& ny) {
    constexpr int N = 16;
    constexpr float R = 1.0f / 32.0f;
    nx = ny = 0;
    int freeCount = 0;
    for (int k = 0; k < N; k++) {
        const float a = k * 6.2831853f / N, cx = std::cos(a), cy = std::sin(a);
        if (!boxBlocked(world, hitboxAt(hb, x + cx * R, y + cy * R), onWater)) {
            nx += cx;
            ny += cy;
            freeCount++;
        }
    }
    const float len = std::sqrt(nx * nx + ny * ny);
    if (freeCount == 0 || freeCount == N || len < 1e-4f) return false;
    nx /= len;
    ny /= len;
    return true;
}

} // namespace

bool moveWithCollision(const World& world, const Hitbox& hb, float& x,
                       float& y, float dx, float dy, bool onWater) {
    if (dx == 0 && dy == 0) return false;
    bool moved = false;
    if (boxBlocked(world, hitboxAt(hb, x, y), onWater)) {
        // Overlapping (e.g. a tile slid onto the player): pushed out first, like Box2D. Buried too
        // deep to push out, it walks free rather than getting stuck.
        if (!pushOut(world, hb, x, y, onWater)) {
            x += dx;
            y += dy;
            return true;
        }
        moved = true;
    }
    for (int axis = 0; axis < 2; axis++) {
        const float d = axis == 0 ? dx : dy;
        if (d == 0) continue;
        const float t = axis == 0 ? sweep(world, hb, x, y, d, 0, onWater)
                                  : sweep(world, hb, x, y, 0, d, onWater);
        moved = moved || t > 0;
        if (t >= 1 || (axis == 0 ? dy : dx) != 0) continue;
        // Blocked moving along one axis only: Box2D pushes the box out along the contact normal, so
        // it slides around round or slanted colliders. Slide the rest of the step along the tangent.
        float nx, ny;
        if (!contactNormal(world, hb, x, y, onWater, nx, ny)) continue;
        const float rx = axis == 0 ? (1 - t) * d : 0, ry = axis == 0 ? 0 : (1 - t) * d;
        const float into = rx * nx + ry * ny;
        if (into >= 0) continue;
        const float tx = rx - into * nx, ty = ry - into * ny;
        if (tx * tx + ty * ty < 1e-8f) continue; // head-on into a flat face: stop, as Box2D does
        if (sweep(world, hb, x, y, tx, ty, onWater) > 0) moved = true;
    }
    return moved;
}

bool findFreeNear(const World& world, const Hitbox& hb, float& x, float& y) {
    for (int r = 0; r <= 16; r++) {
        for (int i = -r; i <= r; i++) {
            for (int j = -r; j <= r; j++) {
                if (std::abs(i) != r && std::abs(j) != r) continue; // ring only
                const float tx = x + i * 0.5f, ty = y + j * 0.5f;
                if (!boxBlocked(world, hitboxAt(hb, tx, ty), false)) {
                    x = tx;
                    y = ty;
                    return true;
                }
            }
        }
    }
    return false;
}
