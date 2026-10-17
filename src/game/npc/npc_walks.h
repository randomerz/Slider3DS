// NPC walks (NPCWalkingContext): NPC.StartWalkAtIndex / StartValidWalk start one of the current
// conditional's walks (NpcWalkDef); the NPC walks its path at its speed, point to point, facing the
// way it goes ("isWalking" set on its Animator), and is parented to each point it reaches (when it
// steps onto the next crossing's tile, to that tile). Sliding a tile of a crossing still ahead
// breaks the walk: the NPC stops (or, with teleportIfInterrupted, jumps to the end and finishes);
// a stopped walk resumes when a tile arrives and its crossings hold again. One walk at a time.
// A hidden NPC's walk waits (Unity stops its coroutine for good; no Village walk is hidden).
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"
#include "game/world/wait.h"

class World;

class NpcWalks {
public:
    void reset(const Area& area);
    // NPC.StartWalkAtIndex: ignored if the index isn't one of the current conditional's walks or a
    // walk is already on (Unity logs it).
    void start(World& world, int npc, int walk);
    // NPC.StartValidWalk: once no tile is moving, the current conditional's first walk whose
    // crossings hold (none: nothing).
    void startValid(World& world, int npc);
    // SGridAnimator.OnSTileMoveStart / OnSTileMoveEnd: breaks or resumes walks.
    void gridEvents(World& world, const std::vector<GridEvent>& events);
    // The walk coroutines: a step each frame (from the frame after a walk starts or resumes).
    void coroutines(World& world, float dt);

    bool walking(int npc) const { return s_[npc].walking; } // NPC.IsWalking

private:
    // A path point: an object (wherever it is now), or a spot (`owner` STile + offset from its
    // center) where the NPC stood when it crossed onto a tile or resumed (SetPathStartToCurrNPCPos).
    struct Point {
        int object = -1;
        int owner = 0;
        float x = 0, y = 0;
    };
    struct State {
        int cond = -1, walk = -1;  // currWalk (a walk of conditional `cond`), -1 = none
        bool walking = false;      // isWalking (false while a broken walk waits to resume)
        Point from;                // remainingPath[0]
        int next = 0;              // remainingPath[1] = path[next]; the path is done at its end
        int crossing = 0;          // remainingStileCrossings = crossings[crossing..]
        bool inSegment = false;    // between `from` and path[next]: t, rate
        float t = 0, rate = 0;     // lerp progress, and per second (speed / the segment's length; -1: one step)
        uint32_t stepFrame = 0;    // World::frame of the last step (one per frame)
        int under = 0;             // CurrentSTileUnderneath (islandId, 0 = none)
        WaitUntil tilesStill;      // StartValidWalk's WaitForTilesToStopMoving
    };

    const NpcWalkDef& def(const Area& area, int npc) const;
    void startFirstValid(World& world, int npc);
    void run(World& world, int npc, float dt); // DoCurrentWalk, from its last yield to the next
    void step(World& world, int npc, float dt);
    void interrupt(World& world, int npc);     // InterruptWalk
    void resume(World& world, int npc);        // ResumeWalk
    void finish(World& world, int npc);        // FinishWalk
    void pointPosition(const World& world, const Point& p, float& x, float& y) const;
    Point here(const World& world, int npc) const; // a spot where the NPC is now
    void setWalking(World& world, int npc, bool walking);

    std::vector<State> s_;
};
