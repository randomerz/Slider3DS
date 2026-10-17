// The pause menu (START), in the original's look (game/ui/menu_skin): the game stops, as
// PauseManager's timeScale 0 does, and the bottom screen shows its pages instead of the Artifact:
//   Paused:  Resume / Options / Quit (UICanvas PausePanel's; its web guide button is left out)
//   Options: Screen Shake (slider) / Auto Move (toggle; a port feature) / Back (GeneralOptionsPanel's
//            widgets, laid out like the Paused page rather than its side-nav panel's list)
// Laid out for the touch screen on MenuSkin's 2x grid, filling it (bigger than the original's).
// D-pad up / down: pick an item (the cursor's brackets frame it)   A: use it   left / right: move the slider or
// flip the toggle   B: back a page (on Paused: resume)   START: resume
// Stylus: tap a button or the toggle's row (a button shows pressed while held), drag the slider
// (only the slider: its label is just a label). Quit asks again (it ends the app; Unity's goes to
// the main menu). The menu changes the Settings it's given in place; whoever owns them saves them
// (main, when the menu closes).
//
// Each page is a table of items (pause_menu.cpp): kind, label, the setting an option shows and
// changes, and its row from its place in the table. A new option: an Item and a table entry; a new
// button: also its case in activate.
#pragma once

#include "game/save/settings.h"
#include "game/ui/menu_skin.h"
#include "platform/platform.h"

class PauseMenu {
public:
    enum class Action { None, Resume, Quit };
    enum class Page { Paused, Options };
    enum Item { RESUME, OPTIONS, QUIT, SCREEN_SHAKE, AUTO_MOVE, BACK };
    enum class Kind { Button, Slider, Toggle };

    // A widget's box on the bottom screen, px (internally: UI px).
    struct Box {
        int x, y, w, h;
        bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    };

    bool isOpen() const { return open_; }
    void open(); // on Paused, Resume selected

    // One frame of input while open. Resume / Quit: the menu just closed.
    Action update(const plat::Input& in, Settings& settings);
    void draw(const Settings& settings, const MenuSkin& skin) const;

    Page page() const { return page_; }
    Item cursor() const;
    bool confirmingQuit() const { return confirmQuit_; }

    // An item's box: a button's, or an option's whole row (its label at the left).
    static Box itemBox(Item item);
    // An option's slider / toggle (inside its row); the item's box for a button.
    static Box controlBox(Item item);

private:
    Action activate(Item item, Settings& settings);
    void step(Item item, int dir, Settings& settings); // left / right on an option
    void go(Page page, Item select);
    int itemAt(int px, int py) const; // index on the page, -1 = none

    bool open_ = false;
    Page page_ = Page::Paused;
    int cursor_ = 0; // index on the page
    bool confirmQuit_ = false;
    // Stylus: a tap is a press and release on the same item (the 3DS reports no position on release).
    bool wasTouching_ = false, sliding_ = false;
    int pressed_ = -1; // item index touched down on
    int lastX_ = 0, lastY_ = 0;
};
