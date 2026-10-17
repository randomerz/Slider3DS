#include "dev_controls.h"

#include <cstdio>
#include <string>

bool DevControls::update(const plat::Input& in, World& world) {
    const Area& area = world.area;
    if (in.pressed & plat::BTN_SELECT) showPerf = !showPerf;
    if ((in.held & plat::BTN_L) && (in.pressed & plat::BTN_X)) {
        // Chad: Sliders 2-8 collected (9's slot, in the middle, empty), next to him. Talk to him to
        // make him jump; then slide another tile to tip him, or his tile 8 to make him fall.
        collected = true;
        for (int id = 2; id <= 8; id++) {
            world.setTileActive(id, true);
            world.state.setCollectible("Slider " + std::to_string(id), "Village", true);
        }
        world.reloadObjects();
        if (!area.chadJumps.empty()) {
            float x, y;
            world.objectPosition(area.chadJumps[0].object, x, y);
            world.placeNear(x - 1, y - 1);
        }
        printf("Chad setup: tiles 2-8 collected, next to Chad; grid %s\n", world.grid.gridString().c_str());
    } else if (in.pressed & plat::BTN_X) {
        noclip = !noclip;
    }

    if ((in.held & plat::BTN_L) && (in.pressed & plat::BTN_R)) {
        // Slider 9. Before the shuffle: Sliders 2-8 collected, the Ruins Fragment dropped next to the
        // player by the Archeologist (pick it up with A, then talk to him). After it: the grid one
        // slide from solved (7 right).
        if (world.state.flag(village::VILLAGE_COMPLETION_FLAG)) {
            world.setGrid("624_879_153");
            printf("Slider 9 setup: slide 7 right to finish; grid %s\n", world.artifact.gridString(world.grid).c_str());
        } else {
            collected = true;
            for (int id = 2; id <= 8; id++) {
                world.setTileActive(id, true);
                world.state.setCollectible("Slider " + std::to_string(id), "Village", true);
            }
            world.reloadObjects();
            for (const NpcDef& n : area.npcs)
                if (area.objects[n.object].path.size() >= 12 &&
                    area.objects[n.object].path.compare(area.objects[n.object].path.size() - 12, 12, "Archeologist") == 0) {
                    float x, y;
                    world.objectPosition(n.object, x, y);
                    world.placeNear(x - 2, y - 1);
                }
            if (area.areaGrid.ruinsFragment >= 0)
                world.items.drop(world, area.areaGrid.ruinsFragment, world.player.x + 1, world.player.y);
            printf("Slider 9 setup: tiles 2-8 collected, the fragment at your feet (A), then talk to the Archeologist\n");
        }
    } else if (!area.spawns.empty() && (in.pressed & plat::BTN_R)) {
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
