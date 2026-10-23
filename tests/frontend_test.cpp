// The app's screens and save points (game/app/frontend): title -> game -> title, what gets written
// when, and the stylus across screen switches.
#include <cmath>
#include <ctime>
#include <string>

#include "game/app/frontend.h"
#include "game/audio/sounds.h"
#include "game/render/area_view.h"
#include "game/render/font.h"
#include "game/render/sprite_sheets.h"
#include "game/ui/menu_skin.h"
#include "game/save/profile.h"
#include "game/ui/title_screen.h"
#include "test.h"
#include "test_platform.h"

namespace {

plat::Input buttons(uint32_t b) {
    plat::Input in;
    in.pressed = in.held = b;
    return in;
}

plat::Input touchAt(int x, int y, uint32_t b = 0) {
    plat::Input in;
    in.pressed = in.held = b;
    in.touching = true;
    in.touchX = x;
    in.touchY = y;
    return in;
}

const TitleArt& titleArt() {
    static TitleArt art;
    static const bool loaded = art.load();
    check(loaded, "the title art loads");
    return art;
}

const EndArt& endArt() {
    static EndArt art;
    static const bool loaded = art.load();
    check(loaded, "the end art loads");
    return art;
}

// "Press any key": the title menu appears (that press is used up), then takes input once its 1 s
// lockout is over.
void showMenu(Frontend& f) {
    f.update({}, 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0);
    for (int i = 0; i < Frontend::MENU_LOCKOUT_FRAMES; i++) f.update({}, 0, 0);
}

bool frames(Frontend& f, int n, const plat::Input& in = {}) {
    bool running = true;
    for (int i = 0; i < n; i++) running = f.update(in, 0, 0) && running;
    return running;
}

// Taps a title menu widget with the stylus (lifted first: a touch held as a page opens is no tap).
void tapTitle(Frontend& f, TitleMenu::Widget w) {
    f.update({}, 0, 0);
    TitleMenu::Box b;
    check(f.title().box(w, b), "title widget " + std::to_string(w) + " shown");
    f.update(touchAt(b.x + b.w / 2, b.y + b.h / 2), 0, 0);
    f.update({}, 0, 0);
}

// Profile files of any slot (a temporary profile written by mistake would be "profile-1").
int profileFiles() {
    int n = 0;
    for (const auto& file : testUserFiles()) n += file.first.rfind("profile", 0) == 0 ? 1 : 0;
    return n;
}

SaveProfile saved(int slot) {
    SaveProfile p;
    check(SaveProfile::load(slot, p) == SaveProfile::Read::Ok, "slot " + std::to_string(slot) + " saved");
    return p;
}

// A fresh app with no files, on a new profile "a" in slot 1, in the game.
void newGame(Frontend& f) {
    tapTitle(f, TitleMenu::PLAY); // (no files: straight to New Save)
    tapTitle(f, (TitleMenu::Widget)(TitleMenu::KEY + 10));
    tapTitle(f, TitleMenu::DONE);
}

} // namespace

TEST(frontend_boots_to_the_title_and_writes_nothing) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    CHECK(f.mode() == Frontend::Mode::Title && frames(f, 30));
    f.shutdown();
    CHECK(testUserFiles().empty() && profileFiles() == 0);
}

TEST(frontend_new_game_saves_once_started) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    const int64_t before = (int64_t)std::time(nullptr);
    newGame(f);
    CHECK(f.mode() == Frontend::Mode::Game && f.slot() == 0);
    const SaveProfile p = saved(0); // (GameSaver.Start)
    CHECK(p.name == "a" && p.state.str("Cat") == "a" && p.lastSaved >= before && p.hasPlayer && p.inHouse);
    const Spawn& start = villageArea().spawns[f.world().startSpawn()];
    float bx, by;
    f.world().grid.ownerBase(start.owner, bx, by);
    CHECK(p.playerX == bx + start.x && p.playerY == by + start.y);
    testUserFiles().clear();
}

