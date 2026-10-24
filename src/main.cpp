// Slider (Stonybrook Village) for the 3DS: boots to the title screen; the screens, the game and the
// save points between them are game/app/frontend's. Here: the platform, the assets and the dev tools.
//   Title (bottom screen): Continue / Play (three profiles, delete, a new one's name) / Options / Quit
//   Circle Pad: walk                  A: action (pick up / drop items, doors, NPC dialogue)
//   START: pause (game/ui/pause_menu: Resume, Options (General, Sound, Cheats), Quit to the title)
//   Cutscenes (the intro's camera dolly): hold A to skip
//   Bottom screen in the game: the Artifact (tap a tile, then a highlighted slot, or drag it there;
//   or the D-pad's cursor and X for those taps)
// Boot: "Loading..." and a bar on the bottom screen between the asset groups (the menu skin first).
// The new-game start is the Default spawn inside tile 1's house. The player's Settings live here,
// loaded at start (the Frontend saves them when they change). Audio (game/audio) plays what the
// Frontend asks each frame, at the Settings' volumes.
// Noclip (the Cheats page's) shows the position point the player moves by.
// Dev builds (SLIDER_DEV=1: plain `make`, PC default; dev_controls.cpp) add, only while the Cheats
// page's Enable Cheats is on: a perf overlay (game/dev/dev_overlay; SELECT cycles its pages: FRAME,
// DRAW, off), L + SELECT in the game (toggle noclip), R on the title screen (boot the Village
// directly, outdoors on tile 1, on a temporary profile that is never written).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "dev_controls.h"
#include "game/app/frontend.h"
#include "game/audio/audio.h"
#include "game/data/area.h"
#include "game/dev/dev_overlay.h"
#include "game/dev/profiler.h"
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

// A boot frame while the assets load: both screens dark, and on the bottom one "Loading..." over a
// bar `progress` (0-1) full, in the menus' look (the menu skin loads first).
void showLoading(const MenuSkin& skin, float progress) {
    constexpr int S = MenuSkin::UI_SCALE, CX = plat::BOTTOM_W / S / 2, CY = plat::BOTTOM_H / S / 2; // (UI px)
    constexpr int BAR_W = 80, BAR_H = 2;
    plat::beginFrame();
    plat::beginScreen(plat::Screen::Top, palette::DARK);
    plat::beginScreen(plat::Screen::Bottom, palette::DARK);
    const char* text = "Loading...";
    skin.drawText(text, CX - skin.textWidth(text) / 2, CY - 6, palette::OFF_WHITE);
    const float x = (float)((CX - BAR_W / 2) * S), y = (float)((CY + 4) * S);
    plat::drawRect(x, y, (float)(BAR_W * S), (float)(BAR_H * S), palette::GRAY);
    plat::drawRect(x, y, std::floor(BAR_W * std::clamp(progress, 0.0f, 1.0f)) * S, (float)(BAR_H * S), palette::OFF_WHITE);
    plat::endFrame();
}

} // namespace

