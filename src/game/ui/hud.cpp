#include "game/ui/hud.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "game/ui/palette.h"
#include "game/world/world.h"
#include "platform/platform.h"

namespace {

constexpr uint32_t PROMPT_EDGE = plat::rgba(0x66, 0x66, 0x66); // Skip Prompt (dialogueHighContrastBG's outline)
constexpr uint32_t PROMPT_FILL = plat::rgba(0xe5, 0xe5, 0xe5);
constexpr float HINT_BOX_ALPHA = 0.878f; // HintsPanel's Image: #1a1a1a at 224/255
constexpr int HINT_BOX_H = 30;           // its height, along the view's bottom edge
constexpr int ACQUIRED_SCALE = 2;        // the item caption at 2x (user: 1x is small on the 3DS; 2x = the UI's pixel)
constexpr int ACQUIRED_MARGIN = 8;       // px kept clear at each side before it wraps
// Its line step: one em (ppem) at 2x — TMP's face line height is 1.027 em (32.9 px at 2x); whole UI
// px keep the 2x grid.

// One line of `font` at `scale`x centered on (cx, cy): its capitals' middle on cy.
void drawCentered(const Font& font, const std::string& text, int cx, int cy, uint32_t color, int scale = 1) {
    const GlyphRect& cap = font.def().glyph('A').text;
    const int baseline = cy - (int)std::floor(cap.oy + cap.h / 2.0f) * scale;
    const int w = font.textWidth(text, scale);
    font.drawText(text, (float)(cx - (w + 1) / 2), (float)baseline, scale, color); // (odd widths: a px left)
}

uint8_t alpha(float a) { return (uint8_t)std::lround(std::min(std::max(a, 0.0f), 1.0f) * 255); }

} // namespace

void drawSkipPrompt(const Font& font, int screenW, int screenH, float fill) {
    // The original's layout on its 1920x1080 canvas over the 320x180 view (6 canvas units a pixel):
    // a 50x12 box (dialogueHighContrastBG, 9-sliced: a 1 px gray outline with open corners, dark
    // inside) 8 px from the right and 11 from the bottom, the fill 1 px inside it, the text on top.
    constexpr int W = 50, H = 12, RIGHT = 8, BOTTOM = 11;
    const int x = screenW - RIGHT - W, y = screenH - BOTTOM - H;
    plat::drawRect((float)x + 1, (float)y, W - 2, 1, PROMPT_EDGE);
    plat::drawRect((float)x + 1, (float)(y + H - 1), W - 2, 1, PROMPT_EDGE);
    plat::drawRect((float)x, (float)y + 1, 1, H - 2, PROMPT_EDGE);
    plat::drawRect((float)(x + W - 1), (float)y + 1, 1, H - 2, PROMPT_EDGE);
    plat::drawRect((float)x + 1, (float)y + 1, W - 2, H - 2, palette::DARK);
    const int filled = (int)std::lround(std::min(std::max(fill, 0.0f), 1.0f) * (W - 2));
    if (filled > 0) plat::drawRect((float)x + 1, (float)y + 1, (float)filled, H - 2, PROMPT_FILL);
    const char* text = "Skip";
    const GlyphRect& cap = font.def().glyph('A').text;
    const int baseline = y + H / 2 - (int)std::floor(cap.oy + cap.h / 2.0f);
    font.drawText(text, (float)(x + (W - font.textWidth(text)) / 2), (float)baseline, 1, palette::OFF_WHITE);
}

void drawTopOverlays(const World& world, const Font& menuFont, const Font& caption) {
    if (world.dollies.promptShown()) drawSkipPrompt(menuFont, plat::TOP_W, plat::TOP_H, world.dollies.promptFill());
    if (const float box = world.hints.boxAlpha(); box > 0) {
        // HintsPanel: a dark strip along the bottom, its hint centered on one line (PixelTitanZero:
        // the original's 12 is under its 16 px pixel size; the 3DS texts fit the width).
        plat::drawRect(0, plat::TOP_H - HINT_BOX_H, plat::TOP_W, HINT_BOX_H,
                       palette::withAlpha(palette::DARK, alpha(box * HINT_BOX_ALPHA)));
        if (world.hints.shownHint() >= 0)
            drawCentered(caption, world.hints.shownText(), plat::TOP_W / 2,
                         plat::TOP_H - HINT_BOX_H / 2, palette::withAlpha(palette::OFF_WHITE, alpha(box * world.hints.textAlpha())));
    }
    if (world.pickup.textShown()) {
        // Item Acquired Text: centered 50 px above the view's middle (its 240x50 box), at 2x. Too
        // wide for the screen (no Village item is: "Legendary Cheese Burger Acquired!" is), it wraps
        // upward: the last line stays there, clear of the item over the player's head.
        // (wrapped once per caption: it shows for 75 frames)
        const Font& font = caption;
        static std::string wrapped;
        static const Font* wrappedWith = nullptr;
        static std::vector<std::string> lines;
        if (world.pickup.text() != wrapped || &font != wrappedWith) {
            wrapped = world.pickup.text();
            wrappedWith = &font;
            lines = font.wrap(wrapped, ACQUIRED_SCALE, plat::TOP_W - 2 * ACQUIRED_MARGIN);
        }
        const int step = font.def().ppem * ACQUIRED_SCALE;
        const int last = plat::TOP_H / 2 - 50;
        for (int i = 0; i < (int)lines.size(); i++)
            drawCentered(font, lines[i], plat::TOP_W / 2, last - ((int)lines.size() - 1 - i) * step, palette::OFF_WHITE,
                         ACQUIRED_SCALE);
    }
    if (const float black = world.effects.black(); black > 0)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, (uint8_t)(black * 255)));
    // (UIEffects' white flashes peak at the Screen Effects setting)
    if (const float white = world.effects.whiteAlpha(world.area) * world.settings.screenEffects; white > 0)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, palette::withAlpha(palette::OFF_WHITE, (uint8_t)(white * 255)));
}
