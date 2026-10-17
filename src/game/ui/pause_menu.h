// The pause menu (START): the game stops, as PauseManager's timeScale 0 does, and the bottom screen
// shows the options instead of the Artifact: Resume, Auto move, Screen shake, Quit.
//   D-pad up / down: pick a row    A: use it    left / right: change a value    B / START: resume
//   stylus: tap a row (the screen shake row: its left half lowers, its right half raises)
// Quit asks again before quitting. The menu changes the Settings it's given in place; whoever
// owns them saves them (main, when the menu closes).
#pragma once

#include "game/render/font.h"
#include "game/save/settings.h"
#include "platform/platform.h"

class PauseMenu {
public:
    enum class Action { None, Resume, Quit };
    enum Row { RESUME, AUTO_MOVE, SCREEN_SHAKE, QUIT, ROW_COUNT };

    bool isOpen() const { return open_; }
    void open(); // cursor on Resume

    // One frame of input while open. Resume: the menu just closed.
    Action update(const plat::Input& in, Settings& settings);
    void draw(const Settings& settings, const Font& font) const;

    int cursor() const { return cursor_; }
    bool confirmingQuit() const { return confirmQuit_; }
    // Row `row`'s box on the bottom screen, px.
    static void rowRect(int row, int& x, int& y, int& w, int& h);

private:
    Action use(int row, int step, Settings& settings); // step: -1 / +1 = left / right, 0 = A
    int rowAt(int px, int py) const;                   // -1 = none

    bool open_ = false;
    int cursor_ = RESUME;
    bool confirmQuit_ = false;
    // Stylus: a tap is a press and release on the same row (the 3DS reports no position on release).
    bool wasTouching_ = false;
    int downRow_ = -1, lastX_ = 0, lastY_ = 0;
};
