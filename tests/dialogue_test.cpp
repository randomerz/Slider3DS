// NPC dialogue: text parsing / layout / typing (game/npc/dialogue_text) with the real dialogue font, and
// the NPC dialogue state machine (game/npc/npcs) on a small synthetic area and the Village.
#include <algorithm>
#include <cmath>
#include <string>

#include "game/events/events.h"
#include "game/npc/dialogue_text.h"
#include "game/npc/npcs.h"
#include "game/render/area_view.h"
#include "game/render/font.h"
#include "game/render/sprite_sheets.h"
#include "game/world/world.h"
#include "platform/platform.h"
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

// One STile; "Npc" (0) with its dialogue trigger "Npc/Talk" (1, radius 2 around the NPC) and message
// ping "Npc/Ping" (3), and a teleport target "Spot" (2) at (5, 1). Conditionals:
//   0 (always): "Hello there" (waits for the player; onStart flag "start0", onEnd "end0"), then
//      "Bye" (onEnd "end1" and nudges the player +x); exhausting it sets "exhausted", nudges +y.
//   1 (flag "next"; always from the beginning): entering teleports the NPC to Spot (5, 1) and flips
//      its sprite. "Moved" (dontInterrupt, 0.2 s delay; once only: onStart "moved", onEnd nudges +x),
//      then "Done".
//   2 (flag "vanish"): entering hides the NPC itself. "Bye now".
//   3 (flag "shout"): entering types at once (NPC.TypeCurrentDialogue). "Hey!" (dontInterrupt).
//   4 (grid "21": tiles 1 and 2 swapped): "Swapped".
//   5 (flag "stay"): entering teleports the NPC to "Here" (5), where it already is.
//   6 (flag "happy"): entering swaps its Animator's controller to "C2".
// Its Animator (on the NPC object) runs controller "C": Idle <-> Sus on the "sus" bool; Cry (played
// by "Hello there"'s animationOnStart) goes back to Idle when its 0.25 s clip ends.
// A second tile (2) sits right of tile 1.
// The nudges (UPPRelativePlayer by 1/128) count how often a call ran; flags can't. "Hello there", "Bye"
// and "Moved" show the Exclamation emote ("Npc/Emotes" (4), left of the face; its Animator "E": the intro
// state's sprite 0, then ExclamationLoop's sprite 1). "C2" (swapped in by "happy") has an Any State
// transition to Jump on its "jump" bool.
const char* NPC_AREA = R"json({
  "ppu": 16,
  "grid": {"width": 2, "height": 1, "targetGrid": "12", "housingOffset": -150},
  "stiles": [{"islandId": 1, "name": "STile (1)", "x": 0, "y": 0, "width": 17, "isTileActive": true},
             {"islandId": 2, "name": "STile (2)", "x": 1, "y": 0, "width": 17, "isTileActive": true}],
  "objects": [{"path": "Npc", "parent": -1, "active": true, "owner": 1}, {"path": "Npc/Talk", "parent": 0, "active": true, "owner": 1},
              {"path": "Spot", "parent": -1, "active": true, "owner": 1, "pos": [5, 1]},
              {"path": "Npc/Ping", "parent": 0, "active": false, "owner": 1, "pos": [0, 1]},
              {"path": "Npc/Emotes", "parent": 0, "active": true, "owner": 1, "pos": [-0.5, 0.5]},
              {"path": "Here", "parent": -1, "active": true, "owner": 1, "pos": [0, 0]}],
  "sheets": [{"name": "s", "file": "s.png", "size": [32, 16]}],
  "sprites": [{"name": "ex0", "sheet": 0, "rect": [0, 0, 16, 16], "pivot": [8, 8], "ppu": 16},
              {"name": "exLoop", "sheet": 0, "rect": [16, 0, 16, 16], "pivot": [8, 8], "ppu": 16}],
  "anims": [{"name": "Exclamation", "frames": [0], "times": [0], "length": 0.25, "loop": false},
            {"name": "ExclamationLoop", "frames": [1], "times": [0], "length": 1, "loop": true}],
  "animatorControllers": [
    {"name": "C", "params": [{"name": "sus", "default": false}], "default": 0, "any": [], "states": [
      {"name": "Idle", "anim": 1, "speed": 1, "transitions": [{"to": 1, "exitTime": -1, "conds": [[0, true]]}]},
      {"name": "Sus", "anim": 0, "speed": 1, "transitions": [{"to": 0, "exitTime": -1, "conds": [[0, false]]}]},
      {"name": "Cry", "anim": 0, "speed": 1, "transitions": [{"to": 0, "exitTime": 1, "conds": []}]}]},
    {"name": "C2", "params": [{"name": "jump", "default": false}], "default": 0,
     "any": [{"to": 1, "exitTime": -1, "conds": [[0, true]]}], "states": [
      {"name": "Happy", "anim": 1, "speed": 1, "transitions": []},
      {"name": "Jump", "anim": 0, "speed": 1, "transitions": []}]},
    {"name": "E", "params": [], "default": 1, "any": [], "states": [
      {"name": "Exclamation", "anim": 0, "speed": 1, "transitions": [{"to": 1, "exitTime": 1, "conds": []}]},
      {"name": "ExclamationLoop", "anim": 1, "speed": 1, "transitions": []}]}],
  "triggerColliders": [{"object": 1, "owner": 1, "kind": "circle", "center": [0, 0], "radius": 2}],
  "triggerEvents": [{"object": 1, "onPlayerEnter": true,
                     "onEnter": [{"method": "NPC.OnDialogueTriggerEnter", "target": 0}],
                     "onExit": [{"method": "NPC.OnDialogueTriggerExit", "target": 0}]}],
  "npcs": [{"object": 0, "name": "N", "facingLeft": false, "pingObject": 3,
    "animator": {"object": 0, "controller": 0, "keepState": false, "controllerObject": 0},
    "emote": {"object": 4, "pos": [-0.5, 0.5], "sortY": 0.5, "sortingLayerValue": 9, "order": 0, "localX": -0.5,
              "facesRight": false, "npcSprite": 0, "npcSpriteFlipped": false, "controller": 2, "keepState": false},
    "text": {"box": [-3.42, 0.55, 3.42, 2.7], "fontSize": 1, "lineAdvance": 0.4309, "wordSpacing": -0.1,
             "charSpacing": 0, "sortingLayerValue": 9, "order": 99},
    "conds": [
      {"conditions": [], "anyOf": false, "alwaysStartFromBeginning": false, "onConditionalEnter": [],
       "onDialogueChainExhausted": [{"method": "VarManager.SetBoolOn", "string": "exhausted"}, {"method": "PlayerPositionChanger.UPPRelativePlayer", "owner": 0, "pos": [0, 0], "dPos": [0, 0.0078125]}],
       "dialogue": [
         {"text": "Hello there", "delay": 0.5, "emoteOnStart": 2, "animationOnStart": "Cry", "waitForAction": true, "manual": false, "noRepeatEvents": false,
          "dontInterrupt": false, "onStart": [{"method": "VarManager.SetBoolOn", "string": "start0"}],
          "onEnd": [{"method": "VarManager.SetBoolOn", "string": "end0"}]},
         {"text": "Bye", "delay": 0, "emoteOnStart": 2, "waitForAction": false, "manual": false, "noRepeatEvents": false,
          "dontInterrupt": false, "onStart": [], "onEnd": [{"method": "VarManager.SetBoolOn", "string": "end1"}, {"method": "PlayerPositionChanger.UPPRelativePlayer", "owner": 0, "pos": [0, 0], "dPos": [0.0078125, 0]}]}]},
      {"conditions": [{"type": "flag", "flag": "next"}], "anyOf": false, "alwaysStartFromBeginning": true,
       "onConditionalEnter": [{"method": "NPC.Teleport", "target": 0, "objectArg": 2},
                              {"method": "SpriteRenderer.set_flipX", "target": 0, "bool": true}],
       "onDialogueChainExhausted": [],
       "dialogue": [
         {"text": "Moved", "delay": 0.2, "emoteOnStart": 2, "waitForAction": false, "manual": false, "noRepeatEvents": true,
          "dontInterrupt": true, "onStart": [{"method": "VarManager.SetBoolOn", "string": "moved"}],
          "onEnd": [{"method": "PlayerPositionChanger.UPPRelativePlayer", "owner": 0, "pos": [0, 0], "dPos": [0.0078125, 0]}]},
         {"text": "Done", "delay": 0, "waitForAction": false, "manual": false, "noRepeatEvents": false,
          "dontInterrupt": false, "onStart": [], "onEnd": []}]},
      {"conditions": [{"type": "flag", "flag": "vanish"}], "anyOf": false, "alwaysStartFromBeginning": false,
       "onConditionalEnter": [{"method": "GameObject.SetActive", "target": 0, "bool": false}],
       "onDialogueChainExhausted": [],
       "dialogue": [{"text": "Bye now", "delay": 0, "waitForAction": false, "manual": false, "noRepeatEvents": false,
                     "dontInterrupt": false, "onStart": [], "onEnd": []}]},
      {"conditions": [{"type": "flag", "flag": "shout"}], "anyOf": false, "alwaysStartFromBeginning": false,
       "onConditionalEnter": [{"method": "NPC.TypeCurrentDialogue", "target": 0}],
       "onDialogueChainExhausted": [],
       "dialogue": [{"text": "Hey!", "delay": 0, "waitForAction": false, "manual": false, "noRepeatEvents": false,
                     "dontInterrupt": true, "onStart": [], "onEnd": []}]},
      {"conditions": [{"type": "grid", "pattern": "21"}], "anyOf": false, "alwaysStartFromBeginning": false,
       "onConditionalEnter": [], "onDialogueChainExhausted": [],
       "dialogue": [{"text": "Swapped", "delay": 0}]},
      {"conditions": [{"type": "flag", "flag": "stay"}], "anyOf": false, "alwaysStartFromBeginning": false,
       "onConditionalEnter": [{"method": "NPC.Teleport", "target": 0, "objectArg": 5}], "onDialogueChainExhausted": [],
       "dialogue": []},
      {"conditions": [{"type": "flag", "flag": "happy"}], "anyOf": false, "alwaysStartFromBeginning": false,
       "onConditionalEnter": [{"method": "Animator.set_runtimeAnimatorController", "target": 0, "controller": 1}],
       "onDialogueChainExhausted": [], "dialogue": []}]}],
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

