// Items the player carries (Item + PlayerAction + PlayerInventory's current item). The action
// button picks up the nearest item whose collider (on an item layer) is within 1 unit of the player
// with a clear line to it; the item flies up over its head along its pick-up curves (only its
// sprite moves until then), its collider turns off and it's carried from then on. With nothing else
// to do, the action button drops it where the drop indicator is: up to 1 unit in the last walking
// direction, short of anything in the way (and not closer than 0.35); the item lands on the STile
// there (parented to it, as Item.DropItem does; carried items are the player's), its collider back
// on, its sprite falling along the same curves. Scripts drop items too (drop(): e.g. Chad's fall),
// each with its own animation. OnPickUp / OnDrop run calls.
// Not yet: canKeep / item cycling, saved item positions, reflections, Item subclasses' behaviour.
#pragma once

#include <functional>
#include <vector>

#include "game/data/area.h"

class World;

class Items {
public:
    static constexpr float PICKUP_RANGE = 1.0f;     // PlayerAction.AttemptItemPickup's overlap circle
    static constexpr float MIN_DROP_DISTANCE = 0.35f;
    static constexpr float MAX_DROP_DISTANCE = 1.0f;
    static constexpr float DROP_BOX_WIDTH = 1.0f;   // the drop box cast: along the direction
    static constexpr float DROP_BOX_HEIGHT = 10.0f / 16.0f; // across it

    void reset(const Area& area); // nothing carried, no animation

    // Animations advance, the carried item follows the player, the drop spot is worked out.
    void update(World& world, float dt);

    // PlayerAction.AttemptItemPickup / AttemptItemDrop; true if something happened.
    bool tryPickUp(World& world);
    bool tryDrop(World& world);
    // Item.DropItem from a script: Area::items[item] lands at world (x, y) (on the STile there), its
    // sprite falling from where the item is; then its OnDrop calls and `done` (the callback).
    void drop(World& world, int item, float x, float y, std::function<void(World&)> done = nullptr);

    int carried() const { return carried_; } // Area::items index, -1 = none
    const ItemDef* carriedDef(const Area& area) const { return carried_ >= 0 ? &area.items[carried_] : nullptr; }
    // PlayerAction.HasItem(name) / Condition playerCarryingItem ("" = any item).
    bool carrying(const Area& area, const std::string& name) const;
    bool busy() const { return phase_ != Phase::None; } // the player's pick-up or drop animation is playing
    // The drop indicator (where a drop would land), if it shows.
    bool dropIndicator(float& x, float& y) const;

    // PlayerAction.SetItemSortingOrderInc / Dec: the carried item's sprite sorting order (houses).
    void sortingOrderInc(int order);
    void sortingOrderDec(int order);
    // The sorting order a carried item's sprite uses, if `spriteObject` is the carried item's.
    bool carriedSortingOrder(const Area& area, int spriteObject, int& order) const;

private:
    enum class Phase { None, PickingUp, Dropping };
    struct Drop {                // Item.AnimateDrop
        int item = -1;
        float t = 0;             // time left
        float startX = 0, startY = 0, endX = 0, endY = 0; // the sprite falls from start to end
        std::function<void(World&)> done;
    };
    // Item.DropItem: the item at (x, y) on the STile there, collider on, its sprite at (fromX, fromY).
    static void startDrop(World& world, Drop& d, int item, float fromX, float fromY, float x, float y);
    static bool stepDrop(World& world, Drop& d, float dt); // true once it has landed (OnDrop ran)
    void computeDrop(const World& world);
    // The item's sprite object at world (x, y) for an animation (its pivot offset kept), or back on the item.
    static void placeSprite(World& world, const ItemDef& it, float x, float y);
    static void resetSprite(World& world, const ItemDef& it);

    int carried_ = -1;
    int animated_ = -1;      // the item the player's pick-up animation moves
    Phase phase_ = Phase::None;
    float t_ = 0;            // pick-up: time since it started
    float startX_ = 0, startY_ = 0; // pick-up: where the item was
    Drop playerDrop_;        // Phase::Dropping
    std::vector<Drop> drops_; // scripts' drops
    bool canDrop_ = false;
    float dropX_ = 0, dropY_ = 0;
    int sortingOrder_ = 0, sortingOrderCalls_ = 0;
};
