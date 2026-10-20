// Dev-build controls (SLIDER_DEV builds only; see main.cpp's header for the button map).
#pragma once

#include <cstddef>

#include "game/world/world.h"
#include "platform/platform.h"

struct DevControls {
    bool noclip = false;       // X: walk through everything (shows the position point)
    bool showInactive = false; // Y: draw uncollected STiles too (L + Y: the ruins, one slide from assembled)
    int perfPage = 1;          // SELECT: the perf overlay's page, cycling (0 off, then DevOverlay::Page + 1)
    bool collected = false;    // B: tiles 2-8 collected (L + B: only 2-4, next to Kevin's knot)
    size_t spawnIndex = 0;     // R: the spawn last jumped to (L + R: Slider 9, before / after the shuffle)

    // Handles this frame's dev buttons. Returns true while L is held: the D-pad then slides the
    // player's tile instead of walking (and B sets up Kevin's knot, Y the ruins).
    bool update(const plat::Input& in, World& world);
};
