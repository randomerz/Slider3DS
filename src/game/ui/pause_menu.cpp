#include "game/ui/pause_menu.h"

#include <iterator>

namespace {

using Item = PauseMenu::Item;

// UICanvas is 320x180 around the screen's center; the bottom screen is 240 tall, so a UI y (up from
// the center) is at 120 - y.
constexpr int CENTER_X = plat::BOTTOM_W / 2, CENTER_Y = plat::BOTTOM_H / 2;
constexpr int TITLE_Y = CENTER_Y - 55;                  // "Paused Text" at y 55
constexpr int BUTTON_W = 160, BUTTON_H = 20, PITCH = 22; // the pause buttons: 160x20, 22 apart
constexpr int FIRST_Y = CENTER_Y - 22;                   // Resume at y 22
// GeneralOptionsPanel's rows: 230 wide, the label at the left, the slider 125 px in, the toggle 150.
constexpr int ROW_W = 230, ROW_X = CENTER_X - ROW_W / 2, SLIDER_DX = 125, TOGGLE_DX = 150;

constexpr Item PAUSED_ITEMS[] = {PauseMenu::RESUME, PauseMenu::OPTIONS, PauseMenu::QUIT};
constexpr Item OPTIONS_ITEMS[] = {PauseMenu::SCREEN_SHAKE, PauseMenu::AUTO_MOVE, PauseMenu::BACK};

int rowCenter(Item item) {
    switch (item) {
    case PauseMenu::RESUME: case PauseMenu::SCREEN_SHAKE: return FIRST_Y;
    case PauseMenu::OPTIONS: case PauseMenu::AUTO_MOVE: return FIRST_Y + PITCH;
    case PauseMenu::QUIT: case PauseMenu::BACK: return FIRST_Y + 2 * PITCH;
    }
    return FIRST_Y;
}

bool isButton(Item item) { return item != PauseMenu::SCREEN_SHAKE && item != PauseMenu::AUTO_MOVE; }

} // namespace

PauseMenu::Box PauseMenu::itemBox(Item item) {
    const int cy = rowCenter(item);
    if (isButton(item)) return {CENTER_X - BUTTON_W / 2, cy - BUTTON_H / 2, BUTTON_W, BUTTON_H};
    return {ROW_X, cy - MenuSkin::TOGGLE_SIZE / 2, ROW_W, MenuSkin::TOGGLE_SIZE};
}

PauseMenu::Box PauseMenu::sliderBox() {
    return {ROW_X + SLIDER_DX, rowCenter(SCREEN_SHAKE) - MenuSkin::SLIDER_H / 2, MenuSkin::SLIDER_W, MenuSkin::SLIDER_H};
}

PauseMenu::Box PauseMenu::toggleBox() {
    return {ROW_X + TOGGLE_DX, rowCenter(AUTO_MOVE) - MenuSkin::TOGGLE_SIZE / 2, MenuSkin::TOGGLE_SIZE,
            MenuSkin::TOGGLE_SIZE};
}

PauseMenu::ItemList PauseMenu::items() const {
    if (page_ == Page::Options) return {OPTIONS_ITEMS, (int)std::size(OPTIONS_ITEMS)};
    return {PAUSED_ITEMS, (int)std::size(PAUSED_ITEMS)};
}

void PauseMenu::open() {
    open_ = true;
    go(Page::Paused, RESUME);
    wasTouching_ = true; // (a touch held when the menu opens isn't a tap on it)
}

void PauseMenu::go(Page page, Item select) {
    page_ = page;
    confirmQuit_ = false;
    pressed_ = -1;
    sliding_ = false;
    cursor_ = 0;
    const ItemList list = items();
    for (int i = 0; i < list.count; i++)
        if (list[i] == select) cursor_ = i;
}

int PauseMenu::itemAt(int px, int py) const {
    const ItemList list = items();
    for (int i = 0; i < list.count; i++)
        if (itemBox(list[i]).contains(px, py)) return i;
    return -1;
}

