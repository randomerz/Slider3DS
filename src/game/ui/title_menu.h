// The title screen's bottom screen (MainMenu.unity's UICanvas, after "press any key"), in the pause
// menu's look (game/ui/menu_skin), laid out for the touch screen on its 2x grid:
//   Main:    Continue (the profile saved last; grayed out without one) / Play / Options / Quit
//   Saves:   "Select Save": the three profiles' cards (name, areas done "n/9", play time "0h32") under
//            the back and trash icons. A profile's card plays it, an empty one opens New Save, an
//            unreadable one only says so (delete it to reuse the slot). The trash icon toggles delete
//            mode ("Delete file?", names with "?", grayed; the icon itself shows only the cursor, as
//            the original's): a profile's card blinks to be confirmed,
//            then Confirm deletes it, leaving the cursor on its card (an empty card still opens New Save). While no slot has a
//            file, Play goes straight to New Save for the first slot (SavePanelManager.OpenSaves:
//            its skip flag is cleared whenever the panel closes, so every Play from Main skips;
//            back from New Save lands on Saves).
//   NewSave: the name field and the original's keyboard (a-z, Caps, Del, Done; 10 letters): Done
//            with a name starts a new game in that slot.
// D-pad: move to the nearest widget that way that lines up with the selected one (same columns for
// up / down, rows for left / right; none: stay). A: use it. B: back (New Save -> Saves -> Main).
// Stylus: tap (a widget shows pressed while held; releasing elsewhere cancels).
// The menu reads the slots' files itself (SaveProfile::load / remove); whoever opens it starts the
// game its Action names.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/save/profile.h"
#include "game/ui/menu_input.h"
#include "game/ui/menu_skin.h"
#include "platform/platform.h"

class TitleMenu {
public:
    enum class Page { Main, Saves, NewSave };
    enum Widget {
        CONTINUE, PLAY, OPTIONS, QUIT,                           // Main
        SAVES_BACK, TRASH, CARD, CONFIRM = CARD + SaveProfile::SLOTS, // Saves (CARD + slot)
        NEW_BACK, KEY, CAPS = KEY + 26, DEL, DONE,                // NewSave (KEY + key index, KEYS order)
    };
    static constexpr const char KEYS[] = "qwertyuiopasdfghjklzxcvbnm"; // the keyboard's rows, top first
    static constexpr int KEY_COUNT = CAPS - KEY;
    static_assert(sizeof(KEYS) - 1 == KEY_COUNT, "one Widget per key");
    static constexpr int MAX_NAME = 10; // KeyboardButton.MAX_PROFILE_NAME_LENGTH

    struct Action {
        enum class Kind { None, Play, NewGame, Options, Quit };
        Kind kind = Kind::None;
        int slot = -1;    // Play / NewGame
        std::string name; // NewGame: the profile's name
    };
    // A slot as its card shows it (read from its file).
    struct Card {
        SaveProfile::Read status = SaveProfile::Read::None;
        std::string name;
        int areas = 0;
        double playTime = 0;
        int64_t lastSaved = 0;
    };
    // A widget's box on the bottom screen, px.
    struct Box {
        int x, y, w, h;
        bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    };

    // Opens on Main (Continue selected, if there's a profile), the slots read afresh.
    void open();
    // One frame (the game steps a fixed 60 fps) of input.
    Action update(const plat::Input& in);
    // Ignores the touch held now (the Options menu closing over it).
    void releaseStylus() { tap_.release(); }
    // Ignores all input for `frames` frames and shows no cursor (StartMainMenu's
    // LockoutSelectablesInCurrentMenu: 1 s while the buttons come in); a touch held through it is no tap.
    void lockOut(int frames) { lockout_ = frames; }
    bool lockedOut() const { return lockout_ > 0; }
    // A widget was used this frame (its UIClick: the click sound).
    bool clicked() const { return clicked_; }
    void draw(const MenuSkin& skin) const;

    Page page() const { return page_; }
    Widget selected() const { return selected_; }
    void select(Widget w) { if (usable(w)) selected_ = w; } // (as if the D-pad got there; tests)
    bool deleting() const { return deleting_; }
    int confirming() const { return confirm_; } // the slot whose card awaits Confirm, -1 = none
    const std::string& name() const { return name_; }
    bool caps() const { return caps_; }
    const Card& card(int slot) const { return cards_[slot]; }
    int continueSlot() const; // the profile saved last, -1 = none (SaveSystem.GetRecentlyPlayedIndex)
    // A widget's box on the current page (screen px); false if it isn't shown or can't be used now.
    bool box(Widget w, Box& out) const;
    // "0h32" (MainMenuSaveButton's time text).
    static std::string timeText(double seconds);

private:
    struct Item {
        Widget widget;
        Box box; // UI px
    };
    void readCards();
    std::vector<Item> items() const; // the current page's usable widgets
    bool usable(Widget w) const;
    int itemAt(int x, int y) const;  // UI px -> index in items(), -1 = none
    void go(Page page, Widget select);
    void back();
    void openNewSave(int slot);
    void move(int dx, int dy);
    Action activate(Widget w);

    Page page_ = Page::Main;
    Widget selected_ = PLAY;
    Card cards_[SaveProfile::SLOTS];
    bool deleting_ = false;
    int confirm_ = -1;
    int newSlot_ = 0;           // New Save's slot
    std::string name_;
    bool caps_ = false;
    MenuTap tap_; // stylus taps on the page's widgets
    BracketBounce bounce_;
    int blinkFrame_ = 0; // frames since a card started awaiting Confirm (its own timer: timeUntilSwap)
    int lockout_ = 0;    // frames left ignoring input
    bool clicked_ = false;
};