TEST(frontend_quit_saves_and_continue_resumes) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    newGame(f);
    for (int i = 0; i < 30; i++) f.update({}, 1, 0); // (walks right for half a second)
    const float x = f.world().player.x, y = f.world().player.y;
    f.update(buttons(plat::BTN_START), 0, 0);
    CHECK(f.pauseMenu().isOpen() && !f.playing());
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0); // Quit: asks again
    CHECK(f.mode() == Frontend::Mode::Game);
    f.update(buttons(plat::BTN_A), 0, 0);
    CHECK(f.mode() == Frontend::Mode::Title && f.title().page() == TitleMenu::Page::Main);
    const SaveProfile p = saved(0);
    CHECK(std::fabs(p.playTime - 30 * Frontend::FRAME_DT) < 1e-3 && p.playerX == x && p.playerY == y);
    CHECK(!f.titleScreen().menuShown()); // (the title's intro again: MainMenu reloads)
    showMenu(f);
    CHECK(f.title().selected() == TitleMenu::CONTINUE && f.title().card(0).name == "a");
    f.update(buttons(plat::BTN_A), 0, 0); // Continue
    CHECK(f.mode() == Frontend::Mode::Game && f.slot() == 0);
    CHECK(f.world().player.x == x && f.world().player.y == y);
    testUserFiles().clear();
}

TEST(frontend_saves_on_a_pick_up_and_on_closing_in_the_game) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    newGame(f);
    int slider2 = -1;
    for (int i = 0; i < (int)villageArea().collectibles.size(); i++)
        if (villageArea().collectibles[i].name == "Slider 2") slider2 = i;
    CHECK(slider2 >= 0);
    if (slider2 < 0) return;
    float cx, cy;
    f.world().objectPosition(villageArea().collectibles[slider2].object, cx, cy);
    f.world().placeNear(cx, cy);
    frames(f, 2 + PICKUP_FRAMES);
    CHECK(saved(0).state.hasCollectible("Slider 2", "Village")); // (SaveGame in DoOnCollect)
    frames(f, 60);
    f.shutdown();
    CHECK(saved(0).playTime > 1.0); // (OnApplicationQuit)
    testUserFiles().clear();
}

// The area's exits (SceneChanger.ChangeScenes): a fade out at double speed with the player held,
// then the ending, saved with the player on the exit's way-back spawn; its Main Menu (A, after its
// lockout) returns to the title, and Continue resumes on that spawn (not inside the exit again).
TEST(frontend_an_exit_fades_out_saves_and_ends) {
    testUserFiles().clear();
    const Area& area = villageArea();
    const int door = area.findObject("Map/World Grid/Alcove/Cave Door/Scene Change Collider");
    int alcove = -1; // the alcove's Cave Door exit and its spawn
    for (size_t i = 0; i < area.spawns.size(); i++)
        if (area.spawns[i].name == "VillageAlcove") alcove = (int)i;
    CHECK(door >= 0 && alcove >= 0);
    if (door < 0 || alcove < 0) return;
    Settings settings;
    Frontend f(area, titleArt(), endArt(), settings);
    showMenu(f);
    newGame(f);
    f.world().leaveArea(door);
    CHECK(f.world().playerLocked());
    const int fadeFrames = (int)std::ceil(area.fadeDuration / 2 / Frontend::FRAME_DT);
    frames(f, 10);
    f.world().leaveArea(door); // (once: a second call while leaving doesn't restart the fade)
    frames(f, fadeFrames - 11, buttons(plat::BTN_START)); // (no pause while leaving)
    CHECK(f.mode() == Frontend::Mode::Game && !f.pauseMenu().isOpen() && f.world().effects.black() > 0.9f);
    frames(f, 2);
    CHECK(f.mode() == Frontend::Mode::End);
    const SaveProfile p = saved(0);
    float bx, by;
    f.world().grid.ownerBase(area.spawns[alcove].owner, bx, by);
    CHECK(p.playerX == bx + area.spawns[alcove].x && p.playerY == by + area.spawns[alcove].y && !p.inHouse);
    frames(f, EndScreen::LOCKOUT_FRAMES - 1, buttons(plat::BTN_A)); // (too early)
    CHECK(f.mode() == Frontend::Mode::End);
    for (uint32_t b : {plat::BTN_SELECT, plat::BTN_B, plat::BTN_START}) f.update(buttons(b), 0, 0); // (only Main Menu)
    CHECK(f.mode() == Frontend::Mode::End);
    f.audio().take();
    f.update(buttons(plat::BTN_A), 0, 0);
    CHECK(f.mode() == Frontend::Mode::Title);
    bool click = false;
    for (const AudioCue& c : f.audio().take()) click = click || (c.kind == AudioCue::Kind::Play && c.name == sounds::UI_CLICK);
    CHECK(click);
    showMenu(f);
    f.update(buttons(plat::BTN_A), 0, 0); // Continue
    CHECK(f.mode() == Frontend::Mode::Game && !f.world().playerLocked());
    CHECK(f.world().player.x == bx + area.spawns[alcove].x && f.world().player.y == by + area.spawns[alcove].y);
    frames(f, 60);
    CHECK(f.mode() == Frontend::Mode::Game); // (not inside the exit)
    testUserFiles().clear();
}

