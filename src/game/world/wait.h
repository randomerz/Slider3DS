// Coroutine waits on World's frame clock (World::frame, advanced at the end of World::update):
// WaitForSeconds / WaitUntil count from the frame after the one they start in, whether they start
// before the coroutine slot (an event this frame), inside it (a chained wait) or between frames.
// New coroutine ports use these; older ones (chad_jump, npcs, the typer) still carry "fresh" flags.
#pragma once

#include <cstdint>

// WaitForSeconds.
struct Wait {
    float left = -1;    // s still to wait, < 0 = not waiting
    uint32_t since = 0; // World::frame when it started

    void start(float seconds, uint32_t frame) {
        left = seconds;
        since = frame;
    }
    void stop() { left = -1; }
    bool waiting() const { return left >= 0; }
    // One frame of it: true in the frame it runs out (then it's stopped). Not in the frame it started.
    bool done(uint32_t frame, float dt) {
        if (left < 0 || since == frame) return false;
        left -= dt;
        if (left > 0) return false;
        left = -1;
        return true;
    }
};

// WaitUntil: the condition is first checked the frame after it starts.
struct WaitUntil {
    bool active = false;
    uint32_t since = 0;

    void start(uint32_t frame) {
        active = true;
        since = frame;
    }
    // Evaluating its condition this frame (a predicate with side effects runs only then).
    bool polling(uint32_t frame) const { return active && since != frame; }
    // True (and stopped) once `condition` holds on a frame after the start.
    bool done(uint32_t frame, bool condition) {
        if (!active || since == frame || !condition) return false;
        active = false;
        return true;
    }
};
