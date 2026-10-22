#include "game/ui/end_screen.h"

#include <algorithm>
#include <cmath>

#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"

namespace {

constexpr int S = MenuSkin::UI_SCALE;
constexpr const char* THANKS = "Thanks for playing!";
constexpr const char* MORE = "More areas coming soon";
constexpr const char* PROMPT = "Press any button";

} // namespace

void EndScreen::start() {
    frames_ = 0;
    touchLifted_ = false;
}

bool EndScreen::update(const plat::Input& in) {
    if (!in.touching) touchLifted_ = true;
    if (frames_ < LOCKOUT_FRAMES) {
        frames_++;
        return false;
    }
    return (in.pressed & ~(uint32_t)plat::BTN_SELECT) != 0 || (in.touching && touchLifted_);
}

float EndScreen::textAlpha() const { return std::min(1.0f, (float)frames_ / FADE_FRAMES); }

void EndScreen::drawTop(const Font& font) const {
    plat::beginScreen(plat::Screen::Top, plat::rgba(0, 0, 0));
    const uint8_t a = (uint8_t)std::lround(textAlpha() * 255);
    const GlyphRect& cap = font.def().glyph('A').text;
    // Two lines around the middle, their capitals' middles 12 UI px apart; whole UI px.
    const int midY = plat::TOP_H / S / 2;
    auto line = [&](const char* text, int cy, uint32_t color) {
        const int baseline = cy - (int)std::floor(cap.oy + cap.h / 2.0f);
        const int x = (plat::TOP_W / S - font.textWidth(text)) / 2;
        font.drawText(text, (float)(x * S), (float)(baseline * S), S, palette::withAlpha(color, a));
    };
    line(THANKS, midY - 6, palette::OFF_WHITE);
    line(MORE, midY + 6, palette::GRAY);
}

void EndScreen::drawBottom(const MenuSkin& skin) const {
    plat::beginScreen(plat::Screen::Bottom, plat::rgba(0, 0, 0));
    if (frames_ < LOCKOUT_FRAMES) return; // (shown once it takes input)
    skin.drawText(PROMPT, (plat::BOTTOM_W / S - skin.textWidth(PROMPT)) / 2, plat::BOTTOM_H / S / 2, palette::GRAY);
}
