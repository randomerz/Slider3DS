#include "game/render/sprite_sheets.h"

#include <cmath>
#include <cstdio>

#include "platform/platform.h"

bool SpriteSheets::load(const Area& area) {
    unload();
    for (const std::string& name : area.sheetNames) {
        plat::Texture* t = plat::loadSheet(name.c_str());
        if (!t) {
            printf("SpriteSheets: failed to load sheet %s\n", name.c_str());
            unload();
            return false;
        }
        sheets_.push_back(t);
    }
    return true;
}

void SpriteSheets::unload() {
    for (plat::Texture* t : sheets_) plat::freeSheet(t);
    sheets_.clear();
}

void SpriteSheets::draw(const Area& area, int index, float x, float y, uint32_t flags, int scale, uint32_t fill) const {
    const SpriteDef& S = area.sprites[index];
    if (S.outlineWidth > 0) { // a solid border: four rectangles (symmetric, so flips don't matter)
        const float l = std::round(x), t = std::round(y), w = (float)(S.w * scale), h = (float)(S.h * scale);
        const float b = (float)(S.outlineWidth * scale);
        plat::drawRect(l, t, w, b, S.outlineColor);
        plat::drawRect(l, t + h - b, w, b, S.outlineColor);
        plat::drawRect(l, t + b, b, h - 2 * b, S.outlineColor);
        plat::drawRect(l + w - b, t + b, b, h - 2 * b, S.outlineColor);
        return;
    }
    plat::drawSprite(sheets_[S.sheet], S.x, S.y, S.w, S.h, std::round(x), std::round(y), flags, scale, fill);
}
