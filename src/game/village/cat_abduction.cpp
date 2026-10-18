#include "game/village/cat_abduction.h"

#include <cmath>

#include "game/data/curve.h"
#include "game/village/village_grid.h"
#include "game/world/world.h"

namespace village {

namespace {

constexpr float TIME_ABDUCT_NORMAL = 6;       // s after the intro cutscene ends
constexpr float TIME_ABDUCT_PICKED = 3;       // s after a pick-up
constexpr float TIME_ABDUCT_FINISH_LERP = 2;  // s drifting onto the sine wave
constexpr float EXPLORESSE_DELAY = 1.4f;      // DoExploresseSequence's wait
constexpr int CAT_SORTING_ORDER = 99;
constexpr const char* EXPLORESSE_SEQ_FLAG = "villageExploresseSequenceDone";
constexpr const char* CAT_FLOAT_AWAY_FLAG = "villageCatFloatAway";

// Item.SetCollider(false), Item.SetSortingOrder(99), transform.SetParent(null).
void freeCat(World& world, const ItemDef& cat) {
    world.objects.setColliderEnabled(cat.colliderSlot, false);
    world.objects.setSortingOrder(cat.spriteObject, CAT_SORTING_ORDER);
    world.objects.setParent(world.area, cat.object, -1);
}

void setBubble(World& world, const CatAbductionDef& d, bool on) { // CatBubbleController.SetBubbleActive
    world.objects.setRendererEnabled(d.bubble, on);
    world.objects.setRendererEnabled(d.electricity, on);
    if (on) world.animators.setTrigger(world, d.bubbleAnimator, "formBubble");
}

} // namespace

void CatAbduction::start(World& world) {
    const CatAbductionDef& d = world.area.areaGrid.catAbduction;
    if (!d.present) return;
    sinX_ = d.sinX;
    sinY_ = d.sinY;
    if (!world.state.flag(INTRO_CUTSCENE_FLAG)) return;
    world.state.setFlag(EXPLORESSE_SEQ_FLAG, true);
    const ItemDef& cat = world.area.items[d.item];
    freeCat(world, cat);
    placeCat(world, d.landingX, d.landingY);
}

void CatAbduction::pickedUp() {
    picked_ = true;
    sinceIntro_ = 0; // (so the player can enjoy carrying the cat)
}

void CatAbduction::update(World& world, float dt) {
    const CatAbductionDef& d = world.area.areaGrid.catAbduction;
    if (!d.present) return;
    if (!started_) {
        if (introFinished_ && (sinceIntro_ += dt) > TIME_ABDUCT_NORMAL) startAbducting(world);
        if (picked_ && (sincePicked_ += dt) > TIME_ABDUCT_PICKED) startAbducting(world);
    }
    if (sinOn_) { // SinWaveAnimation.Update
        sinOrigX_ += sinVelocity_ * dt;
        sinX_ = sinOrigX_;
        sinY_ = sinOrigY_ + d.sinA * std::sin(d.sinB * clock_ + d.sinC) + d.sinD;
    }
    clock_ += dt; // (Time.time is the frame's start)
}

void CatAbduction::startAbducting(World& world) {
    if (started_) return;
    started_ = true;
    const CatAbductionDef& d = world.area.areaGrid.catAbduction;
    const ItemDef& cat = world.area.items[d.item];
    // PlayerInventory.RemoveItem. (Unity drops whatever the player carries from the inventory; the
    // port only lets go of the cat, so another carried item isn't stranded on the player.)
    if (world.items.carried() == d.item) world.items.removeCarried();
    float x, y;
    world.objectPosition(cat.object, x, y);
    freeCat(world, cat);
    placeCat(world, x, y); // (where it was: SetParent keeps the world position)
    // AbductCat, up to its first wait.
    world.effects.shake(0.5f, 0.5f);
    setBubble(world, d, true);
    world.animators.setBool(world, d.animator, "isAwake", true);
    step_ = Step::Waking;
    wait_.start(1, world.frame);
}

void CatAbduction::placeCat(World& world, float x, float y) {
    world.placeObject(world.area.items[world.area.areaGrid.catAbduction.item].object, 0, x, y);
}

void CatAbduction::startLerp(World& world) {
    // (A pick-up animation still running when the abduction started puts the cat in the player's
    // hands as it ends: it drifts off from there, as Unity reads the transform.)
    world.objectPosition(world.area.items[world.area.areaGrid.catAbduction.item].object, fromX_, fromY_);
    t_ = 0;
}

void CatAbduction::lerpCat(World& world, float toX, float toY, float duration, float dt) {
    const float k = world.area.areaGrid.catAbduction.lerpCurve.eval(t_ / duration);
    placeCat(world, mathf::lerp(fromX_, toX, k), mathf::lerp(fromY_, toY, k));
    t_ += dt;
}

void CatAbduction::coroutines(World& world, float dt) {
    const CatAbductionDef& d = world.area.areaGrid.catAbduction;
    if (!d.present) return;
    if (exploresse_.done(world.frame, dt)) world.state.setFlag(CAT_FLOAT_AWAY_FLAG, true); // (and a door sound)
    switch (step_) {
    case Step::None:
    case Step::Done:
        break;
    case Step::Waking:
        if (!wait_.done(world.frame, dt)) break;
        exploresse_.start(EXPLORESSE_DELAY, world.frame);
        if (!sinOn_) { // (its Start, before its first Update: where it was authored)
            sinOrigX_ = d.sinX;
            sinOrigY_ = d.sinY;
        }
        sinOn_ = true;
        sinVelocity_ = 2;
        startLerp(world);
        step_ = Step::ToWave;
        lerpCat(world, sinX_, sinY_, TIME_ABDUCT_FINISH_LERP, dt);
        break;
    case Step::ToWave:
        if (t_ < TIME_ABDUCT_FINISH_LERP) {
            lerpCat(world, sinX_, sinY_, TIME_ABDUCT_FINISH_LERP, dt);
        } else {
            step_ = Step::OverWater; // (once over the water the cat rides the wave)
            overWater_.start(world.frame);
        }
        break;
    case Step::OverWater: {
        bool there = false;
        if (overWater_.polling(world.frame)) { // the WaitUntil's predicate, with its side effects
            if (world.player.x > sinX_) sinVelocity_ = 2.5f;
            placeCat(world, sinX_, sinY_);
            there = sinX_ > d.landingX - 1;
        }
        if (!overWater_.done(world.frame, there)) break;
        // Pop: the bubble bursts and the cat drops onto the landing spot.
        sinOn_ = false;
        sinVelocity_ = 0;
        setBubble(world, d, false);
        startLerp(world);
        step_ = Step::Dropping;
        lerpCat(world, d.landingX, d.landingY, 1, dt);
        break;
    }
    case Step::Dropping:
        if (t_ < 1) {
            lerpCat(world, d.landingX, d.landingY, 1, dt);
        } else {
            placeCat(world, d.landingX, d.landingY);
            step_ = Step::Settling;
            wait_.start(1, world.frame);
        }
        break;
    case Step::Settling:
        if (!wait_.done(world.frame, dt)) break;
        world.animators.setBool(world, d.animator, "isAwake", false);
        step_ = Step::Done;
        break;
    }
}

} // namespace village
