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

// Segment vs polygon offset by (ox, oy).
bool segmentHitsPolygon(float x, float y, float dx, float dy, const Polygon& poly, float ox, float oy) {
    const size_t n = poly.size() / 2;
    if (n < 3) return false;
    if (pointInPolygon(x, y, poly.data(), n, ox, oy)) return true;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
        if (segmentsIntersect(x, y, dx, dy, poly[2 * j] + ox, poly[2 * j + 1] + oy, poly[2 * i] + ox, poly[2 * i + 1] + oy))
            return true;
    return false;
}

bool segmentHitsBox(float x, float y, float dx, float dy, float minX, float minY, float maxX, float maxY) {
    const Polygon box = {minX, minY, maxX, minY, maxX, maxY, minX, maxY};
    return segmentHitsPolygon(x, y, dx, dy, box, 0, 0);
}

bool layerEnabled(const Area& area, const CollisionLayer& L, bool onWater) {
    switch (L.mode) {
    case CollisionMode::OnFoot: if (onWater) return false; break;
    case CollisionMode::OnWater: if (!onWater) return false; break;
    case CollisionMode::Always: if (!L.active) return false; break;
    }
    // An inactive STile's tilemaps (colliders included) are switched off.
    const STileDef* s = L.owner != 0 ? area.stile(L.owner) : nullptr;
    return L.owner == 0 || (s && s->active);
}

} // namespace

bool segmentBlocked(const Area& area, float x, float y, float dx, float dy, bool onWater) {
    return !segmentBlockedBy(area, x, y, dx, dy, onWater).empty();
}

std::string segmentBlockedBy(const Area& area, float x, float y, float dx, float dy, bool onWater) {
    // Inactive STiles: STile.SetTileActive makes their slider colliders solid, blocking the slot
    // and the house interior below it.
    for (const STileDef& s : area.stiles) {
        if (s.active) continue;
        const float cx = (float)(s.x * s.width), cy = (float)(s.y * s.width), half = s.width * 0.5f;
        const float hy = cy + area.housingOffset;
        if (segmentHitsBox(x, y, dx, dy, cx - half, cy - half, cx + half, cy + half) ||
            segmentHitsBox(x, y, dx, dy, cx - half, hy - half, cx + half, hy + half))
            return "inactive STile " + std::to_string(s.islandId);
    }

    for (const CollisionLayer& L : area.collision) {
        if (!layerEnabled(area, L, onWater)) continue;
        float bx, by;
        area.ownerBase(L.owner, bx, by);
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
                    if (segmentHitsPolygon(lx, ly, dx, dy, poly, (float)cx, (float)cy))
                        return (L.owner ? "STile " + std::to_string(L.owner) + " " : std::string()) + L.path;
            }
        }
    }
    return "";
}

bool moveWithCollision(const Area& area, float& x, float& y, float dx, float dy, bool onWater) {
    if (dx == 0 && dy == 0) return false;
    if (!segmentBlocked(area, x, y, dx, dy, onWater)) {
        x += dx; y += dy;
        return true;
    }
    // Like the original, the single-axis fallbacks move the full step length along that axis,
    // so sliding along a wall keeps full speed.
    const float step = std::sqrt(dx * dx + dy * dy);
    if (dx != 0) {
        float sx = dx > 0 ? step : -step;
        if (!segmentBlocked(area, x, y, sx, 0, onWater)) {
            x += sx;
            return true;
        }
    }
    if (dy != 0) {
        float sy = dy > 0 ? step : -step;
        if (!segmentBlocked(area, x, y, 0, sy, onWater)) {
            y += sy;
            return true;
        }
    }
    return false;
}
