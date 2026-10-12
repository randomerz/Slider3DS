// Village walkabout: a placeholder player walks the exported Village with the original's
// collision; the camera follows. The new-game start is the Default spawn inside tile 1's house.
//   D-pad / circle pad: walk          START: quit
// Dev builds (SLIDER_DEV=1: plain `make`, PC default) start outdoors on tile 1 and add:
//   L / R: jump to previous / next spawn point      X: toggle noclip
//   Y: toggle drawing inactive STiles               B: toggle collecting tiles 2-8
//   A + D-pad: slide the tile you stand on (until the Artifact UI exists)
// Bottom screen: the Artifact (tap a tile, then a highlighted slot, or drag it there). Dev builds
// add perf bars for the previous frame: CPU (green), GPU (blue), sprites drawn (gray, 1px = 20;
// red near the sprite budget); the red tick marks 16.7 ms.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "game/area.h"
#include "game/area_view.h"
#include "game/artifact.h"
#include "game/artifact_ui.h"
#include "game/grid.h"
#include "game/physics.h"
#include "game/sprite_sheets.h"
#include "platform/platform.h"

namespace {

constexpr float MOVE_SPEED = 5.0f; // world units per second (Player.moveSpeed)
constexpr float FRAME_DT = 1.0f / 60.0f;
#ifndef SLIDER_DEV
#define SLIDER_DEV 0
#endif
constexpr bool DEV = SLIDER_DEV != 0;
constexpr bool DEV_START_OUTSIDE = DEV;

// The player's BoxCollider2Ds from the Unity prefab, in pixels (size incl. edge radius; offset from
// the player position). The original moves by raycasting from the position point; these boxes are
// what triggers/NPCs see. On foot: 0.5x0.5 + 0.125 edge radius at (0,-1/16). Boat: 1.5x0.75 at (0,-0.375).
struct Hitbox { float w, h, offsetY; };
constexpr Hitbox HITBOX_FOOT = {12, 12, -1};
constexpr Hitbox HITBOX_BOAT = {24, 12, -6};

// Nearest collision-free point to (x, y), searching outward in half-unit rings.
bool findFreeNear(const Area& area, const Grid& grid, float& x, float& y) {
    for (int r = 0; r <= 16; r++) {
        for (int i = -r; i <= r; i++) {
            for (int j = -r; j <= r; j++) {
                if (std::abs(i) != r && std::abs(j) != r) continue; // ring only
                float tx = x + i * 0.5f, ty = y + j * 0.5f;
                if (!segmentBlocked(area, grid, tx, ty, 0, 0, false)) {
                    x = tx;
                    y = ty;
                    return true;
                }
            }
        }
    }
    return false;
}

void placeAtSpawn(const Grid& grid, const Spawn& s, float& x, float& y, bool& onWater) {
    float bx, by;
    grid.ownerBase(s.owner, bx, by);
    x = bx + s.x;
    y = by + s.y;
    onWater = s.inBoat;
}

} // namespace

