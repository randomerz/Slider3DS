#include "game/area_view.h"

#include <algorithm>
#include <cmath>

#include "platform/platform.h"

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

int AreaView::worldSpriteFrame(const Area& area, const WorldSprite& ws, size_t index, float time) {
    if (ws.anim < 0) return ws.sprite;
    const Anim& an = area.anims[ws.anim];
    float start = ws.animStart;
    if (ws.animRandom > 0) {
        // AnimationSetFrame's random start, fixed per sprite (a hash of its index) so runs repeat.
        uint32_t h = (uint32_t)index * 2654435761u;
        start = (float)(h >> 8) / (float)(1u << 24) * ws.animRandom;
    }
    return an.frameAt(time + start * an.length);
}

void AreaView::draw(const Area& area, const Grid& grid, const SortedSprite* extras, size_t extraCount,
                    int minSortLayer, int maxSortLayer) {
    auto inRange = [&](int sortLayer) { return sortLayer >= minSortLayer && sortLayer <= maxSortLayer; };
    auto ownerShown = [&](int owner) { return owner == 0 || grid.tileActive(owner) || showInactive_; };
    items_.clear();

    // Tile layers sort as whole renderers, by the center of their bounds.
    for (size_t i = 0; i < area.layers.size(); i++) {
        const TileLayer& L = area.layers[i];
        if (!inRange(L.sortLayer) || !L.active || !ownerShown(L.owner)) continue;
        float bx, by;
        grid.ownerBase(L.owner, bx, by);
        const float centerY = by + L.originY + L.minY + L.h * 0.5f;
        items_.push_back({L.sortLayer, L.order, centerY, 0, (int)i, -1, 0, 0, 0});
    }
    for (size_t i = 0; i < area.worldSprites.size(); i++) {
        const WorldSprite& ws = area.worldSprites[i];
        if (!inRange(ws.sortLayer) || !ws.active || !ownerShown(ws.owner)) continue;
        float bx, by;
        grid.ownerBase(ws.owner, bx, by);
        const int frame = worldSpriteFrame(area, ws, i, time_);
        float px, py;
        if (!spriteRect(area, frame, bx + ws.x, by + ws.y, ws.flags, px, py)) continue;
        items_.push_back({ws.sortLayer, ws.order, by + ws.sortY, 1, (int)i, frame, bx + ws.x, by + ws.y, ws.flags});
    }
    for (size_t i = 0; i < extraCount; i++) {
        const SortedSprite& e = extras[i];
        if (!inRange(e.sortLayer) || e.sprite < 0) continue;
        items_.push_back({e.sortLayer, e.order, e.sortY, 2, (int)i, e.sprite, e.x, e.y, e.flags});
    }

    std::sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
        if (a.sortLayer != b.sortLayer) return a.sortLayer < b.sortLayer;
        if (a.order != b.order) return a.order < b.order;
        if (a.sortY != b.sortY) return a.sortY > b.sortY;
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.index < b.index;
    });

    for (const Item& it : items_) {
        if (it.kind == 0) drawLayer(area, grid, area.layers[it.index]);
        else drawSprite(area, it.sprite, it.x, it.y, it.flags);
    }
}

void AreaView::drawLayer(const Area& area, const Grid& grid, const TileLayer& L) {
    const float halfW = screenW_ / (2 * ppu_), halfH = screenH_ / (2 * ppu_);
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

bool AreaView::spriteRect(const Area& area, int sprite, float x, float y, uint32_t flags, float& px, float& py) const {
    const SpriteDef& S = area.sprites[sprite];
    // The pivot sits on (x, y). A flipped sprite mirrors around its pivot, like Unity's flipX/flipY.
    const float left = (flags & plat::FLIP_X) ? x - (S.w - S.pivotX) / S.ppu : x - S.pivotX / S.ppu;
    const float top = (flags & plat::FLIP_Y) ? y + S.pivotY / S.ppu : y + (S.h - S.pivotY) / S.ppu;
    worldToScreen(left, top, px, py);
    const float wPx = S.w * ppu_ / S.ppu, hPx = S.h * ppu_ / S.ppu;
    return px < screenW_ && py < screenH_ && px + wPx > 0 && py + hPx > 0;
}

void AreaView::drawSprite(const Area& area, int sprite, float x, float y, uint32_t flags) {
    float px, py;
    if (!spriteRect(area, sprite, x, y, flags, px, py)) return;
    sheets_->draw(area, sprite, px, py, flags);
    spriteCount_++;
}
