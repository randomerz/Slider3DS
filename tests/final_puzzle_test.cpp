// The Village's end (village::FinalPuzzle) on the exported Village: the Archeologist takes the Ruins
// Fragment (FillInHole) and scrambles the grid (ShufflePuzzle: symbols flash back, shakes, a white
// flash, SetGrid); completions then follow the Artifact's buttons and solving it hands over Slider 9.
// Also VillageGrid.Start (CheckHole, a pending completion) and RuinsSymbols.Start.
#include <cmath>
#include <string>

#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

World finaleWorld() { // Sliders 2-8 collected (9's slot, in the middle, empty), the scene reloaded
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

void settle(World& world, int extra = 1) {
    for (int f = 0; f < 600 && !world.artifact.queueEmpty(); f++) world.update(DT, 0, 0, false);
    frames(world, extra);
}

// Frames until a coroutine started between frames gets through `wait` seconds (as in chad_jump_test).
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

int npcNamed(const Area& area, const std::string& tail) {
    for (int i = 0; i < (int)area.npcs.size(); i++) {
        const std::string& p = area.objects[area.npcs[i].object].path;
        if (p.size() >= tail.size() && p.compare(p.size() - tail.size(), tail.size(), tail) == 0) return i;
    }
    return -1;
}

// The player picks up the Ruins Fragment (dropped at its feet).
void carryFragment(World& world) {
    world.items.drop(world, world.area.areaGrid.ruinsFragment, world.player.x + 1, world.player.y);
    frames(world, 40);
    world.items.tryPickUp(world);
    frames(world, 40);
}

const RuinsSymbolsDef& symbols(const World& world) { return world.area.areaGrid.ruinsSymbols; }

std::string shown(const World& world) { // the symbols' renderers, then the hole's: 1 on, 0 off
    std::string s;
    for (int o : symbols(world).symbols) s += world.objects.rendererEnabled(o) ? '1' : '0';
    return s + (world.objects.rendererEnabled(symbols(world).hole) ? '1' : '0');
}

} // namespace

TEST(final_export) {
    const Area& area = villageArea();
    const RuinsSymbolsDef& rs = area.areaGrid.ruinsSymbols;
    CHECK(rs.present && rs.flashColor == 0xe6e6e6ff && rs.flashTime[3] == 0.25f);
    CHECK(area.objects[rs.object].path.find("RuinsText") != std::string::npos);
    CHECK(area.objects[rs.hole].path.find("Missing Piece") != std::string::npos);
    CHECK(area.areaGrid.ruinsFragment >= 0 && area.items[area.areaGrid.ruinsFragment].name == "Ruins Fragment");
    CHECK(area.objects[area.areaGrid.slider8Floor].path.find("ChadWalk0") != std::string::npos);
    CHECK(area.flashDuration == 1 && area.cameraShake.position.size() == 3 && area.cameraShake.position[0].constant[0]);
    // The Archeologist's and Fezziwig's lines call both.
    int fill = 0, shuffle = 0;
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (const DialogueDef& l : c.dialogue) {
                for (const CallDef& k : l.onStart) fill += k.method == CallDef::Method::VillageFillInHole;
                for (const CallDef& k : l.onEnd) shuffle += k.method == CallDef::Method::VillageShufflePuzzle;
            }
    CHECK(fill == 2 && shuffle == 2);
    // As authored: the first three runes and the hole show; the fourth (under the hole) doesn't.
    World world(area);
    world.setTileActive(2, true);
    check(shown(world) == "11101", "symbols / hole: " + shown(world));
}

