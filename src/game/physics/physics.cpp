#include "game/physics/physics.h"

#include "game/world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "game/physics/geometry.h"

namespace {

using namespace geom;

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

// Calls fn(const NearShape&) for each piece of solid geometry whose bounds overlap `region`, in
// boxBlocker's order (slide fences, inactive STiles, collision maps by group, object colliders as
// exported by group, then those of moved objects), until fn returns true; returns whether it did.
template <class Fn>
bool forEachNear(const World& world, const Box& region, bool onWater, int ignoreCollider, Fn&& fn) {
    const Area& area = world.area;
    const Grid& grid = world.grid;
    NearShape s{}; // (each kind sets the fields nearHits reads for it)

    // Edge colliders fencing a slide in progress.
    const std::vector<Wall>& walls = grid.walls();
    for (size_t i = 0; i < walls.size(); i++) {
        const Wall& w = walls[i];
        s.blocker = {Blocker::Wall, (int)i};
        s.wall = &w;
        s.bounds = {std::min(w.x0, w.x1), std::min(w.y0, w.y1), std::max(w.x0, w.x1), std::max(w.y0, w.y1)};
        const bool apart = s.bounds.minX > region.maxX || s.bounds.maxX < region.minX ||
                           s.bounds.minY > region.maxY || s.bounds.maxY < region.minY;
        if (!apart && fn(s)) return true;
    }

    // Inactive STiles: STile.SetTileActive makes their slider colliders solid, blocking the slot
    // and the house interior below it.
    const float half = grid.tileWidth() * 0.5f;
    for (const TileState& t : grid.tiles()) {
        if (t.active || t.collidersOff > 0) continue;
        const float cx = t.posX * grid.tileWidth(), cy = t.posY * grid.tileWidth();
        const float hy = cy + grid.housingOffset();
        s.blocker = {Blocker::InactiveTile, t.islandId};
        for (const Box& b : {Box{cx - half, cy - half, cx + half, cy + half}, Box{cx - half, hy - half, cx + half, hy + half}}) {
            s.bounds = b;
            if (boxesOverlap(region, b) && fn(s)) return true;
        }
    }

    // Broadphase: only groups (an owner's surface or its house interiors: Area::colliderGroups) whose
    // colliders' bounds, at the owner's current position, touch the region. (islandIds are < 64:
    // validated at load.)
    const size_t groups = std::min<size_t>(area.colliderGroups.size(), 128);
    float baseX[64], baseY[64];
    bool near[128];
    for (size_t o = 0; 2 * o < groups; o++) grid.ownerBase((int)o, baseX[o], baseY[o]);
    for (size_t gi = 0; gi < groups; gi++) {
        const ColliderGroup& g = area.colliderGroups[gi];
        const size_t o = gi / 2;
        near[gi] = !g.empty() &&
                   boxesOverlap(region, {g.minX + baseX[o], g.minY + baseY[o], g.maxX + baseX[o], g.maxY + baseY[o]});
    }

    for (size_t gi = 0; gi < groups; gi++) {
        if (!near[gi]) continue;
        const size_t o = gi / 2;
        for (int li : area.colliderGroups[gi].layers) {
            const CollisionLayer& L = area.collision[li];
            if (!layerEnabled(grid, L, onWater)) continue;
            // Cell (cx, cy)'s min corner sits at (ox + cx, oy + cy); a shape may poke into its neighbours.
            const float ox = baseX[o] + L.originX + L.minX, oy = baseY[o] + L.originY + L.minY;
            const int x0 = std::max(0, (int)std::floor(region.minX - ox) - 1);
            const int x1 = std::min(L.w - 1, (int)std::floor(region.maxX - ox) + 1);
            const int y0 = std::max(0, (int)std::floor(region.minY - oy) - 1);
            const int y1 = std::min(L.h - 1, (int)std::floor(region.maxY - oy) + 1);
            for (int cy = y0; cy <= y1; cy++)
                for (int cx = x0; cx <= x1; cx++) {
                    const int shape = L.cell(cx, cy);
                    if (shape < 0) continue;
                    const Area::ShapeBounds& b = area.shapeBounds[shape];
                    // (Offset exactly as the polygon's points are, so the bounds surely contain them.)
                    const float sx = ox + cx, sy = oy + cy;
                    const Box bounds = {b.minX + sx, b.minY + sy, b.maxX + sx, b.maxY + sy};
                    if (!boxesOverlap(region, bounds)) continue;
                    for (const Polygon& poly : area.shapes[shape]) {
                        if (poly.size() < 6) continue;
                        s.blocker = {Blocker::Layer, li};
                        s.bounds = bounds; // (the whole shape's)
                        s.pts = poly.data();
                        s.n = (int)(poly.size() / 2);
                        s.ox = sx;
                        s.oy = sy;
                        if (fn(s)) return true;
                    }
                }
        }
    }

    // Object colliders where they were exported, by group; then those of objects moved since.
    const ObjectStates& objects = world.objects;
    auto collider = [&](int i, float bx, float by) {
        const ObjectCollider& c = area.objectColliders[i];
        const Box bounds = {c.minX + bx, c.minY + by, c.maxX + bx, c.maxY + by};
        if (!boxesOverlap(region, bounds)) return false;
        s.blocker = {Blocker::Object, i};
        s.bounds = bounds;
        s.collider = &c;
        s.ox = bx;
        s.oy = by;
        return fn(s);
    };
    for (size_t gi = 0; gi < groups; gi++) {
        const size_t o = gi / 2;
        if (!near[gi] || (o != 0 && !ownerCollides(grid, (int)o))) continue;
        for (int i : area.colliderGroups[gi].colliders) {
            const ObjectCollider& c = area.objectColliders[i];
            if (i == ignoreCollider || !objects.active(c) || objects.isMoved(area, c.object)) continue;
            if (collider(i, baseX[o], baseY[o])) return true;
        }
    }
    for (int moved : objects.moved())
        for (int k = area.objectColliderStart[moved]; k < area.objectColliderStart[moved + 1]; k++) {
            const int i = area.objectCollidersOf[k];
            const ObjectCollider& c = area.objectColliders[i];
            if (i == ignoreCollider || !objects.active(c) || !ownerCollides(grid, objects.owner(c.object))) continue;
            float bx, by;
            world.objectBase(c.object, bx, by);
            if (collider(i, bx, by)) return true;
        }
    return false;
}

// Whether a gathered piece overlaps the box interior `in`.
bool nearHits(const NearShape& s, const Box& in) {
    switch (s.blocker.kind) {
    case Blocker::Wall: return segmentHitsBox(s.wall->x0, s.wall->y0, s.wall->x1, s.wall->y1, in);
    case Blocker::InactiveTile: return boxesOverlap(in, s.bounds);
    case Blocker::Layer: return boxesOverlap(in, s.bounds) && polygonHitsBox(s.pts, s.n, s.ox, s.oy, in);
    case Blocker::Object: return objectHit(*s.collider, s.ox, s.oy, in);
    case Blocker::None: return false;
    }
    return false;
}

} // namespace

