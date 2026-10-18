// The app's screens and save points (game/app/frontend): title -> game -> title, what gets written
// when, and the stylus across screen switches.
#include <cmath>
#include <ctime>
#include <string>

#include "game/app/frontend.h"
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

// "Press any key": the title menu appears (that press is used up), then the key is let go.
void showMenu(Frontend& f) {
    f.update({}, 0, 0);
    f.update(buttons(plat::BTN_A), 0, 0);
    f.update({}, 0, 0);
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
    Frontend f(villageArea(), titleArt(), settings);
    showMenu(f);
    CHECK(f.mode() == Frontend::Mode::Title && frames(f, 30));
    f.shutdown();
    CHECK(testUserFiles().empty() && profileFiles() == 0);
}

TEST(frontend_new_game_saves_once_started) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), settings);
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
    Frontend f(villageArea(), titleArt(), settings);
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
    Frontend f(villageArea(), titleArt(), settings);
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
    frames(f, 2);
    CHECK(saved(0).state.hasCollectible("Slider 2", "Village")); // (SaveGame in DoOnCollect)
    frames(f, 60);
    f.shutdown();
    CHECK(saved(0).playTime > 1.0); // (OnApplicationQuit)
    testUserFiles().clear();
}

TEST(frontend_direct_boot_never_writes) {
    testUserFiles().clear();
    Settings settings;
    Frontend f(villageArea(), titleArt(), settings);
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
    Frontend f(villageArea(), titleArt(), settings);
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
    Frontend f(villageArea(), titleArt(), settings);
    showMenu(f);
    CHECK(f.title().selected() == TitleMenu::CONTINUE);
    // Continue with A while the stylus already rests on tile 1's Artifact button.
    int bx = 0, by = 0;
    float sx = 0, sy = 0;
    Frontend probe(villageArea(), titleArt(), settings); // (where tile 1's button will be)
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
    Frontend f(area, titleArt(), settings);
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
    step({});
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
    step({});
    step(buttons(plat::BTN_A)); // Continue
    CHECK(f.mode() == Frontend::Mode::Game);
    for (int i = 0; i < 30; i++) step({});
    f.bootDirect();
    for (int i = 0; i < 30; i++) step({});
    testUserFiles().clear();
}
