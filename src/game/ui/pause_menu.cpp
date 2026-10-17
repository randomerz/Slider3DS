#include "game/ui/pause_menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr int ROW_X = 40, ROW_W = plat::BOTTOM_W - 2 * ROW_X, ROW_H = 32;
constexpr int FIRST_ROW_Y = 64, ROW_PITCH = 40;
constexpr int TEXT_SCALE = 2, TITLE_SCALE = 3;
constexpr int PAD = 10; // text inset from a row's sides

constexpr uint32_t ROW_COLOR = plat::rgba(0x2e, 0x2a, 0x26);
constexpr uint32_t CURSOR_COLOR = plat::rgba(0x6a, 0x55, 0x3c);
constexpr uint32_t CURSOR_EDGE = plat::rgba(0xe8, 0xd8, 0xb0);

} // namespace

void PauseMenu::rowRect(int row, int& x, int& y, int& w, int& h) {
    x = ROW_X;
    y = FIRST_ROW_Y + row * ROW_PITCH;
    w = ROW_W;
    h = ROW_H;
}

void PauseMenu::open() {
    open_ = true;
    cursor_ = RESUME;
    confirmQuit_ = false;
    wasTouching_ = true; // (a touch held when the menu opens isn't a tap on it)
    downRow_ = -1;
}

int PauseMenu::rowAt(int px, int py) const {
    for (int row = 0; row < ROW_COUNT; row++) {
        int x, y, w, h;
        rowRect(row, x, y, w, h);
        if (px >= x && px < x + w && py >= y && py < y + h) return row;
    }
    return -1;
}

PauseMenu::Action PauseMenu::use(int row, int step, Settings& settings) {
    if (row != QUIT) confirmQuit_ = false;
    switch (row) {
    case RESUME:
        if (step != 0) return Action::None;
        open_ = false;
        return Action::Resume;
    case AUTO_MOVE:
        settings.autoMove = !settings.autoMove;
        return Action::None;
    case SCREEN_SHAKE: {
        // A steps up, wrapping past 100% to 0; left / right stop at the ends. Whole steps, so a
        // saved value reads back the same.
        const int steps = (int)std::lround(1.0f / Settings::SHAKE_STEP);
        int s = (int)std::lround(settings.screenShake / Settings::SHAKE_STEP);
        s = step == 0 ? (s + 1) % (steps + 1) : std::clamp(s + step, 0, steps);
        settings.screenShake = s * Settings::SHAKE_STEP;
        return Action::None;
    }
    case QUIT:
        if (step != 0) return Action::None;
        if (!confirmQuit_) {
            confirmQuit_ = true;
            return Action::None;
        }
        open_ = false;
        return Action::Quit;
    }
    return Action::None;
}

PauseMenu::Action PauseMenu::update(const plat::Input& in, Settings& settings) {
    if (!open_) return Action::None;
    if (in.pressed & (plat::BTN_B | plat::BTN_START)) {
        open_ = false;
        return Action::Resume;
    }
    const int move = ((in.pressed & plat::BTN_DOWN) ? 1 : 0) - ((in.pressed & plat::BTN_UP) ? 1 : 0);
    if (move != 0) {
        cursor_ = (cursor_ + move + ROW_COUNT) % ROW_COUNT;
        confirmQuit_ = false;
    }
    if (in.pressed & plat::BTN_A) return use(cursor_, 0, settings);
    if (in.pressed & plat::BTN_LEFT) return use(cursor_, -1, settings);
    if (in.pressed & plat::BTN_RIGHT) return use(cursor_, 1, settings);

    Action action = Action::None;
    if (in.touching) {
        lastX_ = in.touchX;
        lastY_ = in.touchY;
        if (!wasTouching_) downRow_ = rowAt(in.touchX, in.touchY);
    } else if (wasTouching_ && downRow_ >= 0 && rowAt(lastX_, lastY_) == downRow_) {
        cursor_ = downRow_;
        int x, y, w, h;
        rowRect(downRow_, x, y, w, h);
        const int step = downRow_ == SCREEN_SHAKE ? (lastX_ < x + w / 2 ? -1 : 1) : 0;
        action = use(downRow_, step, settings);
        downRow_ = -1;
    }
    wasTouching_ = in.touching;
    return action;
}

void PauseMenu::draw(const Settings& settings, const Font& font) const {
    const char* title = "Paused";
    font.drawText(title, (plat::BOTTOM_W - font.textWidth(title, TITLE_SCALE)) / 2.0f, 40, TITLE_SCALE);
    for (int row = 0; row < ROW_COUNT; row++) {
        int x, y, w, h;
        rowRect(row, x, y, w, h);
        if (row == cursor_) {
            plat::drawRect((float)x - 1, (float)y - 1, (float)w + 2, (float)h + 2, CURSOR_EDGE);
            plat::drawRect((float)x, (float)y, (float)w, (float)h, CURSOR_COLOR);
        } else {
            plat::drawRect((float)x, (float)y, (float)w, (float)h, ROW_COLOR);
        }
        const char* label = "";
        char value[16] = "";
        switch (row) {
        case RESUME: label = "Resume"; break;
        case AUTO_MOVE:
            label = "Auto move";
            snprintf(value, sizeof value, "%s", settings.autoMove ? "On" : "Off");
            break;
        case SCREEN_SHAKE:
            label = "Screen shake";
            snprintf(value, sizeof value, "< %d%% >", (int)std::lround(settings.screenShake * 100));
            break;
        case QUIT: label = confirmQuit_ ? "Quit? Press again" : "Quit"; break;
        }
        const float baseline = (float)(y + h / 2 + 5);
        font.drawText(label, (float)(x + PAD), baseline, TEXT_SCALE);
        if (value[0]) font.drawText(value, (float)(x + w - PAD - font.textWidth(value, TEXT_SCALE)), baseline, TEXT_SCALE);
    }
}
