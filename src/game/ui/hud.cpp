#include "game/ui/hud.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"
#include "game/world/world.h"
#include "platform/platform.h"

namespace {

constexpr uint32_t PROMPT_EDGE = plat::rgba(0x66, 0x66, 0x66); // Skip Prompt (dialogueHighContrastBG's outline)
constexpr uint32_t PROMPT_FILL = plat::rgba(0xe5, 0xe5, 0xe5);

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

void drawTopOverlays(const World& world, const MenuSkin& skin) {
    if (world.dollies.promptShown()) drawSkipPrompt(skin.font(), plat::TOP_W, plat::TOP_H, world.dollies.promptFill());
    if (world.pickup.textShown()) {
        // Item Acquired Text: centered 50 px above the view's middle (its 240x50 box; one line).
        const Font& font = skin.captionFont();
        const std::string text = world.pickup.text(world.area);
        const GlyphRect& cap = font.def().glyph('A').text;
        const int cy = plat::TOP_H / 2 - 50;
        const int baseline = cy - (int)std::floor(cap.oy + cap.h / 2.0f);
        font.drawText(text, (float)((plat::TOP_W - font.textWidth(text)) / 2), (float)baseline, 1, palette::OFF_WHITE);
    }
    if (const float black = world.effects.black(); black > 0)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, (uint8_t)(black * 255)));
    if (const float white = world.effects.whiteAlpha(world.area); white > 0)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, palette::withAlpha(palette::OFF_WHITE, (uint8_t)(white * 255)));
}