// Puts the player on the center of `npc`'s dialogue trigger (the trigger whose OnTriggerEnter talks
// to it) and runs a frame.
void standInTalkRange(World& world, int npc) {
    const Area& area = world.area;
    for (const TriggerEventDef& te : area.triggerEvents)
        for (const CallDef& c : te.onEnter)
            if (c.method == CallDef::Method::NpcTriggerEnter && c.target == area.npcs[npc].object)
                for (const ObjectCollider& tc : area.triggerColliders)
                    if (tc.object == te.object) {
                        float bx, by;
                        world.objectBase(tc.object, bx, by);
                        world.player.x = bx + (tc.minX + tc.maxX) * 0.5f;
                        world.player.y = by + (tc.minY + tc.maxY) * 0.5f;
                    }
    world.update(DT, 0, 0, true);
}

// Listens to `npc` for up to `frames` frames, pressing A whenever a line waits for it.
void listen(World& world, int npc, int frames) {
    for (int i = 0; i < frames; i++) {
        world.update(DT, 0, 0, true);
        if (world.npcs.showsPrompt(world.area, npc)) world.interact();
    }
}

int villageNpc(const char* path) { return village().npcOn(village().findObject(path)); }

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
    World world(village());
    check(parseDialogue("<var>CatUpper</var> NOOO", world.state).chars == "CAT NOOO",
          "a new profile sets Cat and CatUpper (SaveProfile's constructor)");
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
    // Explicit line breaks start lines; a trailing one adds no empty bottom line.
    std::vector<GlyphPlace> broken;
    const DialogueText vow = parseDialogue("Juliet, will you\nbe mine?\n", state);
    layoutDialogue(font, vow, box, ppu, broken);
    check(broken[vow.chars.find("be")].y == (float)font.descent &&
              std::fabs(broken[0].y - font.descent - box.lineAdvance * ppu) < 1e-3f,
          "two lines, 'be mine?' at the bottom");
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
    check(world.npcs.currentCond(0) == 0 && world.npcs.pingShown(world, 0) && !world.npcs.textShown(0),
          "started: first conditional, ping up, nothing said");
    standAt(world, 0, 0);
    check(world.npcs.textShown(0) && world.npcs.typer(0).typing() && !world.npcs.pingShown(world, 0), "entering: typing, ping down");
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
    const float nudge = 1.0f / 128;
    check(world.player.x == nudge && world.player.y == nudge, "its onEnd and onDialogueChainExhausted ran once each");
    check(world.npcs.textShown(0), "the last line stays up while the player listens");
    standAt(world, 10, 0);
    check(!world.npcs.textShown(0) && !world.npcs.pingShown(world, 0), "leaving clears it; no ping once exhausted");
    standAt(world, 0, 0);
    check(world.npcs.lineIndex(0) == 1 && world.npcs.textShown(0), "coming back resumes at the last line");
    waitTyped(world);
    check(world.player.x == nudge && world.player.y == 0, "retyped: onEnd runs again, the chain isn't exhausted again");
}

