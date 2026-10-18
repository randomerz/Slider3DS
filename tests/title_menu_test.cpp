// The title screen's menu (game/ui/title_menu): Main, the profiles' cards (play, new, delete), the
// name keyboard.
#include <string>

#include "game/save/profile.h"
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
    press(m, plat::BTN_A);
    CHECK(m.page() == Menu::Page::Saves);
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
    CHECK(testUserFiles().count(SaveProfile::fileName(0)) == 0 && m.confirming() == -1 && !m.box(Menu::CONFIRM, b));
    CHECK(m.deleting()); // (still deleting, as the original)
    tap(m, Menu::TRASH);
    CHECK(!m.deleting());
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
