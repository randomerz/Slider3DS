// The pause menu (START), in the original's look (game/ui/menu_skin): the game stops, as
// PauseManager's timeScale 0 does, and the bottom screen shows its pages instead of the Artifact:
//   Paused:  Resume / Options / Quit (UICanvas PausePanel's; its web guide button is left out)
//   Options: General / Sound / Cheats / Back (the original's side-nav options menu, as pages;
//            Cheats only in the game, as CheatsButtonEnabler's)
//   General: Screen Shake / Screen Effects (sliders) / Auto Move (a port feature; a toggle) / Back
//            (GeneralOptionsPanel's widgets; its PC-only ones left out, and Bigger Text: user's call,
//            its 1.5x dialogue would break the one pixel scale)
//   Sound:   Master / Sound / Music / Ambience (sliders) / Back (SoundPanel's)
//   Cheats:  Enable Cheats (a port addition: the rest is grayed out without it) / Give (a picker
//            over the area's collectibles: left / right or a tap on the name's sides picks, A or
//            its button marks it, given on Resume) / NoClip (a toggle) / Respawn / Back
//            (CheatsPanel's; Teleport, Spawn Anchor and Scroll + Boots wait for more areas)
// Laid out for the touch screen on MenuSkin's 2x grid, filling it (bigger than the original's).
// D-pad up / down: pick an item, stopping at the page's ends and skipping grayed-out ones (a
// button shows brackets at its ends, an option a light label)
// A: use it   left / right: move the slider or flip the toggle   B: back a page (on Paused:
// resume)   START: resume
// Stylus: tap a button or the toggle's row (a button shows pressed while held), drag the slider
// (only the slider: its label is just a label). Quit asks again, then saves and goes back to the
// title screen (QuitHandler; main does both). The menu changes the Settings it's given in place; whoever owns them saves them
// (main, when the menu closes).
//
// Pages are data (pause_menu.cpp's tables): each page has a title, a parent (where B and Back go,
// and which item to select there) and its items, top to bottom; each item a kind, a label, and a
// button's action (resume, quit, open a page, back) or the setting an option shows and changes.
// A new option or button: an Item and a table row; a new submenu: a Page, its table, and an Open
// button. Each Item is one row on one page, and each row's kind has its payload (checked at
// compile time).
#pragma once

#include "game/save/settings.h"
#include "game/ui/menu_input.h"
#include "game/ui/menu_skin.h"
#include "game/world/cheats.h"
#include "platform/platform.h"

class PauseMenu {
public:
    enum class Action { None, Resume, Quit };
    enum class Page { Paused, Options, General, Sound, Cheats };
    enum Item {
        RESUME, OPTIONS, QUIT,                                                // Paused
        GENERAL, SOUND, CHEATS, OPTIONS_BACK,                                 // Options
        SCREEN_SHAKE, SCREEN_EFFECTS, AUTO_MOVE, GENERAL_BACK,                // General
        MASTER_VOLUME, SFX_VOLUME, MUSIC_VOLUME, AMBIENCE_VOLUME, SOUND_BACK, // Sound
        CHEATS_ENABLED, GIVE, NOCLIP, RESPAWN, CHEATS_BACK,                   // Cheats
        ITEM_COUNT
    };
    enum class Kind { Button, Slider, Toggle, Picker };

    // A widget's box on the bottom screen, px.
    struct Box {
        int x, y, w, h;
        bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    };

    bool isOpen() const { return open_; }
    // A button or toggle was used this frame (its UIClick: the click sound; sliders make none).
    bool clicked() const { return clicked_; }
    // Opens on `start` (its first item selected): Paused in the game, Options from the title screen.
    // Backing out of the page it opened on closes it (Resume). `cheats`: the game's (the Cheats page
    // changes them in place; they must outlive the menu's opening); none = no Cheats page (the title).
    void open(Page start = Page::Paused, Cheats* cheats = nullptr);

    // One frame (the game steps a fixed 60 fps) of input while open. Resume / Quit: the menu just
    // closed.
    Action update(const plat::Input& in, Settings& settings);
    void draw(const Settings& settings, const MenuSkin& skin) const;

    Page page() const { return page_; }
    Item cursor() const;
    bool confirmingQuit() const { return confirmQuit_; }
    int giveIndex() const { return give_; } // the collectible Give shows

    // An item's box: a button's, or an option's whole row (its label at the left).
    static Box itemBox(Item item);
    // An option's slider / toggle / picker's button (inside its row); the item's box for a button.
    static Box controlBox(Item item);
    // Give's name is centered on this x (screen px): a tap left of it picks the one before, right
    // of it (up to its button) the one after.
    static int pickerMiddleX();

    struct ItemDef; // (pause_menu.cpp)

private:
    Action handleInput(const plat::Input& in, Settings& settings);
    Action activate(const ItemDef& def, Settings& settings);
    void step(const ItemDef& def, int dir, Settings& settings); // left / right on an option
    void go(Page page, Item select);
    Action back(); // to the parent page (B, Back), or closed (Resume) from the page it opened on
    int itemAt(int x, int y, const Settings& settings) const; // UI px -> usable index on the page, -1 = none
    // Usable: the Cheats page's button needs the game's cheats, its cheats need Settings::cheats too.
    bool enabled(const ItemDef& def, const Settings& settings) const;
    bool flagOn(const ItemDef& def, const Settings& settings) const; // a toggle's
    void flip(const ItemDef& def, Settings& settings);             // a toggle's flag

    bool open_ = false;
    Page page_ = Page::Paused;
    Page root_ = Page::Paused; // the page it opened on
    int cursor_ = 0; // index on the page
    bool confirmQuit_ = false;
    bool clicked_ = false;
    MenuTap tap_; // stylus taps on the page's items (indices)
    bool sliding_ = false; // the touch went down on the pressed item's slider: it drags it
    int tapX_ = 0;         // where the pressed item's touch went down, UI px (a picker's side)
    Cheats* cheats_ = nullptr;
    int give_ = 0; // Give's collectible (back to the first each time the menu opens, as OnEnable)
    BracketBounce bounce_;
};
