#include "game/village/final_puzzle.h"

#include <cstdio>
#include <string>

#include "game/village/village_grid.h"
#include "game/world/world.h"

namespace village {

namespace {

constexpr float FAST_SLIDES = 0.5f;        // gridAnimator.ChangeMovementDuration(0.5f)
constexpr int FLASHES = 3;                 // FlashSymbol: Flash(3)
constexpr float COMPLETIONS_DELAY = 1.2f;  // PlaceTile9: CheckCompletionsAfterDelay(1.2f)

// ShuffleBuildUp, step by step: a camera shake, the symbol it flashes (-1 = none), whether the white
// flash and the shuffle come with it, and the wait after it (-1 = the end).
struct ShuffleStep {
    float shakeDuration, shakeAmount;
    int symbol;
    bool shuffle;
    float wait;
};
constexpr ShuffleStep SHUFFLE_STEPS[] = {
    {0.25f, 0.25f, 0, false, 1},
    {0.25f, 0.25f, 1, false, 1},
    {0.75f, 0.5f, 2, false, 1},
    {1.5f, 2.5f, 3, false, 0.25f},
    {0, 0, -1, true, 0.75f},      // UIEffects.FlashWhite, DoShuffle
    {2, 0.9f, -1, false, -1},
};
constexpr int SHUFFLE_STEP_COUNT = (int)(sizeof(SHUFFLE_STEPS) / sizeof(SHUFFLE_STEPS[0]));

} // namespace

void FinalPuzzle::reset(const Area&) { *this = FinalPuzzle(); }

void FinalPuzzle::start(World& world) {
    const Area& area = world.area;
    const bool complete = world.state.flag(VILLAGE_COMPLETION_FLAG);
    world.grid.durationMultiplier = complete ? FAST_SLIDES : 1; // VillageGrid.Load
    rememberButtons(world);
    if (complete) updateCompletions(world); // SGrid.Start
    // CheckHole (against villageCompletion, not a filled hole, to avoid a softlock).
    const int fragment = area.areaGrid.ruinsFragment;
    if (fragment >= 0) {
        const ItemDef& it = area.items[fragment];
        if (complete) {
            world.objects.setActive(area, it.object, false);
        } else if (world.state.hasCollectible("Slider 3", "Caves") && area.areaGrid.slider8Floor >= 0) {
            // Caves first: the fragment lies where Chad would have dropped it.
            world.objects.moveOnto(area, it.object, area.areaGrid.slider8Floor);
            if (it.colliderSlot >= 0) world.objects.setColliderEnabled(it.colliderSlot, true);
        }
    }
    if (world.state.flag(PENDING_COMPLETE_FLAG) && !world.state.hasCollectible(FINAL_SLIDER, area.name))
        placing_.start(world.frame); // PlaceTile9
}

void FinalPuzzle::activate(World& world) {
    const RuinsSymbolsDef& rs = world.area.areaGrid.ruinsSymbols;
    if (started_ || !rs.present || !world.objects.activeInHierarchy(rs.object)) return;
    started_ = true;
    if (world.state.flag(VILLAGE_COMPLETION_FLAG)) { // the runes as the shuffle left them
        world.objects.setRendererEnabled(rs.hole, false);
        setSymbols(world, true);
    }
}

void FinalPuzzle::setSymbols(World& world, bool shown) {
    const RuinsSymbolsDef& rs = world.area.areaGrid.ruinsSymbols;
    if (!rs.present) return;
    for (int s : rs.symbols) world.objects.setRendererEnabled(s, shown);
}

void FinalPuzzle::flashSymbol(World& world, int symbol) {
    const RuinsSymbolsDef& rs = world.area.areaGrid.ruinsSymbols;
    if (!rs.present) return;
    world.objects.setRendererEnabled(rs.symbols[symbol], true);
    // FlashWhiteSprite.Flash: white at once (StopAllCoroutines: a running flash starts over).
    world.objects.setFill(rs.symbols[symbol], rs.flashColor);
    flashes_[symbol].half = 0;
    flashes_[symbol].wait.start(rs.flashTime[symbol], world.frame);
}

void FinalPuzzle::fillInHole(World& world) {
    const Area& area = world.area;
    if (world.state.flag(VILLAGE_COMPLETION_FLAG)) return;
    const int item = world.items.removeCarried(); // PlayerInventory.RemoveItem
    if (item >= 0) world.objects.setActive(area, area.items[item].object, false);
    if (area.areaGrid.ruinsSymbols.present) world.objects.setRendererEnabled(area.areaGrid.ruinsSymbols.hole, false);
    setSymbols(world, false); // (and the "Puzzle Complete" sound, a SmokePoof)
}

void FinalPuzzle::shufflePuzzle(World& world) {
    if (shuffleStep_ >= 0) return; // shuffleBuildUpCoroutine is set
    shuffleStep_ = 0;
    stepShuffle(world, 0); // StartCoroutine: the first step runs at once
}

void FinalPuzzle::doShuffle(World& world) {
    const Area& area = world.area;
    int collected = 0;
    for (const STileDef& s : area.stiles) collected += world.grid.tileActive(s.islandId) ? 1 : 0;
    if (collected != 8) {
        std::printf("Tried to shuffle village when not 8 tiles were collected! Detected %d tiles.\n", collected);
        return;
    }
    world.setGrid(SHUFFLED_GRID);
    world.grid.durationMultiplier = FAST_SLIDES;
    world.state.setFlag(AUTO_MOVE_FLAG, true);
    world.state.setFlag(VILLAGE_COMPLETION_FLAG, true); // checkCompletion
}

void FinalPuzzle::rememberButtons(const World& world) {
    const Artifact& a = world.artifact;
    buttons_.resize((size_t)(a.width() * a.height()));
    for (int y = 0; y < a.height(); y++)
        for (int x = 0; x < a.width(); x++) buttons_[(size_t)(y * a.width() + x)] = a.buttonAt(x, y);
}

void FinalPuzzle::checkButtons(World& world) {
    if (world.state.version() != stateVersion_) { // (a flag lookup only when the state changed)
        stateVersion_ = world.state.version();
        completionsOn_ = world.state.flag(VILLAGE_COMPLETION_FLAG);
    }
    if (!completionsOn_) return;
    const Artifact& a = world.artifact;
    bool moved = buttons_.size() != (size_t)(a.width() * a.height());
    for (int y = 0; y < a.height() && !moved; y++)
        for (int x = 0; x < a.width() && !moved; x++) moved = buttons_[(size_t)(y * a.width() + x)] != a.buttonAt(x, y);
    if (!moved) return;
    rememberButtons(world);
    updateCompletions(world);
}

void FinalPuzzle::updateCompletions(World& world) {
    // SGrid.UpdateButtonCompletionsHelper: a button is complete in its target slot ('*': any);
    // ArtifactTileButton.SetComplete ignores buttons whose tile isn't collected.
    const Artifact& a = world.artifact;
    for (int y = 0; y < a.height(); y++)
        for (int x = 0; x < a.width(); x++) {
            const char want = world.grid.targetAt(x, y);
            const int id = a.buttonAt(x, y);
            if (world.grid.tileActive(id)) world.artifactButtons.setComplete(id, want == '*' || charToId(want) == id);
        }
    // CheckFinalPlacements.
    const std::string& name = world.area.name;
    if (!world.state.hasCollectible(FINAL_SLIDER, name) && a.gridString(world.grid) == SOLVED_GRID && !placing_.active) {
        world.artifact.canAddMoves = false; // UIArtifact.DisableMovement(false): queued moves still run
        world.state.setFlag(AUTO_MOVE_FLAG, false);
        world.state.setFlag(PENDING_COMPLETE_FLAG, true);
        placing_.start(world.frame); // PlaceTile9 (and the sound, achievements)
    }
}

void FinalPuzzle::giveFinalSlider(World& world) {
    // SGrid.GivePlayerTheCollectible: ActivateCollectible, then at the player, unparented.
    const Area& area = world.area;
    const int c = area.collectible(FINAL_SLIDER);
    if (c < 0) {
        std::printf("Couldn't find collectible with name: %s\n", FINAL_SLIDER);
        return;
    }
    activateCollectible(world, FINAL_SLIDER, true);
    const int object = area.collectibles[c].object;
    world.objects.setParent(area, object, -1);
    world.placeObject(object, 0, world.player.x, world.player.y);
}

void FinalPuzzle::stepShuffle(World& world, float dt) {
    // A step runs when the wait before it is done (the first at once); each wait counts from the
    // frame after its step.
    while (shuffleStep_ >= 0 && shuffleStep_ < SHUFFLE_STEP_COUNT) {
        if (shuffleStep_ > 0 && !shuffleWait_.done(world.frame, dt)) break;
        const ShuffleStep& s = SHUFFLE_STEPS[shuffleStep_++];
        if (s.shakeDuration > 0) world.effects.shake(s.shakeDuration, s.shakeAmount); // (and the rumbles)
        if (s.symbol >= 0) flashSymbol(world, s.symbol);
        if (s.shuffle) {
            world.effects.flashWhite();
            doShuffle(world);
        }
        if (s.wait < 0) break;
        shuffleWait_.start(s.wait, world.frame);
    }
}

void FinalPuzzle::coroutines(World& world, float dt) {
    stepShuffle(world, dt);
    // FlashWhiteSprite._Flash: white and normal for flashTime each, FLASHES times (chained waits).
    const RuinsSymbolsDef& rs = world.area.areaGrid.ruinsSymbols;
    for (int k = 0; k < RuinsSymbolsDef::SYMBOLS; k++) {
        Flash& f = flashes_[k];
        if (f.half < 0 || !f.wait.done(world.frame, dt)) continue;
        if (++f.half >= 2 * FLASHES) {
            f.half = -1;
            world.objects.setFill(rs.symbols[k], 0);
            continue;
        }
        world.objects.setFill(rs.symbols[k], f.half % 2 == 0 ? rs.flashColor : 0);
        f.wait.start(rs.flashTime[k], world.frame);
    }
    if (placing_.done(world.frame, world.artifact.queueEmpty())) {
        giveFinalSlider(world);
        completionsDelay_.start(COMPLETIONS_DELAY, world.frame);
    }
    if (completionsDelay_.done(world.frame, dt)) updateCompletions(world); // the final button shows complete
}

} // namespace village