TEST(npcs_conditional_change_teleports_and_dont_interrupt_finishes) {
    World world(npcArea());
    standAt(world, 10, 0);
    world.state.setFlag("next", true);
    world.update(DT, 0, 0, true);
    float x, y, bx, by;
    world.objectBase(0, x, y);
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
    check(world.player.x == 10 + 1.0f / 128, "'Moved' ended: its onEnd ran");
    world.update(DT, 0, 0, true);
    check(!world.npcs.textShown(0), "the next line isn't dontInterrupt: with nobody there it clears");
    world.state.setFlag("moved", false);
    standAt(world, 5, 1);
    check(world.npcs.lineIndex(0) == 0 && !world.state.flag("moved"),
          "back again: from the beginning (alwaysStartFromBeginning), onStart not repeated (noRepeatEvents)");
    waitTyped(world);
    check(world.player.x == 5, "nor its onEnd");
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
    check(!world.npcs.textShown(exploresse) && world.npcs.pingShown(world, exploresse), "by the bed: Exploresse's ping is up");
    float ex, ey;
    world.objectPosition(area.npcs[exploresse].object, ex, ey);
    world.player.x = ex - 0.5f; // inside her talk trigger (a 2 x 2 box around her)
    world.player.y = ey;
    world.update(DT, 0, 0, false);
    check(world.npcs.textShown(exploresse) && world.npcs.text(exploresse).chars.rfind("You're awake! Cat", 0) == 0,
          "walking up to her: she greets the player: " + world.npcs.text(exploresse).chars);
    // Romeo's first conditional sends him to RomeoStart, off his tile, until tile 1 moves.
    float x, y, sx, sy;
    const int start = area.findObject("Map/World Grid/Objects/RomeoStart/RomeoStart (1)");
    world.objectPosition(area.npcs[romeo].object, x, y);
    world.objectPosition(start, sx, sy);
    check(std::fabs(x - sx) < 1e-3f && std::fabs(y - sy) < 1e-3f && world.objects.parent(area.npcs[romeo].object) == start,
          "Romeo stands at RomeoStart, parented to it");
    // NPCs on uncollected tiles wait for their tile (their objects are off).
    check(!world.objects.activeInHierarchy(area.npcs[kevin].object) && !world.npcs.pingShown(world, kevin), "Kevin's tile 4: not yet");
    world.setTileActive(4, true);
    check(world.npcs.pingShown(world, kevin), "collected: Kevin starts, ping up");
}

