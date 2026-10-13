#include "game/world.h"

#include <cmath>

#include "game/physics.h"

World::World(const Area& a) : area(a) {
    grid.init(area);
    artifact.init(grid);
    reloadObjects();
}

void World::reloadObjects() {
    objects.init(area);
    startChecks.reset(area);
    startChecks.update(area, state, grid, objects);
}

void World::placeAt(const Spawn& spawn) {
    float bx, by;
    grid.ownerBase(spawn.owner, bx, by);
    player.x = bx + spawn.x;
    player.y = by + spawn.y;
    player.onWater = spawn.inBoat;
    player.stile = grid.stileUnder(player.x, player.y);
}

void World::placeNear(float x, float y) {
    player.onWater = false;
    findFreeNear(*this, player.hitbox(area), x, y);
    player.x = x;
    player.y = y;
    player.stile = grid.stileUnder(x, y);
}

void World::update(float dt, float moveX, float moveY, bool noclip) {
    // Tiles slide; the player is parented to the tile underneath and rides along.
    const TileState* t = grid.tile(player.stile);
    const float ox = t ? t->posX : 0, oy = t ? t->posY : 0;
    artifact.update(grid, dt, player.stile);
    if (t) {
        player.x += (t->posX - ox) * grid.tileWidth();
        player.y += (t->posY - oy) * grid.tileWidth();
    }

    startChecks.update(area, state, grid, objects);

    if (moveX != 0) player.facingLeft = moveX < 0;
    if (moveX != 0 || moveY != 0) {
        const float len = std::sqrt(moveX * moveX + moveY * moveY);
        const float dx = moveX / len * MOVE_SPEED * dt, dy = moveY / len * MOVE_SPEED * dt;
        if (noclip) {
            player.x += dx;
            player.y += dy;
        } else {
            moveWithCollision(*this, player.hitbox(area), player.x, player.y, dx, dy, player.onWater);
        }
    }
    player.stile = grid.stileUnder(player.x, player.y, player.stile);
}
