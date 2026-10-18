// The title screen's menu (game/ui/title_menu): Main, the profiles' cards (play, new, delete), the
// name keyboard.
#include <string>

#include "game/save/profile.h"
#include "game/ui/menu_skin.h"
#include "game/ui/title_menu.h"
#include "test.h"
#include "test_platform.h"

namespace {

using Menu = TitleMenu;
using Kind = TitleMenu::Action::Kind;

Menu::Action press(Menu& m, uint32_t button) {
    plat::Input in;
    in.pressed = in.held = button;
    return m.update(in);
}

// A stylus tap on `w`'s middle: touch down, release.
Menu::Action tap(Menu& m, Menu::Widget w) {
    Menu::Box b;
    if (!m.box(w, b)) {
        check(false, "widget " + std::to_string(w) + " is shown");
        return {};
    }
    plat::Input in;
    in.touching = true;
    in.touchX = b.x + b.w / 2;
    in.touchY = b.y + b.h / 2;
    m.update(in);
    return m.update(plat::Input{});
}

void saveProfile(int slot, const char* name, int64_t lastSaved, double playTime = 0) {
    SaveProfile p = SaveProfile::make(name);
    p.lastSaved = lastSaved;
    p.playTime = playTime;
    p.save(slot);
}

Menu opened() {
    Menu m;
    m.open();
    m.update(plat::Input{}); // (lets go of the touch it opened under)
    return m;
}

} // namespace

TEST(title_menu_without_profiles_goes_to_new_save) {
    testUserFiles().clear();
    Menu m = opened();
    CHECK(m.page() == Menu::Page::Main && m.selected() == Menu::PLAY); // (Continue can't be used)
    Menu::Box b;
    CHECK(!m.box(Menu::CONTINUE, b));
    CHECK(press(m, plat::BTN_UP).kind == Kind::None && m.selected() == Menu::PLAY); // (nothing usable above)
    press(m, plat::BTN_DOWN);
    CHECK(m.selected() == Menu::OPTIONS);
}

TEST(title_menu_new_game_with_a_typed_name) {
    testUserFiles().clear();
    Menu m = opened();
    press(m, plat::BTN_A); // Play: no profiles, straight to New Save (slot 1)
    CHECK(m.page() == Menu::Page::NewSave && m.selected() == Menu::KEY);
    CHECK(press(m, plat::BTN_A).kind == Kind::None && m.name() == "q");
    press(m, plat::BTN_RIGHT);
    CHECK(m.selected() == Menu::KEY + 1);
    tap(m, Menu::CAPS);
    CHECK(m.caps());
    tap(m, (Menu::Widget)(Menu::KEY + 2)); // E
    tap(m, Menu::DEL);
    tap(m, Menu::DEL);
    CHECK(m.name().empty());
    CHECK(tap(m, Menu::DONE).kind == Kind::None); // (no name yet)
    for (int i = 0; i < 12; i++) tap(m, (Menu::Widget)(Menu::KEY + 10)); // A x12: 10 at most
    CHECK(m.name() == std::string(10, 'A'));
    const Menu::Action a = tap(m, Menu::DONE);
    CHECK(a.kind == Kind::NewGame && a.slot == 0 && a.name == std::string(10, 'A'));
    // Back from New Save lands on Saves (the skip happens once), on its card.
    press(m, plat::BTN_B);
    CHECK(m.page() == Menu::Page::Saves && m.selected() == Menu::CARD);
    press(m, plat::BTN_B);
    CHECK(m.page() == Menu::Page::Main && m.selected() == Menu::PLAY);
    press(m, plat::BTN_A); // (Saves closed: its skip flag cleared, so Play skips again)
    CHECK(m.page() == Menu::Page::NewSave);
}

TEST(title_menu_keyboard_moves_by_position) {
    testUserFiles().clear();
    Menu m = opened();
    press(m, plat::BTN_A);
    press(m, plat::BTN_DOWN); // q -> a (the row below, half a key in)
    CHECK(m.selected() == Menu::KEY + 10);
    press(m, plat::BTN_DOWN); // a -> z
    CHECK(m.selected() == Menu::KEY + 19);
    press(m, plat::BTN_DOWN); // z -> Caps
    CHECK(m.selected() == Menu::CAPS);
    press(m, plat::BTN_RIGHT);
    press(m, plat::BTN_RIGHT);
    CHECK(m.selected() == Menu::DONE);
    press(m, plat::BTN_UP); // Done -> a key above it
    CHECK(m.selected() >= Menu::KEY + 19 && m.selected() < Menu::CAPS);
    for (int i = 0; i < 4; i++) press(m, plat::BTN_UP);
    CHECK(m.selected() == Menu::NEW_BACK || (m.selected() >= Menu::KEY && m.selected() < Menu::KEY + 10));
}

