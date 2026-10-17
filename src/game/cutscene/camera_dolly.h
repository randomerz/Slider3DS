// CameraDolly (a cutscene camera on a Cinemachine path): StartTrack fades in from black, stops the
// player (no moving, acting or pausing; the Artifact stays shut) and rolls the camera along the
// dolly's track for its duration (the brain cuts to it: no blend). A quarter second before the end
// it fades to black; once black, EndTrack fades back in on the player, who can move again.
// Pressing the action button shows the skip prompt; holding it holdToSkip s fades to black and ends
// it early (the prompt's bar fills by holdCurve; letting go empties it, the prompt stays).
#pragma once

#include <cstdint>

#include "game/data/area.h"

class World;

class CameraDollies {
public:
    void reset(); // none running
    // CameraDolly.StartTrack on Area::cameraDollies[dolly].
    void start(World& world, int dolly);
    // The action button went down while it runs (BindingHeldBehavior: a hold starts).
    void actionPressed();
    // The hold and the Rollercoaster coroutine: one frame (coroutine slot). `actionHeld`: the
    // button is down now.
    void update(World& world, float dt, bool actionHeld);
    // EndTrack (its fade to black ended: World::fadeDone).
    void end(World& world);

    bool running() const { return dolly_ >= 0; } // (it holds the player: LOCK_DOLLY)
    // Where the camera is while a dolly holds it.
    bool camera(const Area& area, float& x, float& y) const;
    // The skip prompt: shown once a hold started, its bar 0..1.
    bool promptShown() const { return running() && prompt_; }
    float promptFill() const { return fill_; }

private:
    int dolly_ = -1;
    float t_ = 0;           // Rollercoaster's t
    bool rolling_ = false;  // the coroutine runs (moves the camera)
    bool fadingOut_ = false; // its FadeToBlack started
    uint32_t since_ = 0;    // World::frame it started (its loop resumes from the next)
    float camT_ = 0;        // the track time the camera was last put at
    bool prompt_ = false, holding_ = false;
    float held_ = 0, fill_ = 0;
};
