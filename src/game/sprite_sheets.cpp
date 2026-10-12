#include "game/sprite_sheets.h"

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

void SpriteSheets::draw(const Area& area, int index, float x, float y, uint32_t flags) const {
    const SpriteDef& S = area.sprites[index];
    plat::drawSprite(sheets_[S.sheet], S.x, S.y, S.w, S.h, std::round(x), std::round(y), flags);
}
