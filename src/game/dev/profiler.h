// A frame's CPU time by section, for the dev overlay. A ProfScope marks a section's code; scopes
// nest, and each section counts only its own time (an inner scope pauses the outer one). Always
// compiled, but off unless setEnabled (main: while the dev overlay shows; never in release): then a
// scope does nothing. On, each edge is a clock read and a logged interval — a few dozen to ~150 a
// frame (AreaView's scope per tile layer) — resolved only when ms() is read (after the frame,
// outside every scope, so the resolving never counts toward a section).
// Single-threaded: the main loop's only — never open a scope on the audio thread.
// Another thread on the main thread's core (the Old 3DS's audio thread) runs while a scope is open
// and would count as that scope's: with setStolenTime, its busy time is taken out of whatever it
// interrupted and charged to AudioThread instead (between scopes too), so the sections add up to
// the core's busy time in the scopes plus the other thread's (the hook set as ms() is read is the
// one used). A span longer than MAX_STOLEN_SPAN_MS
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
    Draw,    // building the frame's draws, but for the Draw* sections below (plat::beginFrame / endFrame
             // excluded: Wait, FrameEnd) — the title screen, the top overlays, starting the screens
    DrawList,    // AreaView: gathering what to draw this frame (layers, sprites, text, particles...)
    DrawSort,    // AreaView: sorting it (Unity's order)
    DrawTiles,   // AreaView: the tile layers' cells
    DrawObjects, // AreaView: everything else it draws (sprites, text, lines, particles)
    DrawBottom,  // the bottom screen (the Artifact, the menus)
    Dev,     // the dev overlay drawing itself (inside Draw)
    FrameEnd, // plat::endFrame: submitting the frame (citro2d flushes its last batch there)
    AudioThread, // the audio thread's time taken from this core (setStolenTime; 0 on its own core)
    Wait,    // plat::beginFrame: waiting on the display's pace and the GPU's last frame (the PC's pacing)
    Count
};

namespace prof {

constexpr double MAX_STOLEN_SPAN_MS = 100;

void setEnabled(bool enabled); // (off: scopes do nothing; set between frames, never inside a scope)
void beginFrame();          // zeroes the frame's times (call before the frame's first scope)
float ms(ProfSection s);    // this frame's time in `s` so far (resolves what the scopes logged)
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
    int outer_;   // the enclosing scope's section (-1: none)
    bool active_; // the profiler was on as it opened
};
