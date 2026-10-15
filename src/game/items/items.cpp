#include "game/items/items.h"

#include <algorithm>
#include <cmath>

#include "game/events/events.h"
#include "game/physics/physics.h"
#include "game/world/world.h"

namespace {

const ObjectCollider& colliderAt(const Area& area, int slot) {
    const int solid = (int)area.objectColliders.size();
    return slot < solid ? area.objectColliders[slot] : area.triggerColliders[slot - solid];
}

bool onLayer(const std::vector<std::string>& layers, const std::string& layer) {
    return layers.empty() || std::find(layers.begin(), layers.end(), layer) != layers.end();
}

// Distance from (px, py) to the collider's shape (circles exactly, others by their bounds).
float distanceTo(const World& world, const ObjectCollider& c, float px, float py) {
    float bx, by;
    world.objectBase(c.object, bx, by);
    if (c.kind == ObjectCollider::Kind::Circle)
        return std::max(0.0f, std::hypot(px - (bx + c.cx), py - (by + c.cy)) - c.radius);
    const float dx = std::max({bx + c.minX - px, 0.0f, px - (bx + c.maxX)});
    const float dy = std::max({by + c.minY - py, 0.0f, py - (by + c.maxY)});
    return std::hypot(dx, dy);
}

// Physics2D.RaycastAll from (x0, y0) to (x1, y1) hits nothing solid but the item's own collider.
bool rayClear(const World& world, float x0, float y0, float x1, float y1, int ignoreCollider) {
    const float len = std::hypot(x1 - x0, y1 - y0);
    const int steps = std::max(1, (int)std::ceil(len * 16));
    const float h = 1.0f / 32;
    const CollisionNear near(world, {std::min(x0, x1) - h, std::min(y0, y1) - h, std::max(x0, x1) + h, std::max(y0, y1) + h},
                             world.player.onWater, ignoreCollider);
    for (int k = 0; k <= steps; k++) {
        const float x = x0 + (x1 - x0) * k / steps, y = y0 + (y1 - y0) * k / steps;
        if (near.blocked({x - h, y - h, x + h, y + h})) return false;
    }
    return true;
}

int solidIndex(const Area& area, int slot) { return slot >= 0 && slot < (int)area.objectColliders.size() ? slot : -1; }

} // namespace

void Items::reset(const Area&) { *this = Items(); }

void Items::placeSprite(World& world, const ItemDef& it, float x, float y) {
    if (it.spriteObject < 0) return;
    const SceneObject& root = world.area.objects[it.object];
    const SceneObject& sprite = world.area.objects[it.spriteObject];
    world.placeObject(it.spriteObject, 0, x + sprite.x - root.x, y + sprite.y - root.y);
}

void Items::resetSprite(World& world, const ItemDef& it) {
    if (it.spriteObject >= 0) world.objects.setPlacement(world.area, it.spriteObject, -1, 0, 0);
}

bool Items::carrying(const Area& area, const std::string& name) const {
    return carried_ >= 0 && (name.empty() || area.items[carried_].name == name);
}

bool Items::dropIndicator(float& x, float& y) const {
    if (carried_ < 0 || phase_ == Phase::PickingUp || !canDrop_) return false;
    x = dropX_;
    y = dropY_;
    return true;
}

void Items::sortingOrderInc(int order) {
    sortingOrderCalls_++;
    sortingOrder_ = order;
}

void Items::sortingOrderDec(int order) {
    if (--sortingOrderCalls_ == 0) sortingOrder_ = order;
}

bool Items::carriedSortingOrder(const Area& area, int spriteObject, int& order) const {
    // Item.SetSortingOrder at pick-up, ResetSortingOrder once a drop has landed.
    const int item = carried_ >= 0 ? carried_ : phase_ == Phase::Dropping ? animated_ : -1;
    if (item < 0 || area.items[item].spriteObject != spriteObject) return false;
    order = sortingOrder_;
    return true;
}

bool Items::tryPickUp(World& world) {
    if (busy() || carried_ >= 0) return false;
    const Area& area = world.area;
    const float px = world.player.x, py = world.player.y;
    int best = -1;
    float bestDistance = 0;
    for (int i = 0; i < (int)area.items.size(); i++) {
        const ItemDef& it = area.items[i];
        if (it.colliderSlot < 0) continue;
        const ObjectCollider& c = colliderAt(area, it.colliderSlot);
        if (!world.objects.active(c) || !onLayer(area.playerAction.itemLayers, c.layer)) continue;
        if (distanceTo(world, c, px, py) > PICKUP_RANGE) continue;
        // CheckItemRaycast: a clear line from the player to the item, or one offset by its radius either side.
        float ix, iy;
        world.objectPosition(it.object, ix, iy);
        const float len = std::hypot(ix - px, iy - py);
        const float nx = len > 0 ? -(iy - py) / len * it.radius : 0, ny = len > 0 ? (ix - px) / len * it.radius : 0;
        const int ignore = solidIndex(area, it.colliderSlot);
        if (!rayClear(world, px, py, ix, iy, ignore) && !rayClear(world, px + nx, py + ny, ix, iy, ignore) &&
            !rayClear(world, px - nx, py - ny, ix, iy, ignore))
            continue;
        if (best < 0 || len < bestDistance) {
            best = i;
            bestDistance = len;
        }
    }
    if (best < 0) return false;
    const ItemDef& it = area.items[best];
    carried_ = animated_ = best;
    phase_ = Phase::PickingUp;
    t_ = 0;
    world.objectPosition(it.object, startX_, startY_);
    for (int o : it.enableOnDrop) world.objects.setActive(area, o, false);
    return true;
}

