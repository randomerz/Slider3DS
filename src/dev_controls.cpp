#include "dev_controls.h"

#include "game/dev/dev_overlay.h"

void DevControls::update(const plat::Input& in, const Settings& settings, Cheats& cheats, bool inGame) {
    if (!settings.cheats || !(in.pressed & plat::BTN_SELECT)) return;
    if (!(in.held & plat::BTN_L)) perfPage = (perfPage + 1) % (DevOverlay::PAGES + 1);
    else if (inGame) cheats.noclip = !cheats.noclip; // (the pause menu's NoClip)
}
