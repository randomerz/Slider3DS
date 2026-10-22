// ItemPickupEffect (UIEffects' WorldCanvas + EffectCanvas): a collectible's pick-up cutscene
// (Collectible.DoPickUp -> StartCutscene, 2.5 s). The collectible hides and the player is held
// (PlayerLock: no moving, acting, pausing or Artifact; PauseManager's restriction) and NPCs' dialogue
// is off (Npcs::setDialogueEnabled: dialogueEnabledAllNPC — no new lines, conditionals wait, so an
// NPC's reaction to the item comes after the cutscene); a dark circle
// (the CircleMask: Unity's Circle sprite masking a #1a1a1a panel) grows from the player to 40 units
// across in 0.5 s (the ItemEffect Animator's ItemExpandCircle: a flat-tangent ease), over the world
// and under the player (its sprite moves to the ScreenEffects sorting layer, the canvas sits there
// at order -1); the item shows 1 unit above the player at its native size; at 0.75 s "<item>
// Acquired!" shows (EffectCanvas, 50 px above the view's middle) and DoOnCollect runs (inventory,
// onCollect, save); at 2 s the text hides and the circle shrinks back in 0.5 s; then all is as before.
// The sound and the music dip are started by the pick-up (game/events/triggers). Part of World; its waits run in the coroutine slot (game/world/wait.h).
#pragma once

#include <cstdint>
#include <string>

#include "game/world/wait.h"

class World;
struct Area;

class ItemPickup {
public:
    static constexpr float TEXT_AFTER = 0.75f;  // WaitForSeconds(0.75): the text, DoOnCollect
    static constexpr float SHOWN_FOR = 1.25f;   // then WaitForSeconds(1.25): isVisible false
    static constexpr float CLOSE_FOR = 0.5f;    // then WaitForSeconds(0.5): the mask off
    static constexpr float CIRCLE_SIZE = 40;    // ItemVisible's size (units across)
    static constexpr float CIRCLE_TIME = 0.5f;  // ItemExpandCircle / ItemContractCircle
    static constexpr int SORT_LAYER = 10;       // ScreenEffects (TagManager's order: Default = 0, Entity = 7)
    static constexpr int ORDER = -1;            // WorldCanvas' sorting order (the player's stays 0)
    static constexpr float ITEM_UP = 1;         // the item Image, 1 unit above the player

    void reset(); // nothing running (a scene load)
    // StartCutscene for collectible `index` (Area::collectibles). One already running ends first
    // (its DoOnCollect runs if it hadn't).
    void start(World& world, int index);
    // Its coroutine: one frame (the coroutine slot).
    void coroutines(World& world, float dt);

    bool active() const { return index_ >= 0; }
    float circleDiameter() const; // units, 0 = none
    int itemSprite(const Area& area) const; // Area::sprites index while shown, -1 = none
    bool textShown() const { return textShown_; }
    std::string text(const Area& area) const; // "<item> Acquired!"

private:
    void collect(World& world); // Collectible.DoOnCollect, once

    enum class Step : uint8_t { Shown, Closing };
    int index_ = -1;
    bool collected_ = false, textShown_ = false;
    Step step_ = Step::Shown;
    float circleT_ = 0; // s into the circle's current animation
    uint32_t since_ = 0; // World::frame the current animation started (it moves from the next)
    Wait wait_;
};
