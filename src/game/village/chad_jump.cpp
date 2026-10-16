#include "game/village/chad_jump.h"

#include <algorithm>
#include <utility>

#include "game/events/events.h"
#include "game/world/world.h"

namespace village {

namespace {

constexpr const char* VILLAGE_COMPLETION_FLAG = "villageCompletion";
constexpr int CHAD_TILE = 8;      // VillageChadJump.TipOnTileMove: tile 8's move ends the tipping (hard-coded)
constexpr float CHIRP_DELAY = 0.5f; // DelayThenChirp
constexpr float TIP_TIME = 0.5f;    // ChadJump.Fall: tipping before he falls


} // namespace

void ChadJumps::reset(const Area& area) { chads_.assign(area.chadJumps.size(), Chad()); }

void ChadJumps::activate(World& world) {
    const Area& area = world.area;
    for (int c = 0; c < (int)chads_.size(); c++) {
        const ChadJumpDef& d = area.chadJumps[c];
        Chad& s = chads_[c];
        if (s.started || !world.objects.activeInHierarchy(d.object)) continue;
        s.started = true;
        // Awake: the item can be picked up off Chad only if the Caves were finished first.
        if (d.item >= 0 && area.items[d.item].colliderSlot >= 0)
            world.objects.setColliderEnabled(area.items[d.item].colliderSlot,
                                             area.areaGrid.type == "VillageGrid" && world.state.hasCollectible("Slider 3", "Caves"));
        if (d.tips) s.fallen = world.state.flag(VILLAGE_COMPLETION_FLAG); // VillageChadJump.Start
    }
}

void ChadJumps::gridEvents(World& world, const std::vector<GridEvent>& events) {
    const Area& area = world.area;
    for (int c = 0; c < (int)chads_.size(); c++) {
        const ChadJumpDef& d = area.chadJumps[c];
        Chad& s = chads_[c];
        for (const GridEvent& e : events) {
            if (!world.objects.activeInHierarchy(d.object)) break; // (OnDisable unsubscribed)
            if (e.kind != GridEvent::Kind::TileMoveEnd) continue;
            // ChadJump.OnTileMoved: his tile arrived while he's up there.
            if (world.grid.tileActive(d.island) && e.islandId == d.island && s.state == State::Jumped) startFall(world, c);
            // VillageChadJump.TipOnTileMove (subscribed after it, on the same object).
            if (!d.tips || !s.onRock || s.fallen) continue;
            if (e.islandId == CHAD_TILE) {
                s.fallen = true;
                continue;
            }
            s.timeTillNormal = d.timeTipped;
            if (!s.tipping) {
                s.slid = true;
                world.animators.setBool(world, d.tipAnimator, "isTipping", true);
                s.tipping = true;
                s.chirp = CHIRP_DELAY;
                s.chirpFresh = true;
            }
        }
    }
}

void ChadJumps::update(World& world, float dt) {
    const Area& area = world.area;
    for (int c = 0; c < (int)chads_.size(); c++) {
        const ChadJumpDef& d = area.chadJumps[c];
        Chad& s = chads_[c];
        if (!d.tips || !s.started || !world.objects.activeInHierarchy(d.object) || s.fallen || !s.onRock) continue;
        if (s.timeTillNormal < 0) {
            // (Unity sets it every frame; only a tip sets it true while he's up, so once is the same.)
            if (s.tipping) world.animators.setBool(world, d.tipAnimator, "isTipping", false);
            s.tipping = false;
        } else {
            s.timeTillNormal -= dt;
        }
    }
}

void ChadJumps::coroutines(World& world, float dt) {
    const Area& area = world.area;
    for (int c = 0; c < (int)chads_.size(); c++) {
        const ChadJumpDef& d = area.chadJumps[c];
        Chad& s = chads_[c];
        if (!world.objects.activeInHierarchy(d.object)) { // an inactive object's coroutines stop
            s.jumping = false;
            s.fall = Chad::Fall::None;
            s.chirp = -1;
            continue;
        }
        if (s.jumping && !std::exchange(s.jumpFresh, false)) {
            s.jumpT += dt;
            if (s.jumpT < d.jumpDuration) {
                const float k = s.jumpT / d.jumpDuration;
                place(world, c, mathf::lerp(d.startX, d.jumpX, d.xCurve.eval(k)), mathf::lerp(d.startY, d.jumpY, d.yCurve.eval(k)));
            } else {
                place(world, c, d.jumpX, d.jumpY);
                s.jumping = false;
                s.state = State::Jumped;
                world.animators.setBool(world, d.animator, "isJumping", false);
                runCalls(world, d.onJumpFinish);
            }
        }
        if (s.fall == Chad::Fall::Waiting && !std::exchange(s.fallFresh, false)) {
            s.fallT -= dt;
            if (s.fallT <= 0) { // WaitForSeconds(0.5) is over: he falls
                world.animators.setBool(world, d.animator, "isFallen", true);
                world.animators.setBool(world, d.animator, "isTipping", false);
                s.fall = Chad::Fall::Moving;
                s.fallT = 0;
                place(world, c, d.jumpX, d.jumpY);
            }
        } else if (s.fall == Chad::Fall::Moving) {
            s.fallT += dt;
            if (s.fallT < d.fallDuration) {
                const float k = s.fallT / d.fallDuration;
                place(world, c, mathf::lerp(d.jumpX, d.fallX, k), mathf::lerp(d.jumpY, d.fallY, k));
            } else {
                place(world, c, d.fallX, d.fallY);
                s.fall = Chad::Fall::None;
                s.state = State::Fell;
                world.objects.setSortingOrder(d.renderer, 0);
                if (d.item >= 0) {
                    // The item drops a unit to his right (DropItem parents it to the STile there), its
                    // collider off until it lands (FinishFall).
                    float x, y;
                    world.objectPosition(d.object, x, y);
                    world.items.drop(world, d.item, x + 1, y, DropDone::ChadFinishFall, c);
                    if (area.items[d.item].colliderSlot >= 0) world.objects.setColliderEnabled(area.items[d.item].colliderSlot, false);
                }
            }
        }
        if (s.chirp >= 0 && !std::exchange(s.chirpFresh, false)) {
            s.chirp -= dt;
            if (s.chirp <= 0) {
                s.chirp = -1;
                world.npcs.typeCurrent(world, d.tipNpc);
            }
        }
    }
}

void ChadJumps::jumpStarter(World& world, int c) {
    const ChadJumpDef& d = world.area.chadJumps[c];
    Chad& s = chads_[c];
    if (s.state == State::Jumping) return;
    // Jump(), up to its first yield.
    s.state = State::Jumping;
    world.animators.setBool(world, d.animator, "isJumping", true);
    world.animators.setBool(world, d.animator, "isFallen", false);
    world.animators.setBool(world, d.animator, "isTipping", false);
    world.objects.setColliderEnabled(d.collider, false);
    world.objects.setSortingOrder(d.renderer, 1);
    place(world, c, mathf::lerp(d.startX, d.jumpX, d.xCurve.eval(0)), mathf::lerp(d.startY, d.jumpY, d.yCurve.eval(0)));
    s.jumping = true;
    s.jumpFresh = true;
    s.jumpT = 0;
}

void ChadJumps::startFall(World& world, int c) {
    const ChadJumpDef& d = world.area.chadJumps[c];
    Chad& s = chads_[c];
    // Fall(), up to its first yield.
    s.state = State::Falling;
    clearBools(world, d.animator);
    world.animators.setBool(world, d.animator, "isTipping", true);
    s.fall = Chad::Fall::Waiting;
    s.fallT = TIP_TIME;
    s.fallFresh = true;
}

void ChadJumps::finishFall(World& world, int c) {
    const ChadJumpDef& d = world.area.chadJumps[c];
    world.objects.setColliderEnabled(d.collider, true);
    if (d.item >= 0) {
        const ItemDef& it = world.area.items[d.item];
        if (it.colliderSlot >= 0) world.objects.setColliderEnabled(it.colliderSlot, true);
        if (it.spriteObject >= 0) world.objects.setSortingOrder(it.spriteObject, 0);
    }
}

void ChadJumps::place(World& world, int c, float x, float y) {
    const ChadJumpDef& d = world.area.chadJumps[c];
    float bx, by;
    world.grid.ownerBase(d.owner, bx, by);
    world.placeObject(d.object, d.owner, bx + x, by + y);
}

void ChadJumps::clearBools(World& world, int animator) {
    const AnimatorControllerDef& ctrl = world.area.animatorControllers[world.animators.controller(animator)];
    for (const std::string& p : ctrl.params) world.animators.setBool(world, animator, p, false);
}

} // namespace village