PauseMenu::Action PauseMenu::activate(Item item, Settings& settings) {
    if (item != QUIT) confirmQuit_ = false;
    switch (item) {
    case RESUME:
        open_ = false;
        return Action::Resume;
    case OPTIONS: go(Page::Options, SCREEN_SHAKE); break;
    case QUIT:
        if (confirmQuit_) {
            open_ = false;
            return Action::Quit;
        }
        confirmQuit_ = true;
        break;
    case AUTO_MOVE: settings.autoMove = !settings.autoMove; break;
    case SCREEN_SHAKE: break; // (a slider: left / right or the stylus)
    case BACK: go(Page::Paused, OPTIONS); break;
    }
    return Action::None;
}

void PauseMenu::step(Item item, int dir, Settings& settings) {
    if (item == SCREEN_SHAKE) settings.setScreenShake((settings.screenShakeStep() + dir) / (float)Settings::SHAKE_STEPS);
    else if (item == AUTO_MOVE) settings.autoMove = !settings.autoMove;
}

PauseMenu::Action PauseMenu::update(const plat::Input& in, Settings& settings) {
    if (!open_) return Action::None;
    if (in.pressed & plat::BTN_START) {
        open_ = false;
        return Action::Resume;
    }
    if (in.pressed & plat::BTN_B) {
        if (page_ == Page::Options) {
            go(Page::Paused, OPTIONS);
            return Action::None;
        }
        open_ = false;
        return Action::Resume;
    }
    const ItemList list = items();
    const int move = ((in.pressed & plat::BTN_DOWN) ? 1 : 0) - ((in.pressed & plat::BTN_UP) ? 1 : 0);
    if (move != 0) {
        cursor_ = (cursor_ + move + list.count) % list.count;
        confirmQuit_ = false;
    }
    if (in.pressed & plat::BTN_A) return activate(list[cursor_], settings);
    if (in.pressed & plat::BTN_LEFT) step(list[cursor_], -1, settings);
    if (in.pressed & plat::BTN_RIGHT) step(list[cursor_], 1, settings);

    Action action = Action::None;
    if (in.touching) {
        lastX_ = in.touchX;
        lastY_ = in.touchY;
        if (!wasTouching_) {
            pressed_ = itemAt(in.touchX, in.touchY);
            if (pressed_ >= 0) {
                cursor_ = pressed_;
                if (list[pressed_] != QUIT) confirmQuit_ = false;
                sliding_ = list[pressed_] == SCREEN_SHAKE;
            }
        }
        if (sliding_) settings.setScreenShake(MenuSkin::sliderValueAt((float)in.touchX, (float)sliderBox().x));
    } else if (wasTouching_) {
        if (pressed_ >= 0 && !sliding_ && itemAt(lastX_, lastY_) == pressed_) action = activate(list[pressed_], settings);
        pressed_ = -1;
        sliding_ = false;
    }
    wasTouching_ = in.touching;
    return action;
}

void PauseMenu::draw(const Settings& settings, const MenuSkin& skin) const {
    skin.drawTitle(page_ == Page::Paused ? "Paused" : "Options", CENTER_X, TITLE_Y);
    const ItemList list = items();
    for (int i = 0; i < list.count; i++) {
        const Item item = list[i];
        const Box b = itemBox(item);
        const bool selected = i == cursor_;
        if (isButton(item)) {
            const bool held = i == pressed_ && wasTouching_ && itemAt(lastX_, lastY_) == i;
            const char* label = item == RESUME ? "Resume"
                                : item == OPTIONS ? "Options"
                                : item == BACK ? "Back"
                                : confirmQuit_ ? "Really quit?" : "Quit";
            skin.drawButton((float)b.x, (float)b.y, (float)b.w, (float)b.h, label,
                            held ? MenuSkin::Button::Pressed : selected ? MenuSkin::Button::Selected : MenuSkin::Button::Normal);
        } else if (item == SCREEN_SHAKE) {
            skin.drawLabel("Screen Shake", (float)b.x, b.y + b.h / 2.0f);
            const Box s = sliderBox();
            skin.drawSlider((float)s.x, (float)s.y, settings.screenShake, selected);
        } else {
            skin.drawLabel("Auto Move", (float)b.x, b.y + b.h / 2.0f);
            const Box t = toggleBox();
            skin.drawToggle((float)t.x, (float)t.y, settings.autoMove, selected);
        }
    }
}
