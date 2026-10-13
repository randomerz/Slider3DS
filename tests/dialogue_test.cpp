// NPC dialogue: text parsing / layout / typing (game/dialogue_text) with the real dialogue font, and
// the NPC dialogue state machine (game/npcs) on a small synthetic area and the Village.
#include <algorithm>
#include <cmath>
#include <string>

#include "game/dialogue_text.h"
#include "game/font.h"
#include "game/npcs.h"
#include "game/world.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

const FontDef& dialogueFont() {
    static FontDef font;
    static bool loaded = false;
    if (!loaded) {
        std::string error;
        loaded = loadFont("fonts/dialogue.json", font, &error);
        check(loaded, "dialogue font loads: " + error);
    }
    return font;
}

// The NPC prefab's text box (7 x 1.9 units less margins, 0.55 above the NPC).
NpcTextDef prefabBox() {
    NpcTextDef box;
    box.minX = -3.42f;
    box.maxX = 3.42f;
    box.minY = 0.55f;
    box.maxY = 2.7f;
    box.lineAdvance = 0.4309f;
    box.wordSpacing = -0.1f;
    return box;
}

// Ticks a typer until it finishes (or `limit` frames); returns the frames it took.
int typeOut(Typer& t, int limit = 1000) {
    int frames = 0;
    while (!t.finished() && frames < limit) {
        t.tick(DT);
        frames++;
    }
    return frames;
}

// One STile; "Npc" (0) with its dialogue trigger "Npc/Talk" (1, radius 2 around the NPC) and a
// teleport target "Spot" (2). Conditionals:
//   0 (always): "Hello there" (waits for the player; onStart flag "start0", onEnd "end0"), then
//      "Bye" (onEnd "end1"); exhausting it sets "exhausted".
//   1 (flag "next"; always from the beginning): entering teleports the NPC to Spot (5, 1) and flips
//      its sprite. "Moved" (dontInterrupt, 0.2 s delay; onStart "moved" once only), then "Done".
const char* NPC_AREA = R"json({
  "ppu": 16,
  "grid": {"width": 1, "height": 1, "targetGrid": "1", "housingOffset": -150},
  "stiles": [{"islandId": 1, "name": "STile (1)", "x": 0, "y": 0, "width": 17, "isTileActive": true}],
  "objects": [{"path": "Npc", "parent": -1, "active": true}, {"path": "Npc/Talk", "parent": 0, "active": true},
              {"path": "Spot", "parent": -1, "active": true}],
  "triggerColliders": [{"object": 1, "owner": 1, "kind": "circle", "center": [0, 0], "radius": 2}],
  "triggerEvents": [{"object": 1, "onPlayerEnter": true,
                     "onEnter": [{"method": "NPC.OnDialogueTriggerEnter", "target": 0}],
                     "onExit": [{"method": "NPC.OnDialogueTriggerExit", "target": 0}]}],
  "npcs": [{"object": 0, "name": "N", "owner": 1, "pos": [0, 0], "facingLeft": false,
    "text": {"box": [-3.42, 0.55, 3.42, 2.7], "fontSize": 1, "lineAdvance": 0.4309, "wordSpacing": -0.1,
             "charSpacing": 0, "sortingLayerValue": 9, "order": 99},
    "conds": [
      {"conditions": [], "anyOf": false, "alwaysStartFromBeginning": false, "onConditionalEnter": [],
       "onDialogueChainExhausted": [{"method": "VarManager.SetBoolOn", "string": "exhausted"}],
       "dialogue": [
         {"text": "Hello there", "delay": 0.5, "waitForAction": true, "manual": false, "noRepeatEvents": false,
          "dontInterrupt": false, "onStart": [{"method": "VarManager.SetBoolOn", "string": "start0"}],
          "onEnd": [{"method": "VarManager.SetBoolOn", "string": "end0"}]},
         {"text": "Bye", "delay": 0, "waitForAction": false, "manual": false, "noRepeatEvents": false,
          "dontInterrupt": false, "onStart": [], "onEnd": [{"method": "VarManager.SetBoolOn", "string": "end1"}]}]},
      {"conditions": [{"type": "flag", "flag": "next"}], "anyOf": false, "alwaysStartFromBeginning": true,
       "onConditionalEnter": [{"method": "NPC.Teleport", "target": 0, "objectArg": 2, "argOwner": 1, "argPos": [5, 1]},
                              {"method": "SpriteRenderer.set_flipX", "target": 0, "bool": true}],
       "onDialogueChainExhausted": [],
       "dialogue": [
         {"text": "Moved", "delay": 0.2, "waitForAction": false, "manual": false, "noRepeatEvents": true,
          "dontInterrupt": true, "onStart": [{"method": "VarManager.SetBoolOn", "string": "moved"}], "onEnd": []},
         {"text": "Done", "delay": 0, "waitForAction": false, "manual": false, "noRepeatEvents": false,
          "dontInterrupt": false, "onStart": [], "onEnd": []}]}]}],
  "player": {"footBox": {"offset": [0, 0], "size": [0.5, 0.5]}, "boatBox": {"offset": [0, 0], "size": [1, 0.5]}}
})json";

