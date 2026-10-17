// The cat's abduction (CatAbductionController) on the exported Village: after the intro (or a
// pick-up) a bubble carries the cat off along a sine wave to the beach. Also the Animator triggers
// and null sprite keys its bubble brought in.
#include <cmath>
#include <string>

#include "game/village/village_grid.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

void frames(World& world, int n) {
    for (int i = 0; i < n; i++) world.update(DT, 0, 0, false);
}

const CatAbductionDef& def(const World& world) { return world.area.areaGrid.catAbduction; }
const ItemDef& cat(const World& world) { return world.area.items[def(world).item]; }

void catAt(const World& world, float& x, float& y) { world.objectPosition(cat(world).object, x, y); }

bool bubbleShown(const World& world) {
    return world.objects.rendererEnabled(def(world).bubble) && world.objects.rendererEnabled(def(world).electricity);
}

bool awake(const World& world) { return world.animators.param(world.area, def(world).animator, "isAwake"); }

// Frames until the abduction starts (at most `limit`), -1 if it doesn't.
int framesUntilStarted(World& world, int limit) {
    for (int n = 1; n <= limit; n++) {
        frames(world, 1);
        if (world.abduction.started()) return n;
    }
    return -1;
}

} // namespace

TEST(cat_abduction_export) {
    const Area& area = villageArea();
    const CatAbductionDef& d = area.areaGrid.catAbduction;
    CHECK(d.present && area.items[d.item].name == "Cat");
    CHECK(area.animators[d.animator].object == area.items[d.item].spriteObject);
    CHECK(area.animators[d.bubbleAnimator].object == d.bubble);
    CHECK(d.sinX == 3 && d.sinY == 5 && d.sinA == 1 && d.sinB == 1 && d.sinC == 0 && d.sinD == 0);
    CHECK(d.landingX == 55.5f && d.landingY == 2.5f);
    CHECK(d.lerpCurve.eval(0) == 0 && std::fabs(d.lerpCurve.eval(0.5f) - 0.5f) < 1e-4f && d.lerpCurve.eval(1) == 1);
    // The bubble's controller: a trigger into a form clip that's one sprite, then null keys.
    const AnimatorControllerDef& c = area.animatorControllers[area.animators[d.bubbleAnimator].controller];
    const int p = c.paramIndex("formBubble");
    CHECK(p >= 0 && c.paramTriggers[p] && !c.paramDefaults[p]);
    const int form = c.stateIndex("BubbleForm");
    CHECK(form >= 0 && c.states[form].anim >= 0);
    const Anim& a = area.anims[c.states[form].anim];
    CHECK(a.frames.size() == 5 && a.frames[0] >= 0 && a.frames[1] == NULL_SPRITE && a.frames[4] == NULL_SPRITE);
    World world(area, DEFAULT_SETTINGS);
    CHECK(!bubbleShown(world)); // (authored off)
}