// Closing the app mid-fade saves the player where the leave puts them, not in the exit.
TEST(frontend_closing_while_leaving_saves_on_the_exit_spawn) {
    testUserFiles().clear();
    const Area& area = villageArea();
    const int door = area.findObject("Map/World Grid/Alcove/Cave Door/Scene Change Collider");
    CHECK(door >= 0);
    if (door < 0) return;
    Settings settings;
    Frontend f(area, titleArt(), endArt(), settings);
    showMenu(f);
    newGame(f);
    f.world().placeNear(30, 30); // (anywhere but the spawn)
    f.world().leaveArea(door);
    frames(f, 5);
    f.shutdown();
    const Spawn& s = area.spawns[f.world().exitSpawn(door)];
    float bx, by;
    f.world().grid.ownerBase(s.owner, bx, by);
    const SaveProfile p = saved(0);
    CHECK(p.playerX == bx + s.x && p.playerY == by + s.y);
    testUserFiles().clear();
}

// The ending's screens draw (Debug asserts on a bad index); a touch held into it is no tap, and its
// button taps on release, so the title doesn't take the stylus as "press any key".
TEST(frontend_the_ending_draws_and_taps_main_menu_on_release) {
    testUserFiles().clear();
    const Area& area = villageArea();
    SpriteSheets sheets;
    MenuSkin skin;
    std::string err;
    CHECK(sheets.load(area) && skin.load(&err));
    Settings settings;
    Frontend f(area, titleArt(), endArt(), settings);
    AreaView view;
    f.bootDirect();
    f.world().leaveArea(0);
    for (int i = 0; i < 120 && f.mode() != Frontend::Mode::End; i++) f.update({}, 0, 0);
    CHECK(f.mode() == Frontend::Mode::End);
    const EndScreen::Box b = EndScreen::buttonBox();
    const plat::Input onButton = touchAt(b.x + b.w / 2, b.y + b.h / 2);
    auto step = [&](const plat::Input& in) {
        f.update(in, 0, 0);
        f.drawTop(view, sheets, skin, false);
        f.drawBottom(sheets, skin);
        testResetDrawCalls();
    };
    for (int i = 0; i < EndScreen::LOCKOUT_FRAMES + 10; i++) step(onButton); // (held from before)
    step({});
    CHECK(f.mode() == Frontend::Mode::End);
    step(onButton);
    step(onButton);
    CHECK(f.mode() == Frontend::Mode::End); // (down: not yet)
    step({});
    CHECK(f.mode() == Frontend::Mode::Title);
    frames(f, 5);
    CHECK(!f.titleScreen().menuShown()); // (the title's intro plays)
    f.bootDirect();
    f.world().leaveArea(0);
    for (int i = 0; i < 120 && f.mode() != Frontend::Mode::End; i++) f.update({}, 0, 0);
    for (int i = 0; i < EndScreen::LOCKOUT_FRAMES - 5; i++) step({});
    for (int i = 0; i < 10; i++) step(onButton); // (down before the button shows, held past it)
    step({});
    CHECK(f.mode() == Frontend::Mode::End);
    f.shutdown();
    CHECK(profileFiles() == 0); // (a temporary profile, still never written)
}

