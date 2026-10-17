#include "game/ui/pause_menu.h"

#include <iterator>

namespace {

using Item = PauseMenu::Item;
using Kind = PauseMenu::Kind;
using Page = PauseMenu::Page;

// The layout, in UI px (MenuSkin's grid: the bottom screen is 160x120 of them): the original's
// PausePanel arrangement, a title over a column of buttons, sized to fill the touch screen.
constexpr int S = MenuSkin::UI_SCALE;
constexpr int UI_W = plat::BOTTOM_W / S;
constexpr int CENTER_X = UI_W / 2;
constexpr int TITLE_Y = 20;                              // the title's middle
constexpr int BUTTON_W = 120, BUTTON_H = 20, PITCH = 24; // rows 4 px apart
constexpr int FIRST_Y = 46;                              // the first row's middle
// Option rows (GeneralOptionsPanel's: label at the left, control at the right), the slider at the
// right end, the toggle centered under the slider's middle.
constexpr int ROW_X = 10, ROW_W = UI_W - 2 * ROW_X;
constexpr int SLIDER_W = 64, SLIDER_X = ROW_X + ROW_W - SLIDER_W;
constexpr int CONTROL_CENTER_X = SLIDER_X + SLIDER_W / 2;

// An item: its kind, label, and for an option the setting it shows and changes (a toggle's flag; a
// slider's value, its setter and how many steps left / right take across it).
struct ItemDef {
    Item item;
    Kind kind;
    const char* label;
    bool Settings::*flag = nullptr;
    float Settings::*value = nullptr;
    void (Settings::*setValue)(float) = nullptr;
    int steps = 0;
};
struct PageDef {
    const char* title;
    const ItemDef* items;
    int count;
};

constexpr ItemDef PAUSED_ITEMS[] = {
    {PauseMenu::RESUME, Kind::Button, "Resume"},
    {PauseMenu::OPTIONS, Kind::Button, "Options"},
    {PauseMenu::QUIT, Kind::Button, "Quit"},
};
constexpr ItemDef OPTIONS_ITEMS[] = {
    {PauseMenu::SCREEN_SHAKE, Kind::Slider, "Screen Shake", nullptr, &Settings::screenShake, &Settings::setScreenShake,
     Settings::SHAKE_STEPS},
    {PauseMenu::AUTO_MOVE, Kind::Toggle, "Auto Move", &Settings::autoMove},
    {PauseMenu::BACK, Kind::Button, "Back"},
};
constexpr PageDef PAGES[] = {
    {"Paused", PAUSED_ITEMS, (int)std::size(PAUSED_ITEMS)},
    {"Options", OPTIONS_ITEMS, (int)std::size(OPTIONS_ITEMS)},
};

const PageDef& pageDef(Page page) { return PAGES[(int)page]; }

// Where `item` is: its def and its index on its page.
const ItemDef& find(Item item, int& index) {
    for (const PageDef& p : PAGES)
        for (int i = 0; i < p.count; i++)
            if (p.items[i].item == item) {
                index = i;
                return p.items[i];
            }
    index = 0;
    return PAGES[0].items[0]; // (every Item is on a page)
}

// An item's box (UI px): a button's, or an option's row.
PauseMenu::Box uiBox(const ItemDef& def, int index) {
    const int cy = FIRST_Y + index * PITCH;
    if (def.kind == Kind::Button) return {CENTER_X - BUTTON_W / 2, cy - BUTTON_H / 2, BUTTON_W, BUTTON_H};
    return {ROW_X, cy - BUTTON_H / 2, ROW_W, BUTTON_H};
}

// An option's slider / toggle (UI px); a button's box.
PauseMenu::Box uiControl(const ItemDef& def, int index) {
    const PauseMenu::Box row = uiBox(def, index);
    const int cy = row.y + row.h / 2;
    switch (def.kind) {
    case Kind::Slider: return {SLIDER_X, cy - MenuSkin::SLIDER_H / 2, SLIDER_W, MenuSkin::SLIDER_H};
    case Kind::Toggle:
        return {CONTROL_CENTER_X - MenuSkin::TOGGLE_SIZE / 2, cy - MenuSkin::TOGGLE_SIZE / 2, MenuSkin::TOGGLE_SIZE,
                MenuSkin::TOGGLE_SIZE};
    case Kind::Button: break;
    }
    return row;
}

PauseMenu::Box toScreen(const PauseMenu::Box& b) { return {b.x * S, b.y * S, b.w * S, b.h * S}; }

} // namespace

PauseMenu::Box PauseMenu::itemBox(Item item) {
    int index;
    const ItemDef& def = find(item, index);
    return toScreen(uiBox(def, index));
}

