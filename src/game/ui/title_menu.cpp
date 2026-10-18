#include "game/ui/title_menu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "game/ui/palette.h"

namespace {

using Widget = TitleMenu::Widget;
using Box = TitleMenu::Box;
using State = MenuSkin::State;

// The layout, in UI px (MenuSkin's grid: the bottom screen is 160x120 of them).
constexpr int S = MenuSkin::UI_SCALE;
constexpr int UI_W = plat::BOTTOM_W / S;
constexpr int CENTER_X = UI_W / 2;
// Main: MainMenu Panel's column of buttons (the pause menu's size and spacing), centered.
constexpr int BUTTON_W = 120, BUTTON_H = 18, BUTTON_SLOT = 24, MAIN_TOP = 12;
// Saves: the title between the back (top-left) and trash (top-right) icons, the cards stacked
// under it (the original's are side by side, too wide for the 2x grid), Confirm under them.
constexpr int ICON_INSET = 2, TITLE_Y = 12;
constexpr int CARD_X = 6, CARD_W = UI_W - 2 * CARD_X, CARD_H = 22, CARD_TOP = 26, CARD_SLOT = 24, CARD_PAD = 5;
constexpr int CONFIRM_W = 70, CONFIRM_Y = 99;
// NewSave: the name field, then the keyboard (Letter Keys' rows: each next one half a key and then a
// key and a half in), Caps / Del / Done under it.
constexpr int FIELD_X = 28, FIELD_Y = 4, FIELD_W = UI_W - 2 * FIELD_X, FIELD_H = 18, FIELD_PAD = 5;
constexpr int KEY_SIZE = 14, KEY_PITCH = 15, KEYS_X = 5, KEYS_TOP = 28, KEY_ROW_PITCH = 16;
constexpr int ROW_LENGTHS[3] = {10, 9, 7};
static_assert(ROW_LENGTHS[0] + ROW_LENGTHS[1] + ROW_LENGTHS[2] == TitleMenu::KEY_COUNT, "the rows hold every key");
constexpr int ROW_INSETS[3] = {0, KEY_PITCH / 2, KEY_PITCH * 3 / 2};
constexpr int KEYBOARD_BUTTON_W = 44, KEYBOARD_BUTTON_Y = 80, KEYBOARD_BUTTON_GAP = 6;
// A card awaiting Confirm hides and shows its text every half second (SavePanelManager.BASE_TIME_UNTIL_SWAP).
constexpr int BLINK_FRAMES = 30;
// Text: the name field's (CFCFCF) and its placeholder.
constexpr uint32_t FIELD_TEXT = plat::rgba(0xcf, 0xcf, 0xcf);
constexpr const char* PLACEHOLDER = "Enter name...";
constexpr int ICON_W = 20, ICON_H = 21; // room for back_button (20x20) and trash icon (19x21)

Box mainButton(int i) { return {CENTER_X - BUTTON_W / 2, MAIN_TOP + i * BUTTON_SLOT + (BUTTON_SLOT - BUTTON_H) / 2, BUTTON_W, BUTTON_H}; }
Box cardBox(int slot) { return {CARD_X, CARD_TOP + slot * CARD_SLOT, CARD_W, CARD_H}; }
Box keyBox(int key) {
    int row = 0, i = key;
    while (i >= ROW_LENGTHS[row]) i -= ROW_LENGTHS[row++];
    return {KEYS_X + ROW_INSETS[row] + i * KEY_PITCH, KEYS_TOP + row * KEY_ROW_PITCH, KEY_SIZE, KEY_SIZE};
}
Box keyboardButton(int i) {
    const int total = 3 * KEYBOARD_BUTTON_W + 2 * KEYBOARD_BUTTON_GAP;
    return {CENTER_X - total / 2 + i * (KEYBOARD_BUTTON_W + KEYBOARD_BUTTON_GAP), KEYBOARD_BUTTON_Y, KEYBOARD_BUTTON_W, BUTTON_H};
}

bool isButton(Widget w) {
    return w <= TitleMenu::QUIT || w == TitleMenu::CONFIRM || w == TitleMenu::CAPS || w == TitleMenu::DEL || w == TitleMenu::DONE;
}

const char* buttonLabel(Widget w) {
    switch (w) {
    case TitleMenu::CONTINUE: return "Continue";
    case TitleMenu::PLAY: return "Play";
    case TitleMenu::OPTIONS: return "Options";
    case TitleMenu::QUIT: return "Quit";
    case TitleMenu::CONFIRM: return "Confirm";
    case TitleMenu::CAPS: return "Caps";
    case TitleMenu::DEL: return "Del";
    case TitleMenu::DONE: return "Done";
    default: return "";
    }
}

} // namespace