const Area& npcArea() {
    static Area area;
    static bool loaded = false;
    if (!loaded) {
        testSetFile("synthetic/npcs.json", NPC_AREA);
        std::string error;
        loaded = loadArea("synthetic/npcs.json", area, &error);
        testClearFiles();
        check(loaded, "synthetic NPC area loads: " + error);
    }
    return area;
}

void standAt(World& world, float x, float y) {
    world.player.x = x;
    world.player.y = y;
    world.update(DT, 0, 0, true); // noclip: nothing solid here
}

// Frames until NPC 0's current line is typed (and the next update has handled it).
void waitTyped(World& world) {
    for (int i = 0; i < 600 && world.npcs.typer(0).typing(); i++) world.update(DT, 0, 0, true);
    world.update(DT, 0, 0, true);
}

bool npcOffered(const World& world, int npc) {
    return std::find(world.interactables.begin(), world.interactables.end(), Interactable{Interactable::Kind::Npc, npc}) !=
           world.interactables.end();
}

} // namespace

TEST(dialogue_parse_replaces_vars_and_marks_effects) {
    GameState state;
    DialogueText t = parseDialogue("You\xE2\x80\x99re awake! <var>Cat</var> has been acting\xE2\x80\xA6 <wavy>strange.</wavy>", state);
    check(t.chars == "You're awake! Cat has been acting... strange.", "quotes, ellipsis, unset var reads as its name: " + t.chars);
    const size_t at = t.chars.find("strange.");
    check(t.wavy[at] && t.wavy[at + 7] && !t.wavy[at - 1], "the <wavy> range is exactly 'strange.'");
    state.setStr("Cat", "Boomo");
    t = parseDialogue("<var>Cat</var>!<type> ...</type>", state);
    check(t.chars == "Boomo! ..." && t.typeFirst == 6 && t.typeLast == 9, "profile string replaced; <type> range " +
                                                                               std::to_string(t.typeFirst) + ".." +
                                                                               std::to_string(t.typeLast));
    t = parseDialogue("a <b>c</b> <var>x", state);
    check(t.chars == "a <b>c</b> <var>x", "unknown or unclosed tags stay as text");
}

