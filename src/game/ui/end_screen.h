// The port's ending (no area follows the Village yet), after the game faded out through an exit
// (World::leaveArea): the original demo's EndOfDemo screen fitted to the 3DS (user's choice). Top
// screen at 1x: its 320x180 canvas centered, baked into one image by tools/make_ui.py (the end art,
// the original's outlined texts in their own fonts, its dark panel; "More areas coming soon" in
// place of the Steam wishlist). Bottom screen: its Main Menu button alone (the Steam and Twitter
// links dropped), at the menus' 2x; A or a tap on it (on release) returns to the title. The top
// fades in from black over FADE_FRAMES; the button shows and takes input after LOCKOUT_FRAMES.
#pragma once

#include "game/ui/menu_input.h"
#include "platform/platform.h"

class MenuSkin;

class EndScreen {
public:
    static constexpr int FADE_FRAMES = 30;    // fades in from black
    static constexpr int LOCKOUT_FRAMES = 60; // then the button shows and takes input

    void start(); // (a touch held now is no tap)
    // One frame; true once Main Menu is pressed.
    bool update(const plat::Input& in);
    // A press this frame (UIClick: the Frontend cues "UI Click").
    bool clicked() const { return clicked_; }
    void drawTop(const MenuSkin& skin) const;
    void drawBottom(const MenuSkin& skin) const;

    struct Box {
        int x, y, w, h;
    };
    static Box buttonBox(); // Main Menu's, bottom-screen px

private:
    int frames_ = 0;
    bool clicked_ = false;
    MenuTap tap_;
    BracketBounce bounce_;
};
