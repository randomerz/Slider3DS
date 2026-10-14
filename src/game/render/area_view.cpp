#include "game/render/area_view.h"

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

SortedSprite placeSprite(const Area& area, const SpritePlacement& p, float baseX, float baseY, float animTime,
                         uint32_t flipFlags) {
    SortedSprite out;
    out.sprite = p.anim >= 0 ? area.anims[p.anim].frameAt(animTime) : p.sprite;
    out.x = baseX + p.x;
    out.y = baseY + p.y;
    out.sortY = baseY + p.sortY;
    out.sortLayer = p.sortLayer;
    out.order = p.order;
    out.flags = p.flags ^ flipFlags;
    return out;
}

float AreaView::animTime(const Area& area, const WorldSprite& ws, size_t index, float time) {
    if (ws.place.anim < 0) return time;
    float start = ws.animStart;
    if (ws.animRandom > 0) {
        // AnimationSetFrame's random start, fixed per sprite (a hash of its index) so runs repeat.
        const uint32_t h = (uint32_t)index * 2654435761u;
        start = (float)(h >> 8) / (float)(1u << 24) * ws.animRandom;
    }
    return time + start * area.anims[ws.place.anim].length;
}

void AreaView::draw(const World& world, int minSortLayer, int maxSortLayer) {
    const Area& area = world.area;
    const Grid& grid = world.grid;
    auto inRange = [&](int sortLayer) { return sortLayer >= minSortLayer && sortLayer <= maxSortLayer; };
    auto ownerShown = [&](int owner) { return owner == 0 || grid.tileActive(owner) || showInactive_; };
    const ObjectStates& objects = world.objects;
    items_.clear();

    // Tile layers sort as whole renderers, by the center of their bounds.
    for (size_t i = 0; i < area.layers.size(); i++) {
        const TileLayer& L = area.layers[i];
        if (!inRange(L.sortLayer) || !L.active || !ownerShown(L.owner)) continue;
        float bx, by;
        grid.ownerBase(L.owner, bx, by);
        const float centerY = by + L.originY + L.minY + L.h * 0.5f;
        Item it{ItemKind::Layer, (int)i, {}};
        it.sprite.sortLayer = L.sortLayer;
        it.sprite.order = L.order;
        it.sprite.sortY = centerY;
        items_.push_back(it);
    }
    for (size_t i = 0; i < area.worldSprites.size(); i++) {
        const WorldSprite& ws = area.worldSprites[i];
        if (!inRange(ws.place.sortLayer) || !objects.active(ws) || !ownerShown(objects.owner(ws.object))) continue;
        float bx, by;
        world.objectBase(ws.object, bx, by);
        SortedSprite sp = placeSprite(area, ws.place, bx, by, animTime(area, ws, i, time_));
        if (objects.flipX(ws.object) >= 0) // set by an event (SpriteRenderer.set_flipX)
            sp.flags = (sp.flags & ~(uint32_t)plat::FLIP_X) | (objects.flipX(ws.object) ? (uint32_t)plat::FLIP_X : 0u);
        float px, py;
        if (!spriteRect(area, sp, px, py)) continue;
        items_.push_back({ItemKind::Sprite, (int)i, sp});
    }
    for (int i = 0; i < (int)area.npcs.size(); i++) {
        const NpcDef& n = area.npcs[i];
        if (!world.npcs.textShown(i) || !font_ || !inRange(n.text.sortLayer) || !objects.activeInHierarchy(n.object) ||
            !ownerShown(objects.owner(n.object)))
            continue;
        // A world-space canvas: sorted by its layer and order, then its position.
        float x, y;
        world.objectPosition(n.object, x, y);
        Item it{ItemKind::Dialogue, i, {}};
        it.sprite.sortLayer = n.text.sortLayer;
        it.sprite.order = n.text.order;
        it.sprite.sortY = y;
        items_.push_back(it);
    }
    const Player& pl = world.player;
    const uint32_t facing = pl.facingLeft ? (uint32_t)plat::FLIP_X : 0u;
    const SpritePlacement* playerParts[] = {&area.player.body, pl.onWater ? &area.player.boat : nullptr,
                                            world.promptShown() ? &area.player.actionIndicator : nullptr};
    for (const SpritePlacement* p : playerParts) {
        if (!p || p->sprite < 0 || !inRange(p->sortLayer)) continue;
        // The body flips with facing (SpriteRenderer.flipX); its children (boat, indicator) don't.
        const uint32_t flip = p == &area.player.body ? facing : 0u;
        items_.push_back({ItemKind::Player, (int)items_.size(), placeSprite(area, *p, pl.x, pl.y, time_, flip)});
    }

    std::sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
        if (a.sprite.sortLayer != b.sprite.sortLayer) return a.sprite.sortLayer < b.sprite.sortLayer;
        if (a.sprite.order != b.sprite.order) return a.sprite.order < b.sprite.order;
        if (a.sprite.sortY != b.sprite.sortY) return a.sprite.sortY > b.sprite.sortY;
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.index < b.index;
    });

    for (const Item& it : items_) {
        if (it.kind == ItemKind::Layer) drawLayer(area, grid, area.layers[it.index]);
        else if (it.kind == ItemKind::Dialogue) drawDialogue(world, it.index);
        else drawSprite(area, it.sprite);
    }
}

