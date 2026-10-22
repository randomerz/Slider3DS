// The tutorial hints (game/hints) on the real Village: the exported list and its 3DS texts, the
// countdowns, what dismisses them, the box's queue and fades, and what the save keeps.
#include <string>

#include "game/events/events.h"
#include "game/render/font.h"
#include "game/world/world.h"
#include "platform/platform.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60.0f;
constexpr int FADE_FRAMES = 30; // UIHints' 0.5 s

int hint(const char* name) { return villageArea().hints.find(name); }

void frames(World& world, int n, float moveX = 0) {
    for (int i = 0; i < n; i++) world.update(DT, moveX, 0, false);
}

// The first exported call to `method` in `calls`, null if none.
const CallDef* findCall(const std::vector<CallDef>& calls, CallDef::Method method) {
    for (const CallDef& c : calls)
        if (c.method == method) return &c;
    return nullptr;
}

} // namespace

TEST(hints_village_exports_its_hints_with_3ds_texts) {
    const HintsDef& h = villageArea().hints;
    CHECK(h.present && h.list.size() == 6 && h.fadeDuration == 0.5f);
    CHECK(h.tileMoves.size() == 1 && h.tileMoves[0].anyTile && h.onMove.size() == 1);
    const std::pair<const char*, const char*> texts[] = {
        {"move", "Use the Circle Pad to move"},
        {"interact", "Press A to interact"},
        {"tab", ""}, // (the Artifact is always on the bottom screen: dropped)
        {"tiles", "Tap sliders on the bottom screen to move them"},
        {"dialogue", "Press A for more dialogue from some NPCs"},
        {"items", "Press A to pick up items"},
    };
    Font font;
    std::string error;
    check(font.load("fonts/caption.json", &error), "caption font: " + error);
    for (const auto& [name, text] : texts) {
        const int i = hint(name);
        CHECK(i >= 0);
        if (i < 0) continue;
        const std::string got = Hints::text(h.list[i]);
        check(got == text, std::string(name) + ": " + got);
        check(font.textWidth(got) <= plat::TOP_W - 8, std::string(name) + " fits the top screen on one line");
    }
    // Any hint the 3DS can't word (a button it lacks) needs an override, or it would silently vanish.
    for (const HintDef& d : h.list) check(!Hints::text(d).empty() || Hints::overridden(d), d.name + " has a 3DS text");
}

TEST(hints_move_shows_after_its_countdown_and_moving_dismisses_it) {
    // triggerOnLoad: 4 s after the scene starts the box fades in with it; walking takes it away
    // (PlayerMove -> DisableHint), for good: the save keeps it complete.
    World world(villageArea(), DEFAULT_SETTINGS);
    frames(world, 4 * 60 - 2);
    CHECK(world.hints.boxAlpha() == 0 && world.hints.shownHint() < 0);
    frames(world, 3);
    CHECK(world.hints.shownHint() == hint("move"));
    frames(world, FADE_FRAMES);
    CHECK(world.hints.boxAlpha() == 1 && world.hints.textAlpha() == 1);
    frames(world, 1, 1); // (a step)
    CHECK(world.state.flag("HintComplete move"));
    frames(world, FADE_FRAMES / 2);
    CHECK(world.hints.boxAlpha() > 0.4f && world.hints.boxAlpha() < 0.6f);
    frames(world, FADE_FRAMES / 2 + 1);
    CHECK(world.hints.boxAlpha() == 0 && world.hints.shownHint() < 0);
    world.reloadObjects(); // (a scene load: completed hints stay away)
    frames(world, 6 * 60);
    CHECK(world.hints.boxAlpha() == 0);
}

TEST(hints_moving_before_the_countdown_means_it_never_shows) {
    World world(villageArea(), DEFAULT_SETTINGS);
    frames(world, 10, 1);
    frames(world, 6 * 60);
    CHECK(world.hints.boxAlpha() == 0 && world.state.flag("HintComplete move"));
}

