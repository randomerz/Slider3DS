// The pause menu (START), in the original's look (game/ui/menu_skin) and layout: the game stops, as
// PauseManager's timeScale 0 does, and the bottom screen shows its pages instead of the Artifact:
//   Paused:  Resume / Options / Quit (UICanvas PausePanel; its web guide button is left out)
//   Options: Screen Shake (slider) / Auto Move (toggle; a port feature) / Back (GeneralOptionsPanel)
// D-pad up / down: pick an item (it shows selected)   A: use it   left / right: move the slider or
// flip the toggle   B: back a page (on Paused: resume)   START: resume
// Stylus: tap a button or the toggle's row (a button shows pressed while held), drag the slider.
// Quit asks again (it ends the app; Unity's goes to the main menu). The menu changes the Settings
// it's given in place; whoever owns them saves them (main, when the menu closes).
#pragma once

#include "game/save/settings.h"
#include "game/ui/menu_skin.h"
#include "platform/platform.h"

class PauseMenu {
public:
    enum class Action { None, Resume, Quit };
    enum class Page { Paused, Options };
    enum Item { RESUME, OPTIONS, QUIT, SCREEN_SHAKE, AUTO_MOVE, BACK };

    // A widget's box on the bottom screen, px.
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
    Item cursor() const { return items()[cursor_]; }
    bool confirmingQuit() const { return confirmQuit_; }

    // An item's box: a button's, or an option's whole row (its label at the left, its control in
    // sliderBox / toggleBox).
    static Box itemBox(Item item);
    static Box sliderBox(); // Screen Shake's slider
    static Box toggleBox(); // Auto Move's toggle

private:
    struct ItemList {
        const Item* items;
        int count;
        Item operator[](int i) const { return items[i]; }
    };
    ItemList items() const; // the page's, top to bottom
    Action activate(Item item, Settings& settings);
    void go(Page page, Item select);
    int itemAt(int px, int py) const; // index on the page, -1 = none
    void step(Item item, int dir, Settings& settings); // left / right on an option

    bool open_ = false;
    Page page_ = Page::Paused;
    int cursor_ = 0; // index on the page
    bool confirmQuit_ = false;
    // Stylus: a tap is a press and release on the same item (the 3DS reports no position on release).
    bool wasTouching_ = false, sliding_ = false;
    int pressed_ = -1; // item index touched down on
    int lastX_ = 0, lastY_ = 0;
};
