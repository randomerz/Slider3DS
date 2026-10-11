// Village walkabout: a placeholder player walks the exported Village with the original's
// collision; the camera follows. Starts at the Default spawn (inside tile 1's house).
//   D-pad / circle pad: walk          L / R: jump to previous / next spawn point
//   X: toggle noclip                  Y: toggle drawing inactive STiles      START: quit
// Bottom screen: the 3x3 grid (active tiles light, inactive dark) with the player's position,
// and perf bars for the previous frame: CPU (green), GPU (blue), sprites drawn (gray, 1px = 20);
// the red tick marks 16.7 ms (60 fps).
#include <cmath>
#include <cstdio>
#include <string>

#include "game/area.h"
#include "game/area_view.h"
#include "game/physics.h"
#include "platform/platform.h"

namespace {

constexpr float MOVE_SPEED = 5.0f; // world units per second (Player.moveSpeed)
constexpr float FRAME_DT = 1.0f / 60.0f;

void placeAtSpawn(const Area& area, const Spawn& s, float& x, float& y, bool& onWater) {
    float bx, by;
    area.ownerBase(s.owner, bx, by);
    x = bx + s.x;
    y = by + s.y;
    onWater = s.inBoat;
}

} // namespace

int main(int, char**) {
    if (!plat::init()) return 1;

    Area area;
    AreaView view;
    std::string error;
    bool loaded = loadArea("village/village.json", area, &error) && view.loadSheets(area);
    if (!loaded) printf("Failed to load village: %s\n", error.c_str());

    float px = 0, py = 0;
    bool onWater = false, noclip = false, showInactive = false;
    size_t spawnIndex = 0;
    if (loaded && !area.spawns.empty()) {
        for (size_t i = 0; i < area.spawns.size(); i++)
            if (area.spawns[i].name == "Default") spawnIndex = i;
        placeAtSpawn(area, area.spawns[spawnIndex], px, py, onWater);
    }
    int frame = 0;

    while (plat::mainLoop()) {
        plat::Input in = plat::pollInput();
        if (in.pressed & plat::BTN_START) break;
        if (in.pressed & plat::BTN_Y) showInactive = !showInactive;
        if (in.pressed & plat::BTN_X) noclip = !noclip;
        if (loaded && !area.spawns.empty() && (in.pressed & (plat::BTN_L | plat::BTN_R))) {
            size_t n = area.spawns.size();
            spawnIndex = (in.pressed & plat::BTN_R) ? (spawnIndex + 1) % n : (spawnIndex + n - 1) % n;
            placeAtSpawn(area, area.spawns[spawnIndex], px, py, onWater);
            printf("Spawn: %s\n", area.spawns[spawnIndex].name.c_str());
        }

        float ix = 0, iy = 0;
        if (in.held & plat::BTN_LEFT)  ix -= 1;
        if (in.held & plat::BTN_RIGHT) ix += 1;
        if (in.held & plat::BTN_UP)    iy += 1;
        if (in.held & plat::BTN_DOWN)  iy -= 1;
        if (loaded && (ix != 0 || iy != 0)) {
            float len = std::sqrt(ix * ix + iy * iy);
            float dx = ix / len * MOVE_SPEED * FRAME_DT, dy = iy / len * MOVE_SPEED * FRAME_DT;
            if (noclip) { px += dx; py += dy; }
            else moveWithCollision(area, px, py, dx, dy, onWater);
        }

        plat::beginFrame();

        plat::beginScreen(plat::Screen::Top, plat::rgba(0x14, 0x14, 0x1c));
        if (loaded) {
            view.draw(area, px, py, plat::TOP_W, plat::TOP_H, frame / 60.0f, showInactive);
            // Placeholder player: 10x14 px standing on its position (the camera centers on it).
            uint32_t c = noclip ? plat::rgba(0xe0, 0xa0, 0x40) : onWater ? plat::rgba(0x50, 0x90, 0xe0)
                                                                         : plat::rgba(0xe0, 0x50, 0x50);
            plat::drawRect(plat::TOP_W / 2 - 5, plat::TOP_H / 2 - 14, 10, 14, c);
            plat::drawRect(plat::TOP_W / 2 - 1, plat::TOP_H / 2 - 1, 2, 2, plat::rgba(0xff, 0xff, 0xff));
        } else {
            plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0x80, 0x20, 0x20));
        }

        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x2a, 0x2a, 0x3a));
        if (loaded) {
            // 3x3 grid map: each slot 60px, row 0 at the bottom. Slots span center +- width/2.
            const int cell = 60, gx = (plat::BOTTOM_W - cell * area.gridW) / 2, gy = 30;
            for (const STileDef& s : area.stiles) {
                float x = gx + s.x * cell, y = gy + (area.gridH - 1 - s.y) * cell;
                uint32_t c = s.active ? plat::rgba(0xb0, 0xb0, 0xb0) : plat::rgba(0x50, 0x50, 0x58);
                plat::drawRect(x + 2, y + 2, cell - 4, cell - 4, c);
            }
            const float w = (float)area.stiles[0].width;
            // Inside a house the player is housingOffset below the grid; show the slot it's under.
            float mapY = py < area.housingOffset / 2.0f ? py - area.housingOffset : py;
            float mx = gx + (px / w + 0.5f) * cell;
            float my = gy + (area.gridH - (mapY / w + 0.5f)) * cell;
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
