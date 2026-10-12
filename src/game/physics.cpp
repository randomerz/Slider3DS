#include "game/physics.h"

#include <algorithm>
#include <cmath>

namespace {

float cross(float ax, float ay, float bx, float by) { return ax * by - ay * bx; }

// Segment p + t*d (t in [0,1]) vs segment a-b, touching counts.
bool segmentsIntersect(float px, float py, float dx, float dy, float ax, float ay, float bx, float by) {
    const float ex = bx - ax, ey = by - ay;
    const float denom = cross(dx, dy, ex, ey);
    const float qx = ax - px, qy = ay - py;
    if (std::fabs(denom) < 1e-9f) {
        // Parallel: overlapping only if collinear and the projections overlap.
        if (std::fabs(cross(qx, qy, dx, dy)) > 1e-6f) return false;
        float len2 = dx * dx + dy * dy;
        if (len2 < 1e-12f) return false;
        float t0 = (qx * dx + qy * dy) / len2;
        float t1 = ((bx - px) * dx + (by - py) * dy) / len2;
        return std::max(t0, t1) >= 0.0f && std::min(t0, t1) <= 1.0f;
    }
    const float t = cross(qx, qy, ex, ey) / denom;
    const float u = cross(qx, qy, dx, dy) / denom;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
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


// Segment vs polygon (n points, flat x,y pairs) offset by (ox, oy).
bool segmentHitsPolygon(float x, float y, float dx, float dy, const float* pts, size_t n, float ox, float oy) {
    if (n < 3) return false;
    if (pointInPolygon(x, y, pts, n, ox, oy)) return true;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
        if (segmentsIntersect(x, y, dx, dy, pts[2 * j] + ox, pts[2 * j + 1] + oy, pts[2 * i] + ox, pts[2 * i + 1] + oy))
            return true;
    return false;
}

bool segmentHitsBox(float x, float y, float dx, float dy, float minX, float minY, float maxX, float maxY) {
    const float box[8] = {minX, minY, maxX, minY, maxX, maxY, minX, maxY};
    return segmentHitsPolygon(x, y, dx, dy, box, 4, 0, 0);
}

// Segment vs circle: does any point of the segment lie within r of (cx, cy)?
bool segmentHitsCircle(float x, float y, float dx, float dy, float cx, float cy, float r) {
    const float len2 = dx * dx + dy * dy;
    float t = len2 > 1e-12f ? ((cx - x) * dx + (cy - y) * dy) / len2 : 0.0f;
    t = std::max(0.0f, std::min(1.0f, t));
    const float ex = x + t * dx - cx, ey = y + t * dy - cy;
    return ex * ex + ey * ey <= r * r;
}

// The colliders of an STile's objects collide like its tilemaps: off while the tile is inactive
// (its slot is solid anyway) or sliding in a move the player isn't riding.
bool ownerCollides(const Grid& grid, int owner) {
    if (owner == 0) return true;
    const TileState* t = grid.tile(owner);
    return t && t->active && t->collidersOff <= 0;
}

bool objectHit(const ObjectCollider& c, float x, float y, float dx, float dy, float ox, float oy) {
    // Cheap reject: the segment's bounds vs the collider's.
    if (std::max(x, x + dx) < c.minX + ox || std::min(x, x + dx) > c.maxX + ox ||
        std::max(y, y + dy) < c.minY + oy || std::min(y, y + dy) > c.maxY + oy)
        return false;
    switch (c.kind) {
    case ObjectCollider::Kind::Circle:
        return segmentHitsCircle(x, y, dx, dy, c.cx + ox, c.cy + oy, c.radius);
    case ObjectCollider::Kind::Poly:
        for (const Polygon& p : c.paths)
            if (segmentHitsPolygon(x, y, dx, dy, p.data(), p.size() / 2, ox, oy)) return true;
        return false;
    case ObjectCollider::Kind::Edge:
        for (const Polygon& p : c.paths)
            for (size_t i = 0; i + 3 < p.size(); i += 2)
                if (segmentsIntersect(x, y, dx, dy, p[i] + ox, p[i + 1] + oy, p[i + 2] + ox, p[i + 3] + oy)) return true;
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

Blocker segmentBlocker(const Area& area, const Grid& grid, float x, float y, float dx, float dy, bool onWater) {
    // Edge colliders fencing a slide in progress.
    const std::vector<Wall>& walls = grid.walls();
    for (size_t i = 0; i < walls.size(); i++) {
        const Wall& w = walls[i];
        if (segmentsIntersect(x, y, dx, dy, w.x0, w.y0, w.x1, w.y1)) return {Blocker::Wall, (int)i};
    }

    // Inactive STiles: STile.SetTileActive makes their slider colliders solid, blocking the slot
    // and the house interior below it.
    const float half = grid.tileWidth() * 0.5f;
    for (const TileState& t : grid.tiles()) {
        if (t.active || t.collidersOff > 0) continue;
        const float cx = t.posX * grid.tileWidth(), cy = t.posY * grid.tileWidth();
        const float hy = cy + grid.housingOffset();
        if (segmentHitsBox(x, y, dx, dy, cx - half, cy - half, cx + half, cy + half) ||
            segmentHitsBox(x, y, dx, dy, cx - half, hy - half, cx + half, hy + half))
            return {Blocker::InactiveTile, t.islandId};
    }

    for (size_t li = 0; li < area.collision.size(); li++) {
        const CollisionLayer& L = area.collision[li];
        if (!layerEnabled(grid, L, onWater)) continue;
        float bx, by;
        grid.ownerBase(L.owner, bx, by);
        // Segment in this layer's cell space: cell (cx, cy)'s min corner sits at (ox + cx, oy + cy).
        const float ox = bx + L.originX + L.minX, oy = by + L.originY + L.minY;
        const float lx = x - ox, ly = y - oy;
        int x0 = std::max(0, (int)std::floor(std::min(lx, lx + dx)) - 1);
        int x1 = std::min(L.w - 1, (int)std::floor(std::max(lx, lx + dx)) + 1);
        int y0 = std::max(0, (int)std::floor(std::min(ly, ly + dy)) - 1);
        int y1 = std::min(L.h - 1, (int)std::floor(std::max(ly, ly + dy)) + 1);
        for (int cy = y0; cy <= y1; cy++) {
            for (int cx = x0; cx <= x1; cx++) {
                int s = L.cell(cx, cy);
                if (s < 0) continue;
                for (const Polygon& poly : area.shapes[s])
                    if (segmentHitsPolygon(lx, ly, dx, dy, poly.data(), poly.size() / 2, (float)cx, (float)cy))
                        return {Blocker::Layer, (int)li};
            }
        }
    }

    for (size_t i = 0; i < area.objectColliders.size(); i++) {
        const ObjectCollider& c = area.objectColliders[i];
        if (!c.active || !ownerCollides(grid, c.owner)) continue;
        float bx, by;
        grid.ownerBase(c.owner, bx, by);
        if (objectHit(c, x, y, dx, dy, bx, by)) return {Blocker::Object, (int)i};
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
    case Blocker::Object: return "object " + area.objectColliders[b.index].path;
    }
    return "";
}

bool moveWithCollision(const Area& area, const Grid& grid, float& x, float& y, float dx, float dy, bool onWater) {
    if (dx == 0 && dy == 0) return false;
    if (!segmentBlocked(area, grid, x, y, dx, dy, onWater)) {
        x += dx;
        y += dy;
        return true;
    }
    // Like the original, the single-axis fallbacks move the full step length along that axis,
    // so sliding along a wall keeps full speed.
    const float step = std::sqrt(dx * dx + dy * dy);
    if (dx != 0) {
        float sx = dx > 0 ? step : -step;
        if (!segmentBlocked(area, grid, x, y, sx, 0, onWater)) {
            x += sx;
            return true;
        }
    }
    if (dy != 0) {
        float sy = dy > 0 ? step : -step;
        if (!segmentBlocked(area, grid, x, y, 0, sy, onWater)) {
            y += sy;
            return true;
        }
    }
    return false;
}
