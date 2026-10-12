// Village walkabout: a placeholder player walks the exported Village with the original's
// collision; the camera follows. For development it starts outdoors on tile 1 (DEV_START_OUTSIDE);
// the real new-game start is the Default spawn inside tile 1's house.
//   D-pad / circle pad: walk          L / R: jump to previous / next spawn point
//   X: toggle noclip                  Y: toggle drawing inactive STiles      START: quit
//   A + D-pad: slide the tile you stand on (debug, until the Artifact UI exists)
//   B: toggle collecting tiles 1-8 (debug)
// Bottom screen: the 3x3 grid (active tiles light, inactive dark, the one you stand on outlined)
// with the player's position, and perf bars for the previous frame: CPU (green), GPU (blue),
// sprites drawn (gray, 1px = 20); the red tick marks 16.7 ms (60 fps).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "game/area.h"
#include "game/area_view.h"
#include "game/artifact.h"
#include "game/grid.h"
#include "game/physics.h"
#include "platform/platform.h"

namespace {

constexpr float MOVE_SPEED = 5.0f; // world units per second (Player.moveSpeed)
constexpr float FRAME_DT = 1.0f / 60.0f;
constexpr bool DEV_START_OUTSIDE = true;

// The player's BoxCollider2Ds from the Unity prefab, in pixels (size incl. edge radius; offset from
// the player position). The original moves by raycasting from the position point; these boxes are
// what triggers/NPCs see. On foot: 0.5x0.5 + 0.125 edge radius at (0,-1/16). Boat: 1.5x0.75 at (0,-0.375).
struct Hitbox { float w, h, offsetY; };
constexpr Hitbox HITBOX_FOOT = {12, 12, -1};
constexpr Hitbox HITBOX_BOAT = {24, 12, -6};

// Nearest collision-free point to (x, y), searching outward in half-unit rings.
bool findFreeNear(const Area& area, float& x, float& y) {
    for (int r = 0; r <= 16; r++) {
        for (int i = -r; i <= r; i++) {
            for (int j = -r; j <= r; j++) {
                if (std::abs(i) != r && std::abs(j) != r) continue; // ring only
                float tx = x + i * 0.5f, ty = y + j * 0.5f;
                if (!segmentBlocked(area, tx, ty, 0, 0, false)) {
                    x = tx;
                    y = ty;
                    return true;
                }
            }
        }
    }
    return false;
}

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
    Grid grid;
    Artifact artifact;
    std::string error;
    bool loaded = loadArea("village/village.json", area, &error) && view.loadSheets(area);
    if (!loaded) printf("Failed to load village: %s\n", error.c_str());
    if (loaded) {
        grid.init(area);
        artifact.init(grid);
    }

    float px = 0, py = 0;
    bool onWater = false, noclip = false, showInactive = false, debugCollected = false;
    size_t spawnIndex = 0;
    if (loaded && !area.spawns.empty()) {
        for (size_t i = 0; i < area.spawns.size(); i++)
            if (area.spawns[i].name == "Default") spawnIndex = i;
        placeAtSpawn(area, area.spawns[spawnIndex], px, py, onWater);
    }
    if (loaded && DEV_START_OUTSIDE) {
        area.ownerBase(1, px, py); // tile 1's center, outdoors
        onWater = false;
        findFreeNear(area, px, py);
    }
    int playerStile = loaded ? grid.stileUnder(px, py) : 0;
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

