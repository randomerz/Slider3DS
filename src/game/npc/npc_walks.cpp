#include "game/npc/npc_walks.h"

#include <cmath>

#include "game/events/events.h"
#include "game/world/world.h"

namespace {

// STileCrossing.CrossingIsValid for crossings[first..]: `slotOf(islandId, x, y)` finds a tile's slot.
template <class SlotOf>
bool crossingsHold(const std::vector<NpcCrossingDef>& crossings, int first, SlotOf slotOf) {
    for (int i = first; i < (int)crossings.size(); i++) {
        const NpcCrossingDef& c = crossings[i];
        int fx = c.fromX, fy = c.fromY, tx = c.toX, ty = c.toY;
        if (c.from != 0) slotOf(c.from, fx, fy);
        if (c.to != 0) slotOf(c.to, tx, ty);
        bool ok = false;
        for (const auto& d : c.dirs) ok = ok || (tx - fx == d.first && ty - fy == d.second);
        if (!ok) return false;
    }
    return true;
}

// Lerp progress per second along a segment; -1 = done after one step (a point repeated, as in
// Chad's path: Unity's rate is infinite).
float rateFor(float speed, float length) { return length > 0 ? speed / length : -1; }

// The slots as a GridEvent carries them.
auto slotIn(const std::vector<int>& slots, int width) {
    return [&slots, width](int id, int& x, int& y) {
        for (int i = 0; i < (int)slots.size(); i++)
            if (slots[i] == id) {
                x = i % width;
                y = i / width;
            }
    };
}

} // namespace

void NpcWalks::reset(const Area& area) { s_.assign(area.npcs.size(), State()); }

const NpcWalkDef& NpcWalks::def(const Area& area, int npc) const {
    const State& st = s_[npc];
    return area.npcs[npc].conds[st.cond].walks[st.walk];
}

void NpcWalks::setWalking(World& world, int npc, bool walking) {
    s_[npc].walking = walking;
    const int animator = world.area.npcs[npc].animator;
    if (animator >= 0) world.animators.setBool(world, animator, "isWalking", walking);
}

void NpcWalks::pointPosition(const World& world, const Point& p, float& x, float& y) const {
    if (p.object >= 0) {
        world.objectPosition(p.object, x, y);
        return;
    }
    world.grid.ownerBase(p.owner, x, y);
    x += p.x;
    y += p.y;
}

NpcWalks::Point NpcWalks::here(const World& world, int npc) const {
    const int object = world.area.npcs[npc].object;
    Point p;
    p.owner = world.objects.owner(object);
    float x, y, bx, by;
    world.objectPosition(object, x, y);
    world.grid.ownerBase(p.owner, bx, by);
    p.x = x - bx;
    p.y = y - by;
    return p;
}

void NpcWalks::start(World& world, int npc, int walk) {
    const NpcDef& n = world.area.npcs[npc];
    const int cond = world.npcs.currentCond(npc);
    if (n.conds.empty() || walk < 0 || walk >= (int)n.conds[cond].walks.size()) return;
    State& st = s_[npc];
    if (st.walk >= 0) return; // ("Did not start NPC walk because one was already occurring!")
    st.cond = cond;
    st.walk = walk;
    const NpcWalkDef& w = def(world.area, npc);
    st.from = Point{w.path[0]};
    st.next = 1;
    st.crossing = 0;
    st.inSegment = false;
    float x, y;
    world.objectPosition(n.object, x, y);
    st.under = world.grid.stileUnder(x, y, world.objects.owner(n.object));
    setWalking(world, npc, true);
    runCalls(world, w.onStarted); // (its first step: this frame's coroutine slot)
}

void NpcWalks::startValid(World& world, int npc) {
    if (world.grid.moving())
        s_[npc].tilesStill.start(world.frame);
    else
        startFirstValid(world, npc);
}

void NpcWalks::startFirstValid(World& world, int npc) {
    const NpcDef& n = world.area.npcs[npc];
    if (n.conds.empty()) return;
    const std::vector<NpcWalkDef>& walks = n.conds[world.npcs.currentCond(npc)].walks;
    const Grid& grid = world.grid;
    auto slotNow = [&grid](int id, int& x, int& y) {
        if (const TileState* t = grid.tile(id)) {
            x = t->x;
            y = t->y;
        }
    };
    for (int i = 0; i < (int)walks.size(); i++)
        if (crossingsHold(walks[i].crossings, 0, slotNow)) {
            start(world, npc, i);
            return;
        }
}

void NpcWalks::run(World& world, int npc, float dt) {
    State& st = s_[npc];
    const NpcWalkDef& w = def(world.area, npc);
    const int object = world.area.npcs[npc].object;
    for (;;) {
        if (!st.inSegment) {
            if (st.next >= (int)w.path.size()) {
                finish(world, npc);
                return;
            }
            float ax, ay, bx, by;
            pointPosition(world, st.from, ax, ay);
            world.objectPosition(w.path[st.next], bx, by);
            const float length = std::hypot(bx - ax, by - ay);
            st.rate = rateFor(world.area.npcs[npc].speed, length);
            st.t = 0;
            st.inSegment = true;
        }
        if (st.t < 1) {
            step(world, npc, dt);
            return; // (WaitForEndOfFrame)
        }
        // The segment's end: on the point, and parented to it.
        const int point = w.path[st.next];
        world.objects.moveOnto(world.area, object, point);
        st.from = Point{point};
        st.next++;
        st.inSegment = false;
    }
}