TEST(final_archeologist_takes_the_fragment_and_shuffles) {
    // The real path: carrying the fragment, his "tyy! now let me take a look..." line fills the hole
    // (onStart) and shuffles (onEnd); 3.25 s later the grid jumps to the scramble, with the player
    // riding its tile.
    World world = finaleWorld();
    const Area& area = world.area;
    // (The authored grid is the solved one, for which he says something else: slide 8 out of place.)
    CHECK(world.artifact.tryMove(world.grid, 0, 1, 1, 1, 0));
    settle(world);
    const int arch = npcNamed(area, "/Archeologist");
    CHECK(arch >= 0);
    float ax, ay;
    world.objectPosition(area.npcs[arch].object, ax, ay);
    world.placeNear(ax - 2, ay - 1);
    world.update(DT, 0, 0, false);
    carryFragment(world);
    CHECK(world.items.carrying(area, "Ruins Fragment"));
    const ItemDef& fragment = area.items[area.areaGrid.ruinsFragment];

    // Carrying it, his conditional teleports him by the hole (NPC.Teleport): go to him there.
    CHECK(world.npcs.currentCond(arch) == 7);
    world.objectPosition(area.npcs[arch].object, ax, ay);
    world.placeNear(ax - 1, ay - 1);
    bool filled = false;
    for (int f = 0; f < 60 * 15 && !filled; f++) {
        world.update(DT, 0, 0, false);
        if (f % 30 == 0) world.action(); // (advances lines that wait for the action button)
        filled = !world.objects.rendererEnabled(symbols(world).hole);
    }
    CHECK(filled && !world.items.carrying(area, "Ruins Fragment") && !world.objects.activeInHierarchy(fragment.object));
    check(shown(world) == "00000", "hidden by FillInHole: " + shown(world));
    const int stile = world.player.stile;
    float bx, by;
    world.grid.ownerBase(stile, bx, by);
    const float relX = world.player.x - bx, relY = world.player.y - by;

    bool shuffled = false;
    for (int f = 0; f < 60 * 15 && !shuffled; f++) {
        world.update(DT, 0, 0, false);
        shuffled = world.artifact.gridString(world.grid) == "182_#43_765";
    }
    CHECK(shuffled && world.state.flag("villageCompletion") && world.state.flag("forceAutoMoveVillage"));
    CHECK(world.grid.durationMultiplier == 0.5f && world.effects.whiteAlpha(area) < 0.1f);
    world.grid.ownerBase(stile, bx, by);
    CHECK(world.player.stile == stile && std::fabs(world.player.x - bx - relX) < 1e-3f && std::fabs(world.player.y - by - relY) < 1e-3f);
    frames(world, 120); // the last symbol's flashes end
    check(shown(world) == "11110", "symbols back: " + shown(world));
}

TEST(final_shuffle_keeps_unity_frames) {
    World world = finaleWorld();
    const Area& area = world.area;
    const RuinsSymbolsDef& rs = symbols(world);
    world.finale.fillInHole(world); // (with nothing carried: just the hole and symbols)
    CHECK(shown(world) == "00000");
    world.finale.shufflePuzzle(world); // between frames, like a dialogue event
    // FlashSymbol(0) at once: shown, white, for flashTime from the next frame; a 0.25 shake.
    CHECK(shown(world) == "10000" && world.objects.fill(rs.symbols[0]) == rs.flashColor);
    frames(world, 1);
    CHECK(std::fabs(world.effects.shakeGain() - 0.25f * ScreenEffects::SHAKE_SETTING) < 1e-6f);
    frames(world, framesFor(0.25f, false) - 2);
    CHECK(world.objects.fill(rs.symbols[0]) == rs.flashColor);
    frames(world, 1);
    CHECK(world.objects.fill(rs.symbols[0]) == 0);
    // FlashSymbol(1) after WaitForSeconds(1), counted from the frame after the call.
    World w2 = finaleWorld();
    w2.finale.fillInHole(w2);
    w2.finale.shufflePuzzle(w2);
    const int one = framesFor(1, true);
    frames(w2, one - 1);
    CHECK(!w2.objects.rendererEnabled(rs.symbols[1]));
    frames(w2, 1);
    CHECK(w2.objects.rendererEnabled(rs.symbols[1]) && w2.objects.fill(rs.symbols[1]) == rs.flashColor);
    // Then 2 and 3 a second apart (each wait counts from the frame after the last step), and 0.25 s
    // after 3 the white flash and the shuffle.
    frames(w2, one - 1);
    CHECK(w2.objects.rendererEnabled(rs.symbols[2]) && !w2.objects.rendererEnabled(rs.symbols[3]));
    frames(w2, one - 1);
    CHECK(w2.objects.rendererEnabled(rs.symbols[3]) && std::fabs(w2.effects.shakeGain() - 2.5f * ScreenEffects::SHAKE_SETTING) < 1e-6f);
    const int quarter = framesFor(0.25f, true) - 1;
    frames(w2, quarter - 1);
    CHECK(w2.artifact.gridString(w2.grid) != "182_#43_765" && w2.effects.whiteAlpha(area) == 0);
    frames(w2, 1);
    CHECK(w2.artifact.gridString(w2.grid) == "182_#43_765");
    CHECK(w2.effects.whiteAlpha(area) == 0); // FlashCoroutine: alpha 0 the frame it starts
    frames(w2, 1);
    CHECK(std::fabs(w2.effects.whiteAlpha(area) - DT / 0.5f) < 1e-5f);
    frames(w2, 29);
    CHECK(w2.effects.whiteAlpha(area) == 1);
    frames(w2, 31);
    CHECK(w2.effects.whiteAlpha(area) == 0);
    // Once per scene: calling it again does nothing.
    frames(w2, 120);
    w2.finale.shufflePuzzle(w2);
    frames(w2, 1);
    CHECK(w2.effects.shakeGain() == 0 && w2.objects.fill(rs.symbols[0]) == 0);
}

