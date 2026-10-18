#include "game/ui/pause_menu.h"

#include <iterator>

using Item = PauseMenu::Item;
using Kind = PauseMenu::Kind;
using Page = PauseMenu::Page;

// What a button does.
enum class Act { None, Resume, Quit, Open, Back };

// An item: its kind and label; a button's action (and the page it opens); an option's setting (a
// toggle's flag, a slider's 0-1 value: Settings::snap keeps it on its steps).
struct PauseMenu::ItemDef {
    Item item;
    Kind kind;
    const char* label;
    Act act = Act::None;
    Page opens = Page::Paused;
    bool Settings::*flag = nullptr;
    float Settings::*value = nullptr;
};

namespace {

using ItemDef = PauseMenu::ItemDef;

// The layout, in UI px (MenuSkin's grid: the bottom screen is 160x120 of them): the original's
// PausePanel arrangement, a title over a column of buttons, sized to fill the touch screen.
constexpr int S = MenuSkin::UI_SCALE;
constexpr int UI_W = plat::BOTTOM_W / S;
constexpr int CENTER_X = UI_W / 2;
constexpr int TITLE_Y = 20; // the title's middle
// Items stack in slots from FIRST_TOP: a button's 24 px (18 px tall, 6 apart), an option row's 15
// (compact, so a page of four options and Back fits under the title).
constexpr int FIRST_TOP = 34;
constexpr int BUTTON_W = 120, BUTTON_H = 18, BUTTON_SLOT = 24;
constexpr int ROW_SLOT = 15;
// Option rows (GeneralOptionsPanel's: label at the left, control at the right, inset from the
// row's ends): the slider at the right end, the toggle centered under it. Selected: a light label.
constexpr int ROW_X = 10, ROW_W = UI_W - 2 * ROW_X, ROW_PAD = 4;
constexpr int SLIDER_W = 60, SLIDER_X = ROW_X + ROW_W - ROW_PAD - SLIDER_W;
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
    {PauseMenu::GENERAL, Kind::Button, "General", Act::Open, Page::General},
    {PauseMenu::SOUND, Kind::Button, "Sound", Act::Open, Page::Sound},
    {PauseMenu::OPTIONS_BACK, Kind::Button, "Back", Act::Back},
};
constexpr ItemDef slider(Item item, const char* label, float Settings::*value) {
    return {item, Kind::Slider, label, Act::None, Page::Paused, nullptr, value};
}
constexpr ItemDef toggle(Item item, const char* label, bool Settings::*flag) {
    return {item, Kind::Toggle, label, Act::None, Page::Paused, flag, nullptr};
}
constexpr ItemDef GENERAL_ITEMS[] = {
    slider(PauseMenu::SCREEN_SHAKE, "Screen Shake", &Settings::screenShake),
    slider(PauseMenu::SCREEN_EFFECTS, "Screen Effects", &Settings::screenEffects),
    toggle(PauseMenu::AUTO_MOVE, "Auto Move", &Settings::autoMove),
    toggle(PauseMenu::BIGGER_TEXT, "Bigger Text", &Settings::bigText),
    {PauseMenu::GENERAL_BACK, Kind::Button, "Back", Act::Back},
};
constexpr ItemDef SOUND_ITEMS[] = {
    slider(PauseMenu::MASTER_VOLUME, "Master", &Settings::masterVolume),
    slider(PauseMenu::SFX_VOLUME, "Sound", &Settings::sfxVolume),
    slider(PauseMenu::MUSIC_VOLUME, "Music", &Settings::musicVolume),
    slider(PauseMenu::AMBIENCE_VOLUME, "Ambience", &Settings::ambienceVolume),
    {PauseMenu::SOUND_BACK, Kind::Button, "Back", Act::Back},
};
constexpr PageDef PAGES[] = {
    {Page::Paused, "Paused", PAUSED_ITEMS, (int)std::size(PAUSED_ITEMS), false, Page::Paused, PauseMenu::RESUME},
    {Page::Options, "Options", OPTIONS_ITEMS, (int)std::size(OPTIONS_ITEMS), true, Page::Paused, PauseMenu::OPTIONS},
    {Page::General, "General", GENERAL_ITEMS, (int)std::size(GENERAL_ITEMS), true, Page::Options, PauseMenu::GENERAL},
    {Page::Sound, "Sound", SOUND_ITEMS, (int)std::size(SOUND_ITEMS), true, Page::Options, PauseMenu::SOUND},
};

