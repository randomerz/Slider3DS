// Village viewer: draws the exported Village on the top screen with a free camera.
// D-pad / circle pad: pan (hold B to pan faster). Y: toggle drawing inactive STiles. START: quit.
// Bottom screen: the 3x3 grid (active tiles light, inactive dark) with the camera's position,
// and perf bars for the previous frame: CPU (green), GPU (blue), sprites drawn (gray, 1px = 20);
// the red tick marks 16.7 ms (60 fps).
#include <cstdio>
#include <string>

#include "game/area.h"
#include "game/area_view.h"
#include "platform/platform.h"

int main(int, char**) {
    if (!plat::init()) return 1;

    Area area;
    AreaView view;
    std::string error;
    bool loaded = loadArea("village/village.json", area, &error) && view.loadSheets(area);
    if (!loaded) printf("Failed to load village: %s\n", error.c_str());

    // Start centered on the 3x3 grid.
    float camX = 25.0f, camY = 25.0f;
    bool showInactive = true;
    int frame = 0;

    while (plat::mainLoop()) {
        plat::Input in = plat::pollInput();
        if (in.pressed & plat::BTN_START) break;
        if (in.pressed & plat::BTN_Y) showInactive = !showInactive;

        float speed = (in.held & plat::BTN_B) ? 0.5f : 0.125f; // world units per frame
        if (in.held & plat::BTN_LEFT)  camX -= speed;
        if (in.held & plat::BTN_RIGHT) camX += speed;
        if (in.held & plat::BTN_UP)    camY += speed;
        if (in.held & plat::BTN_DOWN)  camY -= speed;

        plat::beginFrame();

        plat::beginScreen(plat::Screen::Top, plat::rgba(0x14, 0x14, 0x1c));
        if (loaded)
            view.draw(area, camX, camY, plat::TOP_W, plat::TOP_H, frame / 60.0f, showInactive);
        else
            plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0x80, 0x20, 0x20));

        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x2a, 0x2a, 0x3a));
        if (loaded) {
            // 3x3 grid map: each slot 60px, grid y flipped so row 0 is at the bottom.
            const int cell = 60, gx = (plat::BOTTOM_W - cell * area.gridW) / 2, gy = 30;
            for (const STileDef& s : area.stiles) {
                float x = gx + s.x * cell, y = gy + (area.gridH - 1 - s.y) * cell;
                uint32_t c = s.active ? plat::rgba(0xb0, 0xb0, 0xb0) : plat::rgba(0x50, 0x50, 0x58);
                plat::drawRect(x + 2, y + 2, cell - 4, cell - 4, c);
            }
            float span = (float)(area.gridW * area.stiles[0].width);
            float mx = gx + camX / span * cell * area.gridW;
            float my = gy + (1.0f - camY / span) * cell * area.gridH;
            plat::drawRect(mx - 3, my - 3, 6, 6, plat::rgba(0xe0, 0x60, 0x60));
        }
        plat::FrameStats stats = plat::lastFrameStats();
        const float msPx = 10.0f; // 10px per ms
        plat::drawRect(10, 216, stats.cpuMs * msPx, 5, plat::rgba(0x60, 0xd0, 0x70));
        plat::drawRect(10, 223, stats.gpuMs * msPx, 5, plat::rgba(0x60, 0x90, 0xe0));
        plat::drawRect(10, 230, view.lastSpriteCount() / 20.0f, 5, plat::rgba(0xa0, 0xa0, 0xa0));
        plat::drawRect(10 + 16.7f * msPx, 212, 1, 26, plat::rgba(0xe0, 0x50, 0x50));

        plat::endFrame();
        frame++;
    }

    view.unloadSheets();
    plat::shutdown();
    return 0;
}