TEST(hints_box_shows_them_in_turn) {
    // Two hints up: dismissing the shown one fades its text out and the next one's in (the box
    // stays); dismissing the last fades the box. A removal during a fade waits for it.
    World world(villageArea(), DEFAULT_SETTINGS);
    world.hints.disable(world, hint("move"));
    const int dialogue = hint("dialogue"), interact = hint("interact");
    world.hints.trigger(world, dialogue); // 1 s
    world.hints.trigger(world, interact); // 5 s
    frames(world, 62);
    CHECK(world.hints.shownHint() == dialogue && world.hints.boxAlpha() < 1);
    frames(world, 4 * 60);
    CHECK(world.hints.shownHint() == dialogue && world.hints.boxAlpha() == 1); // (interact waits its turn)
    world.hints.disable(world, dialogue); // (between frames: its fade moves from the next one's coroutines)
    frames(world, FADE_FRAMES);
    CHECK(world.hints.shownHint() == dialogue && world.hints.textAlpha() > 0 && world.hints.boxAlpha() == 1);
    frames(world, 1);
    CHECK(world.hints.shownHint() == interact && world.hints.textAlpha() == 0);
    world.hints.disable(world, interact); // (mid-fade: after it)
    frames(world, FADE_FRAMES);
    CHECK(world.hints.textAlpha() == 1 && world.hints.boxAlpha() == 1);
    frames(world, FADE_FRAMES + 1);
    CHECK(world.hints.boxAlpha() == 0 && world.hints.shownHint() < 0);
}

TEST(hints_exported_calls_drive_them) {
    // Leaving the house (the Artifact Prompt Trigger) starts "tiles" (and "tab": never shown); a
    // real slide dismisses "tiles" (StileMoveHint, through World's grid events), but only once it was
    // started (canDisableHint off).
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.hints.disable(world, hint("move"));
    const int tiles = hint("tiles");
    auto slide = [&]() { // tile 1 into a free slot next to it, run to its end
        const TileState* t = world.grid.tile(1);
        const auto options = world.artifact.moveOptions(world.grid, t->x, t->y);
        CHECK(!options.empty());
        if (options.empty()) return;
        CHECK(world.artifact.tryMove(world.grid, t->x, t->y, options[0].first, options[0].second, world.player.stile));
        frames(world, 120);
    };
    slide();
    CHECK(!world.state.flag("HintComplete tiles"));
    const TriggerEventDef* prompt = nullptr;
    for (const TriggerEventDef& te : area.triggerEvents)
        if (area.pathOf(te.object).find("Artifact Prompt Trigger") != std::string::npos) prompt = &te;
    CHECK(prompt != nullptr);
    if (!prompt) return;
    runCalls(world, prompt->onEnter);
    CHECK(world.state.flag("HintCountdown tiles") && world.state.flag("HintCountdown tab"));
    frames(world, 33 * 60 + 2);
    CHECK(world.hints.boxAlpha() == 0); // ("tab" isn't shown)
    frames(world, 2 * 60 + 1);
    CHECK(world.hints.shownHint() == tiles);
    slide();
    CHECK(world.state.flag("HintComplete tiles") && !world.state.flag("HintCountdown tiles"));
}

TEST(hints_items_can_be_dismissed_once_enabled) {
    // "items" (canDisableHint off): picking an item up (its onPickUp DisableHint) does nothing until
    // Archibald's line runs EnableDisabling; then it completes it.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    const CallDef* dismiss = nullptr;
    for (const ItemDef& it : area.items)
        if (!dismiss) dismiss = findCall(it.onPickUp, CallDef::Method::HintDisable);
    const CallDef* enable = nullptr;
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (const DialogueDef& l : c.dialogue)
                if (!enable) enable = findCall(l.onStart, CallDef::Method::HintEnableDisabling);
    CHECK(dismiss && enable);
    if (!dismiss || !enable) return;
    runCalls(world, {*dismiss});
    CHECK(!world.state.flag("HintComplete items"));
    runCalls(world, {*enable});
    runCalls(world, {*dismiss});
    CHECK(world.state.flag("HintComplete items"));
}

TEST(hints_a_started_countdown_survives_a_load) {
    // HintCountdown is saved: after a scene load the hint counts down again from its full delay.
    World world(villageArea(), DEFAULT_SETTINGS);
    world.hints.disable(world, hint("move"));
    world.hints.trigger(world, hint("items")); // 4 s
    frames(world, 60);
    world.reloadObjects();
    frames(world, 4 * 60 - 2);
    CHECK(world.hints.shownHint() < 0);
    frames(world, 3);
    CHECK(world.hints.shownHint() == hint("items"));
    world.hints.disable(world, hint("items")); // (a saved countdown makes it dismissable: SetBools)
    CHECK(world.state.flag("HintComplete items"));
}
