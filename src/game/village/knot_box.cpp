#include "game/village/knot_box.h"

#include <algorithm>
#include <cmath>

#include "game/physics/geometry.h"
#include "game/world/world.h"

namespace village {

namespace {

// Mathf.Sign: 1 for zero too.
float sign(float v) { return v >= 0 ? 1.0f : -1.0f; }

float cross(float ax, float ay, float bx, float by) { return ax * by - ay * bx; }

// KnotBox.IntersectingSegs: a-b and c-d cross (each pair of ends on opposite sides of the other).
bool intersectingSegs(const float* l1, const float* l2) {
    const float ax = l1[0], ay = l1[1], bx = l1[2], by = l1[3];
    const float cx = l2[0], cy = l2[1], dx = l2[2], dy = l2[3];
    const float oa = cross(dx - ax, dy - ay, cx - ax, cy - ay);
    const float ob = cross(dx - bx, dy - by, cx - bx, cy - by);
    const float oc = cross(bx - cx, by - cy, ax - cx, ay - cy);
    const float od = cross(bx - dx, by - dy, ax - dx, ay - dy);
    return sign(oa) + sign(ob) == 0 && sign(oc) + sign(od) == 0;
}

// Segment a-b vs circle: a Physics2D raycast hits it (also from inside: queriesStartInColliders).
bool segmentHitsCircle(float ax, float ay, float bx, float by, float cx, float cy, float r) {
    const float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    float t = len2 > 0 ? ((cx - ax) * dx + (cy - ay) * dy) / len2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    const float px = ax + t * dx - cx, py = ay + t * dy - cy;
    return px * px + py * py <= r * r;
}

bool segmentsCross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    const float d1 = cross(bx - ax, by - ay, cx - ax, cy - ay), d2 = cross(bx - ax, by - ay, dx - ax, dy - ay);
    const float d3 = cross(dx - cx, dy - cy, ax - cx, ay - cy), d4 = cross(dx - cx, dy - cy, bx - cx, by - cy);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

// RaycastAll from a to b against one collider placed at (ox, oy).
bool rayHits(float ax, float ay, float bx, float by, const ObjectCollider& c, float ox, float oy) {
    switch (c.kind) {
    case ObjectCollider::Kind::Circle: return segmentHitsCircle(ax, ay, bx, by, c.cx + ox, c.cy + oy, c.radius);
    case ObjectCollider::Kind::Poly:
        for (const Polygon& p : c.paths) {
            const size_t n = p.size() / 2;
            if (n >= 3 && geom::pointInPolygon(ax, ay, p.data(), n, ox, oy)) return true;
            for (size_t i = 0, j = n - 1; i < n; j = i++)
                if (segmentsCross(ax, ay, bx, by, p[2 * j] + ox, p[2 * j + 1] + oy, p[2 * i] + ox, p[2 * i + 1] + oy)) return true;
        }
        return false;
    case ObjectCollider::Kind::Edge:
        for (const Polygon& p : c.paths)
            for (size_t i = 0; i + 3 < p.size(); i += 2)
                if (segmentsCross(ax, ay, bx, by, p[i] + ox, p[i + 1] + oy, p[i + 2] + ox, p[i + 3] + oy)) return true;
        return false;
    }
    return false;
}

} // namespace

void knotLine(const World& world, int box, int line, float& x0, float& y0, float& x1, float& y1) {
    const KnotBoxDef& k = world.area.knotBoxes[box];
    const int n = (int)k.nodes.size();
    world.objectPosition(k.nodes[line], x0, y0);
    world.objectPosition(k.nodes[(line + 1) % n], x1, y1);
}

int knotBadLines(const World& world, int box, bool* bad) {
    const KnotBoxDef& k = world.area.knotBoxes[box];
    const int n = std::min((int)k.lines.size(), MAX_KNOT_LINES);
    for (int i = 0; bad && i < n; i++) bad[i] = false;
    if (!world.objects.activeInHierarchy(k.object)) return 0;

    float seg[MAX_KNOT_LINES][4];
    for (int i = 0; i < n; i++) knotLine(world, box, i, seg[i][0], seg[i][1], seg[i][2], seg[i][3]);
    int count = 0;
    for (int i = 0; i < n; i++) {
        const int next = i == n - 1 ? 0 : i + 1;
        bool intersects = false;
        // Physics2D.RaycastAll(a, b - a, |b - a|) on the knot's layer: any collider but its own nodes'.
        const float* s = seg[i];
        if (s[0] != s[2] || s[1] != s[3])
            for (const ObjectCollider& c : k.rayColliders) {
                if (c.object == k.nodes[i] || c.object == k.nodes[next]) continue;
                if (!c.enabled || !world.objects.activeInHierarchy(c.object)) continue;
                float ox, oy;
                world.objectBase(c.object, ox, oy);
                if (rayHits(s[0], s[1], s[2], s[3], c, ox, oy)) intersects = true;
            }
        // Lines that aren't next to it (the first and last are neighbours).
        for (int j = i - 2; j >= 0; j--) {
            if (j == 0 && i == n - 1) break;
            if (intersectingSegs(seg[i], seg[j])) intersects = true;
        }
        for (int j = i + 2; j < n; j++) {
            if (j == n - 1 && i == 0) break;
            if (intersectingSegs(seg[i], seg[j])) intersects = true;
        }
        if (bad) bad[i] = intersects;
        count += intersects;
    }
    return count;
}

void KnotBoxes::update(const World& world) {
    const std::vector<KnotBoxDef>& defs = world.area.knotBoxes;
    boxes_.resize(defs.size());
    for (size_t b = 0; b < defs.size(); b++) {
        Box& box = boxes_[b];
        const int n = std::min((int)defs[b].lines.size(), MAX_KNOT_LINES);
        box.bad.assign(n, 0);
        box.ends.assign(n * 4, 0.0f);
        bool bad[MAX_KNOT_LINES];
        box.badCount = knotBadLines(world, (int)b, bad);
        if (!world.objects.activeInHierarchy(defs[b].object)) continue; // (lines at the origin)
        for (int i = 0; i < n; i++) {
            box.bad[i] = bad[i];
            knotLine(world, (int)b, i, box.ends[4 * i], box.ends[4 * i + 1], box.ends[4 * i + 2], box.ends[4 * i + 3]);
        }
    }
}

void KnotBoxes::line(int box, int line, float& x0, float& y0, float& x1, float& y1) const {
    const float* e = &boxes_[box].ends[4 * line];
    x0 = e[0];
    y0 = e[1];
    x1 = e[2];
    y1 = e[3];
}

void knotCheckParticles(World& world, int box) {
    if (world.knots.badLines(box) != 0) return;
    for (const ParticlesDef& p : world.area.knotBoxes[box].particles) world.particles.play(world, p);
}

void knotSetToCorrectPositions(World& world, int box) {
    const KnotBoxDef& k = world.area.knotBoxes[box];
    for (size_t i = 0; i < k.nodes.size() && i < k.correct.size(); i++) {
        // `position = ...` on the node's parent: it stays in its place in the hierarchy (on its tile).
        const int item = world.objects.parent(k.nodes[i]);
        float bx, by;
        world.grid.ownerBase(k.correct[i].owner, bx, by);
        world.placeObject(item, world.objects.owner(item), bx + k.correct[i].x, by + k.correct[i].y);
    }
}

} // namespace village