TEST(title_menu_continue_plays_the_latest_profile) {
    testUserFiles().clear();
    saveProfile(0, "Old", 100, 3600 + 32 * 60);
    saveProfile(2, "New", 200);
    Menu m = opened();
    CHECK(m.selected() == Menu::CONTINUE && m.continueSlot() == 2);
    const Menu::Action a = press(m, plat::BTN_A);
    CHECK(a.kind == Kind::Play && a.slot == 2);
    CHECK(m.card(0).name == "Old" && Menu::timeText(m.card(0).playTime) == "1h32" && m.card(1).status == SaveProfile::Read::None);
    // Play -> Saves: a profile's card plays it, an empty one opens New Save for its slot.
    press(m, plat::BTN_DOWN);
    press(m, plat::BTN_A);
    CHECK(m.page() == Menu::Page::Saves);
    const Menu::Action b = tap(m, Menu::CARD);
    CHECK(b.kind == Kind::Play && b.slot == 0);
    tap(m, (Menu::Widget)(Menu::CARD + 1));
    CHECK(m.page() == Menu::Page::NewSave);
    tap(m, (Menu::Widget)(Menu::KEY + 5));
    const Menu::Action c = tap(m, Menu::DONE);
    CHECK(c.kind == Kind::NewGame && c.slot == 1 && c.name == "y");
    testUserFiles().clear();
}

TEST(title_menu_deletes_after_confirm) {
    testUserFiles().clear();
    saveProfile(0, "Mittens", 10);
    saveProfile(1, "Tofu", 20);
    Menu m = opened();
    press(m, plat::BTN_DOWN);
    press(m, plat::BTN_A); // Play -> Saves
    Menu::Box b;
    CHECK(!m.box(Menu::CONFIRM, b));
    tap(m, Menu::TRASH);
    CHECK(m.deleting());
    CHECK(tap(m, (Menu::Widget)(Menu::CARD + 1)).kind == Kind::None && m.confirming() == 1); // (not played)
    CHECK(m.box(Menu::CONFIRM, b));
    tap(m, Menu::CARD); // another card: it awaits Confirm instead
    CHECK(m.confirming() == 0);
    tap(m, Menu::CONFIRM);
    CHECK(m.card(0).status == SaveProfile::Read::None && m.card(1).status == SaveProfile::Read::Ok);
    CHECK(m.selected() == Menu::CARD); // (the cursor stays on the deleted card, not back on Back)
    CHECK(testUserFiles().count(SaveProfile::fileName(0)) == 0 && m.confirming() == -1 && !m.box(Menu::CONFIRM, b));
    CHECK(m.deleting()); // (still deleting, as the original)
    press(m, plat::BTN_A); // a second A: the now-empty card opens New Save there, as the original's
    CHECK(m.page() == Menu::Page::NewSave);
    press(m, plat::BTN_B);
    CHECK(m.page() == Menu::Page::Saves && !m.deleting()); // (Saves closed meanwhile: delete mode ended)
    // Leaving Saves ends delete mode.
    tap(m, Menu::TRASH);
    press(m, plat::BTN_B);
    press(m, plat::BTN_A);
    CHECK(m.page() == Menu::Page::Saves && !m.deleting());
    testUserFiles().clear();
}

TEST(title_menu_unreadable_profile_can_only_be_deleted) {
    testUserFiles().clear();
    testUserFiles()[SaveProfile::fileName(0)] = "garbage";
    Menu m = opened();
    CHECK(m.card(0).status == SaveProfile::Read::Bad && m.continueSlot() == -1);
    press(m, plat::BTN_A); // Play: a file is there, so Saves (not New Save over it)
    CHECK(m.page() == Menu::Page::Saves);
    CHECK(tap(m, Menu::CARD).kind == Kind::None && m.page() == Menu::Page::Saves); // (not played, not replaced)
    tap(m, Menu::TRASH);
    tap(m, Menu::CARD);
    tap(m, Menu::CONFIRM);
    CHECK(m.card(0).status == SaveProfile::Read::None);
    testUserFiles().clear();
}