std::string TitleMenu::timeText(double seconds) {
    const int minutes = (int)(seconds / 60);
    char buf[24];
    snprintf(buf, sizeof buf, "%dh%02d", minutes / 60, minutes % 60);
    return buf;
}

void TitleMenu::readCards() {
    for (int slot = 0; slot < SaveProfile::SLOTS; slot++) {
        SaveProfile p;
        Card& c = cards_[slot];
        c = Card();
        c.status = SaveProfile::load(slot, p);
        if (c.status != SaveProfile::Read::Ok) continue;
        c.name = p.name;
        c.areas = p.areasCompleted();
        c.playTime = p.playTime;
        c.lastSaved = p.lastSaved;
    }
}

int TitleMenu::continueSlot() const {
    int best = -1;
    for (int slot = 0; slot < SaveProfile::SLOTS; slot++)
        if (cards_[slot].status == SaveProfile::Read::Ok && (best < 0 || cards_[slot].lastSaved > cards_[best].lastSaved))
            best = slot;
    return best;
}

void TitleMenu::open() {
    readCards();
    deleting_ = false;
    lockout_ = 0;
    go(Page::Main, continueSlot() >= 0 ? CONTINUE : PLAY);
    tap_.release(); // (a touch held as the menu opens isn't a tap on it)
}

void TitleMenu::go(Page page, Widget select) {
    page_ = page;
    selected_ = select;
    confirm_ = -1;
    tap_.cancel(); // (a touch held across pages is no tap on the new one)
}

void TitleMenu::back() {
    switch (page_) {
    case Page::Main: break;
    case Page::Saves:
        deleting_ = false;
        go(Page::Main, PLAY);
        break;
    case Page::NewSave: go(Page::Saves, (Widget)(CARD + newSlot_)); break;
    }
}

void TitleMenu::openNewSave(int slot) {
    newSlot_ = slot;
    name_.clear();
    caps_ = false;
    deleting_ = false;
    go(Page::NewSave, (Widget)KEY);
}

bool TitleMenu::usable(Widget w) const {
    switch (page_) {
    case Page::Main: return w <= QUIT && (w != CONTINUE || continueSlot() >= 0);
    case Page::Saves: return (w >= SAVES_BACK && w < CONFIRM) || (w == CONFIRM && confirm_ >= 0);
    case Page::NewSave: return w >= NEW_BACK && w <= DONE;
    }
    return false;
}

std::vector<TitleMenu::Item> TitleMenu::items() const {
    std::vector<Item> out;
    auto add = [&](Widget w, Box b) {
        if (usable(w)) out.push_back({w, b});
    };
    switch (page_) {
    case Page::Main:
        for (int i = CONTINUE; i <= QUIT; i++) add((Widget)i, mainButton(i - CONTINUE));
        break;
    case Page::Saves:
        add(SAVES_BACK, {ICON_INSET, ICON_INSET, ICON_W, ICON_H});
        add(TRASH, {UI_W - ICON_INSET - ICON_W, ICON_INSET - 1, ICON_W, ICON_H});
        for (int slot = 0; slot < SaveProfile::SLOTS; slot++) add((Widget)(CARD + slot), cardBox(slot));
        add(CONFIRM, {CENTER_X - CONFIRM_W / 2, CONFIRM_Y, CONFIRM_W, BUTTON_H});
        break;
    case Page::NewSave:
        add(NEW_BACK, {ICON_INSET, ICON_INSET, ICON_W, ICON_H});
        for (int k = 0; k < KEY_COUNT; k++) add((Widget)(KEY + k), keyBox(k));
        for (int i = 0; i < 3; i++) add((Widget)(CAPS + i), keyboardButton(i));
        break;
    }
    return out;
}

bool TitleMenu::box(Widget w, Box& out) const {
    for (const Item& it : items())
        if (it.widget == w) {
            out = {it.box.x * S, it.box.y * S, it.box.w * S, it.box.h * S};
            return true;
        }
    return false;
}

int TitleMenu::itemAt(int x, int y) const {
    for (const Item& it : items())
        if (it.box.contains(x, y)) return it.widget;
    return -1;
}