TEST(dialogue_layout_wraps_centers_and_bottom_aligns) {
    const FontDef& font = dialogueFont();
    const NpcTextDef box = prefabBox();
    const float ppu = 16, width = (box.maxX - box.minX) * ppu;
    GameState state;
    const DialogueText t = parseDialogue("Please bring my beloved back, lest I drink this poison! Oh woe is me.", state);
    std::vector<GlyphPlace> at;
    layoutDialogue(font, t, box, ppu, at);
    check(at.size() == t.chars.size(), "a place per character");
    // Lines by baseline, bottom (last) line first.
    std::vector<float> baselines;
    for (const GlyphPlace& p : at)
        if (std::find(baselines.begin(), baselines.end(), p.y) == baselines.end()) baselines.push_back(p.y);
    std::sort(baselines.begin(), baselines.end());
    check(baselines.size() >= 2, "a long line wraps (" + std::to_string(baselines.size()) + " lines)");
    check(baselines[0] == (float)font.descent, "the last line's descender sits on the box bottom");
    for (size_t l = 1; l < baselines.size(); l++)
        check(std::fabs(baselines[l] - baselines[l - 1] - box.lineAdvance * ppu) < 1e-3f, "lines are lineAdvance apart");
    for (float b : baselines) {
        float left = 1e9f, right = -1e9f;
        for (size_t i = 0; i < at.size(); i++)
            if (at[i].y == b && glyphVisible(t.chars[i])) {
                left = std::min(left, at[i].x);
                right = std::max(right, at[i].x + (float)font.glyph(t.chars[i]).advance);
            }
        check(left >= -1e-3f && right <= width + 1e-3f, "each line fits the box");
        check(std::fabs(left - (width - right)) < 1e-3f, "each line is centered");
    }
    // Words aren't broken: every character of a word shares its baseline.
    for (size_t i = 1; i < t.chars.size(); i++)
        if (glyphVisible(t.chars[i]) && glyphVisible(t.chars[i - 1])) check(at[i].y == at[i - 1].y, "words stay whole");
    // A single word wider than the box breaks where it overflows.
    std::vector<GlyphPlace> wide;
    layoutDialogue(font, parseDialogue(std::string(60, 'W'), state), box, ppu, wide);
    check(wide.front().y > wide.back().y, "an overlong word breaks onto a new line");
}

TEST(dialogue_typer_fades_in_one_character_per_step) {
    // TEXT_SPEED 0.03 s at 60 fps = a step every 2 frames; each character shows from its step on.
    GameState state;
    const DialogueText t = parseDialogue("Hi you", state);
    Typer typer;
    typer.start(t);
    check(typer.shown(0) && !typer.shown(1) && typer.typing(), "the first character shows at once");
    typer.tick(DT); // the frame it started in
    typer.tick(DT);
    check(!typer.shown(1), "one frame later: still waiting");
    typer.tick(DT);
    check(typer.shown(1) && !typer.shown(3), "two frames later: the next one");
    const int frames = typeOut(typer);
    check(typer.finished() && !typer.typing() && typer.shown(5), "finishes with everything shown");
    check(frames > 6 && frames < 20, "takes about two frames per character (" + std::to_string(frames) + ")");

    Typer skipped;
    skipped.start(t);
    skipped.skip();
    check(skipped.finished() && skipped.shown(5), "skipping shows everything at once");

    Typer empty;
    empty.start(parseDialogue("", state));
    typeOut(empty, 300);
    check(!empty.finished(), "an empty line never finishes (TMP waits for characters)");
}

TEST(dialogue_type_loop_retypes_the_ellipsis) {
    GameState state;
    const DialogueText t = parseDialogue("Ok<type> ...</type>", state);
    Typer typer;
    typer.start(t);
    typer.skip();
    typer.tick(DT); // started this frame
    typer.tick(DT); // the range is typed: the loop starts timing
    check(typer.shown(3) && typer.shown(4) && typer.shown(5), "typed: the dots show");
    for (int i = 0; i < 15; i++) typer.tick(DT); // 0.25 s: past the 0.2 s pause, in the 'hidden' step
    check(!typer.shown(3) && typer.shown(0), "then the loop hides them (the rest stays)");
    for (int i = 0; i < 12; i++) typer.tick(DT); // +0.2 s
    check(typer.shown(3) && !typer.shown(4), "and types them back one at a time");
}