void NpcWalks::step(World& world, int npc, float dt) {
    State& st = s_[npc];
    const NpcDef& n = world.area.npcs[npc];
    const NpcWalkDef& w = def(world.area, npc);
    float ax, ay, bx, by, x, y;
    pointPosition(world, st.from, ax, ay);
    world.objectPosition(w.path[st.next], bx, by);
    // CurrentSTileUnderneath is FixedUpdate's: where the NPC was before this step.
    world.objectPosition(n.object, x, y);
    st.under = world.grid.stileUnder(x, y, st.under);
    x = ax + (bx - ax) * st.t;
    y = ay + (by - ay) * st.t;
    world.placeObject(n.object, world.objects.owner(n.object), x, y);
    st.t = st.rate < 0 ? 1 : st.t + st.rate * dt;
    // UpdateSpriteFacingDir: flipped when going against its default facing (straight up / down
    // keeps the facing).
    if (n.sprite >= 0) {
        const float dot = (bx - ax) * (n.facingLeft ? -1.0f : 1.0f);
        world.objects.setFlipX(n.sprite, world.objects.flipX(n.sprite) ? dot <= 0 : dot < 0);
    }
    // UpdateSTileCrossings: on the next crossing's tile, parented to it; the segment starts over from here.
    if (st.crossing < (int)w.crossings.size() && w.crossings[st.crossing].to == st.under) {
        st.crossing++;
        world.objects.setParent(world.area, n.object, -1);
        world.placeObject(n.object, st.under, x, y);
        st.from = here(world, npc);
        st.t = 0;
        st.rate = rateFor(n.speed, std::hypot(bx - x, by - y));
    }
}

void NpcWalks::interrupt(World& world, int npc) {
    const NpcWalkDef& w = def(world.area, npc);
    setWalking(world, npc, false);
    runCalls(world, w.onBroken);
    if (w.teleportIfInterrupted) {
        world.npcs.teleport(world, npc, w.path.back());
        finish(world, npc);
    }
}

void NpcWalks::resume(World& world, int npc) {
    State& st = s_[npc];
    runCalls(world, def(world.area, npc).onResumed);
    st.from = here(world, npc);
    st.inSegment = false;
    // Port fix: Unity's ResumeWalk leaves isWalking off, so later slides can't break the walk and
    // the next tile arriving starts it a second time. (No Village walk resumes: Romeo's, the only
    // one with crossings, teleports instead.)
    setWalking(world, npc, true);
}

void NpcWalks::finish(World& world, int npc) {
    State& st = s_[npc];
    const NpcWalkDef& w = def(world.area, npc);
    const NpcDef& n = world.area.npcs[npc];
    if (w.turnAround && n.sprite >= 0) world.objects.setFlipX(n.sprite, !world.objects.flipX(n.sprite));
    st.cond = st.walk = -1;
    st.inSegment = false;
    setWalking(world, npc, false);
    runCalls(world, w.onFinished); // (after the walk is over: it may start the next)
}

void NpcWalks::gridEvents(World& world, const std::vector<GridEvent>& events) {
    // Event by event (as Unity delivers them): a walk started by an earlier event's handlers hears
    // only what comes after.
    for (const GridEvent& e : events) {
        for (int i = 0; i < (int)s_.size(); i++) {
            State& st = s_[i];
            if (st.walk < 0 || !world.objects.activeInHierarchy(world.area.npcs[i].object)) continue; // (OnDisable unsubscribed)
            const NpcWalkDef& w = def(world.area, i);
            if (e.kind == GridEvent::Kind::TileMoveStart && st.walking) {
                // PathBrokenAfterSTileMoved: the tile is in a crossing still ahead.
                for (int c = st.crossing; c < (int)w.crossings.size(); c++)
                    if (w.crossings[c].from == e.islandId || w.crossings[c].to == e.islandId) {
                        interrupt(world, i);
                        break;
                    }
            } else if (e.kind == GridEvent::Kind::TileMoveEnd && !st.walking &&
                       crossingsHold(w.crossings, st.crossing, slotIn(e.slots, world.grid.width()))) {
                resume(world, i);
            }
        }
    }
}

void NpcWalks::coroutines(World& world, float dt) {
    for (int i = 0; i < (int)s_.size(); i++) {
        State& st = s_[i];
        if (!world.objects.activeInHierarchy(world.area.npcs[i].object)) continue;
        if (st.tilesStill.done(world.frame, !world.grid.moving())) startFirstValid(world, i);
        if (st.walking) run(world, i, dt);
    }
}
