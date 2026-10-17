// The cat's abduction (CatAbductionController, with its SinWaveAnimation and CatBubbleController):
// 6 s after the intro dolly ends, or 3 s after the player picks the cat up (picking it up restarts
// the 6 s), the cat is taken from the player, wakes in a bubble (camera shake), drifts 2 s onto a
// sine wave and rides it east over the water (faster while the player is ahead of it) to the beach,
// where the bubble pops and it drops onto its landing spot and falls back asleep 1 s later. 2.4 s in,
// the Exploresse's sequence starts (villageCatFloatAway). Once the intro has been seen, a scene start
// finds the cat on the beach (Start).
// Not ported: its sounds, the bubble's MiniSparkle and the landing's SmokePoof particles, the Alien
// Log Sign (it needs the Boating License, which the Village doesn't have).
#pragma once

#include <cstdint>

#include "game/world/wait.h"

class World;

namespace village {

class CatAbduction {
public:
    void reset() { *this = CatAbduction(); }
    void start(World& world); // CatAbductionController.Start
    void introFinished() { introFinished_ = true; } // SetIntroCutsceneFinished (the intro dolly's end)
    void pickedUp();                                // SetPickedUp (the cat's OnPickUp)
    // Update (the countdowns) and the SinWaveAnimation's Update.
    void update(World& world, float dt);
    // AbductCat and DoExploresseSequence (coroutine slot).
    void coroutines(World& world, float dt);

    bool started() const { return started_; }
    bool landed() const { return step_ == Step::Done; } // asleep again on the beach

private:
    enum class Step : uint8_t { None, Waking, ToWave, OverWater, Dropping, Settling, Done };
    void startAbducting(World& world);
    void placeCat(World& world, float x, float y);
    void lerpCat(World& world, float toX, float toY, float duration, float dt); // one loop step

    bool introFinished_ = false, picked_ = false, started_ = false;
    float sinceIntro_ = 0, sincePicked_ = 0;
    Step step_ = Step::None;
    Wait wait_, exploresse_;
    WaitUntil overWater_;
    float t_ = 0, fromX_ = 0, fromY_ = 0; // the current lerp: time, start
    float catX_ = 0, catY_ = 0;           // where the coroutine last put the cat (world)
    // The SinWaveAnimation: off until the cat rides it.
    bool sinOn_ = false;
    float sinVelocity_ = 0;
    float sinOrigX_ = 0, sinOrigY_ = 0; // origOffset (world: its parent never moves)
    float sinX_ = 0, sinY_ = 0;         // its position
};

} // namespace village