TEST(npcs_dialogue_chain_waits_for_the_player) {
    World world(npcArea());
    standAt(world, 10, 0);
    check(world.npcs.currentCond(0) == 0 && world.npcs.pingShown(0) && !world.npcs.textShown(0),
          "started: first conditional, ping up, nothing said");
    standAt(world, 0, 0);
    check(world.npcs.textShown(0) && world.npcs.typer(0).typing() && !world.npcs.pingShown(0), "entering: typing, ping down");
    check(world.state.flag("start0") && npcOffered(world, 0) && !world.promptShown(), "onStart ran; offered; no prompt while typing");
    waitTyped(world);
    check(world.state.flag("end0") && world.promptShown(), "typed: onEnd ran and the prompt asks for the player");
    for (int i = 0; i < 60; i++) world.update(DT, 0, 0, true);
    check(world.npcs.lineIndex(0) == 0, "a line waiting for the player doesn't move on by itself");
    world.interact();
    check(world.npcs.lineIndex(0) == 1 && world.npcs.typer(0).typing() && !world.promptShown(), "A: the next line types");
    world.interact();
    world.update(DT, 0, 0, true);
    check(world.npcs.typer(0).finished(), "A while typing skips to the end");
    check(world.state.flag("end1") && world.state.flag("exhausted") && !npcOffered(world, 0),
          "the last line ends: the chain is exhausted and the NPC withdraws");
    check(world.npcs.textShown(0), "the last line stays up while the player listens");
    standAt(world, 10, 0);
    check(!world.npcs.textShown(0) && !world.npcs.pingShown(0), "leaving clears it; no ping once exhausted");
    standAt(world, 0, 0);
    check(world.npcs.lineIndex(0) == 1 && world.npcs.textShown(0), "coming back resumes at the last line");
}

TEST(npcs_conditional_change_teleports_and_dont_interrupt_finishes) {
    World world(npcArea());
    standAt(world, 10, 0);
    world.state.setFlag("next", true);
    world.update(DT, 0, 0, true);
    float x, y, bx, by;
    world.objectBase(0, 1, x, y);
    world.grid.ownerBase(1, bx, by);
    check(world.npcs.currentCond(0) == 1 && std::fabs(x - bx - 5) < 1e-4f && std::fabs(y - by - 1) < 1e-4f,
          "the new conditional teleported the NPC (and its trigger) to Spot");
    check(world.objects.flipX(0) == 1, "and flipped its sprite");
    standAt(world, 5, 1);
    check(world.npcs.textShown(0) && world.state.flag("moved"), "entering at the new spot: 'Moved'");
    standAt(world, 10, 0);
    check(world.npcs.textShown(0), "leaving doesn't interrupt a dontInterrupt line");
    for (int i = 0; i < 120 && world.npcs.lineIndex(0) == 0; i++) world.update(DT, 0, 0, true);
    check(world.npcs.lineIndex(0) == 1, "after its delay the chain moved on by itself");
    world.update(DT, 0, 0, true);
    check(!world.npcs.textShown(0), "the next line isn't dontInterrupt: with nobody there it clears");
    world.state.setFlag("moved", false);
    standAt(world, 5, 1);
    check(world.npcs.lineIndex(0) == 0 && !world.state.flag("moved"),
          "back again: from the beginning (alwaysStartFromBeginning), onStart not repeated (noRepeatEvents)");
    // A conditional change while the player listens types the new chain at once (this one again
    // from its beginning).
    for (int i = 0; i < 120 && world.npcs.lineIndex(0) == 0; i++) world.update(DT, 0, 0, true);
    check(world.npcs.lineIndex(0) == 1, "listened on to 'Done'");
    world.state.setFlag("next", false);
    world.update(DT, 0, 0, true);
    check(world.npcs.currentCond(0) == 0 && world.npcs.text(0).chars.rfind("Hello there", 0) == 0,
          "back to the first conditional: its line types right away");
    world.state.setFlag("next", true);
    world.update(DT, 0, 0, true);
    check(world.npcs.currentCond(0) == 1 && world.npcs.lineIndex(0) == 0 && world.npcs.text(0).chars == "Moved",
          "and to the second again: from its beginning");
}

