#include "game/world/world.h"

#include <algorithm>
#include <cmath>

#include "game/events/events.h"
#include "game/physics/physics.h"

namespace {

void setTileObjects(World& world, const STileDef& s, bool active) {
    if (s.objectsObject >= 0) world.objects.setActive(world.area, s.objectsObject, active);
    if (s.tileMapsObject >= 0) world.objects.setActive(world.area, s.tileMapsObject, active);
}

} // namespace

World::World(const Area& a) : area(a) {
    state.newProfile("Cat"); // (until a new-game screen names the profile)
    grid.init(area);
    artifact.init(grid);
    reloadObjects();
}

void World::reloadObjects() {
    objects.init(area);
    for (const STileDef& s : area.stiles) setTileObjects(*this, s, grid.tileActive(s.islandId));
    // SGrid.Start: collectibles already in the inventory stay picked up.
    for (const CollectibleDef& c : area.collectibles)
        if (state.hasCollectible(c.name, c.area)) objects.setActive(area, c.object, false);
    triggers.reset(area);
    interactables.clear();
    npcs.reset(area);
    starts.reset(area);
    starts.update(*this);
}

void World::setTileActive(int islandId, bool active) {
    grid.setTileActive(islandId, active);
    if (const STileDef* s = area.stile(islandId)) setTileObjects(*this, *s, active);
    starts.update(*this); // Start runs before the newly active objects' first update
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

    // Start runs before an object's first update (here: before triggers can touch it), and again
    // for what calls activated. Each skips when nothing became active.
    starts.update(*this);
    triggers.update(*this);
    starts.update(*this);
    npcs.update(*this, dt);
    starts.update(*this);
    if (effects.fade > 0)
        effects.fade = area.fadeDuration > 0 ? std::max(0.0f, effects.fade - dt / area.fadeDuration) : 0;
}

void World::offer(const Interactable& it) {
    if (std::find(interactables.begin(), interactables.end(), it) == interactables.end()) interactables.push_back(it);
}

void World::withdraw(const Interactable& it) {
    interactables.erase(std::remove(interactables.begin(), interactables.end(), it), interactables.end());
}

int World::priority(const Interactable& it) const {
    switch (it.kind) {
    case Interactable::Kind::Interaction: return area.interactions[it.index].priority;
    case Interactable::Kind::Npc: return 0; // NPCDialogueContext's priority isn't serialized
    }
    return 0;
}

bool World::showsPrompt(const Interactable& it) const {
    switch (it.kind) {
    case Interactable::Kind::Interaction: return true; // PlayerConditionals: IInteractable's default
    case Interactable::Kind::Npc: return npcs.showsPrompt(area, it.index);
    }
    return true;
}

void World::interact() {
    if (interactables.empty()) return;
    int highest = priority(interactables[0]);
    for (const Interactable& it : interactables) highest = std::max(highest, priority(it));
    // Calls may change what's on offer (a door teleports the player away), so run a snapshot.
    const std::vector<Interactable> offered = interactables;
    for (const Interactable& it : offered) {
        if (priority(it) != highest) continue;
        switch (it.kind) {
        case Interactable::Kind::Interaction: {
            const InteractionDef& in = area.interactions[it.index];
            if (!in.isCarryingItem) runCalls(*this, in.onSuccess); // PlayerConditionals.CheckCondition
            break;
        }
        case Interactable::Kind::Npc:
            npcs.interact(*this, it.index);
            break;
        }
    }
}

bool World::promptShown() const {
    return std::any_of(interactables.begin(), interactables.end(), [&](const Interactable& it) { return showsPrompt(it); });
}

void World::objectPosition(int object, float& x, float& y) const {
    objectBase(object, x, y);
    x += area.objects[object].x;
    y += area.objects[object].y;
}

void World::objectBase(int object, float& x, float& y) const {
    grid.ownerBase(objects.owner(object), x, y);
    x += objects.offsetX(object);
    y += objects.offsetY(object);
}
