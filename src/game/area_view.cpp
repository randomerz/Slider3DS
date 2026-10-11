#include "game/area_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "platform/platform.h"

bool AreaView::loadSheets(const Area& area) {
    unloadSheets();
    for (const std::string& name : area.sheetNames) {
        plat::Texture* t = plat::loadSheet(name.c_str());
        if (!t) {
            printf("AreaView: failed to load sheet %s\n", name.c_str());
            return false;
        }
        sheets_.push_back(t);
    }
    return true;
}

void AreaView::unloadSheets() {
    for (plat::Texture* t : sheets_) plat::freeSheet(t);
    sheets_.clear();
}

void AreaView::draw(const Area& area, float camX, float camY, int screenW, int screenH, float time,
                    bool showInactiveTiles) {
    const float ppu = (float)area.ppu;
    // Snap the camera to whole pixels so tiles stay pixel-aligned.
    camX = std::round(camX * ppu) / ppu;
    camY = std::round(camY * ppu) / ppu;
    const float halfW = screenW / (2 * ppu), halfH = screenH / (2 * ppu);
    spriteCount_ = 0;

    for (const TileLayer& L : area.layers) {
        if (!L.active) continue;
        if (L.owner != 0) {
            const STileDef* s = area.stile(L.owner);
            if (!s || (!s->active && !showInactiveTiles)) continue;
        }
        float baseX, baseY;
        area.ownerBase(L.owner, baseX, baseY);
        // World position of cell (0,0)'s min corner.
        const float ox = baseX + L.originX + L.minX, oy = baseY + L.originY + L.minY;

        // Visible cells, with a margin for sprites larger than one cell.
        int x0 = std::max(0, (int)std::floor(camX - halfW - ox) - 1);
        int x1 = std::min(L.w - 1, (int)std::ceil(camX + halfW - ox) + 1);
        int y0 = std::max(0, (int)std::floor(camY - halfH - oy) - 2);
        int y1 = std::min(L.h - 1, (int)std::ceil(camY + halfH - oy) + 1);

        for (int cy = y0; cy <= y1; cy++) {
            for (int cx = x0; cx <= x1; cx++) {
                int v = L.cell(cx, cy);
                if (v < 0) continue;
                const TileDef& T = area.tiles[v];
                if (T.frames.empty()) continue;
                int frame = T.frames.size() > 1
                    ? T.frames[(size_t)(time * T.fps) % T.frames.size()]
                    : T.frames[0];
                const SpriteDef& S = area.sprites[frame];

                // The sprite's pivot sits on the cell's anchor point.
                float left = ox + cx + L.anchorX - S.pivotX / S.ppu;
                float top = oy + cy + L.anchorY - S.pivotY / S.ppu + S.h / S.ppu;
                float px = (left - camX) * ppu + screenW * 0.5f;
                float py = (camY - top) * ppu + screenH * 0.5f;
                uint32_t flags = L.xform.empty() ? 0 : L.xform[cy * L.w + cx];
                plat::drawSprite(sheets_[S.sheet], S.x, S.y, S.w, S.h, std::round(px), std::round(py), flags);
                spriteCount_++;
            }
        }
    }
}
