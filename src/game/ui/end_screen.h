// The port's ending (no area follows the Village yet): once the game has faded out through an exit
// (World::leaveArea), a thank-you card on black on both screens; after a second, any button (not
// SELECT: the dev overlay's) or a new touch goes back to the title. Text at the menus' 2x, both
// screens.
#pragma once

#include "game/render/font.h"
#include "platform/platform.h"

class MenuSkin;

class EndScreen {
public:
    static constexpr int FADE_FRAMES = 30;    // the text fades in
    static constexpr int LOCKOUT_FRAMES = 60; // input ignored (the walk into the exit, held buttons)

    void start();
    // One frame; true once the player dismisses it.
    bool update(const plat::Input& in);
    void drawTop(const Font& font) const;
    void drawBottom(const MenuSkin& skin) const;

private:
    float textAlpha() const;

    int frames_ = 0;
    bool touchLifted_ = false; // a touch counts only if it starts on this screen
};
