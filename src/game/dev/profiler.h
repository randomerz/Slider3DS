// A frame's CPU time by section, for the dev overlay. A ProfScope marks a section's code; scopes
// nest, and each section counts only its own time (an inner scope pauses the outer one), so the
// sections add up to the time spent inside scopes. Always compiled: a clock read per scope edge, a
// handful per frame (release builds just never show it). Single-threaded: the main loop's only.
#pragma once

#include <cstdint>

enum class ProfSection : uint8_t {
    Logic,   // the frame's update outside the sections below (input, frontend, scripts)
    Physics, // the player's collision + trigger contacts (with the calls they run)
    Npcs,    // NPCs: conditionals, dialogue + voices, walks
    Anim,    // the Animators
    Fx,      // particles
    Audio,   // Audio's main-loop part: handing the cues over (all of it without an audio thread)
    Count
};

namespace prof {

void beginFrame();          // zeroes the frame's times (call before the frame's first scope)
float ms(ProfSection s);    // this frame's time in `s` so far

} // namespace prof

class ProfScope {
public:
    explicit ProfScope(ProfSection s);
    ~ProfScope();
    ProfScope(const ProfScope&) = delete;
    ProfScope& operator=(const ProfScope&) = delete;

private:
    int outer_; // the enclosing scope's section (-1: none)
};