TEST(frontend_direct_boot_never_writes) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    f.bootDirect();
    CHECK(f.mode() == Frontend::Mode::Game && f.slot() == -1 && f.world().state.str("Cat") == "Boomo");
    frames(f, 10);
    f.update(buttons(plat::BTN_START), 0, 0);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0); // Quit to the title
    CHECK(f.mode() == Frontend::Mode::Title);
    f.shutdown();
    CHECK(profileFiles() == 0);
}

TEST(frontend_options_from_the_title) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    tapTitle(f, TitleMenu::OPTIONS);
    CHECK(f.pauseMenu().isOpen() && f.pauseMenu().page() == PauseMenu::Page::Options);
    f.update(buttons(plat::BTN_A), 0, 0); // General
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0); // Auto Move on
    CHECK(settings.autoMove);
    f.update(buttons(plat::BTN_B), 0, 0);
    // Closing it with B while the stylus rests on Quit: lifting it there is no tap.
    TitleMenu::Box quit;
    CHECK(f.title().box(TitleMenu::QUIT, quit));
    f.update(touchAt(quit.x + 4, quit.y + 4, plat::BTN_B), 0, 0);
    CHECK(!f.pauseMenu().isOpen() && f.mode() == Frontend::Mode::Title);
    CHECK(f.update(touchAt(quit.x + 4, quit.y + 4), 0, 0) && f.update({}, 0, 0));
    CHECK(testUserFiles().count(Settings::FILE_NAME) == 1 && Settings::load().autoMove); // (saved as it closed)
    testUserFiles().clear();
}

TEST(frontend_stylus_held_into_the_game_is_no_artifact_press) {
    testUserFiles().clear();
    SaveProfile::make("Tofu").save(0);
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    CHECK(f.title().selected() == TitleMenu::CONTINUE);
    // Continue with A while the stylus already rests on tile 1's Artifact button.
    int bx = 0, by = 0;
    float sx = 0, sy = 0;
    Frontend probe(villageArea(), titleArt(), endArt(), settings); // (where tile 1's button will be)
    probe.bootDirect();
    CHECK(probe.world().artifact.findButton(1, bx, by));
    probe.artifactUI().slotCenter(villageArea(), bx, by, sx, sy);
    f.update(touchAt((int)sx, (int)sy, plat::BTN_A), 0, 0);
    CHECK(f.mode() == Frontend::Mode::Game);
    f.update(touchAt((int)sx, (int)sy), 0, 0);
    f.update({}, 0, 0);
    CHECK(f.artifactUI().selected() == 0);
    f.update(touchAt((int)sx, (int)sy), 0, 0); // (a fresh tap does select it)
    f.update({}, 0, 0);
    CHECK(f.artifactUI().selected() == 1);
    testUserFiles().clear();
}

TEST(frontend_the_artifact_takes_no_input_while_the_player_is_held) {
    // A cutscene holds the player: the D-pad's cursor, X and the stylus all leave the Artifact alone
    // (a cursor already shown stays; a touch still hides it). Let go, the D-pad works again.
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    showMenu(f);
    newGame(f);
    CHECK(f.mode() == Frontend::Mode::Game);
    f.world().lockPlayer(LOCK_DOLLY, true);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_X), 0, 0);
    CHECK(!f.artifactUI().cursorShown() && f.artifactUI().selected() == 0);
    int bx = 0, by = 0;
    float sx = 0, sy = 0;
    CHECK(f.world().artifact.findButton(1, bx, by));
    f.artifactUI().slotCenter(villageArea(), bx, by, sx, sy);
    f.update(touchAt((int)sx, (int)sy), 0, 0);
    f.update({}, 0, 0);
    CHECK(f.artifactUI().selected() == 0 && f.world().artifact.queueEmpty());
    f.world().lockPlayer(LOCK_DOLLY, false);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    CHECK(f.artifactUI().cursorShown());
    f.world().lockPlayer(LOCK_DOLLY, true);
    f.update(touchAt((int)sx, (int)sy), 0, 0);
    CHECK(!f.artifactUI().cursorShown());
    testUserFiles().clear();
}