// PAGES is indexed by Page; each Item is one row on one page (itemBox finds it by its Item).
constexpr bool pagesInOrder() {
    for (int i = 0; i < (int)std::size(PAGES); i++)
        if ((int)PAGES[i].page != i) return false;
    return true;
}
// Every Item on exactly one page: no Item twice, and as many rows as Items.
constexpr bool itemsOnePageEach() {
    static_assert(PauseMenu::ITEM_COUNT <= 32, "itemsOnePageEach's bitmask");
    unsigned seen = 0;
    int rows = 0;
    for (const PageDef& p : PAGES)
        for (int i = 0; i < p.count; i++, rows++) {
            const unsigned bit = 1u << p.items[i].item;
            if (seen & bit) return false;
            seen |= bit;
        }
    return rows == PauseMenu::ITEM_COUNT;
}
// Each row has what its kind needs: a button an action, a slider a value, a toggle a flag.
constexpr bool itemsConsistent() {
    for (const PageDef& p : PAGES)
        for (int i = 0; i < p.count; i++) {
            const ItemDef& d = p.items[i];
            if (d.kind == Kind::Button ? d.act == Act::None
                                       : d.kind == Kind::Slider ? d.value == nullptr : d.flag == nullptr)
                return false;
        }
    return true;
}
static_assert(pagesInOrder(), "PAGES must be in Page order");
static_assert(itemsOnePageEach(), "each Item must be on one page (give a second Back its own Item)");
static_assert(itemsConsistent(), "a row lacks its kind's action / value / flag");

const PageDef& pageDef(Page page) { return PAGES[(int)page]; }

// Where `item` is: its page and its index there.
const PageDef& find(Item item, int& index) {
    for (const PageDef& p : PAGES)
        for (int i = 0; i < p.count; i++)
            if (p.items[i].item == item) {
                index = i;
                return p;
            }
    index = 0;
    return PAGES[0]; // (every Item is on a page)
}

int slotHeight(const ItemDef& def) { return def.kind == Kind::Button ? BUTTON_SLOT : ROW_SLOT; }

// Item `index`'s box on page `p` (UI px): a button's, or an option's row (its whole slot).
PauseMenu::Box uiBox(const PageDef& p, int index) {
    int top = FIRST_TOP;
    for (int i = 0; i < index; i++) top += slotHeight(p.items[i]);
    const ItemDef& def = p.items[index];
    const int cy = top + slotHeight(def) / 2;
    if (def.kind == Kind::Button) return {CENTER_X - BUTTON_W / 2, cy - BUTTON_H / 2, BUTTON_W, BUTTON_H};
    return {ROW_X, top, ROW_W, ROW_SLOT};
}

// An option's slider / toggle (UI px); a button's box.
PauseMenu::Box uiControl(const PageDef& p, int index) {
    const ItemDef& def = p.items[index];
    const PauseMenu::Box row = uiBox(p, index);
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
    const PageDef& p = find(item, index);
    return toScreen(uiBox(p, index));
}

PauseMenu::Box PauseMenu::controlBox(Item item) {
    int index;
    const PageDef& p = find(item, index);
    return toScreen(uiControl(p, index));
}

PauseMenu::Item PauseMenu::cursor() const { return pageDef(page_).items[cursor_].item; }

void PauseMenu::open(Page start) {
    open_ = true;
    root_ = start;
    go(start, pageDef(start).items[0].item);
    tap_.release(); // (a touch held when the menu opens isn't a tap on it)
}

