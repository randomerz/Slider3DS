// Village walkabout: the player walks the exported Village with the original's collision; the
// camera follows. The new-game start is the Default spawn inside tile 1's house.
//   D-pad / circle pad: walk          A: action (pick up / drop items, doors, NPC dialogue)  START: quit
// Dev builds (SLIDER_DEV=1: plain `make`, PC default; dev_controls.cpp) start outdoors on tile 1 and add:
//   R: jump to the next spawn point                 X: toggle noclip (shows the position point)
//   Y: toggle drawing inactive STiles               B: toggle collecting tiles 2-8
//   L + D-pad: slide the tile you stand on          SELECT: toggle the perf overlay
//   L + B: tiles 2-4 only, next to Kevin's unsolved knot (Slider 5)
//   L + Y: tiles 2-6 only, the ruins one slide from assembled (slide 2 left; Slider 7)
// Bottom screen: the Artifact (tap a tile, then a highlighted slot, or drag it there). Dev builds
// add a perf overlay (game/dev/dev_overlay).
#include <cstdio>
#include <string>

#include "dev_controls.h"
#include "game/data/area.h"
#include "game/render/area_view.h"
#include "game/ui/artifact_ui.h"
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
        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x1a, 0x1a, 0x1a));
        plat::endFrame();
    }
}

// The new-game start (dev builds: outdoors on tile 1 instead of in the house).
void placeAtStart(World& world, DevControls& dev) {
    const Area& area = world.area;
    for (size_t i = 0; i < area.spawns.size(); i++)
        if (area.spawns[i].name == "Default") {
            dev.spawnIndex = i;
            world.placeAt(area.spawns[i]);
        }
    if (DEV) {
        float x, y;
        world.grid.ownerBase(1, x, y);
        world.placeNear(x, y);
    }
}

} // namespace

int main(int, char**) {
    if (!plat::init()) return 1;

    Area area;
    SpriteSheets sheets;
    Font font;
    std::string error;
    if (!loadArea("village/village.json", area, &error) || !sheets.load(area) || !font.load("fonts/dialogue.json", &error)) {
        printf("Failed to load village: %s\n", error.c_str());
        showLoadError();
        plat::shutdown();
        return 1;
    }

    World world(area);
    AreaView view;
    view.setFont(&font);
    ArtifactUI artifactUI;
    artifactUI.init(area, world.artifact);
    DevControls dev;
    dev.showPerf = DEV;
    DevOverlay overlay;
    placeAtStart(world, dev);
    int frame = 0;

    while (plat::mainLoop()) {
        const double updateStart = plat::nowMs();
        const plat::Input in = plat::pollInput();
        if (in.pressed & plat::BTN_START) break;

        const bool devSliding = DEV && dev.update(in, world);
        artifactUI.update(in, world, FRAME_DT);
        float moveX = 0, moveY = 0;
        if (!devSliding) {
            if (in.held & plat::BTN_LEFT) moveX -= 1;
            if (in.held & plat::BTN_RIGHT) moveX += 1;
            if (in.held & plat::BTN_UP) moveY += 1;
            if (in.held & plat::BTN_DOWN) moveY -= 1;
        }
        if (in.pressed & plat::BTN_A) world.action();
        world.update(FRAME_DT, moveX, moveY, DEV && dev.noclip);
        const float updateMs = (float)(plat::nowMs() - updateStart);

        plat::beginFrame();
        plat::beginScreen(plat::Screen::Top, plat::rgba(area.background[0], area.background[1], area.background[2]));
        view.begin(area, sheets, world.player.x, world.player.y, plat::TOP_W, plat::TOP_H, frame * FRAME_DT,
                   DEV && dev.showInactive);
        view.draw(world);
        if (DEV && dev.noclip) // the position point the player moves by (the camera follows it)
            plat::drawRect(plat::TOP_W / 2 - 1, plat::TOP_H / 2 - 1, 2, 2, plat::rgba(0xe0, 0xa0, 0x40));
        if (world.effects.fade > 0) // FadeFromBlack (UIEffects' black panel)
            plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, (uint8_t)(world.effects.fade * 255)));

        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x1a, 0x1a, 0x1a));
        artifactUI.draw(world, sheets);
        if (DEV) {
            overlay.record(plat::lastFrameStats(), updateMs, view.lastSpriteCount());
            if (dev.showPerf) overlay.draw();
        }
        plat::endFrame();
        frame++;
    }

    font.unload();
    sheets.unload();
    plat::shutdown();
    return 0;
}
