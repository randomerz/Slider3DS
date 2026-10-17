// The Village's end (VillageGrid's "Puzzle 8" + RuinsSymbols), ported step for step. The
// Archeologist's (or Fezziwig's) dialogue calls FillInHole (the carried Ruins Fragment goes into
// the hole: hole and symbols hide) and then ShufflePuzzle: the symbols flash back one by one with
// camera shakes, a white flash, and the grid jumps to a scramble (SetGrid) with faster slides,
// auto-move and completions on (villageCompletion). From then on every change to the Artifact's
// buttons updates the completed sprites (UpdateButtonCompletions) and checks for the solution
// (CheckFinalPlacements): Artifact movement stops, and once the queue is empty the Slider 9
// collectible appears under the player (PlaceTile9); 1.2 s later completions update again.
// VillageGrid.Start: CheckHole (the fragment: gone after completion, in the hole if the Caves came
// first) and a pending completion's PlaceTile9; RuinsSymbols.Start shows the finished runes.
// Completions on button changes are SGrid behaviour (checkCompletion); they live here while the
// Village is the only grid. Without exported RuinsSymbols the calls still run, minus the symbols.
// Not yet: sounds, the SmokePoof particles (a textured sheet animation), achievements, the world map.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"
#include "game/world/wait.h"

class World;

namespace village {

constexpr const char* SHUFFLED_GRID = "182_943_765"; // DoShuffle's {7,0,1},{6,4,8},{5,3,2} ([x, y]; 0 = 9)
constexpr const char* SOLVED_GRID = "624_8#7_153";   // CheckFinalPlacements
constexpr const char* FINAL_SLIDER = "Slider 9";

class FinalPuzzle {
public:
    void reset(const Area& area); // a scene load: nothing running
    // VillageGrid.Load + Start (World::reloadObjects, after Starts): slide speed and completions from
    // villageCompletion, CheckHole, a pending completion's PlaceTile9.
    void start(World& world);
    // RuinsSymbols.Start on its object's first activation. World calls it with Starts.
    void activate(World& world);

    void fillInHole(World& world);    // VillageGrid.FillInHole
    void shufflePuzzle(World& world); // VillageGrid.ShufflePuzzle (once per scene)

    // UpdateButtonCompletions on OnGridMove / OnButtonInteract (subscribed once completions are on):
    // here whenever the Artifact's buttons have moved since last time. World calls it after the
    // frame's Artifact update (the stylus has queued its moves by then).
    void checkButtons(World& world);
    // SGrid.SetGrid (World::setGrid) sends neither event: the new layout isn't a change to react to.
    void gridSet(const World& world) { rememberButtons(world); }
    // The coroutines (ShuffleBuildUp, PlaceTile9, CheckCompletionsAfterDelay) and the symbols'
    // FlashWhiteSprite flashes: in World's coroutine slot.
    void coroutines(World& world, float dt);

    bool placingTile9() const { return placing_.active; } // PlaceTile9 is waiting for the move queue
    // UpdateButtonCompletionsHelper (+ CheckFinalPlacements) now, e.g. after a dev setGrid.
    void updateCompletions(World& world);

private:
    void flashSymbol(World& world, int symbol);   // RuinsSymbols.FlashSymbol
    void setSymbols(World& world, bool shown);     // RuinsSymbols.SetSprites
    void stepShuffle(World& world, float dt);      // ShuffleBuildUp: the steps due now
    void doShuffle(World& world);
    void giveFinalSlider(World& world);            // GivePlayerTheCollectible("Slider 9")
    void rememberButtons(const World& world);

    bool started_ = false; // RuinsSymbols.Start ran
    // ShuffleBuildUp: the next step, -1 = not started; it runs once (the coroutine field stays set).
    int shuffleStep_ = -1;
    Wait shuffleWait_;
    WaitUntil placing_;         // PlaceTile9: until the move queue is empty
    Wait completionsDelay_;     // CheckCompletionsAfterDelay
    struct Flash {              // FlashWhiteSprite._Flash: white, normal, ... (2 * FLASHES halves)
        int half = -1;          // -1 = not flashing
        Wait wait;
    };
    Flash flashes_[RuinsSymbolsDef::SYMBOLS];
    std::vector<int> buttons_;  // the Artifact's buttons when completions last looked
    uint32_t stateVersion_ = UINT32_MAX; // GameState::version() when completionsOn_ was read
    bool completionsOn_ = false;         // villageCompletion (checkCompletion)
};

} // namespace village