void AreaView::drawDialogue(const World& world, int npc) {
    const NpcDef& n = world.area.npcs[npc];
    const DialogueText& text = world.npcs.text(npc);
    const Typer& typer = world.npcs.typer(npc);
    if (layouts_.size() != world.area.npcs.size()) layouts_.assign(world.area.npcs.size(), Layout());
    Layout& layout = layouts_[npc];
    if (layout.textVersion != world.npcs.textVersion(npc)) {
        layoutDialogue(font_->def(), text, n.text, ppu_, layout.glyphs);
        layout.textVersion = world.npcs.textVersion(npc);
    }
    const std::vector<GlyphPlace>& glyphs = layout.glyphs;
    float bx, by, sx, sy;
    world.objectBase(n.object, bx, by);
    worldToScreen(bx + n.text.minX, by + n.text.minY, sx, sy); // the box's bottom-left
    // Every backing first, then the letters (the BG font's text renders under the text's).
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < (int)text.chars.size(); i++) {
            if (!glyphVisible(text.chars[i]) || !typer.shown(i)) continue;
            const Glyph& g = font_->def().glyph(text.chars[i]);
            const GlyphRect& r = pass == 0 ? g.fill : g.text;
            const float y = sy - glyphs[i].y - typer.waveOffset(i) * ppu_;
            font_->draw(r, sx + glyphs[i].x + r.ox, y + r.oy);
            spriteCount_++;
        }
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

bool AreaView::spriteRect(const Area& area, const SortedSprite& sp, float& px, float& py) const {
    const SpriteDef& S = area.sprites[sp.sprite];
    // The pivot sits on (x, y). A flipped sprite mirrors around its pivot, like Unity's flipX/flipY.
    const float left = (sp.flags & plat::FLIP_X) ? sp.x - (S.w - S.pivotX) / S.ppu : sp.x - S.pivotX / S.ppu;
    const float top = (sp.flags & plat::FLIP_Y) ? sp.y + S.pivotY / S.ppu : sp.y + (S.h - S.pivotY) / S.ppu;
    worldToScreen(left, top, px, py);
    return px < screenW_ && py < screenH_ && px + S.w > 0 && py + S.h > 0;
}

void AreaView::drawSprite(const Area& area, const SortedSprite& sp) {
    float px, py;
    if (!spriteRect(area, sp, px, py)) return;
    sheets_->draw(area, sp.sprite, px, py, sp.flags);
    spriteCount_++;
}