TEST(frontend_draws_every_screen) {
    // main's loop with both screens drawn each frame (a Debug build asserts on a bad index): the
    // title before and after "press any key", Options from it, a new game, the pause menu, Quit to
    // the title, Continue, a direct boot.
    testUserFiles().clear();
    const Area& area = villageArea();
    SpriteSheets sheets;
    Font font;
    MenuSkin skin;
    std::string err;
    CHECK(sheets.load(area) && font.load("fonts/dialogue.json", &err) && skin.load(&err));
    Settings settings;
    Frontend f(area, titleArt(), endArt(), settings);
    AreaView view;
    view.setFont(&font);
    auto step = [&](const plat::Input& in) {
        f.update(in, 0, 0);
        f.drawTop(view, sheets, skin, false);
        f.drawBottom(sheets, skin);
        testResetDrawCalls();
    };
    for (int i = 0; i < 300; i++) step({}); // (the whole intro, prompt included)
    step(buttons(plat::BTN_A));
    for (int i = 0; i < Frontend::MENU_LOCKOUT_FRAMES; i++) step({});
    TitleMenu::Box b;
    CHECK(f.title().box(TitleMenu::OPTIONS, b));
    step(touchAt(b.x + 4, b.y + 4));
    step({});
    CHECK(f.pauseMenu().isOpen());
    step(buttons(plat::BTN_B));
    step({});
    CHECK(f.title().box(TitleMenu::PLAY, b));
    step(touchAt(b.x + 4, b.y + 4));
    step({});
    for (TitleMenu::Widget w : {(TitleMenu::Widget)TitleMenu::KEY, TitleMenu::DONE}) {
        CHECK(f.title().box(w, b));
        step(touchAt(b.x + b.w / 2, b.y + b.h / 2));
        step({});
    }
    CHECK(f.mode() == Frontend::Mode::Game);
    for (int i = 0; i < 60; i++) step({});
    for (uint32_t key : {plat::BTN_START, plat::BTN_DOWN, plat::BTN_DOWN, plat::BTN_A, plat::BTN_A}) step(buttons(key));
    CHECK(f.mode() == Frontend::Mode::Title);
    for (int i = 0; i < 10; i++) step({});
    step(buttons(plat::BTN_A)); // (any key)
    for (int i = 0; i < Frontend::MENU_LOCKOUT_FRAMES; i++) step({});
    step(buttons(plat::BTN_A)); // Continue
    CHECK(f.mode() == Frontend::Mode::Game);
    for (int i = 0; i < 30; i++) step({});
    f.bootDirect();
    for (int i = 0; i < 30; i++) step({});
    testUserFiles().clear();
}

TEST(frontend_title_menu_slides_up_as_it_comes_in) {
    // MainMenuButtonsUp: a whole screen below on the key's frame, part way up half a second later,
    // in its place (as TitleMenu draws it) once the lockout ends.
    testUserFiles().clear();
    SaveProfile::make("Tofu").save(0); // (Continue usable: it has a box)
    MenuSkin skin;
    SpriteSheets sheets;
    std::string err;
    CHECK(skin.load(&err));
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    auto menuTop = [&](bool alone = false) { // the bottom screen's highest draw, px
        testResetDrawCalls();
        if (alone) f.title().draw(skin);
        else f.drawBottom(sheets, skin);
        float top = 1e9f;
        for (const DrawCall& d : testDrawCalls()) top = std::min(top, d.y);
        return top;
    };
    frames(f, 30);
    f.update(buttons(plat::BTN_A), 0, 0);
    const float start = menuTop();
    frames(f, Frontend::MENU_LOCKOUT_FRAMES / 4);
    const float quarter = menuTop();
    const float quarterDrop = f.titleScreen().menuDrop(titleArt());
    frames(f, Frontend::MENU_LOCKOUT_FRAMES * 3 / 4);
    const float rest = menuTop();
    CHECK(rest == menuTop(true));
    TitleMenu::Box b; // (the top button; no cursor yet at the start or a quarter in)
    CHECK(f.title().box(TitleMenu::CONTINUE, b) && start == b.y + plat::BOTTOM_H);
    // A quarter in (16 frames: the key's own frame counts): Unity's curve (weighted) has it 0.908
    // screens low; Hermite would have it at 0.817.
    CHECK(std::fabs(quarterDrop - 0.908f) < 0.002f && quarter == b.y + 2 * std::lround(quarterDrop * plat::BOTTOM_H / 2));
    testUserFiles().clear();
}

