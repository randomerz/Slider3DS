#include "game/triggers.h"

#include <algorithm>

#include "game/events.h"
#include "game/physics.h"
#include "game/world.h"

namespace {

void enterInteraction(World& world, int index) {
    const InteractionDef& in = world.area.interactions[index];
    if (!in.addToOnAction) {
        // PlayerConditionals.CheckCondition on contact.
        if (!in.isCarryingItem) runCalls(world, in.onSuccess);
        return;
    }
    // Offered unless it needs an item (none carried yet) or excludes where the player is.
    if (in.isCarryingItem) return;
    if ((world.player.onWater && in.excludeBoat) || (!world.player.onWater && in.excludeLand)) return;
    if (std::find(world.interactables.begin(), world.interactables.end(), index) == world.interactables.end())
        world.interactables.push_back(index);
}

void exitInteraction(World& world, int index) {
    auto& v = world.interactables;
    v.erase(std::remove(v.begin(), v.end(), index), v.end());
}

void pickUp(World& world, const CollectibleDef& c) {
    // Collectible.DoPickUp (without the cutscene yet): into the inventory, hidden, onCollect.
    world.state.setCollectible(c.name, c.area, true);
    world.objects.setActive(world.area, c.object, false);
    runCalls(world, c.onCollect);
}

// The object's behaviours react to the player entering (or leaving) its trigger.
void dispatch(World& world, int object, bool enter) {
    const Area& area = world.area;
    for (const TriggerEventDef& te : area.triggerEvents)
        if (te.object == object && te.onPlayerEnter) runCalls(world, enter ? te.onEnter : te.onExit);
    for (size_t i = 0; i < area.interactions.size(); i++)
        if (area.interactions[i].object == object) {
            if (enter) enterInteraction(world, (int)i);
            else exitInteraction(world, (int)i);
        }
    if (enter)
        for (const CollectibleDef& c : area.collectibles)
            if (c.object == object) pickUp(world, c);
}

} // namespace

void TriggerTracker::reset(const Area& area) {
    touching_.assign(area.objects.size(), 0);
    now_.assign(area.objects.size(), 0);
}

void TriggerTracker::update(World& world) {
    const Area& area = world.area;
    const Box box = hitboxAt(world.player.hitbox(area), world.player.x, world.player.y);
    std::fill(now_.begin(), now_.end(), (uint8_t)0);
    for (const ObjectCollider& c : area.triggerColliders)
        if (!now_[c.object] && colliderOverlaps(world, c, box)) now_[c.object] = 1;

    // Exits first, then enters; a call may teleport the player or change objects, which the next
    // update sees. After the swap, now_ holds the previous contacts.
    touching_.swap(now_);
    const std::vector<uint8_t>& was = now_;
    for (size_t o = 0; o < was.size(); o++)
        if (was[o] && !touching_[o]) dispatch(world, (int)o, false);
    for (size_t o = 0; o < was.size(); o++)
        if (!was[o] && touching_[o]) dispatch(world, (int)o, true);
}