Blocker boxBlocker(const World& world, const Box& box, bool onWater, int ignoreCollider) {
    const Box in = inner(box);
    Blocker hit;
    forEachNear(world, box, onWater, ignoreCollider, [&](const NearShape& s) {
        if (!nearHits(s, in)) return false;
        hit = s.blocker;
        return true;
    });
    return hit;
}

CollisionNear::CollisionNear(const World& world, const Box& region, bool onWater, int ignoreCollider)
    : world_(world), region_(region), onWater_(onWater), ignoreCollider_(ignoreCollider) {
    overflow_ = forEachNear(world, region, onWater, ignoreCollider, [&](const NearShape& s) {
        if (count_ == CAPACITY) return true;
        shapes_[count_++] = s;
        return false;
    });
}

Blocker CollisionNear::blocker(const Box& box) const {
    const bool inside = box.minX >= region_.minX && box.minY >= region_.minY && box.maxX <= region_.maxX &&
                        box.maxY <= region_.maxY;
    if (overflow_ || !inside) return boxBlocker(world_, box, onWater_, ignoreCollider_);
    const Box in = inner(box);
    for (int i = 0; i < count_; i++)
        if (nearHits(shapes_[i], in)) return shapes_[i].blocker;
    return {};
}

bool colliderOverlaps(const World& world, const ObjectCollider& c, const Box& box) {
    if (!world.objects.active(c) || !ownerCollides(world.grid, world.objects.owner(c.object))) return false;
    float bx, by;
    world.objectBase(c.object, bx, by);
    return objectHit(c, bx, by, inner(box));
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
float sweep(const CollisionNear& near, const Hitbox& hb, float& x, float& y, float dx, float dy) {
    auto freeAt = [&](float t) { return !near.blocked(hitboxAt(hb, x + t * dx, y + t * dy)); };
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

// Box2D's push-out for a box that overlaps something: the nearest free spot along 8 directions,
// in 1/16 steps within a unit, then 1/4 steps out to 4 units. False if it is buried deeper.
bool pushOut(const CollisionNear& near, const Hitbox& hb, float& x, float& y) {
    static const float dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7071f, 0.7071f}, {-0.7071f, 0.7071f},
                                     {0.7071f, -0.7071f}, {-0.7071f, -0.7071f}};
    for (int k = 1; k <= 16 + 12; k++) {
        const float r = k <= 16 ? k / 16.0f : 1.0f + (k - 16) / 4.0f;
        for (const auto& d : dirs) {
            if (!near.blocked(hitboxAt(hb, x + d[0] * r, y + d[1] * r))) {
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
bool contactNormal(const CollisionNear& near, const Hitbox& hb, float x, float y, float& nx, float& ny) {
    constexpr int N = 16;
    constexpr float R = 1.0f / 32.0f;
    nx = ny = 0;
    int freeCount = 0;
    for (int k = 0; k < N; k++) {
        const float a = k * 6.2831853f / N, cx = std::cos(a), cy = std::sin(a);
        if (!near.blocked(hitboxAt(hb, x + cx * R, y + cy * R))) {
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
    // Everything below stays within the step's reach of the start (plus the contact probes); what
    // strays further (a deep push-out) takes the full query.
    const float reach = std::fabs(dx) + std::fabs(dy) + 1.0f / 16.0f;
    const Box start = hitboxAt(hb, x, y);
    if (dx == 0 && dy == 0 && !boxBlocked(world, start, onWater)) return false; // standing free: one query
    const CollisionNear near(world, {start.minX - reach, start.minY - reach, start.maxX + reach, start.maxY + reach}, onWater);
    bool moved = false;
    if (near.blocked(start)) {
        // Overlapping (a tile slid onto the player, a collider switched on around it): pushed out
        // first, like Box2D, even when not moving. Buried deeper than the push-out reaches (only
        // possible inside an uncollected slot), it walks free rather than being stuck for good.
        if (!pushOut(near, hb, x, y)) {
            x += dx;
            y += dy;
            return dx != 0 || dy != 0;
        }
        moved = true;
    }
    if (dx == 0 && dy == 0) return moved;
    for (int axis = 0; axis < 2; axis++) {
        const float d = axis == 0 ? dx : dy;
        if (d == 0) continue;
        const float t = axis == 0 ? sweep(near, hb, x, y, d, 0) : sweep(near, hb, x, y, 0, d);
        moved = moved || t > 0;
        if (t >= 1 || (axis == 0 ? dy : dx) != 0) continue;
        // Blocked moving along one axis only: Box2D pushes the box out along the contact normal, so
        // it slides around round or slanted colliders. Slide the rest of the step along the tangent.
        float nx, ny;
        if (!contactNormal(near, hb, x, y, nx, ny)) continue;
        const float rx = axis == 0 ? (1 - t) * d : 0, ry = axis == 0 ? 0 : (1 - t) * d;
        const float into = rx * nx + ry * ny;
        if (into >= 0) continue;
        const float tx = rx - into * nx, ty = ry - into * ny;
        if (tx * tx + ty * ty < 1e-8f) continue; // head-on into a flat face: stop, as Box2D does
        if (sweep(near, hb, x, y, tx, ty) > 0) moved = true;
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
