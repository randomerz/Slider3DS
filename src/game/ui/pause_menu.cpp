#include "game/ui/pause_menu.h"

#include <cmath>
#include <iterator>

using Item = PauseMenu::Item;
using Kind = PauseMenu::Kind;
using Page = PauseMenu::Page;

// What a button does.
enum class Act { None, Resume, Quit, Open, Back };

// An item: its kind and label; a button's action (and the page it opens); an option's setting (a
// toggle's flag; a slider's value, its setter and how many steps left / right take across it).
struct PauseMenu::ItemDef {
    Item item;
    Kind kind;
    const char* label;
    Act act = Act::None;
    Page opens = Page::Paused;
    bool Settings::*flag = nullptr;
    float Settings::*value = nullptr;
    void (Settings::*setValue)(float) = nullptr;
    int steps = 0;
};

namespace {

using ItemDef = PauseMenu::ItemDef;

// The layout, in UI px (MenuSkin's grid: the bottom screen is 160x120 of them): the original's
// PausePanel arrangement, a title over a column of buttons, sized to fill the touch screen.
constexpr int S = MenuSkin::UI_SCALE;
constexpr int UI_W = plat::BOTTOM_W / S;
constexpr int CENTER_X = UI_W / 2;
constexpr int TITLE_Y = 20;                              // the title's middle
constexpr int BUTTON_W = 120, BUTTON_H = 18, PITCH = 24; // buttons 6 px apart
constexpr int ROW_H = 20;                                // option rows
// The selected button's brackets bounce: at rest, then 1 px out, each half a second.
constexpr float BOUNCE_PERIOD = 1.0f;
constexpr int FIRST_Y = 46;                              // the first row's middle
// Option rows (GeneralOptionsPanel's: label at the left, control at the right, inset from the
// row's ends): the slider at the right end, the toggle centered under it. Selected: a light label.
constexpr int ROW_X = 10, ROW_W = UI_W - 2 * ROW_X, ROW_PAD = 4;
constexpr int SLIDER_W = 64, SLIDER_X = ROW_X + ROW_W - ROW_PAD - SLIDER_W;
constexpr int CONTROL_CENTER_X = SLIDER_X + SLIDER_W / 2;

// A page: its title, items, and parent (B and Back go there, selecting `returnTo`; none for the top).
struct PageDef {
    Page page;
    const char* title;
    const ItemDef* items;
    int count;
    bool hasParent;
    Page parent;
    Item returnTo;
};

constexpr ItemDef PAUSED_ITEMS[] = {
    {PauseMenu::RESUME, Kind::Button, "Resume", Act::Resume},
    {PauseMenu::OPTIONS, Kind::Button, "Options", Act::Open, Page::Options},
    {PauseMenu::QUIT, Kind::Button, "Quit", Act::Quit},
};
constexpr ItemDef OPTIONS_ITEMS[] = {
    {PauseMenu::SCREEN_SHAKE, Kind::Slider, "Screen Shake", Act::None, Page::Paused, nullptr, &Settings::screenShake,
     &Settings::setScreenShake, Settings::SHAKE_STEPS},
    {PauseMenu::AUTO_MOVE, Kind::Toggle, "Auto Move", Act::None, Page::Paused, &Settings::autoMove},
    {PauseMenu::OPTIONS_BACK, Kind::Button, "Back", Act::Back},
};
constexpr PageDef PAGES[] = {
    {Page::Paused, "Paused", PAUSED_ITEMS, (int)std::size(PAUSED_ITEMS), false, Page::Paused, PauseMenu::RESUME},
    {Page::Options, "Options", OPTIONS_ITEMS, (int)std::size(OPTIONS_ITEMS), true, Page::Paused, PauseMenu::OPTIONS},
};

// PAGES is indexed by Page; each Item is one row on one page (itemBox finds it by its Item).
constexpr bool pagesInOrder() {
    for (int i = 0; i < (int)std::size(PAGES); i++)
        if ((int)PAGES[i].page != i) return false;
    return true;
}
constexpr bool itemsUnique() {
    int seen = 0;
    for (const PageDef& p : PAGES)
        for (int i = 0; i < p.count; i++) {
            const int bit = 1 << p.items[i].item;
            if (seen & bit) return false;
            seen |= bit;
        }
    return true;
}
static_assert(pagesInOrder(), "PAGES must be in Page order");
static_assert(itemsUnique(), "an Item may be on one page only (give a second Back its own Item)");

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
    return {ROW_X, cy - ROW_H / 2, ROW_W, ROW_H};
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

void PauseMenu::back() {
    const PageDef& p = pageDef(page_);
    go(p.parent, p.returnTo);
}

int PauseMenu::itemAt(int x, int y) const {
    const PageDef& p = pageDef(page_);
    for (int i = 0; i < p.count; i++)
        if (uiBox(p.items[i], i).contains(x, y)) return i;
    return -1;
}

PauseMenu::Action PauseMenu::activate(const ItemDef& def, Settings& settings) {
    if (def.act != Act::Quit) confirmQuit_ = false;
    if (def.kind == Kind::Toggle) settings.*def.flag = !(settings.*def.flag);
    switch (def.act) {
    case Act::None: break; // (an option; a slider moves with left / right or the stylus)
    case Act::Resume:
        open_ = false;
        return Action::Resume;
    case Act::Quit:
        if (confirmQuit_) {
            open_ = false;
            return Action::Quit;
        }
        confirmQuit_ = true;
        break;
    case Act::Open: go(def.opens, pageDef(def.opens).items[0].item); break;
    case Act::Back: back(); break;
    }
    return Action::None;
}

void PauseMenu::step(const ItemDef& def, int dir, Settings& settings) {
    if (def.kind == Kind::Toggle) settings.*def.flag = !(settings.*def.flag);
    if (def.kind == Kind::Slider) (settings.*def.setValue)(settings.*def.value + dir / (float)def.steps); // (it snaps)
}

PauseMenu::Action PauseMenu::update(const plat::Input& in, Settings& settings, float dt) {
    if (!open_) return Action::None;
    const Action action = handleInput(in, settings);
    // The bounce restarts on each newly selected item.
    if (page_ != bouncePage_ || cursor_ != bounceCursor_) {
        bouncePage_ = page_;
        bounceCursor_ = cursor_;
        bounceTime_ = 0;
    } else {
        bounceTime_ = std::fmod(bounceTime_ + dt, BOUNCE_PERIOD);
    }
    return action;
}

PauseMenu::Action PauseMenu::handleInput(const plat::Input& in, Settings& settings) {
    if (in.pressed & plat::BTN_START) {
        open_ = false;
        return Action::Resume;
    }
    if (in.pressed & plat::BTN_B) {
        if (pageDef(page_).hasParent) {
            back();
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
    if (in.pressed & plat::BTN_A) return activate(p.items[cursor_], settings);
    if (in.pressed & plat::BTN_LEFT) step(p.items[cursor_], -1, settings);
    if (in.pressed & plat::BTN_RIGHT) step(p.items[cursor_], 1, settings);

    Action action = Action::None;
    if (in.touching) {
        lastX_ = in.touchX / S;
        lastY_ = in.touchY / S;
        if (!wasTouching_) {
            pressed_ = itemAt(lastX_, lastY_);
            if (pressed_ >= 0) {
                const ItemDef& def = p.items[pressed_];
                cursor_ = pressed_;
                if (def.act != Act::Quit) confirmQuit_ = false;
                if (def.kind == Kind::Slider) {
                    // Only the slider drags, though a finger may land a little above or below it.
                    const Box s = uiControl(def, pressed_), row = uiBox(def, pressed_);
                    sliding_ = Box{s.x, row.y, s.w, row.h}.contains(lastX_, lastY_);
                }
            }
        }
        if (sliding_) {
            const ItemDef& def = p.items[pressed_];
            const Box s = uiControl(def, pressed_);
            (settings.*def.setValue)(MenuSkin::sliderValueAt(in.touchX / (float)S, s.x, s.w));
        }
    } else if (wasTouching_) {
        if (pressed_ >= 0 && !sliding_ && itemAt(lastX_, lastY_) == pressed_) action = activate(p.items[pressed_], settings);
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
            const char* label = def.act == Act::Quit && confirmQuit_ ? "Really quit?" : def.label;
            // (A frame's worth of float error mustn't shift the halves: 30 x 1/60 s is just under 0.5.)
            const bool out = std::fmod(bounceTime_ + 1e-4f, BOUNCE_PERIOD) >= BOUNCE_PERIOD / 2;
            skin.drawButton(b.x, b.y, b.w, b.h, label, selected, held, out ? 1 : 0);
            break;
        }
        case Kind::Slider:
            skin.drawLabel(def.label, b.x + ROW_PAD, b.y + b.h / 2, selected);
            skin.drawSlider(c.x, c.y, c.w, settings.*def.value, selected);
            break;
        case Kind::Toggle:
            skin.drawLabel(def.label, b.x + ROW_PAD, b.y + b.h / 2, selected);
            skin.drawToggle(c.x, c.y, settings.*def.flag, selected);
            break;
        }
    }
}