PauseMenu::Box PauseMenu::controlBox(Item item) {
    int index;
    const ItemDef& def = find(item, index);
    return toScreen(uiControl(def, index));
}

PauseMenu::Item PauseMenu::cursor() const { return pageDef(page_).items[cursor_].item; }

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
    const PageDef& p = pageDef(page);
    for (int i = 0; i < p.count; i++)
        if (p.items[i].item == select) cursor_ = i;
}

int PauseMenu::itemAt(int px, int py) const {
    const PageDef& p = pageDef(page_);
    for (int i = 0; i < p.count; i++)
        if (toScreen(uiBox(p.items[i], i)).contains(px, py)) return i;
    return -1;
}

PauseMenu::Action PauseMenu::activate(Item item, Settings& settings) {
    if (item != QUIT) confirmQuit_ = false;
    int index;
    const ItemDef& def = find(item, index);
    if (def.kind == Kind::Toggle) settings.*def.flag = !(settings.*def.flag);
    if (def.kind != Kind::Button) return Action::None; // (a slider: left / right or the stylus)
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
    case BACK: go(Page::Paused, OPTIONS); break;
    case SCREEN_SHAKE: case AUTO_MOVE: break; // (options)
    }
    return Action::None;
}

void PauseMenu::step(Item item, int dir, Settings& settings) {
    int index;
    const ItemDef& def = find(item, index);
    if (def.kind == Kind::Toggle) settings.*def.flag = !(settings.*def.flag);
    if (def.kind == Kind::Slider) (settings.*def.setValue)(settings.*def.value + dir / (float)def.steps); // (it snaps)
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
    const PageDef& p = pageDef(page_);
    const int move = ((in.pressed & plat::BTN_DOWN) ? 1 : 0) - ((in.pressed & plat::BTN_UP) ? 1 : 0);
    if (move != 0) {
        cursor_ = (cursor_ + move + p.count) % p.count;
        confirmQuit_ = false;
    }
    if (in.pressed & plat::BTN_A) return activate(p.items[cursor_].item, settings);
    if (in.pressed & plat::BTN_LEFT) step(p.items[cursor_].item, -1, settings);
    if (in.pressed & plat::BTN_RIGHT) step(p.items[cursor_].item, 1, settings);

    Action action = Action::None;
    if (in.touching) {
        lastX_ = in.touchX;
        lastY_ = in.touchY;
        if (!wasTouching_) {
            pressed_ = itemAt(in.touchX, in.touchY);
            if (pressed_ >= 0) {
                const ItemDef& def = p.items[pressed_];
                cursor_ = pressed_;
                if (def.item != QUIT) confirmQuit_ = false;
                if (def.kind == Kind::Slider) {
                    // Only the slider drags, though a finger may land a little above or below it.
                    const Box s = controlBox(def.item), row = itemBox(def.item);
                    sliding_ = Box{s.x, row.y, s.w, row.h}.contains(in.touchX, in.touchY);
                }
            }
        }
        if (sliding_) {
            const ItemDef& def = p.items[pressed_];
            const Box s = uiControl(def, pressed_);
            (settings.*def.setValue)(MenuSkin::sliderValueAt((float)in.touchX, s.x, s.w));
        }
    } else if (wasTouching_) {
        if (pressed_ >= 0 && !sliding_ && itemAt(lastX_, lastY_) == pressed_) action = activate(p.items[pressed_].item, settings);
        pressed_ = -1;
        sliding_ = false;
    }
    wasTouching_ = in.touching;
    return action;
}

void PauseMenu::draw(const Settings& settings, const MenuSkin& skin) const {
    const PageDef& p = pageDef(page_);
    skin.drawTitle(p.title, CENTER_X, TITLE_Y);
    for (int i = 0; i < p.count; i++) {
        const ItemDef& def = p.items[i];
        const Box b = uiBox(def, i), c = uiControl(def, i);
        const bool selected = i == cursor_;
        switch (def.kind) {
        case Kind::Button: {
            const bool held = i == pressed_ && wasTouching_ && itemAt(lastX_, lastY_) == i;
            const char* label = def.item == QUIT && confirmQuit_ ? "Really quit?" : def.label;
            skin.drawButton(b.x, b.y, b.w, b.h, label,
                            held ? MenuSkin::Button::Pressed : selected ? MenuSkin::Button::Selected : MenuSkin::Button::Normal);
            break;
        }
        case Kind::Slider:
            skin.drawLabel(def.label, b.x, b.y + b.h / 2, selected);
            skin.drawSlider(c.x, c.y, c.w, settings.*def.value, selected);
            break;
        case Kind::Toggle:
            skin.drawLabel(def.label, b.x, b.y + b.h / 2, selected);
            skin.drawToggle(c.x, c.y, settings.*def.flag, selected);
            break;
        }
    }
}