TEST(final_completions_and_slider_9) {
    World world = finaleWorld();
    const Area& area = world.area;
    world.finale.shufflePuzzle(world);
    frames(world, 60 * 6);
    CHECK(world.state.flag("villageCompletion"));
    // Nothing is in its target slot after the scramble.
    CHECK(!world.artifactButtons.complete(1) && !world.artifactButtons.complete(8));
    // One slide from solved: every button in its target slot shows complete; not 7 (or 9, uncollected).
    CHECK(world.setGrid("624_879_153"));
    world.update(DT, 0, 0, false);
    std::string complete;
    for (int id = 1; id <= 9; id++) complete += world.artifactButtons.complete(id) ? '1' : '0';
    check(complete == "111111010", "complete: " + complete);
    // Solving it stops Artifact movement (queued moves still run) and, once they're done, the final
    // Slider appears under the player, who picks it up; 1.2 s after it appeared button 9 shows complete.
    CHECK(world.artifact.tryMove(world.grid, 1, 1, 2, 1, world.player.stile));
    world.update(DT, 0, 0, false);
    CHECK(!world.artifact.canAddMoves && world.artifact.canQueue && world.finale.placingTile9());
    CHECK(world.state.flag("villagePendingComplete") && !world.state.flag("forceAutoMoveVillage"));
    CHECK(world.artifactButtons.complete(7));
    const CollectibleDef& c = area.collectibles[area.collectible("Slider 9")];
    CHECK(!world.objects.activeInHierarchy(c.object));
    int f = 0;
    while (!world.objects.activeInHierarchy(c.object) && f++ < 600) world.update(DT, 0, 0, false);
    CHECK(world.artifact.queueEmpty() && !world.finale.placingTile9());
    float cx, cy;
    world.objectPosition(c.object, cx, cy);
    CHECK(std::fabs(cx - world.player.x) < 1e-3f && std::fabs(cy - world.player.y) < 1e-3f);
    const int delay = framesFor(1.2f, true) - 1; // counted from the frame after it appeared
    frames(world, 2);
    CHECK(world.state.hasCollectible("Slider 9", "Village") && world.grid.tileActive(9));
    CHECK(!world.artifactButtons.complete(9));
    frames(world, delay - 3);
    CHECK(!world.artifactButtons.complete(9));
    frames(world, 1);
    CHECK(world.artifactButtons.complete(9));
}