void TitleMenu::move(int dx, int dy) {
    // The nearest usable widget that way that lines up with this one (shares some of its columns for
    // up / down, its rows for left / right, or misses them by at most NEAR px: the keyboard's rows are
    // offset by half a key): the smallest gap, then the closest center. Nothing lines up: the cursor
    // stays (no diagonal jumps).
    constexpr int NEAR = 2;
    const std::vector<Item> all = items();
    const Item* from = nullptr;
    for (const Item& it : all)
        if (it.widget == selected_) from = &it;
    if (!from) {
        if (!all.empty()) selected_ = all.front().widget;
        return;
    }
    const Box& f = from->box;
    const Item* best = nullptr;
    int bestGap = 0, bestOff = 0;
    for (const Item& it : all) {
        const Box& b = it.box;
        // The gap between the boxes along the move (>= 0: entirely that way), and whether they line up.
        const int gap = dx > 0 ? b.x - (f.x + f.w) : dx < 0 ? f.x - (b.x + b.w) : dy > 0 ? b.y - (f.y + f.h) : f.y - (b.y + b.h);
        const bool linedUp = dx != 0 ? b.y < f.y + f.h + NEAR && f.y < b.y + b.h + NEAR
                                     : b.x < f.x + f.w + NEAR && f.x < b.x + b.w + NEAR;
        if (&it == from || gap < 0 || !linedUp) continue;
        const int off = dx != 0 ? std::abs((b.y * 2 + b.h) - (f.y * 2 + f.h)) : std::abs((b.x * 2 + b.w) - (f.x * 2 + f.w));
        if (!best || gap < bestGap || (gap == bestGap && off < bestOff)) {
            best = &it;
            bestGap = gap;
            bestOff = off;
        }
    }
    if (best) selected_ = best->widget;
}

TitleMenu::Action TitleMenu::activate(Widget w) {
    Action a;
    if (w == CONTINUE) {
        a.kind = Action::Kind::Play;
        a.slot = continueSlot();
    } else if (w == PLAY) {
        bool none = true; // (no files at all: an unreadable one isn't written over)
        for (const Card& c : cards_) none = none && c.status == SaveProfile::Read::None;
        if (none) {
            openNewSave(0);
        } else {
            go(Page::Saves, CARD);
        }
    } else if (w == OPTIONS) {
        a.kind = Action::Kind::Options;
    } else if (w == QUIT) {
        a.kind = Action::Kind::Quit;
    } else if (w == SAVES_BACK || w == NEW_BACK) {
        back();
    } else if (w == TRASH) {
        deleting_ = !deleting_;
        confirm_ = -1;
    } else if (w >= CARD && w < CARD + SaveProfile::SLOTS) {
        const int slot = w - CARD;
        const Card& c = cards_[slot];
        if (c.status == SaveProfile::Read::None) {
            openNewSave(slot);
        } else if (deleting_) {
            confirm_ = slot;
            blinkFrame_ = 0;
        } else if (c.status == SaveProfile::Read::Ok) {
            a.kind = Action::Kind::Play;
            a.slot = slot;
        }
    } else if (w == CONFIRM) {
        if (!SaveProfile::remove(confirm_)) printf("Couldn't delete profile %d\n", confirm_ + 1);
        readCards();
        confirm_ = -1;
    } else if (w >= KEY && w < CAPS) {
        if ((int)name_.size() < MAX_NAME) {
            const char c = KEYS[w - KEY];
            name_ += caps_ ? (char)(c - 'a' + 'A') : c;
        }
    } else if (w == CAPS) {
        caps_ = !caps_;
    } else if (w == DEL) {
        if (!name_.empty()) name_.pop_back();
    } else if (w == DONE) {
        if (!name_.empty()) {
            a.kind = Action::Kind::NewGame;
            a.slot = newSlot_;
            a.name = name_;
        }
    }
    return a;
}

TitleMenu::Action TitleMenu::update(const plat::Input& in) {
    const Page page = page_;
    Action action;
    if (lockout_ > 0) {
        lockout_--;
        tap_.update(in, S, [](int, int) { return -1; }); // (a touch that goes down now is on nothing)
    } else if (in.pressed & plat::BTN_B) back();
    else if (in.pressed & plat::BTN_A) action = activate(selected_);
    else if (in.pressed & plat::BTN_UP) move(0, -1);
    else if (in.pressed & plat::BTN_DOWN) move(0, 1);
    else if (in.pressed & plat::BTN_LEFT) move(-1, 0);
    else if (in.pressed & plat::BTN_RIGHT) move(1, 0);

    if (lockout_ == 0 && page_ == page) { // (a button changed the page: the touch waits for its release)
        const MenuTap::Frame f = tap_.update(in, S, [&](int x, int y) { return itemAt(x, y); });
        if (f.down >= 0) selected_ = (Widget)f.down;
        if (f.tapped >= 0) action = activate((Widget)f.tapped);
    }
    // Selection follows the page: a widget gone (Confirm after a delete) gives way to the first one.
    if (!usable(selected_)) {
        const std::vector<Item> all = items();
        if (!all.empty()) selected_ = all.front().widget;
    }
    bounce_.update(lockedOut() ? -1 : (int)page_ * (DONE + 1) + selected_); // (restarts as the cursor appears)
    blinkFrame_++;
    return action;
}

