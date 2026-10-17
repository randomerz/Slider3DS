#include "game/cutscene/camera_dolly.h"

#include "game/world/world.h"

namespace {

constexpr float END_FADE_LEAD = 0.25f; // Rollercoaster fades to black this long before the end (s)

} // namespace

void CameraDollies::reset() { *this = CameraDollies(); }

void CameraDollies::start(World& world, int dolly) {
    if (dolly < 0 || dolly >= (int)world.area.cameraDollies.size()) return;
    *this = CameraDollies();
    dolly_ = dolly;
    rolling_ = true;
    since_ = world.frame;
    world.effects.fadeFromBlack(); // (the camera is at the track's start this frame: t = 0)
}

void CameraDollies::actionPressed() {
    if (!running()) return;
    prompt_ = holding_ = true; // InitializeSkipPrompt
    held_ = fill_ = 0;
}

void CameraDollies::update(World& world, float dt, bool actionHeld) {
    if (!running()) return;
    const CameraDollyDef& d = world.area.cameraDollies[dolly_];
    // ICheckForBindingHeld: the bar follows the hold; letting go empties it, a full hold skips.
    if (holding_) {
        held_ += dt;
        fill_ = d.holdCurve.eval(held_ / d.holdToSkip);
        if (!actionHeld) {
            holding_ = false;
            fill_ = 0;
        } else if (held_ >= d.holdToSkip) {
            holding_ = false;
            world.effects.fadeToBlack(FadeDone::DollyEnd); // SkipToEndOfTrack: the camera stops where it is
            rolling_ = false;
        }
    }
    // Rollercoaster, from the frame after StartTrack.
    if (!rolling_ || world.frame == since_) return;
    t_ += dt;
    if (!fadingOut_ && t_ >= d.duration - END_FADE_LEAD) {
        fadingOut_ = true;
        world.effects.fadeToBlack(FadeDone::DollyEnd);
    }
    if (t_ < d.duration)
        camT_ = t_;
    else
        rolling_ = false; // (the camera holds the end until the fade's over)
}

void CameraDollies::end(World& world) {
    if (!running()) return;
    world.effects.fadeFromBlack();
    *this = CameraDollies(); // the player moves again, the camera cuts back to it, the prompt goes
}

bool CameraDollies::camera(const Area& area, float& x, float& y) const {
    if (!running()) return false;
    area.cameraDollies[dolly_].at(camT_, x, y);
    return true;
}
