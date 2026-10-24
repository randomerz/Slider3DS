#include "game/events/triggers.h"

#include <algorithm>

#include "game/audio/sounds.h"
#include "game/events/events.h"
#include "game/physics/physics.h"
#include "game/world/world.h"

namespace {

void enterInteraction(World& world, int index) {
    const InteractionDef& in = world.area.interactions[index];
    if (!in.addToOnAction) {
        // PlayerConditionals.CheckCondition on contact.
        if (!in.isCarryingItem) runCalls(world, in.onSuccess);
        return;
    }
    // Offered unless it needs an item the player isn't carrying (checked on entering) or excludes
    // where the player is.
    if (in.isCarryingItem && !world.items.carrying(world.area, in.itemName)) return;
    if ((world.player.onWater && in.excludeBoat) || (!world.player.onWater && in.excludeLand)) return;
    world.offer({Interactable::Kind::Interaction, index});
}

void pickUp(World& world, int index) {
    // Collectible.DoPickUp: ItemPickupEffect's cutscene (hides it; DoOnCollect at 0.75 s: into the
    // inventory, onCollect, saved), its sound ("<area> Pick Up") and its dip in the music (to 0.2
    // over its 2 s: UIEffects' soundDampenLength).
    world.audio.play(world.area.name + " Pick Up");
    world.audio.dampenMusic(0.2f, 2);
    world.pickup.start(world, index);
}

// The object's components react to the player entering (or leaving) its trigger.
void dispatch(World& world, int object, bool enter) {
    const Area& area = world.area;
    using K = ComponentRef::Kind;
    for (const ComponentRef& ref : area.components(object)) {
        switch (ref.kind) {
        case K::TriggerEvent: {
            const TriggerEventDef& te = area.triggerEvents[ref.index];
            if (te.onPlayerEnter) runCalls(world, enter ? te.onEnter : te.onExit);
            break;
        }
        case K::Interaction:
            if (enter) enterInteraction(world, ref.index);
            else world.withdraw({Interactable::Kind::Interaction, ref.index});
            break;
        case K::Collectible:
            if (enter) pickUp(world, ref.index);
            break;
        case K::ConditionChecker:
        case K::Item: // (picked up with the action button, not by touch)
        case K::Npc:  // (its dialogue trigger is a child with an OnTriggerEnter calling it)
            break;
        }
    }
}

} // namespace

void TriggerTracker::reset(const Area& area) {
    touching_.assign(area.objects.size(), 0);
    now_.assign(area.objects.size(), 0);
    lastOnWater_ = -1;
}

void TriggerTracker::update(World& world) {
    const Area& area = world.area;
    // Player.SetIsOnWater swaps the player's collider (foot <-> boat): Unity sends every trigger
    // an exit for the old one and an enter for the new one.
    const int onWater = world.player.onWater ? 1 : 0;
    if (lastOnWater_ >= 0 && onWater != lastOnWater_)
        for (size_t o = 0; o < touching_.size(); o++)
            if (touching_[o]) {
                touching_[o] = 0;
                dispatch(world, (int)o, false);
            }
    lastOnWater_ = onWater;

    const Box box = hitboxAt(world.player.hitbox(area), world.player.x, world.player.y);
    std::fill(now_.begin(), now_.end(), (uint8_t)0);
    if (!world.player.noClip) // (noclip: the player's colliders are off, touching nothing)
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
