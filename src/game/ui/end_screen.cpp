#include "game/ui/end_screen.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"

namespace {

constexpr int S = MenuSkin::UI_SCALE;
// EndOfDemo.unity's 320x180 canvas, centered on the top screen.
constexpr int CANVAS_W = 320, CANVAS_H = 180;
constexpr int CANVAS_X = (plat::TOP_W - CANVAS_W) / 2, CANVAS_Y = (plat::TOP_H - CANVAS_H) / 2;
constexpr int PANEL_H = 60;                                    // DarkPanel: the canvas's bottom 60 px,
constexpr uint32_t PANEL = plat::rgba(0, 0, 0, 100);           // black at 0.392
constexpr int TITLE_CX = CANVAS_X + CANVAS_W / 2 - 45;         // Title Text: (-45, 55) from the middle
constexpr int TITLE_CY = CANVAS_Y + CANVAS_H / 2 - 55;
constexpr int TEXT_CY = CANVAS_Y + CANVAS_H - 42;              // Text: centered 42.5 px up from the bottom
constexpr int LINE_ADVANCE = 10;                               // px between its two lines
constexpr const char* TITLE = "Thanks for playing Slider!";
constexpr const char* LINE_1 = "This is the end of the demo.";
constexpr const char* LINE_2 = "More areas coming soon";
// Main Menu (its button), centered on the bottom screen like the pause menu's (UI px).
constexpr int BUTTON_W = 120, BUTTON_H = 18;
constexpr int BUTTON_X = (plat::BOTTOM_W / S - BUTTON_W) / 2, BUTTON_Y = (plat::BOTTOM_H / S - BUTTON_H) / 2;
constexpr int BUTTON = 0; // its MenuTap item

// `text` centered on (cx, cy) (its capitals' middle), 1x, light over dark copies 1 px around.
void drawOutlined(const Font& font, const char* text, int cx, int cy) {
    const GlyphRect& cap = font.def().glyph('A').text;
    const float x = (float)(cx - font.textWidth(text) / 2);
    const float baseline = (float)(cy - (int)std::floor(cap.oy + cap.h / 2.0f));
    for (const auto& d : {std::pair{-1, 0}, std::pair{1, 0}, std::pair{0, -1}, std::pair{0, 1}})
        font.drawText(text, x + d.first, baseline + d.second, 1, palette::DARK);
    font.drawText(text, x, baseline, 1, palette::OFF_WHITE);
}

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
    plat::drawRect(CANVAS_X, CANVAS_Y + CANVAS_H - PANEL_H, CANVAS_W, PANEL_H, PANEL);
    const Font& font = skin.font();
    drawOutlined(font, TITLE, TITLE_CX, TITLE_CY);
    drawOutlined(font, LINE_1, plat::TOP_W / 2, TEXT_CY - LINE_ADVANCE / 2);
    drawOutlined(font, LINE_2, plat::TOP_W / 2, TEXT_CY + LINE_ADVANCE / 2);
    if (frames_ < FADE_FRAMES) // (from black)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, (uint8_t)std::lround(255 - 255.0f * frames_ / FADE_FRAMES)));
}

void EndScreen::drawBottom(const MenuSkin& skin) const {
    plat::beginScreen(plat::Screen::Bottom, palette::DARK);
    if (frames_ < LOCKOUT_FRAMES) return;
    skin.drawButton(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, "Main Menu", true, tap_.held(BUTTON), bounce_.out());
}
