// The player's Settings (game/save/settings) and the pause menu that changes them (game/ui/pause_menu).
#include <cmath>
#include <string>

#include "game/save/settings.h"
#include "game/ui/pause_menu.h"
#include "test.h"
#include "test_platform.h"

namespace {

plat::Input press(uint32_t buttons) {
    plat::Input in;
    in.pressed = in.held = buttons;
    return in;
}

// A stylus tap at (x, y): down for two frames, then up (the 3DS reports no position on release).
PauseMenu::Action tap(PauseMenu& menu, Settings& s, int x, int y) {
    plat::Input in;
    in.touching = true;
    in.touchX = x;
    in.touchY = y;
    menu.update(in, s);
    menu.update(in, s);
    return menu.update(plat::Input{}, s);
}

void rowPoint(int row, bool rightHalf, int& px, int& py) {
    int x, y, w, h;
    PauseMenu::rowRect(row, x, y, w, h);
    px = rightHalf ? x + w - 4 : x + 4;
    py = y + h / 2;
}

} // namespace

TEST(settings_round_trip) {
    Settings s;
    CHECK(!s.autoMove && s.screenShake == 0.5f); // SettingsManager's defaults
    s.autoMove = true;
    s.screenShake = 0.3f;
    const Settings back = Settings::parse(s.serialize());
    CHECK(back.autoMove && std::abs(back.screenShake - 0.3f) < 1e-6f);
}

TEST(settings_parse_keeps_defaults_for_bad_lines) {
    const Settings s = Settings::parse("junk\r\nautoMove=yes\nscreenShake=7\nfuture=1\n=\n");
    CHECK(!s.autoMove);              // not a number: default
    CHECK(s.screenShake == 1.0f);    // clamped
    CHECK(Settings::parse("") == Settings{});
    CHECK(Settings::parse("screenShake=nan\nautoMove=1") == (Settings{true, 0.5f})); // (last line without \n)
}

TEST(settings_load_and_save_through_user_files) {
    testUserFiles().clear();
    CHECK(Settings::load() == Settings{}); // nothing saved yet
    Settings s{true, 0.8f};
    CHECK(s.save());
    CHECK(testUserFiles().count(Settings::FILE_NAME) == 1);
    const Settings back = Settings::load();
    CHECK(back.autoMove && std::abs(back.screenShake - 0.8f) < 1e-6f);
    testUserFiles().clear();
}

TEST(pause_menu_buttons) {
    PauseMenu menu;
    Settings s;
    CHECK(!menu.isOpen() && menu.update(press(plat::BTN_A), s) == PauseMenu::Action::None);
    menu.open();
    CHECK(menu.isOpen() && menu.cursor() == PauseMenu::RESUME);
    menu.update(press(plat::BTN_DOWN), s);
    CHECK(menu.cursor() == PauseMenu::AUTO_MOVE);
    menu.update(press(plat::BTN_A), s);
    CHECK(s.autoMove);
    menu.update(press(plat::BTN_RIGHT), s);
    CHECK(!s.autoMove);
    menu.update(press(plat::BTN_DOWN), s);
    menu.update(press(plat::BTN_RIGHT), s);
    CHECK(std::abs(s.screenShake - 0.6f) < 1e-6f);
    for (int i = 0; i < 6; i++) menu.update(press(plat::BTN_RIGHT), s);
    CHECK(s.screenShake == 1.0f); // stops at 100%
    menu.update(press(plat::BTN_A), s);
    CHECK(s.screenShake == 0.0f); // A wraps
    menu.update(press(plat::BTN_LEFT), s);
    CHECK(s.screenShake == 0.0f);
    menu.update(press(plat::BTN_UP), s);
    menu.update(press(plat::BTN_UP), s);
    menu.update(press(plat::BTN_UP), s); // wraps to Quit
    CHECK(menu.cursor() == PauseMenu::QUIT);
    CHECK(menu.update(press(plat::BTN_A), s) == PauseMenu::Action::None && menu.confirmingQuit());
    menu.update(press(plat::BTN_UP), s); // moving away cancels
    CHECK(!menu.confirmingQuit());
    menu.update(press(plat::BTN_DOWN), s);
    menu.update(press(plat::BTN_A), s);
    CHECK(menu.update(press(plat::BTN_A), s) == PauseMenu::Action::Quit && !menu.isOpen());

    menu.open();
    CHECK(menu.update(press(plat::BTN_B), s) == PauseMenu::Action::Resume && !menu.isOpen());
    menu.open();
    CHECK(menu.update(press(plat::BTN_START), s) == PauseMenu::Action::Resume);
    menu.open();
    CHECK(menu.update(press(plat::BTN_A), s) == PauseMenu::Action::Resume); // on Resume
}

TEST(pause_menu_stylus) {
    PauseMenu menu;
    Settings s;
    menu.open();
    menu.update(plat::Input{}, s); // (the stylus seen up: taps count from here)
    int x, y;
    rowPoint(PauseMenu::AUTO_MOVE, false, x, y);
    CHECK(tap(menu, s, x, y) == PauseMenu::Action::None && s.autoMove && menu.cursor() == PauseMenu::AUTO_MOVE);
    rowPoint(PauseMenu::SCREEN_SHAKE, true, x, y);
    tap(menu, s, x, y);
    CHECK(std::abs(s.screenShake - 0.6f) < 1e-6f);
    rowPoint(PauseMenu::SCREEN_SHAKE, false, x, y);
    tap(menu, s, x, y);
    tap(menu, s, x, y);
    CHECK(std::abs(s.screenShake - 0.4f) < 1e-6f);
    // A press on one row released on another does nothing.
    int x2, y2;
    rowPoint(PauseMenu::RESUME, false, x2, y2);
    plat::Input in;
    in.touching = true;
    in.touchX = x;
    in.touchY = y;
    menu.update(in, s);
    in.touchX = x2;
    in.touchY = y2;
    menu.update(in, s);
    CHECK(menu.update(plat::Input{}, s) == PauseMenu::Action::None && menu.isOpen());
    CHECK(tap(menu, s, x2, y2) == PauseMenu::Action::Resume && !menu.isOpen());

    // A touch already held when the menu opens isn't a tap.
    menu.open();
    in.touchX = x2;
    in.touchY = y2;
    menu.update(in, s);
    CHECK(menu.update(plat::Input{}, s) == PauseMenu::Action::None && menu.isOpen());
}
