#include "dev_controls.h"

#include <cstdio>
#include <string>

bool DevControls::update(const plat::Input& in, World& world) {
    const Area& area = world.area;
    if (in.pressed & plat::BTN_SELECT) showPerf = !showPerf;
    if (in.pressed & plat::BTN_X) noclip = !noclip;

    if (!area.spawns.empty() && (in.pressed & plat::BTN_R)) {
        spawnIndex = (spawnIndex + 1) % area.spawns.size();
        world.placeAt(area.spawns[spawnIndex]);
        printf("Spawn: %s\n", area.spawns[spawnIndex].name.c_str());
    }

    if ((in.held & plat::BTN_L) && (in.pressed & plat::BTN_Y)) {
        // The ruins (Slider 7): Sliders 2-6 collected, 7-8 not; 3 1 / 6 _ 2, so sliding 2 left
        // assembles them (and 3 1 and 3 above 6 already light their rods). Next to the arrow.
        collected = false;
        for (int id = 2; id <= 8; id++) {
            world.setTileActive(id, id <= 6);
            world.state.setCollectible("Slider " + std::to_string(id), "Village", id <= 6);
        }
        world.artifact.setGrid(world.grid, "314_692_578");
        world.reloadObjects();
        if (!area.ruinsArrows.empty()) {
            float x, y;
            world.objectPosition(area.ruinsArrows[0].object, x, y);
            world.placeNear(x, y - 3);
        }
        printf("Ruins setup: tiles 2-6 collected, slide 2 left to assemble; grid %s\n", world.grid.gridString().c_str());
    } else if (in.pressed & plat::BTN_Y) {
        showInactive = !showInactive;
    }

    if ((in.held & plat::BTN_L) && (in.pressed & plat::BTN_B)) {
        // Kevin's knot: Sliders 2-4 collected (the knot's tile 4 in the grid, unsolved), 5-8 not;
        // reload as a save with them would, and stand next to Kevin.
        collected = false;
        for (int id = 2; id <= 8; id++) {
            world.setTileActive(id, id <= 4);
            world.state.setCollectible("Slider " + std::to_string(id), "Village", id <= 4);
        }
        world.reloadObjects();
        for (const NpcDef& n : area.npcs)
            for (const NpcCondDef& c : n.conds)
                for (const ConditionDef& d : c.conditions)
                    if (d.type == ConditionDef::Type::Spec && d.spec == ConditionDef::Spec::KnotSolved) {
                        float x, y;
                        world.objectPosition(n.object, x, y);
                        world.placeNear(x, y - 1);
                    }
        printf("Knot setup: tiles 2-4 collected, next to Kevin; grid %s\n", world.grid.gridString().c_str());
    } else if (in.pressed & plat::BTN_B) {
        // Collect (or drop) Sliders 2-8, then reload the scene objects as a save with them would.
        collected = !collected;
        for (int id = 2; id <= 8; id++) {
            world.setTileActive(id, collected);
            world.state.setCollectible("Slider " + std::to_string(id), "Village", collected);
        }
        world.reloadObjects();
        printf("Tiles 2-8 %s; grid %s\n", collected ? "collected" : "removed", world.grid.gridString().c_str());
    }

    const bool sliding = (in.held & plat::BTN_L) != 0;
    const int stile = world.player.stile;
    if (sliding && stile != 0) {
        const int dx = (in.pressed & plat::BTN_RIGHT) ? 1 : (in.pressed & plat::BTN_LEFT) ? -1 : 0;
        const int dy = (in.pressed & plat::BTN_UP) ? 1 : (in.pressed & plat::BTN_DOWN) ? -1 : 0;
        if ((dx != 0) != (dy != 0)) {
            // The Artifact's view of where the tile is (it may still be queued to get there).
            int bx = 0, by = 0;
            world.artifact.findButton(stile, bx, by);
            const bool ok = world.artifact.tryMove(world.grid, bx, by, bx + dx, by + dy, stile);
            printf("Slide tile %d (%d,%d)->(%d,%d): %s; artifact %s\n", stile, bx, by, bx + dx, by + dy,
                   ok ? "queued" : "refused", world.artifact.gridString(world.grid).c_str());
        }
    }
    return sliding;
}
