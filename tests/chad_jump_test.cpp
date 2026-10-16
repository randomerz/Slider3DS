// Chad (ChadJump + VillageChadJump) on the exported Village: his dialogue makes him jump onto the
// rock; other tiles' slides tip him (DidSlideAfterJumping); a slide of his own tile 8 makes him fall
// and drop the Ruins Fragment next to him (ChadFell), which can only be picked up from then on.
#include <cmath>
#include <string>

#include "game/events/conditions.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

// Sliders 2-8 collected (the start grid: 9's slot, in the middle, is empty), the scene reloaded.
World chadWorld() {
    World world(villageArea());
    for (int id = 2; id <= 8; id++) {
        world.setTileActive(id, true);
        world.state.setCollectible("Slider " + std::to_string(id), "Village", true);
    }
    world.reloadObjects();
    world.update(DT, 0, 0, false);
    return world;
}

void frames(World& world, int n) {
    for (int i = 0; i < n; i++) world.update(DT, 0, 0, false);
}

// Updates until the Artifact's moves are done, plus `extra` frames.
void settle(World& world, int extra = 1) {
    for (int f = 0; f < 600 && !world.artifact.queueEmpty(); f++) world.update(DT, 0, 0, false);
    frames(world, extra);
}

// Chad's position relative to his STile (transform.localPosition).
void chadAt(const World& world, float& x, float& y) {
    const ChadJumpDef& d = world.area.chadJumps[0];
    float bx, by;
    world.grid.ownerBase(d.owner, bx, by);
    world.objectPosition(d.object, x, y);
    x -= bx;
    y -= by;
}

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

const ConditionDef* chadSpec(const Area& area, ConditionDef::Spec spec) {
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (const ConditionDef& d : c.conditions)
                if (d.type == ConditionDef::Type::Spec && d.spec == spec) return &d;
    return nullptr;
}

bool param(const World& world, const char* name) { return world.animators.param(world.area, world.area.chadJumps[0].animator, name); }

// Frames until a coroutine started between frames gets through `wait` seconds: one frame before it
// resumes (yield / WaitForSeconds start counting the next frame), then dt at a time.
int framesFor(float wait, bool countDown) {
    int n = 1;
    if (countDown) {
        float left = wait;
        do { n++; left -= DT; } while (left > 0);
    } else {
        float t = 0;
        do { n++; t += DT; } while (t < wait);
    }
    return n;
}

int chadNpc(const Area& area) { return area.chadJumps[0].tipNpc; }

// Jumps and lands on the rock.
void jump(World& world) {
    world.chad.jumpStarter(world, 0);
    frames(world, 70);
}

} // namespace

TEST(chad_export) {
    const Area& area = villageArea();
    CHECK(area.chadJumps.size() == 1);
    const ChadJumpDef& d = area.chadJumps.at(0);
    CHECK(area.pathOf(d.object) == "Map/Village 8Puzzle/STile (8)/Objects/Chad");
    CHECK(d.island == 8 && d.owner == 8 && d.startX == 3.5f && d.startY == 3 && d.jumpX == 5 && d.jumpY == 4 &&
          d.fallX == 6 && d.fallY == 3 && d.jumpDuration == 1 && d.fallDuration == 0.5f);
    CHECK(d.item >= 0 && area.items[d.item].name == "Ruins Fragment");
    CHECK(area.objectColliders[d.collider].object == d.object && area.objectColliders[d.collider].layer == "NPC");
    CHECK(d.tips && d.timeTipped == 3 && area.npcs[d.tipNpc].object == d.object && d.tipAnimator == d.animator);
    CHECK(d.yCurve.eval(0.5f) > 1.1f); // (the y curve overshoots; Mathf.Lerp clamps it)
    CHECK(d.onJumpFinish.size() == 1 && d.onJumpFinish[0].method == CallDef::Method::ChadSetIsOnRock);
    // His second line ends with JumpStarter; both specs are his.
    bool starter = false;
    for (const NpcCondDef& c : area.npcs[d.tipNpc].conds)
        for (const DialogueDef& line : c.dialogue)
            for (const CallDef& call : line.onEnd) starter |= call.method == CallDef::Method::ChadJumpStarter && call.data == 0;
    CHECK(starter);
    CHECK(chadSpec(area, ConditionDef::Spec::ChadFell) && chadSpec(area, ConditionDef::Spec::ChadTipped));
}

TEST(chad_fragment_out_of_reach_until_he_falls) {
    // ChadJump.Awake: the fragment's collider is off unless the Caves' Slider 3 is collected.
    const ItemDef& it = villageArea().items[villageArea().chadJumps[0].item];
    World world = chadWorld();
    CHECK(!world.objects.active(world.area.objectColliders[it.colliderSlot]));
    World caves(villageArea());
    caves.state.setCollectible("Slider 3", "Caves", true);
    for (int id = 2; id <= 8; id++) caves.setTileActive(id, true);
    caves.reloadObjects();
    CHECK(caves.objects.active(caves.area.objectColliders[it.colliderSlot]));
}

