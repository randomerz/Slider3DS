// The player's Settings (game/save/settings) and the pause menu that changes them (game/ui/pause_menu).
#include <cmath>
#include <string>

#include "game/save/settings.h"
#include "game/ui/pause_menu.h"
#include "test.h"
#include "test_platform.h"

namespace {

using Menu = PauseMenu;

plat::Input press(uint32_t buttons) {
    plat::Input in;
    in.pressed = in.held = buttons;
    return in;
}

plat::Input touch(int x, int y) {
    plat::Input in;
    in.touching = true;
    in.touchX = x;
    in.touchY = y;
    return in;
}

// A stylus tap at (x, y): down for two frames, then up (the 3DS reports no position on release).
Menu::Action tap(Menu& menu, Settings& s, int x, int y) {
    menu.update(touch(x, y), s);
    menu.update(touch(x, y), s);
    return menu.update(plat::Input{}, s);
}

Menu::Action tapItem(Menu& menu, Settings& s, Menu::Item item) {
    const Menu::Box b = Menu::itemBox(item);
    return tap(menu, s, b.x + 4, b.y + b.h / 2);
}

// An open menu that has seen the stylus up (taps count from there).
void openMenu(Menu& menu, Settings& s) {
    menu.open();
    menu.update(plat::Input{}, s);
}

bool near(float a, float b) { return std::abs(a - b) < 1e-6f; }

} // namespace

TEST(settings_round_trip) {
    Settings s;
    CHECK(!s.autoMove && s.screenShake == 0.5f); // SettingsManager's defaults
    s.autoMove = true;
    s.setScreenShake(0.3f);
    CHECK(Settings::parse(s.serialize()) == s); // exactly: values sit on steps
}

TEST(settings_parse_keeps_defaults_for_bad_lines) {
    const Settings s = Settings::parse("junk\r\nautoMove=yes\nscreenShake=7\nfuture=1\n=\n");
    CHECK(!s.autoMove);           // not a number: default
    CHECK(s.screenShake == 1.0f); // clamped
    CHECK(Settings::parse("") == Settings{});
    CHECK(Settings::parse("screenShake=nan\nautoMove=1") == (Settings{true, 0.5f})); // (last line without \n)
    CHECK(near(Settings::parse("screenShake=0.62").screenShake, 0.6f));             // onto a step
}

TEST(settings_screen_shake_steps) {
    Settings s;
    s.setScreenShake(0.04f);
    CHECK(s.screenShake == 0.0f);
    s.setScreenShake(0.96f);
    CHECK(s.screenShake == 1.0f);
    s.setScreenShake(-3);
    CHECK(s.screenShake == 0.0f);
    s.setScreenShake(0.68f);
    CHECK(near(s.screenShake, 0.7f));
}

TEST(settings_load_and_save_through_user_files) {
    testUserFiles().clear();
    CHECK(Settings::load() == Settings{}); // nothing saved yet
    Settings s;
    s.autoMove = true;
    s.setScreenShake(0.8f);
    CHECK(s.save());
    CHECK(testUserFiles().count(Settings::FILE_NAME) == 1);
    CHECK(Settings::load() == s);
    testUserFiles().clear();
}

TEST(pause_menu_buttons) {
    Menu menu;
    Settings s;
    CHECK(!menu.isOpen() && menu.update(press(plat::BTN_A), s) == Menu::Action::None);
    menu.open();
    CHECK(menu.isOpen() && menu.page() == Menu::Page::Paused && menu.cursor() == Menu::RESUME);
    menu.update(press(plat::BTN_DOWN), s);
    CHECK(menu.cursor() == Menu::OPTIONS);
    menu.update(press(plat::BTN_A), s); // into Options, on the slider
    CHECK(menu.page() == Menu::Page::Options && menu.cursor() == Menu::SCREEN_SHAKE);
    menu.update(press(plat::BTN_RIGHT), s);
    CHECK(near(s.screenShake, 0.6f));
    for (int i = 0; i < 6; i++) menu.update(press(plat::BTN_RIGHT), s);
    CHECK(s.screenShake == 1.0f); // stops at 100%
    menu.update(press(plat::BTN_A), s); // A does nothing on a slider
    CHECK(s.screenShake == 1.0f && menu.page() == Menu::Page::Options);
    menu.update(press(plat::BTN_DOWN), s);
    CHECK(menu.cursor() == Menu::AUTO_MOVE);
    menu.update(press(plat::BTN_A), s);
    CHECK(s.autoMove);
    menu.update(press(plat::BTN_LEFT), s);
    CHECK(!s.autoMove);
    menu.update(press(plat::BTN_B), s); // back to Paused, on Options
    CHECK(menu.isOpen() && menu.page() == Menu::Page::Paused && menu.cursor() == Menu::OPTIONS);
    menu.update(press(plat::BTN_A), s);
    menu.update(press(plat::BTN_UP), s); // wraps to Back
    CHECK(menu.cursor() == Menu::BACK);
    menu.update(press(plat::BTN_A), s);
    CHECK(menu.page() == Menu::Page::Paused && menu.cursor() == Menu::OPTIONS);

    menu.update(press(plat::BTN_DOWN), s);
    CHECK(menu.cursor() == Menu::QUIT);
    CHECK(menu.update(press(plat::BTN_A), s) == Menu::Action::None && menu.confirmingQuit());
    menu.update(press(plat::BTN_UP), s); // moving away cancels
    CHECK(!menu.confirmingQuit());
    menu.update(press(plat::BTN_DOWN), s);
    menu.update(press(plat::BTN_A), s);
    CHECK(menu.update(press(plat::BTN_A), s) == Menu::Action::Quit && !menu.isOpen());

    menu.open();
    CHECK(menu.update(press(plat::BTN_B), s) == Menu::Action::Resume && !menu.isOpen());
    menu.open();
    menu.update(press(plat::BTN_DOWN), s);
    menu.update(press(plat::BTN_A), s);
    CHECK(menu.update(press(plat::BTN_START), s) == Menu::Action::Resume); // from Options too
    menu.open();
    CHECK(menu.page() == Menu::Page::Paused && menu.update(press(plat::BTN_A), s) == Menu::Action::Resume);
}

