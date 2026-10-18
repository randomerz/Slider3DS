// Village walkabout: the player walks the exported Village with the original's collision; the
// camera follows. The new-game start is the Default spawn inside tile 1's house.
//   D-pad / circle pad: walk          A: action (pick up / drop items, doors, NPC dialogue)
//   START: pause (game/ui/pause_menu: Resume, Options (General, Sound), Quit)
//   Cutscenes (the intro's camera dolly): hold A to skip
// Dev builds (SLIDER_DEV=1: plain `make`, PC default; dev_controls.cpp) start outdoors on tile 1 and add:
//   R: jump to the next spawn point                 X: toggle noclip (shows the position point)
//   Y: toggle drawing inactive STiles               B: toggle collecting tiles 2-8
//   L + D-pad: slide the tile you stand on          SELECT: toggle the perf overlay
//   L + B: tiles 2-4 only, next to Kevin's unsolved knot (Slider 5)
//   L + Y: tiles 2-6 only, the ruins one slide from assembled (slide 2 left; Slider 7)
// Bottom screen: the Artifact (tap a tile, then a highlighted slot, or drag it there), or the pause
// menu. Dev builds add a perf overlay (game/dev/dev_overlay). The player's Settings live here,
// loaded at start and saved when they've changed: as the pause menu closes, and when the app exits.
// The save profile (game/save/profile) is saved as Unity's GameSaver / SaveSystem.SaveGame do: once
// the scene has started, when a script asks (a collectible's pick-up), and on quitting (the pause
// menu's Quit, the app closing) — unless the scene was booted directly (as now, until the title
// screen picks a slot): then, as in Unity's editor, it plays a temporary profile that is never written.
#include <cstdio>
#include <string>

#include "dev_controls.h"
#include "game/data/area.h"
#include "game/render/area_view.h"
#include "game/save/profile.h"
#include "game/save/settings.h"
#include "game/ui/artifact_ui.h"
#include "game/ui/hud.h"
#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"
#include "game/ui/pause_menu.h"
#include "game/dev/dev_overlay.h"
#include "game/render/font.h"
#include "game/render/sprite_sheets.h"
#include "game/world/world.h"
#include "platform/platform.h"

namespace {

constexpr float FRAME_DT = 1.0f / 60.0f;
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

// Writes `settings` if they differ from what was last saved (one attempt per change).
void saveIfChanged(const Settings& settings, Settings& saved) {
    if (settings == saved) return;
    if (!settings.save()) printf("Couldn't save the settings\n");
    saved = settings;
}

// Writes the game into `profile` and its slot (-1: a temporary profile, never written).
void saveGame(const World& world, SaveProfile& profile, int slot) {
    if (slot < 0) return;
    world.save(profile);
    if (!profile.save(slot)) printf("Couldn't save the profile\n");
}


} // namespace

int main(int, char**) {
    if (!plat::init()) return 1;

    Area area;
    SpriteSheets sheets;
    Font font;
    MenuSkin menuSkin;
    std::string error;
    if (!loadArea("village/village.json", area, &error) || !sheets.load(area) || !font.load("fonts/dialogue.json", &error) ||
        !menuSkin.load(&error)) {
        printf("Failed to load village: %s\n", error.c_str());
        showLoadError();
        plat::shutdown();
        return 1;
    }

    Settings settings = Settings::load();
    Settings savedSettings = settings;
    World world(area, settings);
    AreaView view;
    view.setFont(&font);
    ArtifactUI artifactUI;
    artifactUI.init(area, world.artifact);
    DevControls dev;
    dev.showPerf = DEV;
    DevOverlay overlay;
    PauseMenu pauseMenu;
    // The scene booted directly: a temporary profile (SaveSystem.Current's "Boomo", index -1).
    const int slot = -1;
    SaveProfile profile = SaveProfile::make("Boomo");
    world.load(profile); // (a new game at the start)
    if (const int start = world.placeAtStart(); start >= 0) dev.spawnIndex = start;
    if (DEV) { // (dev builds start outdoors on tile 1)
        float x, y;
        world.grid.ownerBase(1, x, y);
        world.placeNear(x, y);
    }
    saveGame(world, profile, slot); // (GameSaver.Start)

    while (plat::mainLoop()) {
        const double updateStart = plat::nowMs();
        const plat::Input in = plat::pollInput();
        if (pauseMenu.isOpen()) {
            const PauseMenu::Action action = pauseMenu.update(in, settings);
            if (action != PauseMenu::Action::None) saveIfChanged(settings, savedSettings);
            if (action == PauseMenu::Action::Quit) break;
            if (action == PauseMenu::Action::Resume) artifactUI.releaseStylus();
        } else if ((in.pressed & plat::BTN_START) && !world.playerLocked()) {
            pauseMenu.open();
            artifactUI.releaseStylus();
        } else {
            const bool devSliding = DEV && dev.update(in, world);
            if (!world.playerLocked()) artifactUI.update(in, world, FRAME_DT);
            else artifactUI.releaseStylus(); // (the Artifact stays shut while the player is held)
            FrameInput frame;
            if (!devSliding) {
                if (in.held & plat::BTN_LEFT) frame.moveX -= 1;
                if (in.held & plat::BTN_RIGHT) frame.moveX += 1;
                if (in.held & plat::BTN_UP) frame.moveY += 1;
                if (in.held & plat::BTN_DOWN) frame.moveY -= 1;
            }
            frame.actionHeld = (in.held & plat::BTN_A) != 0;
            if (in.pressed & plat::BTN_A) world.action();
            world.update(FRAME_DT, frame, DEV && dev.noclip);
            if (world.saveRequested) {
                world.saveRequested = false;
                saveGame(world, profile, slot);
            }
        }
        const float updateMs = (float)(plat::nowMs() - updateStart);

        plat::beginFrame();
        plat::beginScreen(plat::Screen::Top, plat::rgba(area.background[0], area.background[1], area.background[2]));
        float camX, camY;
        world.camera(camX, camY);
        view.begin(area, sheets, camX, camY, plat::TOP_W, plat::TOP_H, world.frame * FRAME_DT, DEV && dev.showInactive);
        view.draw(world);
        if (DEV && dev.noclip) // the position point the player moves by (the camera follows it)
            plat::drawRect(plat::TOP_W / 2 - 1, plat::TOP_H / 2 - 1, 2, 2, plat::rgba(0xe0, 0xa0, 0x40));
        drawTopOverlays(world, menuSkin.font());
        if (pauseMenu.isOpen()) plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, 0x80));

        plat::beginScreen(plat::Screen::Bottom, palette::DARK);
        if (pauseMenu.isOpen()) pauseMenu.draw(settings, menuSkin);
        else artifactUI.draw(world, sheets);
        if (DEV) {
            overlay.record(plat::lastFrameStats(), updateMs, view.lastSpriteCount());
            if (dev.showPerf) overlay.draw();
        }
        plat::endFrame();
    }
    saveIfChanged(settings, savedSettings); // (closed while paused: HOME > Close, the PC window)
    saveGame(world, profile, slot);         // (Quit, or the app closing: GameSaver.OnApplicationQuit)

    menuSkin.unload();
    font.unload();
    sheets.unload();
    plat::shutdown();
    return 0;
}