TEST(cat_abduction_after_the_intro) {
    World world(villageArea(), DEFAULT_SETTINGS);
    world.placeAt(world.area.spawns[0]);
    village::houseExit(world);
    int f = 0;
    for (; f < 600 && world.dollies.running(); f++) frames(world, 1);
    CHECK(!world.dollies.running() && !world.abduction.started());
    float x0, y0;
    catAt(world, x0, y0);
    // 6 s after the dolly's end (counted from the next frame), the cat is taken.
    const int n = framesUntilStarted(world, 400);
    check(n >= 360 && n <= 362, "started after " + std::to_string(n) + " frames");
    float x, y;
    catAt(world, x, y);
    CHECK(x == x0 && y == y0); // (where it lay)
    CHECK(bubbleShown(world) && awake(world) && world.objects.parent(cat(world).object) == -1);
    CHECK(!world.objects.active(world.area.objectColliders[cat(world).colliderSlot]));
    CHECK(world.objects.sortingOrder(cat(world).spriteObject) == 99);
    frames(world, 1); // (the Animator takes the trigger: the bubble forms)
    CHECK(world.animators.stateName(world.area, def(world).bubbleAnimator) == "BubbleForm");
    CHECK(!world.animators.param(world.area, def(world).bubbleAnimator, "formBubble")); // (used up)
    // 1 s in it starts drifting; DoExploresseSequence sets its flag 1.4 s later.
    frames(world, 58);
    catAt(world, x, y);
    CHECK(x == x0 && y == y0);
    frames(world, 3);
    catAt(world, x, y);
    CHECK(x != x0 || y != y0);
    frames(world, 80);
    CHECK(!world.state.flag("villageCatFloatAway"));
    frames(world, 6);
    CHECK(world.state.flag("villageCatFloatAway"));
    CHECK(world.animators.stateName(world.area, def(world).bubbleAnimator) == "BubbleIdle");
    // On the wave, eastward over the water, then the pop and the drop onto the beach.
    float lastX = x;
    bool east = true;
    for (f = 0; f < 60 * 60 && bubbleShown(world); f++) {
        frames(world, 1);
        catAt(world, x, y);
        east = east && x >= lastX;
        lastX = x;
    }
    CHECK(!bubbleShown(world) && east && x > def(world).landingX - 1);
    frames(world, 61);
    catAt(world, x, y);
    CHECK(x == def(world).landingX && y == def(world).landingY && awake(world));
    frames(world, 62); // (the drop and the 1 s wait: float time may land a frame late)
    CHECK(!awake(world) && world.abduction.landed());
}

TEST(cat_abduction_after_a_pick_up) {
    // Carrying the cat: 3 s after the pick-up it's taken from the player.
    World world(villageArea(), DEFAULT_SETTINGS);
    world.state.setFlag(village::INTRO_CUTSCENE_FLAG, false);
    float cx, cy;
    catAt(world, cx, cy);
    world.placeNear(cx - 0.8f, cy);
    world.action();    // (picks the cat up)
    frames(world, 31);     // the pick-up animation, then OnPickUp
    CHECK(world.items.carried() == def(world).item && !world.abduction.started());
    const int n = framesUntilStarted(world, 400);
    check(n >= 178 && n <= 182, "started " + std::to_string(n) + " frames after the pick-up");
    CHECK(world.items.carried() == -1 && bubbleShown(world));
}

TEST(cat_abduction_start_after_the_intro_finds_it_on_the_beach) {
    World world(villageArea(), DEFAULT_SETTINGS);
    world.state.setFlag(village::INTRO_CUTSCENE_FLAG, true);
    world.reloadObjects(); // (a scene start)
    float x, y;
    catAt(world, x, y);
    CHECK(x == def(world).landingX && y == def(world).landingY);
    CHECK(world.state.flag("villageExploresseSequenceDone") && !world.abduction.started());
    CHECK(!world.objects.active(world.area.objectColliders[cat(world).colliderSlot]));
    frames(world, 60 * 10);
    CHECK(!world.abduction.started()); // (no intro this time)
}

TEST(animator_null_key_shows_nothing) {
    World world(villageArea(), DEFAULT_SETTINGS);
    const int bubble = def(world).bubbleAnimator;
    world.animators.setTrigger(world, bubble, "formBubble");
    frames(world, 1);
    CHECK(world.animators.stateName(world.area, bubble) == "BubbleForm" && world.animators.sprite(bubble) >= 0);
    frames(world, 30); // past 0.25 s: the null keys
    CHECK(world.animators.sprite(bubble) == NULL_SPRITE);
    frames(world, 32); // past the clip's end: back to idle, the trigger used up
    CHECK(world.animators.stateName(world.area, bubble) == "BubbleIdle" && world.animators.sprite(bubble) >= 0);
    frames(world, 60);
    CHECK(world.animators.stateName(world.area, bubble) == "BubbleIdle");
}