bool Items::tryDrop(World& world) {
    if (!canDrop_ || carried_ < 0 || phase_ == Phase::PickingUp) return false;
    const Area& area = world.area;
    const ItemDef& it = area.items[carried_];
    world.objectPosition(it.object, startX_, startY_); // the carry point
    endX_ = dropX_;
    endY_ = dropY_;
    // Item.DropItem: onto the STile under it (where it's carried), at the landing spot, collider on.
    world.placeObject(it.object, world.grid.stileUnder(startX_, startY_), endX_, endY_);
    if (it.colliderSlot >= 0) world.objects.setColliderEnabled(it.colliderSlot, true);
    placeSprite(world, it, startX_, startY_); // (AnimateDrop's first step runs at once: from the carry point)
    animated_ = carried_;
    carried_ = -1;
    phase_ = Phase::Dropping;
    t_ = it.pickUpDuration;
    canDrop_ = false;
    return true;
}

void Items::update(World& world, float dt) {
    const Area& area = world.area;
    const PlayerActionDef& pa = area.playerAction;
    const float carryX = world.player.x + pa.carryX, carryY = world.player.y + pa.carryY;

    if (phase_ == Phase::PickingUp) {
        // AnimatePickUp: the sprite flies to the (moving) carry point; the item itself stays put.
        const ItemDef& it = area.items[animated_];
        t_ += dt;
        const float k = std::min(t_ / it.pickUpDuration, 1.0f);
        placeSprite(world, it, startX_ + (carryX - startX_) * it.curveX(k), startY_ + (carryY - startY_) * it.curveY(k));
        if (t_ >= it.pickUpDuration) {
            // AnimatePickUpEnd: the item at the carry point, collider off, OnPickUp.
            world.placeObject(it.object, 0, carryX, carryY);
            resetSprite(world, it);
            if (it.colliderSlot >= 0) world.objects.setColliderEnabled(it.colliderSlot, false);
            phase_ = Phase::None;
            animated_ = -1;
            runCalls(world, it.onPickUp);
        }
    } else if (phase_ == Phase::Dropping) {
        // AnimateDrop: the item already sits where it lands; its sprite falls back along the curves.
        const ItemDef& it = area.items[animated_];
        t_ -= dt;
        const float k = std::max(t_ / it.pickUpDuration, 0.0f);
        placeSprite(world, it, endX_ + (startX_ - endX_) * it.curveX(k), endY_ + (startY_ - endY_) * it.curveY(k));
        if (t_ < 0) {
            resetSprite(world, it);
            phase_ = Phase::None;
            animated_ = -1;
            runCalls(world, it.onDrop);
        }
    }
    if (carried_ >= 0 && phase_ != Phase::PickingUp) world.placeObject(area.items[carried_].object, 0, carryX, carryY);
    if (carried_ >= 0 && phase_ != Phase::PickingUp) computeDrop(world);
}

void Items::computeDrop(const World& world) {
    // GetClosestValidDropPosition: a box (DROP_BOX_WIDTH along the direction, DROP_BOX_HEIGHT across)
    // swept from the player (the boat's drop point on water) in the last walking direction; the
    // drop lands just short of the first thing it hits, or MAX_DROP_DISTANCE away.
    const Area& area = world.area;
    const Player& p = world.player;
    const float bx = p.x + (p.onWater ? area.playerAction.boatDropX : 0), by = p.y + (p.onWater ? area.playerAction.boatDropY : 0);
    const float len = std::hypot(p.lastMoveX, p.lastMoveY);
    if (len == 0) {
        canDrop_ = false;
        dropX_ = bx;
        dropY_ = by;
        return;
    }
    const float dx = p.lastMoveX / len, dy = p.lastMoveY / len;
    const float hx = 0.5f * (std::fabs(dx) * DROP_BOX_WIDTH + std::fabs(dy) * DROP_BOX_HEIGHT);
    const float hy = 0.5f * (std::fabs(dy) * DROP_BOX_WIDTH + std::fabs(dx) * DROP_BOX_HEIGHT);
    // The geometry around the whole sweep, gathered once; if none of it touches the sweep (open
    // ground, the usual case), the drop goes the full distance.
    const float ex = bx + dx * MAX_DROP_DISTANCE, ey = by + dy * MAX_DROP_DISTANCE;
    const Box sweep = {std::min(bx, ex) - hx, std::min(by, ey) - hy, std::max(bx, ex) + hx, std::max(by, ey) + hy};
    const CollisionNear near(world, sweep, p.onWater);
    auto blockedAt = [&](float s) {
        const float cx = bx + dx * s, cy = by + dy * s;
        return near.blocked({cx - hx, cy - hy, cx + hx, cy + hy});
    };
    float reach = MAX_DROP_DISTANCE;
    if (near.blocked(sweep)) {
        // Otherwise the first blocked step (steps shorter than the box, so nothing slips between),
        // then bisection between it and the last clear one.
        const float step = 1.0f / 8;
        for (float s = 0; s <= MAX_DROP_DISTANCE + 1e-4f; s += step) {
            if (!blockedAt(s)) continue;
            float clear = s - step, hit = s;
            if (clear < 0) {
                reach = 0;
                break;
            }
            for (int k = 0; k < 3; k++) {
                const float mid = (clear + hit) * 0.5f;
                (blockedAt(mid) ? hit : clear) = mid;
            }
            reach = clear;
            break;
        }
    }
    dropX_ = bx + dx * reach;
    dropY_ = by + dy * reach;
    canDrop_ = reach > MIN_DROP_DISTANCE;
}
