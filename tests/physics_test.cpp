// Headless checks of the collision against the real exported Village (PC only).
// Run: build-pc/Debug/slider3ds_tests.exe   (exit code 0 = all passed)
#include <cmath>
#include <cstdio>
#include <string>

#include "game/area.h"
#include "game/physics.h"

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

void spawnPos(const Area& area, const Spawn& s, float& x, float& y) {
    area.ownerBase(s.owner, x, y);
    x += s.x;
    y += s.y;
}

} // namespace

int main(int, char**) {
    Area area;
    std::string error;
    if (!loadArea("village/village.json", area, &error)) {
        printf("FAIL  load: %s\n", error.c_str());
        return 1;
    }
    check(!area.collision.empty() && !area.shapes.empty(), "collision layers loaded");

    // No spawn starts inside solid collision (the player would be stuck). Spawns on tiles that
    // aren't collected yet (e.g. VillageWaterfall on tile 6) are only used once they are, so
    // check with every tile active.
    Area allActive = area;
    for (STileDef& t : allActive.stiles) t.active = true;
    for (const Spawn& s : allActive.spawns) {
        float x, y;
        spawnPos(allActive, s, x, y);
        std::string by = segmentBlockedBy(allActive, x, y, 0, 0, s.inBoat);
        check(by.empty(), "spawn " + s.name + " starts free" + (by.empty() ? "" : " (blocked by " + by + ")"));
    }

    // From the Default spawn (inside a house), walking any direction is eventually stopped by a
    // wall, and stepping never jumps through one: each step's segment was checked clear.
    const Spawn* def = area.spawn("Default");
    check(def != nullptr, "Default spawn exists");
    if (def) {
        const float step = 5.0f / 60.0f;
        const float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        const char* names[4] = {"right", "left", "up", "down"};
        for (int d = 0; d < 4; d++) {
            float x, y;
            spawnPos(area, *def, x, y);
            const float sx = x, sy = y;
            int frames = 0;
            while (frames < 600 && moveWithCollision(area, x, y, dirs[d][0] * step, dirs[d][1] * step, false))
                frames++;
            float dist = std::fabs(x - sx) + std::fabs(y - sy);
            check(frames < 600 && dist < 17.0f,
                  std::string("walk ") + names[d] + " from Default stops at a wall inside the house (" +
                      std::to_string(dist) + " units)");
        }
    }

    // Inactive STiles are solid: walking into tile 9's slot (center 17,17, inactive at start) from
    // tile 1 (active, center 0,0) must be blocked at the slot edge.
    const STileDef* t9 = area.stile(9);
    if (t9 && !t9->active) {
        float edge = t9->x * t9->width - t9->width * 0.5f; // left edge of tile 9's slot
        check(segmentBlocked(area, edge - 0.25f, 17.0f, 0.5f, 0.0f, false), "inactive tile 9's slot is solid");
    }

    // On water the land colliders apply: sailing west from the Beach spawn (on the ocean) stops
    // at the coast instead of crossing onto land.
    if (const Spawn* beach = area.spawn("VillageBeach")) {
        float x, y;
        spawnPos(area, *beach, x, y);
        const float startX = x;
        int frames = 0;
        while (frames < 600 && moveWithCollision(area, x, y, -5.0f / 60.0f, 0, true)) frames++;
        std::string by = segmentBlockedBy(area, x, y, -5.0f / 60.0f, 0, true);
        check(frames < 600, "raft sailing west from Beach stops after " + std::to_string(startX - x) +
                                " units (at x=" + std::to_string(x) + ", blocked by " + by + ")");
    }

    printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