TEST(title_menu_tap_released_elsewhere_does_nothing) {
    testUserFiles().clear();
    Menu m = opened();
    Menu::Box b;
    CHECK(m.box(Menu::QUIT, b));
    plat::Input in;
    in.touching = true;
    in.touchX = b.x + 2;
    in.touchY = b.y + 2;
    m.update(in);
    CHECK(m.selected() == Menu::QUIT);
    in.touchY = 1; // dragged off it
    m.update(in);
    CHECK(m.update(plat::Input{}).kind == Kind::None);
    CHECK(tap(m, Menu::QUIT).kind == Kind::Quit && tap(m, Menu::OPTIONS).kind == Kind::Options);
}

TEST(title_menu_moves_only_to_what_lines_up) {
    // The D-pad goes to the nearest widget that way sharing columns (up / down) or rows (left /
    // right) with the selected one; nothing lines up: the cursor stays (no diagonal jumps).
    testUserFiles().clear();
    saveProfile(0, "Mittens", 10);
    Menu m = opened();
    auto from = [&](Menu::Widget w, uint32_t dir) {
        m.select(w);
        press(m, dir);
        return m.selected();
    };
    press(m, plat::BTN_DOWN);
    press(m, plat::BTN_A); // Saves
    CHECK(from(Menu::SAVES_BACK, plat::BTN_DOWN) == Menu::CARD && from(Menu::TRASH, plat::BTN_DOWN) == Menu::CARD);
    CHECK(from(Menu::SAVES_BACK, plat::BTN_RIGHT) == Menu::TRASH && from(Menu::TRASH, plat::BTN_LEFT) == Menu::SAVES_BACK);
    CHECK(from(Menu::CARD, plat::BTN_UP) == Menu::SAVES_BACK);
    CHECK(from((Menu::Widget)(Menu::CARD + 1), plat::BTN_LEFT) == Menu::CARD + 1);
    CHECK(from((Menu::Widget)(Menu::CARD + 1), plat::BTN_RIGHT) == Menu::CARD + 1);
    CHECK(from((Menu::Widget)(Menu::CARD + 2), plat::BTN_DOWN) == Menu::CARD + 2); // (no Confirm shown)
    tap(m, (Menu::Widget)(Menu::CARD + 1)); // New Save
    auto key = [](char c) { return (Menu::Widget)(Menu::KEY + (int)(std::string(Menu::KEYS).find(c))); };
    CHECK(from(key('a'), plat::BTN_LEFT) == key('a') && from(key('z'), plat::BTN_LEFT) == key('z')); // (rows don't wrap)
    CHECK(from(key('p'), plat::BTN_RIGHT) == key('p') && from(Menu::DONE, plat::BTN_RIGHT) == Menu::DONE);
    CHECK(from(Menu::CAPS, plat::BTN_LEFT) == Menu::CAPS && from(Menu::CAPS, plat::BTN_UP) == key('z'));
    CHECK(from(Menu::DONE, plat::BTN_UP) == key('m') && from(key('q'), plat::BTN_UP) == Menu::NEW_BACK);
    CHECK(from(key('e'), plat::BTN_UP) == key('e')); // (nothing above it)
    // The middle row's ends miss the bottom row by the space between keys: still lined up (NEAR).
    CHECK(from(key('a'), plat::BTN_DOWN) == key('z') && from(key('l'), plat::BTN_DOWN) == key('m'));
    testUserFiles().clear();
}

TEST(title_menu_lockout_hides_the_cursor) {
    // LockoutSelectablesInCurrentMenu deselects: no brackets until the second is over.
    testUserFiles().clear();
    Menu m = opened();
    m.lockOut(3);
    MenuSkin skin;
    std::string error;
    check(skin.load(&error), "menu skin loads " + error);
    auto drawn = [&] {
        testResetDrawCalls();
        m.draw(skin);
        return testDrawCalls().size();
    };
    const size_t locked = drawn();
    for (int i = 0; i < 3; i++) m.update(plat::Input{});
    CHECK(!m.lockedOut() && drawn() > locked); // (the selected button's brackets)
}