void TitleMenu::draw(const MenuSkin& skin) const {
    // The cursor, none while locked out (LockoutSelectablesInCurrentMenu deselects).
    const int cursor = lockedOut() ? -1 : selected_;
    auto held = [&](Widget w) { return tap_.held(w); };
    auto state = [&](Widget w) { return held(w) ? State::Pressed : w == cursor ? State::Selected : State::Normal; };
    if (page_ == Page::Saves) {
        const char* title = deleting_ ? "Delete file?" : "Select Save";
        skin.drawText(title, CENTER_X - skin.textWidth(title) / 2, TITLE_Y, palette::GRAY);
    }
    if (page_ == Page::Main) // (Continue shows, grayed, even when it can't be used)
        if (!usable(CONTINUE)) skin.drawButton(mainButton(0).x, mainButton(0).y, BUTTON_W, BUTTON_H, "Continue", false, false, 0, false);
    if (page_ == Page::NewSave) {
        skin.drawFrame(MenuSkin::Frame::Light, FIELD_X, FIELD_Y, FIELD_W, FIELD_H);
        const int cy = FIELD_Y + FIELD_H / 2;
        if (name_.empty()) skin.drawText(PLACEHOLDER, FIELD_X + FIELD_PAD, cy, palette::DIM);
        else skin.drawText(name_, FIELD_X + FIELD_PAD, cy, FIELD_TEXT);
    }
    for (const Item& it : items()) {
        const Widget w = it.widget;
        const Box& b = it.box;
        if (isButton(w)) {
            skin.drawButton(b.x, b.y, b.w, b.h, buttonLabel(w), w == cursor, held(w), bounce_.out());
        } else if (w == SAVES_BACK || w == NEW_BACK || w == TRASH) {
            const MenuSkin::Icon icon = w == TRASH ? MenuSkin::Icon::Trash : MenuSkin::Icon::Back;
            int iw, ih;
            skin.iconSize(icon, iw, ih);
            skin.drawIcon(icon, b.x + (b.w - iw) / 2, b.y + (b.h - ih) / 2, state(w));
        } else if (w >= KEY && w < CAPS) {
            const bool on = w == cursor || held(w);
            skin.drawFrame(on ? MenuSkin::Frame::Light : MenuSkin::Frame::Dark, b.x, b.y, b.w, b.h);
            const char letter[2] = {caps_ ? (char)(KEYS[w - KEY] - 'a' + 'A') : KEYS[w - KEY], 0};
            const int drop = held(w) ? MenuSkin::PRESS_DROP : 0;
            skin.drawText(letter, b.x + (b.w - skin.textWidth(letter) + 1) / 2, b.y + b.h / 2 + drop, palette::OFF_WHITE);
        } else { // a card
            const int slot = w - CARD;
            const Card& c = cards_[slot];
            const State st = state(w);
            skin.drawCard(b.x, b.y, b.w, b.h, st);
            if (confirm_ == slot && blinkFrame_ % (2 * BLINK_FRAMES) >= BLINK_FRAMES) continue; // (blinking: hidden)
            const int cy = b.y + (b.h - MenuSkin::CARD_EDGE) / 2 + (st == State::Pressed ? MenuSkin::PRESS_DROP : 0);
            const uint32_t dark = deleting_ ? palette::GRAY : palette::DARK;
            std::string name = c.status == SaveProfile::Read::Ok ? c.name
                               : c.status == SaveProfile::Read::Bad ? "[ Unreadable ]" : "[ Empty ]";
            if (deleting_ && c.status == SaveProfile::Read::Ok) name += "?";
            skin.drawText(name, b.x + CARD_PAD, cy, dark);
            if (c.status != SaveProfile::Read::Ok) continue;
            const std::string time = timeText(c.playTime);
            const int timeX = b.x + b.w - CARD_PAD - skin.textWidth(time);
            skin.drawText(time, timeX, cy, deleting_ ? palette::GRAY : palette::DIM);
            char areas[16];
            snprintf(areas, sizeof areas, "%d/%d", c.areas, SaveProfile::AREAS);
            skin.drawText(areas, timeX - CARD_PAD - skin.textWidth(areas), cy, dark);
        }
    }
}