int main(int, char**) {
    // The boot's steps, timed for the dev overlay: s since the last lap.
    double lapStart = plat::nowMs();
    auto lap = [&lapStart]() {
        const double now = plat::nowMs();
        const float s = (float)(now - lapStart) / 1000;
        lapStart = now;
        return s;
    };
    if (!plat::init()) return 1;

    Area area;
    SpriteSheets sheets;
    Font font;
    MenuSkin menuSkin;
    TitleArt titleArt;
    EndArt endArt;
    Audio audio;
    std::string error;
    bool loaded = menuSkin.load(&error); // (first: the loading frames use it)
    const float initS = lap();
    if (loaded) showLoading(menuSkin, 0);
    loaded = loaded && loadArea("village/village.json", area, &error);
    const float areaS = lap();
    if (loaded) showLoading(menuSkin, 0.25f);
    loaded = loaded && sheets.load(area);
    const float sheetsS = lap();
    if (loaded) showLoading(menuSkin, 0.5f);
    loaded = loaded && font.load("fonts/dialogue.json", &error) && titleArt.load(&error) && endArt.load(&error);
    const float uiS = lap();
    if (loaded) showLoading(menuSkin, 0.75f);
    loaded = loaded && audio.load(&error);
    const float audioS = lap();
    if (!loaded) {
        printf("Failed to load village: %s\n", error.c_str());
        showLoadError();
        plat::shutdown();
        return 1;
    }

    audio.startThread(); // (mixing and decoding off the main loop, where the platform has a thread for it)
    Settings settings = Settings::load();
    Frontend front(area, titleArt, endArt, settings);
    const float gameS = lap(); // (the settings, the World, the save profiles' headers)
    AreaView view;
    view.setFont(&font);
    DevControls dev;
    DevOverlay overlay;
    overlay.setBoot(initS, areaS, sheetsS, uiS, audioS, gameS);
    // The audio thread on the game's core (an Old 3DS): its time comes out of the sections it
    // interrupts (elsewhere it runs alongside: nothing to take out). Only while the overlay shows.
    const bool audioSharesCore = DEV && audio.threadLoad().core == 0;

    while (plat::mainLoop()) {
        // The profiler runs only while the overlay shows (scopes are free otherwise; never in release).
        const int perfPage = DEV ? dev.shownPage(settings) : 0; // (this frame's: SELECT below changes the next)
        const bool profiling = perfPage != 0;
        prof::setEnabled(profiling);
        prof::setStolenTime(audioSharesCore && profiling ? &Audio::threadBusyCallback : nullptr, &audio);
        prof::beginFrame();
        { // (the update: Logic, with its sections nested; closes before the draw)
            const ProfScope logic(ProfSection::Logic);
            const plat::Input in = plat::pollInput();
            if (DEV) dev.update(in, settings, front.cheats(), front.playing()); // (the overlay's page: next frame on)
            if (DEV && settings.cheats && front.mode() == Frontend::Mode::Title && !front.pauseMenu().isOpen() &&
                (in.pressed & plat::BTN_R)) {
                front.bootDirect(); // (outdoors on tile 1)
                float x, y;
                front.world().grid.ownerBase(1, x, y);
                front.world().placeNear(x, y);
            } else {
                float moveX = 0, moveY = 0;
                if (in.held & plat::BTN_CPAD_LEFT) moveX -= 1; // (the D-pad is the Artifact's cursor)
                if (in.held & plat::BTN_CPAD_RIGHT) moveX += 1;
                if (in.held & plat::BTN_CPAD_UP) moveY += 1;
                if (in.held & plat::BTN_CPAD_DOWN) moveY -= 1;
                if (!front.update(in, moveX, moveY)) break;
            }
            const ProfScope sound(ProfSection::Audio);
            audio.setVolumes(settings);
            audio.setPaused(front.audioPaused());
            audio.setListener(front.listener());
            audio.run(front.audio().take());
            audio.pump();
        }

        {
            const ProfScope wait(ProfSection::Wait);
            plat::beginFrame();
        }
        const bool inGame = front.mode() == Frontend::Mode::Game;
        {
            const ProfScope draw(ProfSection::Draw);
            front.drawTop(view, sheets, menuSkin);
            if (inGame && front.noclip()) // the position point the player moves by (the camera follows it)
                plat::drawRect(plat::TOP_W / 2 - 1, plat::TOP_H / 2 - 1, 2, 2, plat::rgba(0xe0, 0xa0, 0x40));
            {
                const ProfScope bottom(ProfSection::DrawBottom);
                front.drawBottom(sheets, menuSkin);
            }
            if (perfPage != 0) {
                const ProfScope self(ProfSection::Dev);
                overlay.draw((DevOverlay::Page)(perfPage - 1)); // (the frames before this one)
            }
        }
        {
            const ProfScope end(ProfSection::FrameEnd);
            plat::endFrame();
        }
        if (profiling) // (hidden: nothing to record — the profiler is off, its times 0)
            overlay.record(plat::lastFrameStats(), inGame ? view.lastSpriteCount() : 0, inGame ? view.lastTileCount() : 0,
                                audio.threadLoad());
    }
    front.shutdown();

    audio.unload();
    titleArt.unload();
    endArt.unload();
    menuSkin.unload();
    font.unload();
    sheets.unload();
    plat::shutdown();
    return 0;
}
