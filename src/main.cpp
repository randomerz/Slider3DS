// Slider (Stonybrook Village) for the 3DS: boots to the title screen; the screens, the game and the
// save points between them are game/app/frontend's. Here: the platform, the assets and the dev tools.
//   Title (bottom screen): Continue / Play (three profiles, delete, a new one's name) / Options / Quit
//   D-pad / circle pad: walk          A: action (pick up / drop items, doors, NPC dialogue)
//   START: pause (game/ui/pause_menu: Resume, Options (General, Sound), Quit to the title)
//   Cutscenes (the intro's camera dolly): hold A to skip
//   Bottom screen in the game: the Artifact (tap a tile, then a highlighted slot, or drag it there)
// The new-game start is the Default spawn inside tile 1's house. The player's Settings live here,
// loaded at start (the Frontend saves them when they change). Audio (game/audio) plays what the
// Frontend asks each frame, at the Settings' volumes.
// Dev builds (SLIDER_DEV=1: plain `make`, PC default; dev_controls.cpp) add a perf overlay
// (game/dev/dev_overlay; SELECT toggles it), R on the title screen (boot the Village directly,
// outdoors on tile 1, on a temporary profile that is never written) and in the game:
//   R: jump to the next spawn point                 X: toggle noclip (shows the position point)
//   Y: toggle drawing inactive STiles               B: toggle collecting tiles 2-8
//   L + D-pad: slide the tile you stand on
//   L + B: tiles 2-4 only, next to Kevin's unsolved knot (Slider 5)
//   L + Y: tiles 2-6 only, the ruins one slide from assembled (slide 2 left; Slider 7)
#include <cstdio>
#include <string>

#include "dev_controls.h"
#include "game/app/frontend.h"
#include "game/audio/audio.h"
#include "game/data/area.h"
#include "game/dev/dev_overlay.h"
#include "game/render/area_view.h"
#include "game/render/font.h"
#include "game/render/sprite_sheets.h"
#include "game/save/settings.h"
#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"
#include "game/ui/title_screen.h"
#include "platform/platform.h"

namespace {

#ifndef SLIDER_DEV
#define SLIDER_DEV 0
#endif
constexpr bool DEV = SLIDER_DEV != 0;

// Shows a red top screen until START (the area failed to load; the reason is on the console).
void showLoadError() {
    while (plat::mainLoop() && !(plat::pollInput().pressed & plat::BTN_START)) {
        plat::beginFrame();
        plat::beginScreen(plat::Screen::Top, plat::rgba(0x80, 0x20, 0x20));
        plat::beginScreen(plat::Screen::Bottom, palette::DARK);
        plat::endFrame();
    }
}

} // namespace

int main(int, char**) {
    if (!plat::init()) return 1;

    Area area;
    SpriteSheets sheets;
    Font font;
    MenuSkin menuSkin;
    TitleArt titleArt;
    Audio audio;
    std::string error;
    if (!loadArea("village/village.json", area, &error) || !sheets.load(area) || !font.load("fonts/dialogue.json", &error) ||
        !menuSkin.load(&error) || !titleArt.load(&error) || !audio.load(&error)) {
        printf("Failed to load village: %s\n", error.c_str());
        showLoadError();
        plat::shutdown();
        return 1;
    }

    Settings settings = Settings::load();
    Frontend front(area, titleArt, settings);
    AreaView view;
    view.setFont(&font);
    DevControls dev;
    dev.showPerf = DEV;
    if (const int start = front.world().startSpawn(); start >= 0) dev.spawnIndex = start;
    DevOverlay overlay;

    while (plat::mainLoop()) {
        const double updateStart = plat::nowMs();
        const plat::Input in = plat::pollInput();
        bool devSliding = false;
        if (DEV && front.mode() == Frontend::Mode::Title && !front.pauseMenu().isOpen() && (in.pressed & plat::BTN_R)) {
            front.bootDirect(); // (outdoors on tile 1)
            float x, y;
            front.world().grid.ownerBase(1, x, y);
            front.world().placeNear(x, y);
        } else {
            if (DEV && front.playing()) devSliding = dev.update(in, front.world());
            float moveX = 0, moveY = 0;
            if (!devSliding) {
                if (in.held & plat::BTN_LEFT) moveX -= 1;
                if (in.held & plat::BTN_RIGHT) moveX += 1;
                if (in.held & plat::BTN_UP) moveY += 1;
                if (in.held & plat::BTN_DOWN) moveY -= 1;
            }
            if (!front.update(in, moveX, moveY, DEV && dev.noclip)) break;
        }
        audio.setVolumes(settings);
        audio.setPaused(front.audioPaused());
        audio.setListener(front.listener());
        audio.run(front.audio().take());
        audio.pump();
        const float updateMs = (float)(plat::nowMs() - updateStart);

        plat::beginFrame();
        const bool inGame = front.mode() == Frontend::Mode::Game;
        front.drawTop(view, sheets, menuSkin, DEV && dev.showInactive);
        if (inGame && DEV && dev.noclip) // the position point the player moves by (the camera follows it)
            plat::drawRect(plat::TOP_W / 2 - 1, plat::TOP_H / 2 - 1, 2, 2, plat::rgba(0xe0, 0xa0, 0x40));
        front.drawBottom(sheets, menuSkin);
        if (DEV) {
            overlay.record(plat::lastFrameStats(), updateMs, inGame ? view.lastSpriteCount() : 0);
            if (dev.showPerf) overlay.draw();
        }
        plat::endFrame();
    }
    front.shutdown();

    audio.unload();
    titleArt.unload();
    menuSkin.unload();
    font.unload();
    sheets.unload();
    plat::shutdown();
    return 0;
}