TEST(npcs_validation_rejects_bad_references) {
    const char* cases[][3] = {
        {R"("method": "NPC.OnDialogueTriggerEnter", "target": 0)", R"("method": "NPC.OnDialogueTriggerEnter", "target": 1)",
         "dialogue trigger call on an object without an NPC"},
        {R"("objectArg": 2})", R"("objectArg": 9})", "teleport -> missing object"},
        {R"(, "objectArg": 2})", "}", "teleport without a target"},
        {R"("npcs": [{"object": 0)", R"("npcs": [{"object": 7)", "npc -> missing object"},
        {R"("pingObject": 3)", R"("pingObject": 8)", "npc -> missing ping object"},
        {R"("to": 1, "exitTime": -1, "conds": [[0, true]])", R"("to": 7, "exitTime": -1, "conds": [[0, true]])",
         "animator transition -> missing state"},
        {R"("conds": [[0, true]])", R"("conds": [[3, true]])", "animator condition -> missing parameter"},
        {R"("controller": 0, "keepState")", R"("controller": 5, "keepState")", "animator -> missing controller"},
        {R"("target": 0, "controller": 1)", R"("target": 1, "controller": 1)", "controller swap on an object without an Animator"},
        {R"("controller": 2, "keepState")", R"("controller": 9, "keepState")", "emote -> missing controller"},
        {R"("text": "Bye", "delay": 0, "emoteOnStart": 2)", R"("text": "Bye", "delay": 0, "emoteOnStart": 7)",
         "emote value out of range"},
        {R"("box": [-3.42, 0.55, 3.42, 2.7])", R"("box": [3.42, 0.55, -3.42, 2.7])", "npc text box inside out"},
        {R"("owner": 1, "pos": [5, 1])", R"("owner": 4, "pos": [5, 1])", "object owner is not an STile"},
        {R"("object": 1, "owner": 1, "kind")", R"("object": 1, "owner": 0, "kind")", "component owner isn't its object's"},
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

TEST(npcs_hiding_itself_ends_the_conversation) {
    // Exploresse / Kevin style: a conditional's onEnter hides the NPC; its trigger goes with it.
    World world(npcArea());
    standAt(world, 0, 0);
    check(world.npcs.textShown(0) && npcOffered(world, 0), "talking");
    world.state.setFlag("vanish", true);
    world.update(DT, 0, 0, true);
    world.update(DT, 0, 0, true);
    check(!world.objects.activeInHierarchy(0) && !world.npcs.textShown(0) && !npcOffered(world, 0),
          "it hid itself: the trigger exit cleared the text and withdrew it");
}

TEST(npcs_type_current_dialogue_without_the_player) {
    // NPC.TypeCurrentDialogue from onConditionalEnter types even with nobody in the trigger; a
    // dontInterrupt line then stays up.
    World world(npcArea());
    standAt(world, 10, 0);
    world.state.setFlag("shout", true);
    world.update(DT, 0, 0, true);
    world.update(DT, 0, 0, true);
    check(world.npcs.textShown(0) && world.npcs.text(0).chars == "Hey!", "it shouts with the player away");
}

TEST(npcs_hidden_mid_line_stop_their_clocks) {
    // Unity stops a deactivated object's coroutines: the typing never finishes on its own (A still
    // skips it), even once the NPC is shown again.
    World world(npcArea());
    standAt(world, 10, 0);
    world.state.setFlag("shout", true);
    world.update(DT, 0, 0, true);
    check(world.npcs.typer(0).typing(), "typing 'Hey!'");
    world.objects.setActive(world.area, 0, false);
    for (int i = 0; i < 30; i++) world.update(DT, 0, 0, true);
    world.objects.setActive(world.area, 0, true);
    for (int i = 0; i < 120; i++) world.update(DT, 0, 0, true);
    check(world.npcs.typer(0).typing() && !world.npcs.typer(0).finished(), "shown again: still mid-line, frozen");
}

TEST(npcs_emotes_show_beside_the_face) {
    World world(npcArea());
    SpritePlacement e;
    standAt(world, 10, 0);
    check(world.npcs.emote(0) == 0 && !world.npcs.emoteSprite(world, 0, e), "no emote at first");
    standAt(world, 0, 0);
    check(world.npcs.emote(0) == NpcEmoteDef::Exclamation && world.npcs.emoteSprite(world, 0, e) && e.sprite == 0 &&
              e.x == -0.5f,
          "its line starts: Exclamation's intro, on the left (the side it faces)");
    for (int i = 0; i < 20; i++) world.update(DT, 0, 0, true); // 0.33 s: past the 0.25 s intro
    check(world.npcs.emoteSprite(world, 0, e) && e.sprite == 1, "then the loop clip");
    standAt(world, 10, 0);
    check(world.npcs.emote(0) == 0, "the box closes: the line's emoteOnLeave (none)");
    world.state.setFlag("next", true); // teleports and flips the NPC's sprite
    world.update(DT, 0, 0, true);
    standAt(world, 5, 1);
    check(world.npcs.emoteSprite(world, 0, e) && e.x == 0.5f, "flipped: the emote moves to the other side");
}

TEST(npcs_recheck_when_the_grid_alone_changes) {
    // Conditionals that read only the game state and the grid are re-checked when either changes:
    // here only the grid does.
    World world(npcArea());
    standAt(world, 30, 0);
    check(world.npcs.currentCond(0) == 0, "first conditional");
    check(world.grid.setGrid("21"), "swap the tiles");
    world.update(DT, 0, 0, true);
    check(world.npcs.currentCond(0) == 4, "the grid conditional wins without any state change");
    // Kinds that read more (carried items, specs) make an NPC re-check every frame.
    const Area& v = village();
    auto byName = [&](const char* name) {
        for (const NpcDef& n : v.npcs)
            if (n.name == name) return &n;
        return (const NpcDef*)nullptr;
    };
    World vw(v);
    auto polls = [&](const char* name) {
        const NpcDef* n = byName(name);
        return n && vw.npcs.pollsEveryFrame((int)(n - v.npcs.data()));
    };
    check(polls("Cathy"), "Cathy (carried cats, specs) re-checks every frame");
    check(byName("Mike") && !polls("Mike"), "Mike (items only) re-checks on changes");
}

TEST(npcs_teleport_onto_their_own_spot_does_nothing) {
    // NPC.Teleport only moves (and reparents) when the target is somewhere else.
    World world(npcArea());
    standAt(world, 30, 0);
    world.state.setFlag("stay", true);
    world.update(DT, 0, 0, true);
    check(world.npcs.currentCond(0) == 5 && world.objects.parent(0) == -1, "already there: not reparented");
}

TEST(npcs_line_versions_stay_unique_across_reloads) {
    // AreaView caches a line's layout by textVersion, and survives World::reloadObjects.
    World world(npcArea());
    standAt(world, 0, 0);
    const uint32_t before = world.npcs.textVersion(0);
    world.reloadObjects();
    standAt(world, 0, 0);
    check(world.npcs.textShown(0) && world.npcs.textVersion(0) != before, "the reloaded NPC's first line has a new version");
}

TEST(npcs_emote_keeps_playing_across_lines_and_restarts_when_shown_again) {
    World world(npcArea());
    SpritePlacement e;
    standAt(world, 0, 0);
    waitTyped(world); // "Hello there" (Exclamation) typed: well past the 0.25 s intro
    check(world.npcs.emoteSprite(world, 0, e) && e.sprite == 1, "looping");
    world.interact(); // "Bye": Exclamation again
    check(world.npcs.emoteSprite(world, 0, e) && e.sprite == 1, "the same emote on the next line keeps playing");
    standAt(world, 30, 0); // the box closes: emoteOnLeave none
    standAt(world, 0, 0);  // "Bye" again: Exclamation from the start
    check(world.npcs.emoteSprite(world, 0, e) && e.sprite == 0, "turned off and on again: it restarts");
}

TEST(npcs_emote_hidden_with_its_npc) {
    World world(npcArea());
    SpriteSheets sheets;
    check(sheets.load(world.area), "sheets load");
    AreaView view;
    auto emoteDraws = [&]() {
        testResetDrawCalls();
        view.begin(world.area, sheets, 0, 0, plat::TOP_W, plat::TOP_H, 0, false);
        view.draw(world);
        int n = 0;
        for (const DrawCall& c : testDrawCalls()) n += c.sheet == "s";
        return n;
    };
    standAt(world, 0, 0);
    check(emoteDraws() == 1, "talking: the emote is drawn");
    world.objects.setActive(world.area, 0, false);
    check(emoteDraws() == 0, "the NPC hidden: so is its emote");
}

TEST(animators_follow_bools_exit_times_and_npc_lines) {
    World world(npcArea());
    standAt(world, 30, 0);
    const Area& a = world.area;
    auto state = [&]() { return world.animators.stateName(a, 0); };
    check(state() == "Idle" && world.animators.sprite(0) == 1, "default state Idle, showing its clip");
    world.animators.setBool(world, 0, "sus", true);
    world.update(DT, 0, 0, true);
    check(state() == "Sus" && world.animators.sprite(0) == 0, "sus: Idle -> Sus");
    world.animators.setBool(world, 0, "sus", false);
    world.update(DT, 0, 0, true);
    check(state() == "Idle", "and back");
    standAt(world, 0, 0); // "Hello there": animationOnStart Cry
    check(state() == "Cry", "a line's animationOnStart plays its state");
    for (int i = 0; i < 20; i++) world.update(DT, 0, 0, true); // 0.33 s: past Cry's 0.25 s clip
    check(state() == "Idle", "the exit-time transition takes it back to Idle");
    // Play on the state it's already in doesn't restart it.
    world.animators.play(world, 0, "Cry");
    for (int i = 0; i < 10; i++) world.update(DT, 0, 0, true);
    world.animators.play(world, 0, "Cry");
    for (int i = 0; i < 6; i++) world.update(DT, 0, 0, true); // 0.27 s since the first Play
    check(state() == "Idle", "a second Play of the current state didn't restart it");
    // Re-enabling the object rebinds the Animator: default state and parameters.
    world.animators.setBool(world, 0, "sus", true);
    world.update(DT, 0, 0, true);
    world.objects.setActive(a, 0, false);
    world.update(DT, 0, 0, true);
    world.objects.setActive(a, 0, true);
    world.update(DT, 0, 0, true);
    check(state() == "Idle" && !world.animators.param(a, 0, "sus"), "re-enabled: rebound to the defaults");
    world.state.setFlag("happy", true);
    world.update(DT, 0, 0, true);
    check(state() == "Happy", "Animator.set_runtimeAnimatorController: the new controller's default state");
    world.animators.setBool(world, 0, "jump", true);
    world.update(DT, 0, 0, true);
    check(state() == "Jump", "an Any State transition");
}

TEST(animators_rebind_before_a_play_on_a_newly_shown_object) {
    // Unity rebinds an Animator inside SetActive(true): a Play right after (same call chain) survives.
    World world(npcArea());
    standAt(world, 30, 0);
    world.objects.setActive(world.area, 0, false);
    world.update(DT, 0, 0, true);
    world.objects.setActive(world.area, 0, true);
    world.animators.play(world, 0, "Cry"); // before any update notices the object came back
    world.update(DT, 0, 0, true);
    check(world.animators.stateName(world.area, 0) == "Cry", "the Play wasn't wiped by a later rebind");
}

TEST(npcs_empty_animation_goes_back_to_idle) {
    // NPCAnimatorController.Play(""): after a named state, back to "Idle".
    World world(npcArea());
    standAt(world, 0, 0); // "Hello there": Cry (the NPC's current animation is now "Cry")
    waitTyped(world);
    world.animators.play(world, 0, "Sus"); // somewhere else meanwhile
    world.interact(); // "Bye": animationOnStart ""
    check(world.animators.stateName(world.area, 0) == "Idle", "Play(\"\") after Cry: Idle");
}

TEST(animators_village_archibald_turns_sus) {
    // Archibald's first line sets his Animator's "sus" bool (NPCAnimatorController.SetBoolToTrue).
    const Area& area = village();
    World world(area);
    world.setTileActive(3, true);
    const int npc = area.npcOn(area.findObject("Map/Village 8Puzzle/STile (3)/Objects/Archibald"));
    check(npc >= 0 && area.npcs[npc].animator >= 0, "Archibald has an Animator");
    if (npc < 0 || area.npcs[npc].animator < 0) return;
    const int animator = area.npcs[npc].animator;
    check(world.animators.stateName(area, animator) == "ArchibaldIdle", "idle at first");
    standInTalkRange(world, npc);
    world.update(DT, 0, 0, true);
    check(world.npcs.textShown(npc) && world.animators.stateName(area, animator) == "ArchibaldIdleSus",
          "talking: he turns sus (" + world.animators.stateName(area, animator) + ")");
}

TEST(village_archeologist_hands_out_slider_3) {
    // The Archeologist's first chain ends with VillageGrid.ActivateSliderCollectible(3).
    const Area& area = village();
    World world(area);
    world.setTileActive(2, true);
    const int npc = villageNpc("Map/Village 8Puzzle/STile (2)/Objects/Archeologist");
    const int slider = area.collectible("Slider 3");
    check(npc >= 0 && slider >= 0, "the Archeologist and Slider 3");
    if (npc < 0 || slider < 0) return;
    const int object = area.collectibles[slider].object;
    check(!world.objects.activeInHierarchy(object), "Slider 3 hidden at first");
    standInTalkRange(world, npc);
    listen(world, npc, 1200);
    check(world.npcs.lineIndex(npc) == 3 && world.objects.activeInHierarchy(object), "after her last line, Slider 3 is out");
}

TEST(village_fish_back_pierre_hands_out_slider_8) {
    // VillageGrid.TurnFishOn (Sam's line at the river) sets the fish flag; Pierre's CheckFishOn spec
    // then picks the conditional whose line activates Slider 8 (on his tile).
    const Area& area = village();
    World world(area);
    world.setTileActive(6, true);
    const int npc = villageNpc("Map/Village 8Puzzle/STile (6)/Objects/Pierre");
    const int slider = area.collectible("Slider 8");
    check(npc >= 0 && slider >= 0, "Pierre and Slider 8");
    if (npc < 0 || slider < 0) return;
    const int object = area.collectibles[slider].object;
    check(!world.objects.activeInHierarchy(object), "Slider 8 hidden at first (its Start)");
    CallDef fish;
    fish.method = CallDef::Method::VillageTurnFishOn;
    runCall(world, fish);
    standInTalkRange(world, npc);
    world.update(DT, 0, 0, true);
    check(world.npcs.text(npc).chars.rfind("The fish are coming back!", 0) == 0 && world.objects.activeInHierarchy(object),
          "'The fish are coming back!' and Slider 8 is out: " + world.npcs.text(npc).chars);
}

TEST(village_activate_collectible_beats_its_start) {
    // SGrid.ActivateCollectible clears shouldDisableAtStart, so the collectible's later Start (its tile
    // collected) doesn't hide it; ActivateSliderCollectible doesn't, so Start hides it again.
    const Area& area = village();
    World world(area);
    const int breadge = area.collectible("Breadge"), slider9 = area.collectible("Slider 9");
    check(breadge >= 0 && slider9 >= 0 && area.collectibles[breadge].disableAtStart && area.collectibles[slider9].disableAtStart,
          "Breadge (tile 9) and Slider 9 (tile 8) hide at start");
    if (breadge < 0 || slider9 < 0) return;
    CallDef a, b;
    a.method = CallDef::Method::VillageActivateCollectible;
    a.stringArg = "Breadge";
    b.method = CallDef::Method::VillageActivateSlider;
    b.intArg = 9;
    runCall(world, a);
    runCall(world, b);
    world.setTileActive(8, true); // their Starts run now
    world.setTileActive(9, true);
    check(world.objects.activeInHierarchy(area.collectibles[breadge].object), "Breadge stays out");
    check(!world.objects.activeInHierarchy(area.collectibles[slider9].object), "Slider 9's Start hid it");
    world.state.setCollectible("Breadge", "Village", true);
    world.objects.setActive(area, area.collectibles[breadge].object, false);
    runCall(world, a);
    check(!world.objects.activeInHierarchy(area.collectibles[breadge].object), "already collected: not again");
}

TEST(village_explode_opens_the_cave_door) {
    const Area& area = village();
    World world(area);
    CallDef c;
    c.method = CallDef::Method::VillageExplode;
    runCall(world, c);
    check(world.objects.activeInHierarchy(area.areaGrid.caveDoorEntrance) &&
              !world.objects.activeInHierarchy(area.areaGrid.caveDoorRocks) && world.state.flag("caveDoorExploded"),
          "entrance on, rocks off, flag set");
}

TEST(village_archibald_slider_4_can_be_picked_up) {
    // With the Coffee, Archibald's lines show Slider 4 and switch its (initially disabled) trigger
    // on (BoxCollider2D.set_enabled): walking onto it picks it up and collects tile 4.
    const Area& area = village();
    World world(area);
    world.setTileActive(3, true);
    world.state.setCollectible("Coffee", "Village", true);
    const int npc = villageNpc("Map/Village 8Puzzle/STile (3)/Objects/Archibald");
    const int slider = area.collectible("Slider 4");
    check(npc >= 0 && slider >= 0, "Archibald and Slider 4");
    if (npc < 0 || slider < 0) return;
    const int object = area.collectibles[slider].object;
    const ObjectCollider* trigger = nullptr;
    for (const ObjectCollider& c : area.triggerColliders)
        if (c.object == object) trigger = &c;
    check(trigger && !world.objects.active(*trigger), "its trigger starts disabled");
    if (!trigger) return;
    standInTalkRange(world, npc);
    listen(world, npc, 1200);
    check(world.objects.activeInHierarchy(object) && world.objects.active(*trigger), "shown, trigger on");
    float bx, by;
    world.objectBase(object, bx, by);
    world.player.x = bx + (trigger->minX + trigger->maxX) * 0.5f;
    world.player.y = by + (trigger->minY + trigger->maxY) * 0.5f - area.player.footBox.offY;
    world.update(DT, 0, 0, true);
    check(world.state.hasCollectible("Slider 4", "Village") && world.grid.tileActive(4), "picked up: tile 4 collected");
}

TEST(animators_player_runs_while_moving) {
    // Player.Update sets isRunning from the input and isOnWater from the boat; the body shows the state.
    const Area& area = village();
    World world(area);
    const int animator = area.player.animator;
    check(animator >= 0, "the player has an Animator");
    if (animator < 0) return;
    world.placeNear(0, 0);
    world.update(DT, 0, 0, false);
    check(world.animators.stateName(area, animator) == "PlayerIdle", "standing: idle");
    world.update(DT, 1, 0, false);
    check(world.animators.stateName(area, animator) == "PlayerRun", "walking: run cycle");
    world.update(DT, 0, 0, false);
    check(world.animators.stateName(area, animator) == "PlayerIdle", "stopping: idle again");
    world.player.onWater = true;
    world.update(DT, 0, 0, true);
    check(world.animators.stateName(area, animator) == "PlayerPirateIdle", "in the boat");
}

TEST(npcs_emote_shown_again_mid_intro_restarts_it) {
    // Off and on again while its Animator is still in the intro state: the intro starts over.
    World world(npcArea());
    SpritePlacement e;
    standAt(world, 0, 0); // "Hello there": Exclamation's 0.25 s intro
    for (int i = 0; i < 8; i++) world.update(DT, 0, 0, true);
    standAt(world, 30, 0); // the box closes: emote off (its Animator keeps going)
    standAt(world, 0, 0);  // on again, 10 frames in
    for (int i = 0; i < 8; i++) world.update(DT, 0, 0, true); // 18 frames since the first start: past 0.25 s
    check(world.npcs.emoteSprite(world, 0, e) && e.sprite == 0, "still the intro: it restarted");
}