TEST(final_start_reads_the_save) {
    const Area& area = villageArea();
    const ItemDef& fragment = area.items[area.areaGrid.ruinsFragment];
    // Completed: fast slides, completions, the fragment gone, the runes finished.
    World done = finaleWorld();
    done.state.setFlag("villageCompletion", true);
    done.reloadObjects();
    CHECK(done.grid.durationMultiplier == 0.5f && !done.objects.activeInHierarchy(fragment.object));
    check(shown(done) == "11110", "runes after completion: " + shown(done));
    bool completions = true;
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++) {
            const int id = done.artifact.buttonAt(x, y);
            if (done.grid.tileActive(id)) completions &= done.artifactButtons.complete(id) == (charToId(done.grid.targetAt(x, y)) == id);
        }
    CHECK(completions && done.artifactButtons.complete(6)); // (6 starts in its slot)
    // Not completed: normal slides, the fragment with Chad; with the Caves' Slider 3, in the hole.
    World fresh = finaleWorld();
    CHECK(fresh.grid.durationMultiplier == 1 && fresh.objects.activeInHierarchy(fragment.object));
    CHECK(fresh.objects.parent(fragment.object) != area.areaGrid.slider8Floor);
    fresh.state.setCollectible("Slider 3", "Caves", true);
    fresh.reloadObjects();
    float fx, fy, hx, hy;
    fresh.objectPosition(fragment.object, fx, fy);
    fresh.objectPosition(area.areaGrid.slider8Floor, hx, hy);
    CHECK(fresh.objects.parent(fragment.object) == area.areaGrid.slider8Floor && std::fabs(fx - hx) < 1e-3f && std::fabs(fy - hy) < 1e-3f);
    // Solved but Slider 9 not handed over yet (quit in between): PlaceTile9 on load.
    World pending = finaleWorld();
    pending.state.setFlag("villagePendingComplete", true);
    pending.reloadObjects();
    const int slider9 = area.collectibles[area.collectible("Slider 9")].object;
    CHECK(pending.finale.placingTile9());
    frames(pending, 1); // WaitUntil checks from the next frame
    CHECK(pending.finale.placingTile9() && !pending.objects.activeInHierarchy(slider9));
    frames(pending, 1);
    CHECK(!pending.finale.placingTile9() && pending.objects.activeInHierarchy(slider9));
    frames(pending, 2);
    CHECK(pending.state.hasCollectible("Slider 9", "Village"));
}

TEST(screen_effects_shake_and_flash) {
    const Area& area = villageArea();
    ScreenEffects fx;
    fx.reset();
    fx.update(area, DT);
    CHECK(fx.shakeGain() == 0 && fx.shakeX() == 0 && fx.shakeY() == 0);
    // CameraShake.Shake(1, 2): gain 2 * setting falling linearly to 0 over 1 s, then gone.
    fx.shake(1, 2);
    fx.update(area, DT);
    CHECK(fx.shakeGain() == 2 * ScreenEffects::SHAKE_SETTING);
    float most = 0;
    bool moved = false;
    for (int f = 1; f < 30; f++) {
        fx.update(area, DT);
        most = std::max(most, std::max(std::fabs(fx.shakeX()), std::fabs(fx.shakeY())));
        moved |= fx.shakeX() != 0;
    }
    CHECK(std::fabs(fx.shakeGain() - (1 - 29 * DT) * 2 * ScreenEffects::SHAKE_SETTING) < 1e-4f);
    float bound = 0; // the channels' amplitudes (a cosine's half, Perlin's half at most)
    for (const CameraShakeDef::Channel& ch : area.cameraShake.position) bound += std::max(ch.amplitude[0], ch.amplitude[1]);
    CHECK(moved && most > 0 && most <= bound * 2 * ScreenEffects::SHAKE_SETTING);
    for (int f = 0; f < 40; f++) fx.update(area, DT);
    CHECK(fx.shakeGain() == 0 && fx.shakeX() == 0);
    // A stronger shake wins while both run.
    fx.shake(1, 1);
    fx.shake(1, 3);
    fx.update(area, DT);
    CHECK(fx.shakeGain() == 3 * ScreenEffects::SHAKE_SETTING);
}