void PauseMenu::go(Page page, Item select) {
    page_ = page;
    confirmQuit_ = false;
    tap_.cancel();
    sliding_ = false;
    cursor_ = 0;
    const PageDef& p = pageDef(page);
    for (int i = 0; i < p.count; i++)
        if (p.items[i].item == select) cursor_ = i;
}

PauseMenu::Action PauseMenu::back() {
    const PageDef& p = pageDef(page_);
    if (page_ == root_ || !p.hasParent) {
        open_ = false;
        return Action::Resume;
    }
    go(p.parent, p.returnTo);
    return Action::None;
}

int PauseMenu::itemAt(int x, int y) const {
    const PageDef& p = pageDef(page_);
    for (int i = 0; i < p.count; i++)
        if (uiBox(p, i).contains(x, y)) return i;
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
    case Act::Back: return back();
    }
    return Action::None;
}

void PauseMenu::step(const ItemDef& def, int dir, Settings& settings) {
    if (def.kind == Kind::Toggle) settings.*def.flag = !(settings.*def.flag);
    if (def.kind == Kind::Slider) settings.*def.value = Settings::snap(settings.*def.value + dir / (float)Settings::STEPS);
}

PauseMenu::Action PauseMenu::update(const plat::Input& in, Settings& settings) {
    if (!open_) return Action::None;
    const Action action = handleInput(in, settings);
    bounce_.update((int)page_ * ITEM_COUNT + cursor_);
    return action;
}

PauseMenu::Action PauseMenu::handleInput(const plat::Input& in, Settings& settings) {
    if (in.pressed & plat::BTN_START) {
        open_ = false;
        return Action::Resume;
    }
    if (in.pressed & plat::BTN_B) return back();
    const PageDef& p = pageDef(page_);
    const int move = ((in.pressed & plat::BTN_DOWN) ? 1 : 0) - ((in.pressed & plat::BTN_UP) ? 1 : 0);
    if (move != 0) {
        cursor_ = (cursor_ + move + p.count) % p.count;
        confirmQuit_ = false;
    }
    if (in.pressed & plat::BTN_A) return activate(p.items[cursor_], settings);
    if (in.pressed & plat::BTN_LEFT) step(p.items[cursor_], -1, settings);
    if (in.pressed & plat::BTN_RIGHT) step(p.items[cursor_], 1, settings);

    const bool wasSliding = sliding_;
    const MenuTap::Frame f = tap_.update(in, S, [&](int x, int y) { return itemAt(x, y); });
    if (f.down >= 0) {
        const ItemDef& def = p.items[f.down];
        cursor_ = f.down;
        if (def.act != Act::Quit) confirmQuit_ = false;
        if (def.kind == Kind::Slider) {
            // Only the slider drags, though a finger may land a little above or below it.
            const Box s = uiControl(p, f.down), row = uiBox(p, f.down);
            sliding_ = Box{s.x, row.y, s.w, row.h}.contains(in.touchX / S, in.touchY / S);
        }
    }
    if (!in.touching) sliding_ = false;
    if (sliding_) {
        const Box s = uiControl(p, tap_.pressed());
        settings.*p.items[tap_.pressed()].value = Settings::snap(MenuSkin::sliderValueAt(in.touchX / (float)S, s.x, s.w));
    }
    if (f.tapped >= 0 && !wasSliding) return activate(p.items[f.tapped], settings);
    return Action::None;
}

void PauseMenu::draw(const Settings& settings, const MenuSkin& skin) const {
    const PageDef& p = pageDef(page_);
    skin.drawTitle(p.title, CENTER_X, TITLE_Y);
    for (int i = 0; i < p.count; i++) {
        const ItemDef& def = p.items[i];
        const Box b = uiBox(p, i), c = uiControl(p, i);
        const bool selected = i == cursor_;
        switch (def.kind) {
        case Kind::Button: {
            const char* label = def.act == Act::Quit && confirmQuit_ ? "Really quit?" : def.label;
            skin.drawButton(b.x, b.y, b.w, b.h, label, selected, tap_.held(i), bounce_.out());
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