TEST(chad_jumps_onto_the_rock) {
    World world = chadWorld();
    const ChadJumpDef& d = world.area.chadJumps[0];
    world.chad.jumpStarter(world, 0);
    float x, y;
    chadAt(world, x, y);
    CHECK(near(x, d.startX) && near(y, d.startY) && param(world, "isJumping"));
    CHECK(!world.objects.active(world.area.objectColliders[d.collider]) && world.objects.sortingOrder(d.renderer) == 1);
    float top = y;
    for (int f = 0; f < 59; f++) { // up to just before 1 s: on his way, never above the rock (Mathf.Lerp clamps)
        frames(world, 1);
        chadAt(world, x, y);
        top = std::max(top, y);
    }
    check(x > d.startX && x < d.jumpX && top <= d.jumpY + 1e-4f, "mid-jump x " + std::to_string(x) + ", top " + std::to_string(top));
    frames(world, 3);
    chadAt(world, x, y);
    CHECK(near(x, d.jumpX) && near(y, d.jumpY) && !param(world, "isJumping"));
    CHECK(!world.chad.fell(0) && !world.chad.slidAfterJumping(0));
    // Up there his collider stays off (only EnableCollider, from another area, turns it back on).
    CHECK(!world.objects.active(world.area.objectColliders[d.collider]));
}

TEST(chad_tips_when_other_tiles_slide) {
    World world = chadWorld();
    const ChadJumpDef& d = world.area.chadJumps[0];
    // Before he's up there, slides don't bother him.
    CHECK(world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0)); // 7 into the middle
    settle(world);
    CHECK(!world.chad.slidAfterJumping(0) && !param(world, "isTipping"));
    jump(world);
    CHECK(world.artifact.tryMove(world.grid, 1, 1, 2, 1, 0)); // 7 back
    settle(world, 0);
    CHECK(world.chad.slidAfterJumping(0) && param(world, "isTipping"));
    CHECK(checkCondition(*chadSpec(world.area, ConditionDef::Spec::ChadTipped), world));
    // timeTipped (3 s) later he's steady again; he stays up.
    frames(world, (int)(d.timeTipped / DT) - 10);
    CHECK(param(world, "isTipping"));
    frames(world, 20);
    CHECK(!param(world, "isTipping") && !world.chad.fell(0));
    float x, y;
    chadAt(world, x, y);
    CHECK(near(x, d.jumpX) && near(y, d.jumpY));
}

TEST(chad_falls_when_his_tile_slides) {
    World world = chadWorld();
    const Area& area = world.area;
    const ChadJumpDef& d = area.chadJumps[0];
    const ItemDef& it = area.items[d.item];
    // Moving tile 8 before the jump does nothing.
    CHECK(world.artifact.tryMove(world.grid, 0, 1, 1, 1, 0));
    settle(world);
    CHECK(!world.chad.fell(0) && !param(world, "isTipping"));
    jump(world);
    CHECK(world.artifact.tryMove(world.grid, 1, 1, 0, 1, 0)); // 8 back: he tips, then falls
    settle(world, 0);
    CHECK(param(world, "isTipping") && !param(world, "isFallen") && !world.chad.fell(0));
    frames(world, 31); // WaitForSeconds(0.5)
    CHECK(param(world, "isFallen") && !param(world, "isTipping"));
    float x, y;
    chadAt(world, x, y);
    CHECK(x >= d.jumpX && x < d.fallX);
    frames(world, 30);
    chadAt(world, x, y);
    CHECK(near(x, d.fallX) && near(y, d.fallY) && world.chad.fell(0));
    CHECK(checkCondition(*chadSpec(area, ConditionDef::Spec::ChadFell), world));
    CHECK(world.objects.sortingOrder(d.renderer) == 0);

    // The fragment drops a unit to his right; its collider (and his) come on once it lands.
    float cx, cy, ix, iy, sx, sy;
    world.objectPosition(d.object, cx, cy);
    world.objectPosition(it.object, ix, iy);
    CHECK(near(ix, cx + 1) && near(iy, cy));
    CHECK(!world.objects.active(area.objectColliders[it.colliderSlot]) && !world.objects.active(area.objectColliders[d.collider]));
    world.objectPosition(it.spriteObject, sx, sy);
    float highest = sy;
    for (int f = 0; f < 29; f++) { // the sprite falls from his head, never above it (Mathf.Lerp clamps)
        frames(world, 1);
        world.objectPosition(it.spriteObject, sx, sy);
        highest = std::max(highest, sy);
    }
    check(highest <= cy + 0.8125f + 1e-3f, "fragment sprite peaked at " + std::to_string(highest - cy));
    frames(world, 3);
    world.objectPosition(it.spriteObject, sx, sy);
    CHECK(near(sx, ix) && near(sy, iy));
    CHECK(world.objects.active(area.objectColliders[it.colliderSlot]) && world.objects.active(area.objectColliders[d.collider]));
    CHECK(world.objects.sortingOrder(it.spriteObject) == 0);
    // DropItem parented it to the STile it landed on (tile 8): it's no longer Chad's.
    const STileDef* s8 = area.stile(8);
    CHECK(world.objects.parent(it.object) == area.objects[s8->objectsObject].parent);

    // Fallen, he doesn't tip any more.
    CHECK(world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0));
    settle(world);
    CHECK(!param(world, "isTipping"));

    // Now the player can pick it up.
    world.placeNear(ix + 0.75f, iy);
    world.update(DT, 0, 0, false);
    CHECK(world.items.tryPickUp(world));
    frames(world, 40);
    CHECK(world.items.carrying(area, "Ruins Fragment") && world.objects.parent(it.object) == -1);
}

