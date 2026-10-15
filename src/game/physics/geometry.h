// The shape tests behind player collision: does a piece of geometry overlap a box's interior?
// Every test is strict: geometry that only touches the interior's boundary doesn't overlap it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

// An axis-aligned box in world units.
struct Box {
    float minX, minY, maxX, maxY;
};

namespace geom {

// Overlaps count only past this depth, so a box resting flush against a collider is free.
constexpr float EPS = 1e-4f;

// The box's interior, shrunk by EPS.
inline Box inner(const Box& b) { return {b.minX + EPS, b.minY + EPS, b.maxX - EPS, b.maxY - EPS}; }

inline bool boxesOverlap(const Box& a, const Box& b) {
    return a.minX < b.maxX && b.minX < a.maxX && a.minY < b.maxY && b.minY < a.maxY;
}

// Segment a-b vs box `in` (already shrunk): Liang-Barsky clipping.
inline bool segmentHitsBox(float ax, float ay, float bx, float by, const Box& in) {
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
        if (t0 >= t1) return false; // (only touching the box: not a hit)
    }
    return true;
}

inline bool pointInPolygon(float x, float y, const float* pts, size_t n, float ox, float oy) {
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
inline bool polygonHitsBox(const float* pts, size_t n, float ox, float oy, const Box& in) {
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
        if (segmentHitsBox(pts[2 * j] + ox, pts[2 * j + 1] + oy, pts[2 * i] + ox, pts[2 * i + 1] + oy, in)) return true;
    return pointInPolygon((in.minX + in.maxX) * 0.5f, (in.minY + in.maxY) * 0.5f, pts, n, ox, oy);
}

inline bool circleHitsBox(float cx, float cy, float r, const Box& in) {
    const float nx = std::max(in.minX, std::min(cx, in.maxX)), ny = std::max(in.minY, std::min(cy, in.maxY));
    const float ex = nx - cx, ey = ny - cy;
    return ex * ex + ey * ey < r * r;
}

} // namespace geom
