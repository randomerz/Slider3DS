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

// The STile's own object (what Item.DropItem parents to), -1 = none (the scene root).
int stileObject(const Area& area, int islandId) {
    const STileDef* s = area.stile(islandId);
    return s && s->objectsObject >= 0 ? area.objects[s->objectsObject].parent : -1;
}

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

void Items::sortingOrderInc(World& world, int order) {
    sortingOrderCalls_++;
    setSortingOrder(world, order);
}

void Items::sortingOrderDec(World& world, int order) {
    if (--sortingOrderCalls_ == 0) setSortingOrder(world, order);
}

void Items::setSortingOrder(World& world, int order) {
    // PlayerInventory.SetItemSortingOrder: the current (carried) item's too.
    sortingOrder_ = order;
    if (carried_ >= 0 && world.area.items[carried_].spriteObject >= 0)
        world.objects.setSortingOrder(world.area.items[carried_].spriteObject, order);
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
    if (it.spriteObject >= 0) world.objects.setSortingOrder(it.spriteObject, sortingOrder_); // Item.SetSortingOrder
    for (int o : it.enableOnDrop) world.objects.setActive(area, o, false);
    return true;
}

bool Items::tryDrop(World& world) {
    if (!canDrop_ || carried_ < 0 || phase_ == Phase::PickingUp) return false;
    const Area& area = world.area;
    float x, y;
    world.objectPosition(area.items[carried_].object, x, y); // the carry point
    startDrop(world, playerDrop_, carried_, x, y, dropX_, dropY_);
    carried_ = -1;
    phase_ = Phase::Dropping;
    canDrop_ = false;
    return true;
}

void Items::drop(World& world, int item, float x, float y, DropDone done, int arg) {
    float fromX, fromY;
    world.objectPosition(world.area.items[item].object, fromX, fromY);
    drops_.emplace_back();
    startDrop(world, drops_.back(), item, fromX, fromY, x, y);
    drops_.back().done = done;
    drops_.back().doneArg = arg;
}

void Items::startDrop(World& world, Drop& d, int item, float fromX, float fromY, float x, float y) {
    const ItemDef& it = world.area.items[item];
    d = Drop{item, it.pickUpDuration, fromX, fromY, x, y, DropDone::None, -1};
    // AnimateDrop's first step (it runs at once): the item at the landing spot, collider on, the
    // sprite where the item was. Then DropItem parents it to the STile under it there.
    const int stile = world.grid.stileUnder(x, y);
    world.objects.setParent(world.area, it.object, stileObject(world.area, stile));
    world.placeObject(it.object, stile, x, y);
    if (it.colliderSlot >= 0) world.objects.setColliderEnabled(it.colliderSlot, true);
    placeSprite(world, it, fromX, fromY);
}

bool Items::stepDrop(World& world, Drop& d, float dt) {
    // The sprite falls back along the pick-up curves; then it's back on the item, and OnDrop.
    const ItemDef& it = world.area.items[d.item];
    d.t -= dt;
    const float k = std::max(d.t / it.pickUpDuration, 0.0f);
    placeSprite(world, it, mathf::lerp(d.endX, d.startX, it.xCurve.eval(k)), mathf::lerp(d.endY, d.startY, it.yCurve.eval(k)));
    if (d.t >= 0) return false;
    resetSprite(world, it);
    runCalls(world, it.onDrop);
    return true;
}

void Items::carryPoint(const World& world, float& x, float& y) {
    // pickedItemLocation: as authored, moved by the player's Animator (it bobs with idle / run).
    const Area& area = world.area;
    const PlayerActionDef& pa = area.playerAction;
    x = world.player.x + pa.carryX;
    y = world.player.y + pa.carryY;
    if (pa.carryTarget >= 0 && area.player.animator >= 0) {
        const AnimatorDef::Target& t = area.animators[area.player.animator].targets[pa.carryTarget];
        float ax, ay;
        world.animators.targetPosition(area.player.animator, pa.carryTarget, ax, ay);
        x += ax - t.localX;
        y += ay - t.localY;
    }
}

void Items::follow(World& world) {
    if (carried_ < 0 || phase_ == Phase::PickingUp) return;
    float x, y;
    carryPoint(world, x, y);
    world.placeObject(world.area.items[carried_].object, 0, x, y);
}

void Items::update(World& world, float dt) {
    const Area& area = world.area;
    float carryX, carryY;
    carryPoint(world, carryX, carryY);

    if (phase_ == Phase::PickingUp) {
        // AnimatePickUp: the sprite flies to the (moving) carry point; the item itself stays put.
        const ItemDef& it = area.items[animated_];
        t_ += dt;
        const float k = std::min(t_ / it.pickUpDuration, 1.0f);
        placeSprite(world, it, mathf::lerp(startX_, carryX, it.xCurve.eval(k)), mathf::lerp(startY_, carryY, it.yCurve.eval(k)));
        if (t_ >= it.pickUpDuration) {
            // AnimatePickUpEnd: the item at the carry point, collider off, OnPickUp; then the player's
            // (FinishPicking parents it to the carry point).
            world.objects.setParent(area, it.object, -1);
            world.placeObject(it.object, 0, carryX, carryY);
            resetSprite(world, it);
            if (it.colliderSlot >= 0) world.objects.setColliderEnabled(it.colliderSlot, false);
            phase_ = Phase::None;
            animated_ = -1;
            runCalls(world, it.onPickUp);
        }
    } else if (phase_ == Phase::Dropping && stepDrop(world, playerDrop_, dt)) {
        // FinishDropping: ResetSortingOrder (to Awake's: as exported), dropCallback.
        const ItemDef& it = area.items[playerDrop_.item];
        phase_ = Phase::None;
        if (it.spriteObject >= 0) world.objects.setSortingOrder(it.spriteObject, ObjectStates::EXPORTED_ORDER);
        for (int o : it.enableOnDrop) world.objects.setActive(area, o, true);
    }
    // Scripts' drops (a callback may start another drop: by index, finished ones removed after).
    for (size_t i = 0; i < drops_.size(); i++)
        if (drops_[i].item >= 0 && stepDrop(world, drops_[i], dt)) {
            drops_[i].item = -1;
            world.dropLanded(drops_[i].done, drops_[i].doneArg);
        }
    drops_.erase(std::remove_if(drops_.begin(), drops_.end(), [](const Drop& d) { return d.item < 0; }), drops_.end());
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