TEST(pause_menu_stylus) {
    Menu menu;
    Settings s;
    openMenu(menu, s);
    CHECK(tapItem(menu, s, Menu::OPTIONS) == Menu::Action::None && menu.page() == Menu::Page::Options);
    CHECK(tapItem(menu, s, Menu::AUTO_MOVE) == Menu::Action::None && s.autoMove && menu.cursor() == Menu::AUTO_MOVE);

    // Dragging the slider sets the value under the stylus, on steps; releasing anywhere keeps it.
    const Menu::Box slider = Menu::controlBox(Menu::SCREEN_SHAKE);
    const int cy = slider.y + slider.h / 2;
    menu.update(touch(slider.x + slider.w / 2, cy), s);
    CHECK(near(s.screenShake, 0.5f) && menu.cursor() == Menu::SCREEN_SHAKE);
    menu.update(touch(slider.x + slider.w, cy), s);
    CHECK(s.screenShake == 1.0f);
    menu.update(touch(slider.x - 30, cy + 40), s); // off the slider: clamped, still dragging
    CHECK(s.screenShake == 0.0f);
    menu.update(touch(slider.x + slider.w * 3 / 10, cy), s);
    menu.update(plat::Input{}, s);
    CHECK(near(s.screenShake, 0.3f) && menu.page() == Menu::Page::Options);

    // The label is just a label: a tap on it moves the cursor, not the value. A finger a little
    // above the slider still drags it.
    const Menu::Box row = Menu::itemBox(Menu::SCREEN_SHAKE);
    menu.update(press(plat::BTN_DOWN), s);
    CHECK(tap(menu, s, row.x + 4, row.y + row.h / 2) == Menu::Action::None);
    CHECK(near(s.screenShake, 0.3f) && menu.cursor() == Menu::SCREEN_SHAKE);
    CHECK(tap(menu, s, slider.x - 4, row.y + row.h / 2) == Menu::Action::None && near(s.screenShake, 0.3f));
    menu.update(touch(slider.x + slider.w - 1, row.y), s);
    menu.update(plat::Input{}, s);
    CHECK(s.screenShake == 1.0f);

    // A press on one item released on another does nothing.
    const Menu::Box back = Menu::itemBox(Menu::BACK), toggle = Menu::itemBox(Menu::AUTO_MOVE);
    menu.update(touch(back.x + 4, back.y + 4), s);
    menu.update(touch(toggle.x + 4, toggle.y + 4), s);
    CHECK(menu.update(plat::Input{}, s) == Menu::Action::None && menu.page() == Menu::Page::Options && s.autoMove);
    CHECK(tapItem(menu, s, Menu::BACK) == Menu::Action::None && menu.page() == Menu::Page::Paused);
    CHECK(tapItem(menu, s, Menu::RESUME) == Menu::Action::Resume && !menu.isOpen());

    // A touch already held when the menu opens isn't a tap.
    menu.open();
    const Menu::Box resume = Menu::itemBox(Menu::RESUME);
    menu.update(touch(resume.x + 4, resume.y + 4), s);
    CHECK(menu.update(plat::Input{}, s) == Menu::Action::None && menu.isOpen());
}

TEST(pause_menu_boxes_fit_and_dont_overlap) {
    const Menu::Item paused[] = {Menu::RESUME, Menu::OPTIONS, Menu::QUIT};
    const Menu::Item options[] = {Menu::SCREEN_SHAKE, Menu::AUTO_MOVE, Menu::BACK};
    for (const auto* page : {paused, options})
        for (int i = 0; i < 3; i++) {
            const Menu::Box a = Menu::itemBox(page[i]);
            CHECK(a.x >= 0 && a.y >= 0 && a.x + a.w <= plat::BOTTOM_W && a.y + a.h <= plat::BOTTOM_H);
            if (i > 0) {
                const Menu::Box prev = Menu::itemBox(page[i - 1]);
                CHECK(prev.y + prev.h <= a.y);
            }
        }
    // Each option's control sits inside its row.
    const Menu::Box shake = Menu::itemBox(Menu::SCREEN_SHAKE), slider = Menu::controlBox(Menu::SCREEN_SHAKE);
    const Menu::Box autoRow = Menu::itemBox(Menu::AUTO_MOVE), toggle = Menu::controlBox(Menu::AUTO_MOVE);
    CHECK(shake.contains(slider.x, slider.y) && shake.contains(slider.x + slider.w - 1, slider.y + slider.h - 1));
    CHECK(autoRow.contains(toggle.x, toggle.y) && autoRow.contains(toggle.x + toggle.w - 1, toggle.y + toggle.h - 1));
    // The toggle is centered under the slider (Label Toggle Pair: x 165 = the slider's middle).
    CHECK(toggle.x * 2 + toggle.w == slider.x * 2 + slider.w);
}
