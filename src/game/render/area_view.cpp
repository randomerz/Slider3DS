#include "game/render/area_view.h"

#include <algorithm>
#include <cmath>

#include "game/village/cat_trick.h"
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
        if (!inRange(ws.place.sortLayer) || !objects.active(ws) || !objects.rendererEnabled(ws.object) ||
            objects.sprite(ws.object) == ObjectStates::NO_SPRITE || !ownerShown(objects.owner(ws.object)))
            continue;
        float bx, by;
        world.objectBase(ws.object, bx, by);
        SortedSprite sp = placeSprite(area, ws.place, bx, by, animTime(area, ws, i, time_));
        if (objects.sprite(ws.object) >= 0) sp.sprite = objects.sprite(ws.object); // set by a script
        sp.fill = objects.fill(ws.object);
        if (objects.sortingOrder(ws.object) != ObjectStates::EXPORTED_ORDER) sp.order = objects.sortingOrder(ws.object);
        const int animator = area.animatorOn(ws.object);
        if (animator >= 0 && world.animators.sprite(animator) >= 0) sp.sprite = world.animators.sprite(animator);
        if (objects.flipX(ws.object) >= 0) // set by an event (SpriteRenderer.set_flipX)
            sp.flags = (sp.flags & ~(uint32_t)plat::FLIP_X) | (objects.flipX(ws.object) ? (uint32_t)plat::FLIP_X : 0u);
        float px, py;
        if (!spriteRect(area, sp, px, py)) continue;
        items_.push_back({ItemKind::Sprite, (int)i, sp});
    }
    for (int i = 0; i < (int)area.npcs.size(); i++) {
        const NpcEmoteDef& e = area.npcs[i].emote;
        SpritePlacement emote;
        if (e.object < 0 || !world.npcs.emoteSprite(world, i, emote) || !inRange(emote.sortLayer) ||
            !objects.activeInHierarchy(e.object) || !ownerShown(objects.owner(e.object)))
            continue;
        float bx, by;
        world.objectBase(e.object, bx, by);
        const SortedSprite sp = placeSprite(area, emote, bx, by, 0);
        float px, py;
        if (spriteRect(area, sp, px, py)) items_.push_back({ItemKind::Emote, i, sp});
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
    // CatTrick's count: a world-space canvas like the dialogue's, while UpdateText has it on.
    for (int i = 0; i < (int)area.catTricks.size(); i++) {
        const CatTrickDef& c = area.catTricks[i];
        if (!font_ || !inRange(c.text.sortLayer) || !objects.activeInHierarchy(c.textObjects[0]) ||
            !ownerShown(objects.owner(c.object)))
            continue;
        float x, y;
        world.objectPosition(c.textObjects[0], x, y);
        Item it{ItemKind::Text, i, {}};
        it.sprite.sortLayer = c.text.sortLayer;
        it.sprite.order = c.text.order;
        it.sprite.sortY = y;
        items_.push_back(it);
    }
    // Knot lines (LineRenderers, as KnotBox left them this frame): sorted by their bounds' center.
    lines_.clear();
    for (int b = 0; b < (int)area.knotBoxes.size(); b++) {
        const KnotBoxDef& k = area.knotBoxes[b];
        if (!objects.activeInHierarchy(k.object)) continue; // (OnDisable put its lines at the origin)
        for (int i = 0; i < (int)k.lines.size(); i++) {
            const KnotLineDef& l = k.lines[i];
            if (!inRange(l.sortLayer) || !objects.activeInHierarchy(l.object) || !ownerShown(objects.owner(l.object))) continue;
            LineDraw d;
            world.knots.line(b, i, d.x0, d.y0, d.x1, d.y1);
            d.width = l.width;
            d.color = world.knots.lineBad(b, i) ? k.bad : k.good;
            Item it{ItemKind::Line, (int)lines_.size(), {}};
            it.sprite.sortLayer = l.sortLayer;
            it.sprite.order = l.order;
            it.sprite.sortY = (d.y0 + d.y1) * 0.5f;
            lines_.push_back(d);
            items_.push_back(it);
        }
    }
    const std::vector<Particles::Particle>& particles = world.particles.particles();
    for (int i = 0; i < (int)particles.size(); i++) {
        const Particles::Particle& p = particles[i];
        if (!inRange(p.def->sortLayer)) continue;
        float sx, sy; // (off screen: not worth sorting)
        worldToScreen(p.x, p.y, sx, sy);
        const float half = p.def->size * ppu_ * 0.5f + 1;
        if (sx + half < 0 || sx - half > screenW_ || sy + half < 0 || sy - half > screenH_) continue;
        Item it{ItemKind::Particle, i, {}};
        it.sprite.sortLayer = p.def->sortLayer;
        it.sprite.order = p.def->order;
        it.sprite.sortY = p.y;
        items_.push_back(it);
    }

    float dropX, dropY; // PlayerAction's item drop indicator
    if (world.items.dropIndicator(dropX, dropY) && area.playerAction.dropIndicator.sprite >= 0 &&
        inRange(area.playerAction.dropIndicator.sortLayer))
        items_.push_back({ItemKind::Player, (int)items_.size(), placeSprite(area, area.playerAction.dropIndicator, dropX, dropY, time_)});
    const Player& pl = world.player;
    const uint32_t facing = pl.facingLeft ? (uint32_t)plat::FLIP_X : 0u;
    const SpritePlacement* playerParts[] = {&area.player.body, pl.onWater ? &area.player.boat : nullptr,
                                            world.promptShown() ? &area.player.actionIndicator : nullptr};
    for (const SpritePlacement* p : playerParts) {
        if (!p || p->sprite < 0 || !inRange(p->sortLayer)) continue;
        // The body flips with facing (SpriteRenderer.flipX); its children (boat, indicator) don't.
        const uint32_t flip = p == &area.player.body ? facing : 0u;
        SortedSprite sp = placeSprite(area, *p, pl.x, pl.y, time_, flip);
        const int animator = area.player.animator;
        if (p == &area.player.body && animator >= 0 && world.animators.sprite(animator) >= 0)
            sp.sprite = world.animators.sprite(animator); // its Animator's state (idle, run, boat)
        items_.push_back({ItemKind::Player, (int)items_.size(), sp});
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
        else if (it.kind == ItemKind::Text) drawCatCount(world, it.index);
        else if (it.kind == ItemKind::Line) drawLine(lines_[it.index]);
        else if (it.kind == ItemKind::Particle) drawParticle(particles[it.index]);
        else drawSprite(area, it.sprite);
    }
}

