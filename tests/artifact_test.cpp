// Artifact move queue driving the Grid, and the player riding a sliding tile.
#include <cmath>
#include <string>

#include "game/grid/artifact.h"
#include "game/physics/physics.h"
#include "game/world/world.h"
#include "test.h"

namespace {
constexpr float DT = 1.0f / 60.0f;

// Runs the Artifact until every queued move has finished (or ~10 s pass); returns seconds taken.
float settle(Artifact& artifact, Grid& grid, int playerStile = 0) {
    int frames = 0;
    for (; frames < 600 && !artifact.queueEmpty(); frames++) artifact.update(grid, DT, playerStile);
    return frames * DT;
}
} // namespace

TEST(artifact_move_rules) {
    Grid grid;
    grid.init(villageArea());
    Artifact artifact;
    artifact.init(grid);
    // Only tile 1 (bottom-left) is collected: it can slide 1 or 2 slots right or up.
    auto opts = artifact.moveOptions(grid, 0, 0);
    check(opts.size() == 4, "tile 1 has 4 move options, got " + std::to_string(opts.size()));
    CHECK(!artifact.tryMove(grid, 0, 0, 1, 1, 0)); // diagonal
    CHECK(!artifact.tryMove(grid, 1, 0, 2, 0, 0)); // moving an empty slot
    CHECK(!artifact.tryMove(grid, 0, 0, -1, 0, 0)); // off the grid
    artifact.canQueue = false;
    CHECK(!artifact.tryMove(grid, 0, 0, 1, 0, 0));
    artifact.canQueue = true;
    CHECK(artifact.queueEmpty() && grid.gridString() == "###_###_1##");
}

TEST(artifact_slide_animates) {
    Grid grid;
    grid.init(villageArea());
    Artifact artifact;
    artifact.init(grid);
    const TileState* t1 = grid.tile(1);
    CHECK(artifact.tryMove(grid, 0, 0, 2, 0, 0));
    check(t1->x == 2 && t1->y == 0 && t1->posX == 0.0f, "logical slot updates at once, drawn position doesn't");
    // Outline of the 3 slots: bottom 3 + ends 2 (their outer neighbours are off-grid) + top 3,
    // raised by both the slot and the one above like Unity's SGridBackground pairs = 11, x2 for houses.
    check(grid.walls().size() == 22, "slide fenced, walls " + std::to_string(grid.walls().size()));
    for (int i = 0; i < 30; i++) artifact.update(grid, DT, 0);
    check(t1->posX > 0.0f && t1->posX < 1.0f, "half way, tile 1 has eased only a little (" +
                                                  std::to_string(t1->posX) + " slots)");
    float t = settle(artifact, grid);
    check(std::fabs(t - 0.5f) < 0.05f, "a 1 s slide finishes on time (" + std::to_string(t + 0.5f) + " s total)");
    CHECK(t1->posX == 2.0f && t1->posY == 0.0f && grid.walls().empty());
    check(grid.gridString() == "###_###_##1" && artifact.gridString(grid) == grid.gridString(),
          "grid and artifact agree: " + grid.gridString());
}

TEST(artifact_queue_chains_and_speeds_up) {
    Grid grid;
    grid.init(villageArea());
    Artifact artifact;
    artifact.init(grid);
    CHECK(artifact.tryMove(grid, 0, 0, 1, 0, 0) && artifact.tryMove(grid, 1, 0, 1, 1, 0) &&
          artifact.tryMove(grid, 1, 1, 1, 2, 0));
    check(artifact.gridString(grid) == "#1#_###_###" && grid.gridString() == "###_###_#1#",
          "artifact shows the queued result; grid only the running move");
    // One move runs and two wait; a third may wait, a fourth may not (UIArtifact.maxMoveQueueSize 3).
    CHECK(artifact.tryMove(grid, 1, 2, 0, 2, 0));
    check(!artifact.tryMove(grid, 0, 2, 0, 1, 0), "a 4th waiting move is refused");
    float t = settle(artifact, grid);
    // Full speed would be 4 s; durations shrink by 0.1 s per chained move, down to 0.5 s.
    check(t > 2.5f && t < 3.6f, "4 chained slides took " + std::to_string(t) + " s");
    check(grid.gridString() == "1##_###_###" && artifact.gridString(grid) == grid.gridString(),
          "ends where the artifact said: " + grid.gridString());
}

TEST(artifact_set_grid_resyncs) {
    Grid grid;
    grid.init(villageArea());
    Artifact artifact;
    artifact.init(grid);
    CHECK(artifact.tryMove(grid, 0, 0, 1, 0, 0) && artifact.tryMove(grid, 1, 0, 2, 0, 0));
    CHECK(!artifact.queueEmpty() && grid.moving());
    CHECK(artifact.setGrid(grid, "182_043_765"));
    CHECK(artifact.queueEmpty() && !grid.moving() && grid.walls().empty());
    check(artifact.gridString(grid) == grid.gridString(), "artifact matches the new grid: " + artifact.gridString(grid));
    // Moves work again afterwards: tile 1, now top-left, is surrounded by empty slots.
    CHECK(!artifact.moveOptions(grid, 0, 2).empty());
    CHECK(!artifact.setGrid(grid, "nonsense") && artifact.gridString(grid) == grid.gridString());
}

TEST(world_player_rides_tile) {
    // World::update: the player standing on tile 1 rides it as it slides up one slot; while it
    // slides, the player is fenced in and the slot it slid over (tile 8's) isn't solid.
    World world(villageArea(), DEFAULT_SETTINGS);
    world.placeNear(0, 3); // on tile 1, outdoors (somewhere free: a resting player is pushed out)
    const float startY = world.player.y;
    CHECK(world.player.stile == 1);
    CHECK(world.artifact.tryMove(world.grid, 0, 0, 0, 1, world.player.stile));
    // Tile 8 (the empty slot above) jumped below and has its collider off while 1 slides over it.
    CHECK(world.grid.tile(8)->collidersOff == 1);
    bool fencedIn = false, tile8Solid = false;
    for (int i = 0; i < 120 && !world.artifact.queueEmpty(); i++) {
        world.update(DT, 0, 0, false);
        if (i == 30) {
            float tx, ty; // tile 1's center; its right edge (a fence while it slides) is 8.5 to the right
            world.grid.ownerBase(1, tx, ty);
            const float py = world.player.y;
            fencedIn = boxBlocker(world, {tx + 8.0f, py - 0.01f, tx + 9.0f, py + 0.01f}, false).kind == Blocker::Wall;
            Blocker b = boxBlocker(world, {-0.01f, -3.01f, 0.01f, -2.99f}, false); // inside tile 8's new slot
            tile8Solid = b.kind == Blocker::InactiveTile && b.index == 8;
        }
    }
    check(std::fabs(world.player.y - startY - 17.0f) < 1e-3f,
          "player rode tile 1 up one slot (moved " + std::to_string(world.player.y - startY) + ")");
    CHECK(world.player.stile == 1);
    CHECK(fencedIn);
    check(!tile8Solid, "tile 8's slot is not solid mid-slide (its collider is off)");
    CHECK(world.grid.walls().empty() && world.grid.tile(8)->collidersOff == 0);
    Blocker after = boxBlocker(world, {-0.01f, -3.01f, 0.01f, -2.99f}, false);
    check(after.kind == Blocker::InactiveTile && after.index == 8, "tile 8's slot is solid again after the slide");
}
