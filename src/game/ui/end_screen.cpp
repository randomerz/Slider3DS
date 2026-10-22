#include "game/ui/end_screen.h"

#include <cmath>

#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"

namespace {

constexpr int S = MenuSkin::UI_SCALE;
// EndOfDemo.unity's 320x180 canvas (baked: tools/make_ui.py), centered on the top screen at 1x.
constexpr int CANVAS_W = 320, CANVAS_H = 180;
constexpr int CANVAS_X = (plat::TOP_W - CANVAS_W) / 2, CANVAS_Y = (plat::TOP_H - CANVAS_H) / 2;
// Main Menu (its button), centered on the bottom screen like the pause menu's (UI px).
constexpr int BUTTON_W = 120, BUTTON_H = 18;
constexpr int BUTTON_X = (plat::BOTTOM_W / S - BUTTON_W) / 2, BUTTON_Y = (plat::BOTTOM_H / S - BUTTON_H) / 2;
constexpr int BUTTON = 0; // its MenuTap item

} // namespace

EndScreen::Box EndScreen::buttonBox() { return {BUTTON_X * S, BUTTON_Y * S, BUTTON_W * S, BUTTON_H * S}; }

void EndScreen::start() {
    frames_ = 0;
    clicked_ = false;
    tap_.release();
    bounce_ = BracketBounce();
}

bool EndScreen::update(const plat::Input& in) {
    clicked_ = false;
    const bool ready = frames_ >= LOCKOUT_FRAMES;
    if (!ready) frames_++;
    const MenuTap::Frame f = tap_.update(in, S, [&](int x, int y) {
        return ready && x >= BUTTON_X && x < BUTTON_X + BUTTON_W && y >= BUTTON_Y && y < BUTTON_Y + BUTTON_H ? BUTTON : -1;
    });
    if (!ready) return false;
    bounce_.update(BUTTON);
    clicked_ = f.down == BUTTON || (in.pressed & plat::BTN_A);
    return f.tapped == BUTTON || (in.pressed & plat::BTN_A);
}

void EndScreen::drawTop(const MenuSkin& skin) const {
    plat::beginScreen(plat::Screen::Top, palette::DARK); // (the art's own background around it)
    skin.drawEndArt(CANVAS_X, CANVAS_Y);
    if (frames_ < FADE_FRAMES) // (from black)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, (uint8_t)std::lround(255 - 255.0f * frames_ / FADE_FRAMES)));
}

void EndScreen::drawBottom(const MenuSkin& skin) const {
    plat::beginScreen(plat::Screen::Bottom, palette::DARK);
    if (frames_ < LOCKOUT_FRAMES) return;
    skin.drawButton(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, "Main Menu", true, tap_.held(BUTTON), bounce_.out());
}
