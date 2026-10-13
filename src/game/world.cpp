#include "game/world.h"

#include <algorithm>
#include <cmath>

#include "game/events.h"
#include "game/physics.h"

World::World(const Area& a) : area(a) {
    grid.init(area);
    artifact.init(grid);
    reloadObjects();
}

void World::reloadObjects() {
    objects.init(area);
    // SGrid.Start: collectibles already in the inventory stay picked up.
    for (const CollectibleDef& c : area.collectibles)
        if (state.hasCollectible(c.name, c.area)) objects.setActive(area, c.object, false);
    triggers.reset(area);
    interactables.clear();
    starts.reset(area);
    starts.update(*this);
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

    if (moveX != 0) player.facingLeft = moveX < 0;
    float dx = 0, dy = 0;
    if (moveX != 0 || moveY != 0) {
        const float len = std::sqrt(moveX * moveX + moveY * moveY);
        dx = moveX / len * MOVE_SPEED * dt;
        dy = moveY / len * MOVE_SPEED * dt;
    }
    if (noclip) {
        player.x += dx;
        player.y += dy;
    } else {
        // Also with no input: Box2D pushes a resting player out of whatever it overlaps.
        moveWithCollision(*this, player.hitbox(area), player.x, player.y, dx, dy, player.onWater);
    }
    player.stile = grid.stileUnder(player.x, player.y, player.stile);

    triggers.update(*this);
    starts.update(*this); // components that became active this frame (e.g. shown by a trigger)
    if (effects.fade > 0)
        effects.fade = area.fadeDuration > 0 ? std::max(0.0f, effects.fade - dt / area.fadeDuration) : 0;
}

void World::offer(const Interactable& it) {
    for (const Interactable& o : interactables)
        if (o.sameAs(it)) return;
    interactables.push_back(it);
}

void World::withdraw(const Interactable& it) {
    interactables.erase(std::remove_if(interactables.begin(), interactables.end(),
                                       [&](const Interactable& o) { return o.sameAs(it); }),
                        interactables.end());
}

void World::interact() {
    if (interactables.empty()) return;
    int highest = interactables[0].priority;
    for (const Interactable& it : interactables) highest = std::max(highest, it.priority);
    // Calls may change what's on offer (a door teleports the player away), so run a snapshot.
    const std::vector<Interactable> offered = interactables;
    for (const Interactable& it : offered) {
        if (it.priority != highest) continue;
        switch (it.kind) {
        case Interactable::Kind::Interaction: {
            const InteractionDef& in = area.interactions[it.index];
            if (!in.isCarryingItem) runCalls(*this, in.onSuccess); // PlayerConditionals.CheckCondition
            break;
        }
        }
    }
}

bool World::promptShown() const {
    return std::any_of(interactables.begin(), interactables.end(), [](const Interactable& it) { return it.showsPrompt; });
}

void World::objectBase(int object, int exportedOwner, float& x, float& y) const {
    grid.ownerBase(objects.owner(object, exportedOwner), x, y);
    x += objects.offsetX(object);
    y += objects.offsetY(object);
}
