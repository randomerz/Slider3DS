// The port's ending (no area follows the Village yet), after the game faded out through an exit
// (World::leaveArea): the original demo's EndOfDemo screen fitted to the 3DS (user's choice). Top
// screen at 1x: its canvas (320x180, or the user's extension up to the screen's 400x240) centered,
// baked into one image by tools/make_ui.py (the end art, the original's outlined texts in their own
// fonts, its dark panel; "More areas coming soon" in place of the Steam wishlist): EndArt. Bottom screen: its Main Menu button alone (the Steam and Twitter
// links dropped), at the menus' 2x; A or a tap on it (on release) returns to the title. The top
// fades in from black over FADE_FRAMES; the button shows and takes input after LOCKOUT_FRAMES.
#pragma once

#include <string>

#include "game/ui/menu_input.h"
#include "platform/platform.h"

class MenuSkin;

// The ending's top-screen image (assets/ui/end.json + its sheet). Loaded at boot with the other UI
// assets (a missing one fails the boot); main owns it.
class EndArt {
public:
    EndArt() = default;
    EndArt(const EndArt&) = delete;
    EndArt& operator=(const EndArt&) = delete;
    ~EndArt() { unload(); }

    // False (with `error`) if missing, or not between 320x180 and the top screen's size.
    bool load(std::string* error = nullptr);
    void unload();
    int width() const { return w_; }
    int height() const { return h_; }
    void draw(int x, int y) const; // 1x, its top-left at screen px (x, y)

private:
    plat::Texture* sheet_ = nullptr;
    int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
};

class EndScreen {
public:
    static constexpr int FADE_FRAMES = 30;    // fades in from black
    static constexpr int LOCKOUT_FRAMES = 60; // then the button shows and takes input

    void start(); // (a touch held now is no tap)
    // One frame; true once Main Menu is pressed.
    bool update(const plat::Input& in);
    // A press this frame (UIClick: the Frontend cues "UI Click").
    bool clicked() const { return clicked_; }
    void drawTop(const EndArt& art) const; // (centered)
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