        // Debug grid controls.
        const bool sliding = (in.held & plat::BTN_A) != 0;
        if (loaded && (in.pressed & plat::BTN_B)) {
            debugCollected = !debugCollected;
            for (int id = 2; id <= 8; id++) grid.setTileActive(id, debugCollected);
            printf("Tiles 2-8 %s; grid %s\n", debugCollected ? "collected" : "removed", grid.gridString().c_str());
        }
        if (loaded && sliding && playerStile != 0) {
            const STileDef* s = area.stile(playerStile);
            int dx = (in.pressed & plat::BTN_RIGHT) ? 1 : (in.pressed & plat::BTN_LEFT) ? -1 : 0;
            int dy = (in.pressed & plat::BTN_UP) ? 1 : (in.pressed & plat::BTN_DOWN) ? -1 : 0;
            if ((dx != 0) != (dy != 0)) {
                // The Artifact's view of where the tile is (it may still be queued to get there).
                int bx = -1, by = -1;
                for (int y = 0; y < grid.height(); y++)
                    for (int x = 0; x < grid.width(); x++)
                        if (artifact.buttonAt(x, y) == s->islandId) { bx = x; by = y; }
                bool ok = artifact.tryMove(grid, bx, by, bx + dx, by + dy, playerStile);
                printf("Slide tile %d (%d,%d)->(%d,%d): %s; artifact %s\n", s->islandId, bx, by, bx + dx, by + dy,
                       ok ? "queued" : "refused", artifact.gridString(area).c_str());
            }
        }

        // Tiles slide; the player is parented to the tile underneath and rides along.
        if (loaded) {
            playerStile = grid.stileUnder(px, py, playerStile);
            const STileDef* s = playerStile ? area.stile(playerStile) : nullptr;
            float ox = s ? s->posX : 0, oy = s ? s->posY : 0;
            artifact.update(grid, FRAME_DT, playerStile);
            if (s) {
                px += (s->posX - ox) * s->width;
                py += (s->posY - oy) * s->width;
            }
        }

        float ix = 0, iy = 0;
        if (!sliding) {
            if (in.held & plat::BTN_LEFT)  ix -= 1;
            if (in.held & plat::BTN_RIGHT) ix += 1;
            if (in.held & plat::BTN_UP)    iy += 1;
            if (in.held & plat::BTN_DOWN)  iy -= 1;
        }
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
            // Placeholder player: its collider box, centered on screen (the camera follows it),
            // with the position point in white.
            const Hitbox& hb = onWater ? HITBOX_BOAT : HITBOX_FOOT;
            uint32_t c = noclip ? plat::rgba(0xe0, 0xa0, 0x40) : onWater ? plat::rgba(0x50, 0x90, 0xe0)
                                                                         : plat::rgba(0xe0, 0x50, 0x50);
            plat::drawRect(plat::TOP_W / 2 - hb.w / 2, plat::TOP_H / 2 - hb.offsetY - hb.h / 2, hb.w, hb.h, c);
            plat::drawRect(plat::TOP_W / 2 - 1, plat::TOP_H / 2 - 1, 2, 2, plat::rgba(0xff, 0xff, 0xff));
        } else {
            plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0x80, 0x20, 0x20));
        }

        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x2a, 0x2a, 0x3a));
        if (loaded) {
            // 3x3 grid map: each slot 60px, row 0 at the bottom; tiles at their animated positions.
            const int cell = 60, gx = (plat::BOTTOM_W - cell * area.gridW) / 2, gy = 30;
            for (int pass = 0; pass < 2; pass++)  // empty slots first, so sliding tiles draw over them
            for (const STileDef& s : area.stiles) {
                if (s.active != (pass == 1)) continue;
                float x = gx + s.posX * cell, y = gy + (area.gridH - 1 - s.posY) * cell;
                if (s.islandId == playerStile)
                    plat::drawRect(x, y, cell, cell, plat::rgba(0xe0, 0xe0, 0x60));
                uint32_t c = s.active ? plat::rgba(0xb0, 0xb0, 0xb0) : plat::rgba(0x50, 0x50, 0x58);
                plat::drawRect(x + 2, y + 2, cell - 4, cell - 4, c);
                // Tile number as pips (no font yet): one 4px square per unit, rows of 3.
                for (int i = 0; i < s.islandId; i++)
                    plat::drawRect(x + 8 + (i % 3) * 7, y + 8 + (i / 3) * 7, 4, 4, plat::rgba(0x30, 0x30, 0x38));
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