int main(int, char**) {
    if (!plat::init()) return 1;

    Area area;
    SpriteSheets sheets;
    AreaView view;
    Grid grid;
    Artifact artifact;
    ArtifactUI artifactUI;
    std::string error;
    bool loaded = loadArea("village/village.json", area, &error) && sheets.load(area);
    if (!loaded) printf("Failed to load village: %s\n", error.c_str());
    if (loaded) {
        grid.init(area);
        artifact.init(grid);
        artifactUI.init(area, artifact);
    }

    float px = 0, py = 0;
    bool onWater = false, noclip = false, showInactive = false, debugCollected = false;
    size_t spawnIndex = 0;
    if (loaded && !area.spawns.empty()) {
        for (size_t i = 0; i < area.spawns.size(); i++)
            if (area.spawns[i].name == "Default") spawnIndex = i;
        placeAtSpawn(grid, area.spawns[spawnIndex], px, py, onWater);
    }
    if (loaded && DEV_START_OUTSIDE) {
        grid.ownerBase(1, px, py); // tile 1's center, outdoors
        onWater = false;
        findFreeNear(area, grid, px, py);
    }
    int playerStile = loaded ? grid.stileUnder(px, py) : 0;
    int frame = 0;

    while (plat::mainLoop()) {
        plat::Input in = plat::pollInput();
        if (in.pressed & plat::BTN_START) break;
        if (DEV && (in.pressed & plat::BTN_Y)) showInactive = !showInactive;
        if (DEV && (in.pressed & plat::BTN_X)) noclip = !noclip;
        if (DEV && loaded && !area.spawns.empty() && (in.pressed & (plat::BTN_L | plat::BTN_R))) {
            size_t n = area.spawns.size();
            spawnIndex = (in.pressed & plat::BTN_R) ? (spawnIndex + 1) % n : (spawnIndex + n - 1) % n;
            placeAtSpawn(grid, area.spawns[spawnIndex], px, py, onWater);
            printf("Spawn: %s\n", area.spawns[spawnIndex].name.c_str());
        }

        // Debug grid controls.
        const bool sliding = DEV && (in.held & plat::BTN_A) != 0;
        if (DEV && loaded && (in.pressed & plat::BTN_B)) {
            debugCollected = !debugCollected;
            for (int id = 2; id <= 8; id++) grid.setTileActive(id, debugCollected);
            printf("Tiles 2-8 %s; grid %s\n", debugCollected ? "collected" : "removed", grid.gridString().c_str());
        }
        if (loaded && sliding && playerStile != 0) {
            int dx = (in.pressed & plat::BTN_RIGHT) ? 1 : (in.pressed & plat::BTN_LEFT) ? -1 : 0;
            int dy = (in.pressed & plat::BTN_UP) ? 1 : (in.pressed & plat::BTN_DOWN) ? -1 : 0;
            if ((dx != 0) != (dy != 0)) {
                // The Artifact's view of where the tile is (it may still be queued to get there).
                int bx = 0, by = 0;
                artifact.findButton(playerStile, bx, by);
                bool ok = artifact.tryMove(grid, bx, by, bx + dx, by + dy, playerStile);
                printf("Slide tile %d (%d,%d)->(%d,%d): %s; artifact %s\n", playerStile, bx, by, bx + dx, by + dy,
                       ok ? "queued" : "refused", artifact.gridString(grid).c_str());
            }
        }

        // Tiles slide; the player is parented to the tile underneath and rides along.
        if (loaded) {
            playerStile = grid.stileUnder(px, py, playerStile);
            const TileState* t = grid.tile(playerStile);
            float ox = t ? t->posX : 0, oy = t ? t->posY : 0;
            artifactUI.update(in, area, grid, artifact, playerStile, FRAME_DT);
            artifact.update(grid, FRAME_DT, playerStile);
            if (t) {
                px += (t->posX - ox) * grid.tileWidth();
                py += (t->posY - oy) * grid.tileWidth();
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
            else moveWithCollision(area, grid, px, py, dx, dy, onWater);
        }

        plat::beginFrame();

        plat::beginScreen(plat::Screen::Top, loaded ? plat::rgba(area.background[0], area.background[1], area.background[2])
                                                   : plat::rgba(0x14, 0x14, 0x1c));
        if (loaded) {
            view.begin(area, sheets, px, py, plat::TOP_W, plat::TOP_H, frame / 60.0f, showInactive);
            view.drawLayers(area, grid);
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

        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x1a, 0x1a, 0x1a));
        if (loaded) {
            artifactUI.draw(area, grid, artifact, sheets);
        }
        if (DEV) {
            plat::FrameStats stats = plat::lastFrameStats();
            const float msPx = 10.0f; // 10px per ms
            const int sprites = view.lastSpriteCount();
            const bool nearBudget = sprites > plat::MAX_DRAWS_PER_FRAME * 3 / 4;
            plat::drawRect(10, 216, stats.cpuMs * msPx, 5, plat::rgba(0x60, 0xd0, 0x70));
            plat::drawRect(10, 223, stats.gpuMs * msPx, 5, plat::rgba(0x60, 0x90, 0xe0));
            plat::drawRect(10, 230, sprites / 20.0f, 5,
                           nearBudget ? plat::rgba(0xe0, 0x50, 0x50) : plat::rgba(0xa0, 0xa0, 0xa0));
            plat::drawRect(10 + 16.7f * msPx, 212, 1, 26, plat::rgba(0xe0, 0x50, 0x50));
        }

        plat::endFrame();
        frame++;
    }

    sheets.unload();
    plat::shutdown();
    return 0;
}
