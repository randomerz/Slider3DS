#include "game/area_view.h"

#include <algorithm>
#include <cmath>


void AreaView::begin(const Area& area, const SpriteSheets& sheets, float camX, float camY, int screenW, int screenH, float time,
                     bool showInactiveTiles) {
    sheets_ = &sheets;
    ppu_ = (float)area.ppu;
    // Snap the camera to whole pixels so tiles stay pixel-aligned.
    camX_ = std::round(camX * ppu_) / ppu_;
    camY_ = std::round(camY * ppu_) / ppu_;
    screenW_ = screenW;
    screenH_ = screenH;
    time_ = time;
    showInactive_ = showInactiveTiles;
    spriteCount_ = 0;
}

void AreaView::worldToScreen(float wx, float wy, float& sx, float& sy) const {
    sx = (wx - camX_) * ppu_ + screenW_ * 0.5f;
    sy = (camY_ - wy) * ppu_ + screenH_ * 0.5f;
}

void AreaView::drawLayers(const Area& area, const Grid& grid, int minSortLayer, int maxSortLayer) {
    const float halfW = screenW_ / (2 * ppu_), halfH = screenH_ / (2 * ppu_);

    // Layers and statics are each sorted by (sortLayer, order) at load; merge them.
    size_t si = 0;
    const std::vector<StaticSprite>& statics = area.statics;
    while (si < statics.size() && statics[si].sortLayer < minSortLayer) si++;

    for (const TileLayer& L : area.layers) {
        if (L.sortLayer < minSortLayer) continue;
        if (L.sortLayer > maxSortLayer) break;
        for (; si < statics.size() && (statics[si].sortLayer < L.sortLayer ||
                                       (statics[si].sortLayer == L.sortLayer && statics[si].order <= L.order));
             si++)
            drawStatic(area, grid, statics[si]);
        if (!L.active) continue;
        if (L.owner != 0 && !grid.tileActive(L.owner) && !showInactive_) continue;
        float baseX, baseY;
        grid.ownerBase(L.owner, baseX, baseY);
        // World position of cell (0,0)'s min corner.
        const float ox = baseX + L.originX + L.minX, oy = baseY + L.originY + L.minY;

        // Visible cells, with a margin for sprites larger than one cell.
        int x0 = std::max(0, (int)std::floor(camX_ - halfW - ox) - 1);
        int x1 = std::min(L.w - 1, (int)std::ceil(camX_ + halfW - ox) + 1);
        int y0 = std::max(0, (int)std::floor(camY_ - halfH - oy) - 2);
        int y1 = std::min(L.h - 1, (int)std::ceil(camY_ + halfH - oy) + 1);

        for (int cy = y0; cy <= y1; cy++) {
            for (int cx = x0; cx <= x1; cx++) {
                int v = L.cell(cx, cy);
                if (v < 0) continue;
                const TileDef& T = area.tiles[v];
                if (T.frames.empty()) continue;
                int frame = T.frames.size() > 1
                    ? T.frames[(size_t)(time_ * T.fps) % T.frames.size()]
                    : T.frames[0];
                const SpriteDef& S = area.sprites[frame];

                // The sprite's pivot sits on the cell's anchor point.
                float left = ox + cx + L.anchorX - S.pivotX / S.ppu;
                float top = oy + cy + L.anchorY - S.pivotY / S.ppu + S.h / S.ppu;
                float px, py;
                worldToScreen(left, top, px, py);
                uint32_t flags = L.xform.empty() ? 0 : L.xform[cy * L.w + cx];
                sheets_->draw(area, frame, px, py, flags);
                spriteCount_++;
            }
        }
    }
    for (; si < statics.size() && statics[si].sortLayer <= maxSortLayer; si++) drawStatic(area, grid, statics[si]);
}

void AreaView::drawStatic(const Area& area, const Grid& grid, const StaticSprite& st) {
    if (!st.active || (st.owner != 0 && !grid.tileActive(st.owner) && !showInactive_)) return;
    float baseX, baseY;
    grid.ownerBase(st.owner, baseX, baseY);
    const SpriteDef& S = area.sprites[st.sprite];
    // The pivot sits on the transform position.
    const float left = baseX + st.x - S.pivotX / S.ppu, top = baseY + st.y - S.pivotY / S.ppu + S.h / S.ppu;
    float px, py;
    worldToScreen(left, top, px, py);
    const float wPx = S.w * ppu_ / S.ppu, hPx = S.h * ppu_ / S.ppu;
    if (px >= screenW_ || py >= screenH_ || px + wPx <= 0 || py + hPx <= 0) return; // off screen
    sheets_->draw(area, st.sprite, px, py, st.flags);
    spriteCount_++;
}