TEST(chad_talking_makes_him_jump_and_a_tip_makes_him_speak) {
    // The real path: his second line's onEnd calls JumpStarter. Then, with the player away, the first
    // tip types his current dialogue 0.5 s later (DelayThenChirp); a slide while he's still tipping doesn't.
    World world = chadWorld();
    const Area& area = world.area;
    const ChadJumpDef& d = area.chadJumps[0];
    float cx, cy;
    world.objectPosition(d.object, cx, cy);
    world.placeNear(cx - 1, cy - 1);
    bool jumped = false;
    for (int f = 0; f < 60 * 15 && !jumped; f++) {
        world.update(DT, 0, 0, false);
        if (f % 30 == 0) world.action(); // (advances lines that wait for the action button)
        jumped = param(world, "isJumping");
    }
    CHECK(jumped && world.state.flag("VillageFoundChad"));
    world.placeNear(cx - 12, cy); // out of his dialogue trigger
    frames(world, 90);
    float x, y;
    chadAt(world, x, y);
    CHECK(near(x, d.jumpX) && near(y, d.jumpY));
    frames(world, 120); // whatever he was saying is gone
    CHECK(!world.npcs.textShown(chadNpc(area)));

    CHECK(world.artifact.tryMove(world.grid, 2, 1, 1, 1, 0)); // 7 into the middle: a tip
    settle(world, 0);
    const uint32_t before = world.npcs.textVersion(chadNpc(area));
    frames(world, framesFor(0.5f, true) - 2);
    CHECK(world.npcs.textVersion(chadNpc(area)) == before);
    frames(world, 2);
    check(world.npcs.textVersion(chadNpc(area)) != before && world.npcs.textShown(chadNpc(area)), "he speaks 0.5 s after the tip");
    CHECK(world.npcs.currentCond(chadNpc(area)) == 1); // "Woah, what's shaking?" (DidSlideAfterJumping)
    const uint32_t spoke = world.npcs.textVersion(chadNpc(area));
    CHECK(world.artifact.tryMove(world.grid, 1, 1, 2, 1, 0)); // 7 back while he's still tipping
    settle(world, 40);
    CHECK(world.npcs.textVersion(chadNpc(area)) == spoke);
}

TEST(chad_coroutines_keep_unity_frames) {
    // A coroutine started between frames resumes from the frame after next; the fall's
    // WaitForSeconds(0.5) holds him on the rock, then he moves from the frame after it ends.
    World world = chadWorld();
    const ChadJumpDef& d = world.area.chadJumps[0];
    world.chad.jumpStarter(world, 0);
    const int land = framesFor(d.jumpDuration, false); // (the last frame runs the landing)
    frames(world, land - 1);
    CHECK(param(world, "isJumping"));
    frames(world, 1);
    CHECK(!param(world, "isJumping"));

    CHECK(world.artifact.tryMove(world.grid, 0, 1, 1, 1, 0)); // 8 into the middle: he falls
    settle(world, 0); // (the frame his tile arrived)
    const int waited = framesFor(0.5f, true) - 1; // (started inside that frame: its skip was that frame)
    frames(world, waited - 1);
    float x, y;
    chadAt(world, x, y);
    CHECK(!param(world, "isFallen") && near(x, d.jumpX) && near(y, d.jumpY));
    frames(world, 1); // the wait ends: fallen, still where he was
    chadAt(world, x, y);
    CHECK(param(world, "isFallen") && near(x, d.jumpX) && near(y, d.jumpY));
    frames(world, 1);
    chadAt(world, x, y);
    CHECK(x > d.jumpX + 1e-4f);
}
