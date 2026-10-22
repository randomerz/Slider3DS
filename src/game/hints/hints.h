// The tutorial hints: PlayerActionHints (each hint's countdown from TriggerHint to showing it, and
// its DisableHint / EnableDisabling rules) + StileMoveHint (calls on tile moves) + UIHints (the box
// along the bottom of the top screen showing one hint at a time, in the order they came; it fades
// in / out over Area::hints.fadeDuration, and a removed hint's text fades out and the next one's in).
// The hints' state is kept in GameState like the original's Save keys ("HintCountdown <name>",
// "HintComplete <name>"; its "Hint <name>" = shouldDisplay is always !complete, so it isn't kept),
// written as it changes and read back on a scene load. Texts are the 3DS's (`text`). Part of World:
// calls (game/events), the player's input (World::update / action) and grid events reach it there.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"

class World;

class Hints {
public:
    // A scene load: PlayerActionHints.OnEnable's Load (triggerOnLoad starts its countdown) and
    // UIHints.Clear (nothing shown).
    void load(World& world);
    void trigger(World& world, int hint);         // TriggerHint
    void disable(World& world, int hint);         // DisableHint (only once it can be)
    void enableDisabling(int hint);               // EnableDisabling
    // StileMoveHint: their calls for these grid events (each one active hears them).
    void gridEvents(World& world, const std::vector<GridEvent>& events);
    // One frame: the countdowns (Update), then the box's fades (its coroutines).
    void update(World& world, float dt);

    // The box: its alpha (CanvasGroup) and its text's (0..1), the hint shown (-1 = none).
    float boxAlpha() const { return boxAlpha_; }
    float textAlpha() const { return textAlpha_; }
    int shownHint() const { return active_; }

    // A hint's text on the 3DS (HintData.GetFormattedHintText with the 3DS's buttons): its
    // placeholders as the 3DS's buttons ("Up/Left/Down/Right" as the Circle Pad), or the port's own
    // text where the original's names a mouse or a key; "" = the port doesn't show it.
    static std::string text(const HintDef& hint);

private:
    struct State {
        bool countdown = false;  // isInCountdown
        bool canDisable = false; // canDisableHint
        bool complete = false;   // hasBeenCompleted (shouldDisplay = !complete)
        bool added = false;      // hasBeenAddedToDisplay
        bool shown = false;      // the 3DS shows it (it has a text)
        float left = 0;          // timeUntilTrigger
    };
    void save(World& world, int hint) const; // its Save keys, into GameState

    // UIHints.
    enum class Fade { None, BoxIn, BoxOut, TextOut, TextIn };
    void add(World& world, int hint);    // AddHint
    void remove(World& world, int hint); // RemoveHint
    void refresh(World& world);          // UpdateHint
    void startFade(World& world, Fade fade);

    std::vector<State> s_;
    std::vector<int> list_, removals_; // hintList, hintRemovalQueue (hint indices)
    bool visible_ = false;             // isVisible
    int active_ = -1;                  // activeHint
    Fade fade_ = Fade::None;
    float fadeT_ = 0;
    uint32_t fadeSince_ = 0; // World::frame the fade started (it moves from the next)
    float boxAlpha_ = 0, textAlpha_ = 1;
};