void AreaView::drawLine(const LineDraw& l) {
    float x0, y0, x1, y1;
    worldToScreen(l.x0, l.y0, x0, y0);
    worldToScreen(l.x1, l.y1, x1, y1);
    const float pad = l.width * ppu_;
    if (std::max(x0, x1) < -pad || std::min(x0, x1) > screenW_ + pad || std::max(y0, y1) < -pad ||
        std::min(y0, y1) > screenH_ + pad)
        return;
    plat::drawLine(x0, y0, x1, y1, l.width * ppu_, l.color);
    spriteCount_++;
}

void AreaView::drawParticle(const Particles::Particle& p) {
    // A billboard square around its position.
    const float size = p.def->size * ppu_;
    float x, y;
    worldToScreen(p.x, p.y, x, y);
    x -= size * 0.5f;
    y -= size * 0.5f;
    if (x + size < 0 || x > screenW_ || y + size < 0 || y > screenH_) return;
    plat::drawRect(x, y, size, size, Particles::color(p));
    spriteCount_++;
}

void AreaView::drawDialogue(const World& world, int npc) {
    const NpcDef& n = world.area.npcs[npc];
    const DialogueText& text = world.npcs.text(npc);
    const Typer& typer = world.npcs.typer(npc);
    if (layouts_.size() != world.area.npcs.size()) layouts_.assign(world.area.npcs.size(), Layout());
    Layout& layout = layouts_[npc];
    if (layout.textVersion != world.npcs.textVersion(npc) || layout.glyphs.size() != text.chars.size()) {
        layoutDialogue(font_->def(), text, n.text, ppu_, layout.glyphs);
        layout.textVersion = world.npcs.textVersion(npc);
    }
    float bx, by, sx, sy;
    world.objectBase(n.object, bx, by);
    worldToScreen(bx + n.text.minX, by + n.text.minY, sx, sy); // the box's bottom-left
    drawGlyphs(text, layout.glyphs, sx, sy, &typer);
}

void AreaView::drawCatCount(const World& world, int catTrick) {
    const CatTrickDef& c = world.area.catTricks[catTrick];
    if (countLayouts_.size() != world.area.catTricks.size()) countLayouts_.assign(world.area.catTricks.size(), CountLayout());
    CountLayout& layout = countLayouts_[catTrick];
    const int count = village::CatTricks::count(world);
    if (layout.count != count) {
        layout.text = parseDialogue(village::CatTricks::text(count), world.state);
        layoutDialogue(font_->def(), layout.text, c.text, ppu_, layout.glyphs);
        layout.count = count;
    }
    float bx, by, sx, sy;
    world.objectBase(c.object, bx, by);
    worldToScreen(bx + c.text.minX, by + c.text.minY, sx, sy);
    drawGlyphs(layout.text, layout.glyphs, sx, sy, nullptr);
}

void AreaView::drawGlyphs(const DialogueText& text, const std::vector<GlyphPlace>& glyphs, float sx, float sy,
                          const Typer* typer) {
    // Every backing first, then the letters (the BG font's text renders under the text's).
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < (int)text.chars.size(); i++) {
            if (!glyphVisible(text.chars[i]) || (typer && !typer->shown(i))) continue;
            const Glyph& g = font_->def().glyph(text.chars[i]);
            const GlyphRect& r = pass == 0 ? g.fill : g.text;
            const float y = sy - glyphs[i].y - (typer ? typer->waveOffset(i) * ppu_ : 0);
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
    sheets_->draw(area, sp.sprite, px, py, sp.flags, 1, sp.fill);
    spriteCount_++;
}