TEST(npcs_village_new_game) {
    const Area& area = village();
    World world(area);
    world.placeAt(*area.spawn("Default"));
    world.update(DT, 0, 0, false);
    auto npc = [&](const char* path) { return area.npcOn(area.findObject(path)); };
    const int exploresse = npc("Map/Village 8Puzzle/STile (1)/All Tile Maps/House Tile Maps/Decorations/Exploresse");
    const int romeo = npc("Map/Village 8Puzzle/STile (1)/Objects/Romeo");
    const int kevin = npc("Map/Village 8Puzzle/STile (4)/Objects/Kevin");
    check(exploresse >= 0 && romeo >= 0 && kevin >= 0, "found Exploresse, Romeo, Kevin");
    if (exploresse < 0 || romeo < 0 || kevin < 0) return;
    check(!world.npcs.textShown(exploresse) && world.npcs.pingShown(exploresse), "by the bed: Exploresse's ping is up");
    const NpcDef& ex = area.npcs[exploresse];
    float ex0, ey0;
    world.grid.ownerBase(ex.owner, ex0, ey0);
    world.player.x = ex0 + ex.x - 0.5f; // inside her talk trigger (a 2 x 2 box around her)
    world.player.y = ey0 + ex.y;
    world.update(DT, 0, 0, false);
    check(world.npcs.textShown(exploresse) && world.npcs.text(exploresse).chars.rfind("You're awake! Cat", 0) == 0,
          "walking up to her: she greets the player: " + world.npcs.text(exploresse).chars);
    // Romeo's first conditional sends him to RomeoStart, off his tile, until tile 1 moves.
    float x, y, sx, sy;
    world.objectBase(area.npcs[romeo].object, area.npcs[romeo].owner, x, y);
    const int start = area.findObject("Map/World Grid/Objects/RomeoStart/RomeoStart (1)");
    const TransformArgDef* target = nullptr;
    for (const TransformArgDef& t : area.transformArgs)
        if (t.object == start) target = &t;
    check(target != nullptr, "RomeoStart is a teleport target");
    if (!target) return;
    world.grid.ownerBase(target->owner, sx, sy);
    check(std::fabs(x + area.npcs[romeo].x - sx - target->x) < 1e-3f && std::fabs(y + area.npcs[romeo].y - sy - target->y) < 1e-3f,
          "Romeo stands at RomeoStart");
    // NPCs on uncollected tiles wait for their tile (their objects are off).
    check(!world.objects.activeInHierarchy(area.npcs[kevin].object) && !world.npcs.pingShown(kevin), "Kevin's tile 4: not yet");
    world.setTileActive(4, true);
    check(world.npcs.pingShown(kevin), "collected: Kevin starts, ping up");
}

TEST(npcs_validation_rejects_bad_references) {
    const char* cases[][3] = {
        {R"("method": "NPC.OnDialogueTriggerEnter", "target": 0)", R"("method": "NPC.OnDialogueTriggerEnter", "target": 1)",
         "dialogue trigger call on an object without an NPC"},
        {R"("objectArg": 2,)", R"("objectArg": 9,)", "teleport -> missing object"},
        {R"("objectArg": 2, "argOwner": 1, "argPos": [5, 1])", R"("argOwner": 1)", "teleport without a target"},
        {R"("argOwner": 1)", R"("argOwner": 4)", "teleport target owner is not an STile"},
        {R"("npcs": [{"object": 0)", R"("npcs": [{"object": 7)", "npc -> missing object"},
        {R"("name": "N", "owner": 1)", R"("name": "N", "owner": 3)", "npc owner is not an STile"},
    };
    for (const auto& c : cases) {
        std::string json = NPC_AREA;
        const size_t at = json.find(c[0]);
        check(at != std::string::npos, std::string("fixture has ") + c[0]);
        if (at == std::string::npos) continue;
        json.replace(at, std::string(c[0]).size(), c[1]);
        testSetFile("synthetic/bad_npcs.json", json);
        Area a;
        std::string error;
        check(!loadArea("synthetic/bad_npcs.json", a, &error), std::string("rejects ") + c[2]);
        testClearFiles();
    }
}
