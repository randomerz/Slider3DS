// Dev-build controls (SLIDER_DEV builds only; see main.cpp's header for the button map). They work
// only with Settings::cheats on (the pause menu's Cheats page), so a stray press does nothing.
#pragma once

#include "game/save/settings.h"
#include "game/world/cheats.h"
#include "platform/platform.h"

struct DevControls {
    int perfPage = 0; // SELECT: the perf overlay's page, cycling (0 off, then DevOverlay::Page + 1)

    // On every screen (title, pause menu, game): SELECT cycles the overlay; L + SELECT (`inGame`)
    // toggles noclip.
    void update(const plat::Input& in, const Settings& settings, Cheats& cheats, bool inGame);
    // The overlay's page showing, 0 = none (cheats off: none).
    int shownPage(const Settings& settings) const { return settings.cheats ? perfPage : 0; }
};
