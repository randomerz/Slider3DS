#include "game/fx/item_pickup.h"

#include <algorithm>

#include "game/events/events.h"
#include "game/world/world.h"

namespace {

// The circle's keys: flat tangents at both ends (an unweighted Hermite: 3u^2 - 2u^3).
float ease(float u) {
    u = std::min(std::max(u, 0.0f), 1.0f);
    return u * u * (3 - 2 * u);
}

} // namespace

void ItemPickup::reset() { *this = ItemPickup(); }

void ItemPickup::start(World& world, int index) {
    if (active() && step_ == Step::Opening) collect(world); // (a pick-up during one: the first's is in by then)
    const CollectibleDef& c = world.area.collectibles[index];
    world.objects.setActive(world.area, c.object, false); // DespwanCollectable
    world.lockPlayer(LOCK_PICKUP, true);
    world.npcs.setDialogueEnabled(false);
    index_ = index;
    text_ = c.name + " Acquired!"; // ("<item/> Acquired!" with the collectible's translated name: English = its name)
    step_ = Step::Opening;
    circleT_ = 0;
    since_ = world.frame;
    wait_.start(TEXT_AFTER, world.frame);
}

void ItemPickup::collect(World& world) {
    const CollectibleDef& c = world.area.collectibles[index_];
    world.state.setCollectible(c.name, c.area, true); // PlayerInventory.AddCollectible
    runCalls(world, c.onCollect);
    world.saveRequested = true; // SaveSystem.SaveGame
}

void ItemPickup::coroutines(World& world, float dt) {
    if (!active()) return;
    if (world.frame != since_) circleT_ += dt;
    if (!wait_.done(world.frame, dt)) return;
    if (step_ == Step::Opening) { // 0.75 s
        step_ = Step::Shown;
        collect(world);
        wait_.start(SHOWN_FOR, world.frame);
    } else if (step_ == Step::Shown) { // 2 s: isVisible false
        step_ = Step::Closing;
        circleT_ = 0;
        since_ = world.frame;
        wait_.start(CLOSE_FOR, world.frame);
    } else { // 2.5 s: the mask off, the player let go, NPCs talk again
        world.lockPlayer(LOCK_PICKUP, false);
        world.npcs.setDialogueEnabled(true);
        index_ = -1;
        text_.clear();
    }
}

float ItemPickup::circleDiameter() const {
    if (!active()) return 0;
    const float u = ease(circleT_ / CIRCLE_TIME);
    return CIRCLE_SIZE * (step_ == Step::Closing ? 1 - u : u);
}

int ItemPickup::itemSprite(const Area& area) const {
    return active() ? area.collectibles[index_].sprite : -1;
}