TEST(frontend_title_key_waits_for_the_second_frame_and_locks_the_menu_out) {
    testUserFiles().clear();
    SaveProfile::make("Tofu").save(0);
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    f.update(buttons(plat::BTN_A), 0, 0); // (frame 1: the listener isn't there yet)
    CHECK(!f.titleScreen().menuShown());
    f.update(buttons(plat::BTN_A), 0, 0);
    CHECK(f.titleScreen().menuShown() && f.title().selected() == TitleMenu::CONTINUE);
    // A second press right after doesn't play: the buttons are locked out for 1 s
    // (LockoutSelectablesInCurrentMenu), then they work.
    for (int i = 0; i < Frontend::MENU_LOCKOUT_FRAMES - 1; i++) f.update(buttons(plat::BTN_A), 0, 0);
    CHECK(f.mode() == Frontend::Mode::Title);
    f.update({}, 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0);
    CHECK(f.mode() == Frontend::Mode::Game && f.slot() == 0);
    testUserFiles().clear();
}

TEST(frontend_title_touch_that_shows_the_menu_is_no_tap) {
    // Tapping to skip the intro where Play will appear, held while it appears and let go: no Play.
    testUserFiles().clear();
    Settings settings;
    Frontend probe(villageArea(), titleArt(), endArt(), settings);
    showMenu(probe);
    TitleMenu::Box play;
    CHECK(probe.title().box(TitleMenu::PLAY, play));
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    f.update({}, 0, 0);
    for (int i = 0; i < Frontend::MENU_LOCKOUT_FRAMES + 10; i++) f.update(touchAt(play.x + 4, play.y + 4), 0, 0);
    CHECK(f.titleScreen().menuShown());
    f.update({}, 0, 0);
    CHECK(f.title().page() == TitleMenu::Page::Main && f.mode() == Frontend::Mode::Title);
    tapTitle(f, TitleMenu::PLAY); // (a fresh tap does)
    CHECK(f.title().page() == TitleMenu::Page::NewSave);
}

TEST(title_prompt_fades_on_the_originals_curve) {
    // AnyKeyFadeIn's alpha keys ease in and out (flat tangents): a quarter of the way in it's
    // 0.156, not 0.25; AnyKeyFadeOut plays it backwards 6x as fast from full.
    TitleScreen s;
    s.start(titleArt());
    const plat::Input none;
    while (s.time() < 4.5f - 1e-4f) s.update(villageArea().cameraShake, none, 1.0f / 60, 0);
    const float in = s.promptAlpha(titleArt());
    check(in > 0.14f && in < 0.17f, "a quarter in: " + std::to_string(in));
    while (s.time() < 6.5f - 1e-4f) s.update(villageArea().cameraShake, none, 1.0f / 60, 0);
    CHECK(s.promptAlpha(titleArt()) == 1.0f);
    plat::Input key;
    key.pressed = plat::BTN_A;
    s.update(villageArea().cameraShake, key, 1.0f / 60, 0);
    for (int i = 0; i < 4; i++) s.update(villageArea().cameraShake, none, 1.0f / 60, 0); // (5 frames: 1/12 s)
    const float out = s.promptAlpha(titleArt());
    check(out > 0.8f && out < 0.88f, "a quarter out: " + std::to_string(out));
}

