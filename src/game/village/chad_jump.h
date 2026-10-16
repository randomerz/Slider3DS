// ChadJump + VillageChadJump (Chad, who holds the Ruins Fragment), ported step for step: his
// dialogue calls JumpStarter and he jumps onto a rock along two curves (OnJumpFinish then calls
// SetIsOnRock). Once he's up, OnSTileMoveEnd of his own STile makes him tip, fall off and drop the
// item next to him (FinishFall once it lands); while he's on the rock, other tiles' moves tip him
// for timeTipped seconds, and the first tip has him say something (TypeCurrentDialogue). Specs:
// ChadFell, DidSlideAfterJumping. Grid events come from Grid::events (World::sendGridEvents).
// Not yet: the "Fall" / "Hurt" sounds, ResetJump / EnableCollider (other areas call them).
#pragma once

#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"

class World;

namespace village {

class ChadJumps {
public:
    void reset(const Area& area); // a scene load: nothing awake, standing at the start
    // Awake / Start of what became active: ChadJump.Awake sets the item's collider (on only with
    // the Caves' Slider 3), VillageChadJump.Start reads villageCompletion. World calls it with Starts.
    void activate(World& world);
    // OnSTileMoveEnd for the active ones (OnEnable subscribed them).
    void gridEvents(World& world, const std::vector<GridEvent>& events);
    // VillageChadJump.Update: a tip wears off. Runs before the frame's grid events, as Unity's
    // Update comes before the coroutine that sends OnSTileMoveEnd.
    void update(World& world, float dt);
    // The coroutines (the jump, the fall, the chirp after a tip), after the scripts' updates.
    void coroutines(World& world, float dt);

    void jumpStarter(World& world, int c); // ChadJump.JumpStarter on Area::chadJumps[c]
    void setIsOnRock(int c) { chads_.at(c).onRock = true; } // VillageChadJump.SetIsOnRock
    bool fell(int c) const { return chads_.at(c).state == State::Fell; } // ChadJump.ChadFell
    bool slidAfterJumping(int c) const { return chads_.at(c).slid; }     // VillageChadJump.DidSlideAfterJumping

private:
    enum class State { Standing, Jumping, Jumped, Falling, Fell }; // ChadJump.JumpState
    struct Chad {
        bool awake = false, started = false;
        State state = State::Standing;
        // The coroutines (fresh: started this frame, so they resume from the next one).
        bool jumping = false, jumpFresh = false;
        float jumpT = 0;
        enum class Fall { None, Waiting, Moving } fall = Fall::None;
        bool fallFresh = false;
        float fallT = 0;          // Waiting: time left; Moving: time since it started
        float chirp = -1;         // DelayThenChirp: time left, -1 = none
        bool chirpFresh = false;
        // VillageChadJump
        bool onRock = false, fallen = false, tipping = false, slid = false;
        float timeTillNormal = 0;
    };

    void place(World& world, int c, float x, float y); // transform.localPosition (owner-relative)
    void clearBools(World& world, int animator);        // every bool parameter false
    void startFall(World& world, int c);
    void finishFall(World& world, int c);

    std::vector<Chad> chads_;
};

} // namespace village
