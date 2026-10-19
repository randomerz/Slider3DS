// A frame's CPU time by section, for the dev overlay. A ProfScope marks a section's code; scopes
// nest, and each section counts only its own time (an inner scope pauses the outer one). Always
// compiled: a clock read per scope edge, a handful per frame (release builds just never show it).
// Single-threaded: the main loop's only — never open a scope on the audio thread.
// Another thread on the main thread's core (the Old 3DS's audio thread) runs while a scope is open
// and would count as that scope's: with setStolenTime, its busy time is taken out of whatever it
// interrupted and charged to AudioThread instead (between scopes too), so the sections add up to
// the core's busy time in the scopes plus the other thread's. A span longer than MAX_STOLEN_SPAN_MS
// (back from HOME / sleep, a long load) charges nothing stolen: the other thread's log doesn't
// reach that far back.
#pragma once

#include <cstdint>

enum class ProfSection : uint8_t {
    Logic,   // the frame's update outside the sections below (input, frontend, scripts)
    Physics, // the player's collision + trigger contacts (with the calls they run)
    Npcs,    // NPCs: conditionals, dialogue + voices, walks
    Anim,    // the Animators
    Fx,      // particles
    Audio,   // Audio's main-loop part: handing the cues over (all of it without an audio thread)
    Draw,    // building the frame's draws (plat::beginFrame / endFrame excluded: their waits, the PC's pacing)
    Dev,     // the dev overlay drawing itself (inside Draw)
    FrameEnd, // plat::endFrame: submitting the frame (citro2d flushes its last batch there)
    AudioThread, // the audio thread's time taken from this core (setStolenTime; 0 on its own core)
    Count
};

namespace prof {

constexpr double MAX_STOLEN_SPAN_MS = 100;

void beginFrame();          // zeroes the frame's times (call before the frame's first scope)
float ms(ProfSection s);    // this frame's time in `s` so far
// `busyBetween(t0, t1, user)`: ms another thread on this core was busy within [t0, t1] (plat::nowMs
// times); nullptr: none.
void setStolenTime(double (*busyBetween)(double t0, double t1, void* user), void* user);

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