TEST(title_art_rejects_a_bad_spread) {
    // title_extra.json is generated; a broken one fails the load (and the boot) instead of drawing
    // the background collapsed or mirrored, or the original's layout.
    std::string original;
    CHECK(plat::readFile("ui/title_extra.json", original));
    for (const char* spread : {"[0, 1]", "[1, -2]", "[]"}) {
        testSetFile("ui/title_extra.json", std::string("{\"spread\": ") + spread + ", \"stars\": []}");
        TitleArt art;
        std::string error;
        check(!art.load(&error) && error.find("spread") != std::string::npos, std::string("rejects spread ") + spread);
    }
    for (const char* layout : {"\"logoDown\": 8", "\"playerRises\": false", "\"logoDown\": 8, \"playerRises\": 0"}) {
        testSetFile("ui/title_extra.json", std::string("{\"spread\": [1, 1], ") + layout + ", \"stars\": []}");
        TitleArt art;
        std::string error;
        check(!art.load(&error) && error.find("logoDown") != std::string::npos, std::string("rejects layout ") + layout);
    }
    testSetFile("ui/title_extra.json", original);
    TitleArt art;
    CHECK(art.load());
}

TEST(title_art_rejects_a_menu_slide_longer_than_the_lockout) {
    // The menu's hit boxes stay where it comes to rest: it must be there before input comes back.
    std::string original;
    CHECK(plat::readFile("ui/title.json", original));
    const size_t slide = original.find("\"menuSlide\"");
    const size_t last = original.find("[1, 0,", slide);
    CHECK(slide != std::string::npos && last != std::string::npos && last < original.find('\n', slide));
    std::string longer = original;
    longer.replace(last, 6, "[1.5, 0,");
    testSetFile("ui/title.json", longer);
    TitleArt art;
    std::string error;
    CHECK(!art.load(&error) && error.find("lockout") != std::string::npos);
    testSetFile("ui/title.json", original);
    CHECK(art.load());
}

namespace {

int countCues(const std::vector<AudioCue>& cues, AudioCue::Kind kind, const std::string& name, float value = -1) {
    int n = 0;
    for (const AudioCue& c : cues) n += c.kind == kind && c.name == name && (value < 0 || c.value == value) ? 1 : 0;
    return n;
}

} // namespace

TEST(frontend_asks_for_the_title_music_the_clicks_and_the_villages_music) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), endArt(), settings);
    using K = AudioCue::Kind;
    // MainMenuManager.Start: its music (not restarted if playing), the full arrangement held back.
    std::vector<AudioCue> cues = f.audio().take();
    CHECK(cues.size() == 2 && cues[0].kind == K::PlayMusic && cues[0].name == tracks::MAIN_MENU && !cues[0].restart);
    CHECK(countCues(cues, K::SetParameter, audio_params::MAIN_MENU_ACTIVATED, 0) == 1);
    showMenu(f); // (StartMainMenu)
    cues = f.audio().take();
    CHECK(countCues(cues, K::SetParameter, audio_params::MAIN_MENU_ACTIVATED, 1) == 1 &&
          countCues(cues, K::Play, sounds::UI_CLICK) == 0);
    newGame(f); // (three taps: Play, a key, Done)
    cues = f.audio().take();
    CHECK(countCues(cues, K::Play, sounds::UI_CLICK) == 3);
    CHECK(countCues(cues, K::PlayMusic, tracks::VILLAGE) == 1); // (VillageGrid.Start, once)
    // A tap on an Artifact button clicks (its UIClick).
    int bx, by;
    float sx, sy;
    CHECK(f.world().artifact.findButton(1, bx, by));
    f.artifactUI().slotCenter(villageArea(), bx, by, sx, sy);
    f.update({}, 0, 0);
    f.update(touchAt((int)sx, (int)sy), 0, 0);
    f.update({}, 0, 0);
    CHECK(countCues(f.audio().take(), K::Play, sounds::UI_CLICK) == 1);
    CHECK(!f.audioPaused());
    // Pause > Quit (asked twice: two clicks) and the title's music again.
    f.update(buttons(plat::BTN_START), 0, 0);
    CHECK(f.audioPaused()); // (the game's pause menu holds its pausable sounds)
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_DOWN), 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0);
    cues = f.audio().take();
    CHECK(f.mode() == Frontend::Mode::Title && countCues(cues, K::Play, sounds::UI_CLICK) == 2 &&
          countCues(cues, K::PlayMusic, tracks::MAIN_MENU) == 1);
    // The title's Options isn't the game's pause.
    showMenu(f);
    tapTitle(f, TitleMenu::OPTIONS);
    CHECK(f.pauseMenu().isOpen() && !f.audioPaused());
    testUserFiles().clear();
}
