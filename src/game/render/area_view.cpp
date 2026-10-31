#include "game/render/area_view.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <optional>

#include "game/dev/profiler.h"
#include "game/ui/palette.h"
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
    spriteCount_ = tileCount_ = 0;
    if (tilesFor_ != &area || tileSheets_ != &sheets) prepareTiles(area);
    for (int i : animatedTiles_) { // (each animated tile's frame once, not per cell)
        const TileDef& T = area.tiles[i];
        tileFrame_[i] = T.frames[(size_t)(time_ * T.fps) % T.frames.size()];
    }
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
    // (the dev profiler's DrawList / DrawSort / DrawTiles / DrawObjects: the overlay's DRAW page)
    std::optional<ProfScope> list(ProfSection::DrawList);
    items_.clear();

    // Tile layers sort as whole renderers, by the center of their bounds (none off screen: e.g. the
    // house interiors while outdoors, and the outdoors inside one).
    for (size_t i = 0; i < area.layers.size(); i++) {
        const TileLayer& L = area.layers[i];
        if (!inRange(L.sortLayer) || !L.active || !ownerShown(L.owner)) continue;
        float ax, ay;
        int x0, x1, y0, y1;
        layerRange(area, grid, (int)i, ax, ay, x0, x1, y0, y1);
        if (x0 > x1 || y0 > y1) continue;
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
        float sx, sy; // (its pivot further off screen than any sprite reaches: skipped before the rest)
        worldToScreen(bx + ws.place.x, by + ws.place.y, sx, sy);
        if (sx < -spriteReach_ || sy < -spriteReach_ || sx > screenW_ + spriteReach_ || sy > screenH_ + spriteReach_) continue;
        SortedSprite sp = placeSprite(area, ws.place, bx, by, animTime(area, ws, i, time_));
        if (objects.sprite(ws.object) >= 0) sp.sprite = objects.sprite(ws.object); // set by a script
        sp.fill = objects.fill(ws.object);
        if (objects.sortingOrder(ws.object) != ObjectStates::EXPORTED_ORDER) sp.order = objects.sortingOrder(ws.object);
        const int animator = area.animatorOn(ws.object);
        if (animator >= 0 && world.animators.sprite(animator) != Animators::NOT_YET) sp.sprite = world.animators.sprite(animator);
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
    // Other world-space text (CatTrick's count): canvases like the dialogue's.
    world.worldTexts(texts_);
    for (int i = 0; i < (int)texts_.size(); i++) {
        const WorldText& t = texts_[i];
        if (!font_ || !inRange(t.box->sortLayer) || !ownerShown(objects.owner(t.object))) continue;
        float x, y;
        world.objectPosition(t.sortObject, x, y);
        Item it{ItemKind::Text, i, {}};
        it.sprite.sortLayer = t.box->sortLayer;
        it.sprite.order = t.box->order;
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
        const float reach = particleReach_[p.defIndex];
        if (sx + reach < 0 || sx - reach > screenW_ || sy + reach < 0 || sy - reach > screenH_) continue;
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
        if (!p || p->sprite < 0) continue;
        // (the body moves to ScreenEffects during a pick-up: ranged by the layer it's drawn on)
        const int layer = p == &area.player.body && world.pickup.active() ? area.screenEffectsLayer : p->sortLayer;
        if (!inRange(layer)) continue;
        // The body flips with facing (SpriteRenderer.flipX); its children (boat, indicator) don't.
        const uint32_t flip = p == &area.player.body ? facing : 0u;
        SortedSprite sp = placeSprite(area, *p, pl.x, pl.y, time_, flip);
        sp.sortLayer = layer;
        const int animator = area.player.animator;
        if (p == &area.player.body && animator >= 0 && world.animators.sprite(animator) != Animators::NOT_YET)
            sp.sprite = world.animators.sprite(animator); // its Animator's state (idle, run, boat)
        items_.push_back({ItemKind::Player, (int)items_.size(), sp});
    }

    // ItemPickupEffect's canvas (around the player, under it): the circle, then the item inside it.
    if (const float d = world.pickup.circleDiameter(); d > 0 && inRange(area.screenEffectsLayer)) {
        Item circle{ItemKind::Pickup, 0, {}};
        circle.sprite.sortLayer = area.screenEffectsLayer;
        circle.sprite.order = ItemPickup::ORDER;
        circle.sprite.sortY = pl.y;
        items_.push_back(circle);
        const int item = world.pickup.itemSprite(area);
        if (item >= 0) {
            // Its Image: the sprite's rect centered 1 unit up, at native size (1 px a pixel); shown
            // once the circle (its mask) covers it all.
            const SpriteDef& s = area.sprites[item];
            const float hw = s.w / s.ppu / 2, hh = s.h / s.ppu / 2;
            const float far = std::sqrt(hw * hw + (ItemPickup::ITEM_UP + hh) * (ItemPickup::ITEM_UP + hh));
            if (d / 2 >= far) {
                Item it{ItemKind::Pickup, 1, {}};
                it.sprite.sprite = item;
                it.sprite.x = pl.x + (s.pivotX - s.w / 2.0f) / s.ppu;
                it.sprite.y = pl.y + ItemPickup::ITEM_UP + (s.pivotY - s.h / 2.0f) / s.ppu;
                it.sprite.sortY = pl.y;
                it.sprite.sortLayer = area.screenEffectsLayer;
                it.sprite.order = ItemPickup::ORDER;
                items_.push_back(it);
            }
        }
    }

    list.reset();
    std::optional<ProfScope> sort(ProfSection::DrawSort);
    std::sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
        if (a.sprite.sortLayer != b.sprite.sortLayer) return a.sprite.sortLayer < b.sprite.sortLayer;
        if (a.sprite.order != b.sprite.order) return a.sprite.order < b.sprite.order;
        if (a.sprite.sortY != b.sprite.sortY) return a.sprite.sortY > b.sprite.sortY;
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.index < b.index;
    });

    sort.reset();
    const ProfScope objectsScope(ProfSection::DrawObjects);
    for (const Item& it : items_) {
        if (it.kind == ItemKind::Layer) {
            const ProfScope tiles(ProfSection::DrawTiles); // (a scope per layer, not per cell: ~2 clock reads)
            drawLayer(area, grid, it.index);
        } else if (it.kind == ItemKind::Dialogue) drawDialogue(world, it.index);
        else if (it.kind == ItemKind::Text) drawWorldText(world, it.index);
        else if (it.kind == ItemKind::Line) drawLine(lines_[it.index]);
        else if (it.kind == ItemKind::Particle) drawParticle(area, particles[it.index]);
        else if (it.kind == ItemKind::Pickup && it.index == 0) drawPickupCircle(world);
        else drawSprite(area, it.sprite);
    }
}

void AreaView::drawPickupCircle(const World& world) {
    float sx, sy;
    worldToScreen(world.player.x, world.player.y, sx, sy); // (the WorldCanvas follows the player)
    plat::drawCircle(sx, sy, world.pickup.circleDiameter() / 2 * ppu_, palette::DARK);
    spriteCount_++;
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

void AreaView::drawParticle(const Area& area, const Particles::Particle& p) {
    // A billboard around its position: its texture sheet frame, turned and tinted (its start color
    // and fade), else a square of its color.
    const float size = p.def->size * ppu_;
    float x, y;
    worldToScreen(p.x, p.y, x, y);
    if (const int frame = Particles::frameSprite(p); frame >= 0)
        sheets_->drawTinted(area, frame, x, y, p.rotation, Particles::color(p));
    else
        plat::drawRect(x - size * 0.5f, y - size * 0.5f, size, size, Particles::color(p));
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
    textOrigin(bx, by, n.text.minX, n.text.minY, sx, sy); // the box's bottom-left
    drawGlyphs(text, layout.glyphs, sx, sy, &typer);
}

void AreaView::drawWorldText(const World& world, int i) {
    const WorldText& t = texts_[i];
    if (textLayouts_.size() < texts_.size()) textLayouts_.resize(texts_.size());
    TextLayout& layout = textLayouts_[i];
    if (layout.box != t.box || layout.text != t.text) {
        layout.parsed = parseDialogue(t.text, world.state);
        layoutDialogue(font_->def(), layout.parsed, *t.box, ppu_, layout.glyphs);
        layout.box = t.box;
        layout.text = t.text;
    }
    float bx, by, sx, sy;
    world.objectBase(t.object, bx, by);
    textOrigin(bx, by, t.box->minX, t.box->minY, sx, sy);
    drawGlyphs(layout.parsed, layout.glyphs, sx, sy, nullptr);
}

void AreaView::textOrigin(float bx, float by, float minX, float minY, float& sx, float& sy) const {
    // Whole pixels: the base rounded as its sprites are (their pivots are whole pixels), plus the
    // box's offset rounded. Each letter then lands the same pixels from it however the base moves
    // between pixels (a walking NPC, a sliding tile), and the text keeps with the sprite.
    worldToScreen(bx, by, sx, sy);
    sx = std::round(sx) + std::round(minX * ppu_);
    sy = std::round(sy) - std::round(minY * ppu_);
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

namespace {

// A sprite side (0 top, 1 bottom, 2 left, 3 right) on screen after a cell's xform: rotated 90 CCW,
// then flipped (plat::DrawFlags' order).
int screenSide(int side, uint32_t flags) {
    if (flags & plat::ROT90) side = side == 0 ? 2 : side == 1 ? 3 : side == 2 ? 1 : 0;
    if (flags & plat::FLIP_X) side = side == 2 ? 3 : side == 3 ? 2 : side;
    if (flags & plat::FLIP_Y) side = side == 0 ? 1 : side == 1 ? 0 : side;
    return side;
}

} // namespace

// Keyed by the Area's and the sheets' addresses: both are loaded once for the app's life (main) or a
// test's. A future in-place reload of either (dev hot-reload) must reset tilesFor_ (textures and
// tile indices would go stale).
void AreaView::prepareTiles(const Area& area) {
    assert(sheets_->loaded() || area.sprites.empty());
    tilesFor_ = &area;
    tileSheets_ = sheets_;
    tileSprites_.assign(area.sprites.size(), TileSprite{});
    spriteReach_ = 0;
    for (size_t i = 0; i < area.sprites.size(); i++) {
        const SpriteDef& S = area.sprites[i];
        TileSprite& t = tileSprites_[i];
        if (S.outlineWidth == 0 && S.sheet >= 0) t.tex = sheets_->texture(S.sheet);
        t.x = (int16_t)S.x;
        t.y = (int16_t)S.y;
        t.w = (int16_t)S.w;
        t.h = (int16_t)S.h;
        t.offX = -S.pivotX / S.ppu * ppu_;
        t.offY = -(S.h - S.pivotY) / S.ppu * ppu_;
        const float k = ppu_ / S.ppu; // (world sprites draw at their pixel size; the reach in this view's px)
        spriteReach_ = std::max({spriteReach_, std::abs(S.pivotX) * k, std::abs(S.w - S.pivotX) * k,
                                 std::abs(S.pivotY) * k, std::abs(S.h - S.pivotY) * k, (float)S.w, (float)S.h});
    }
    particleReach_.clear();
    for (const ParticlesDef& d : area.particles) {
        // A square's half, a turned frame's half diagonal (frames draw at native size); + 1 slack (the
        // draws themselves aren't rounded).
        float reach = d.size * ppu_ * 0.5f;
        for (int f : d.frames) reach = std::max(reach, 0.5f * std::hypot((float)area.sprites[f].w, (float)area.sprites[f].h));
        particleReach_.push_back(reach + 1);
    }
    tileFrame_.assign(area.tiles.size(), -1);
    animatedTiles_.clear();
    for (size_t i = 0; i < area.tiles.size(); i++) {
        const TileDef& T = area.tiles[i];
        if (!T.frames.empty()) tileFrame_[i] = T.frames[0];
        if (T.frames.size() > 1) animatedTiles_.push_back((int)i);
    }
    layerBounds_.assign(area.layers.size(), LayerBounds{});
    solidRects_.assign(area.layers.size(), {});
    inRect_.assign(area.layers.size(), {});
    for (size_t li = 0; li < area.layers.size(); li++) {
        const TileLayer& L = area.layers[li];
        LayerBounds& b = layerBounds_[li];
        bool any = false;
        for (int16_t v : L.cells) {
            if (v < 0) continue;
            for (int s : area.tiles[v].frames) {
                const TileSprite& t = tileSprites_[s];
                if (!any || t.offX < b.left) b.left = t.offX;
                if (!any || t.offX + t.w > b.right) b.right = t.offX + t.w;
                if (!any || t.offY < b.top) b.top = t.offY;
                if (!any || t.offY + t.h > b.bottom) b.bottom = t.offY + t.h;
                any = true;
            }
        }
        // The cell's own square, from its anchor (screen px; y down).
        const float cellL = -L.anchorX * ppu_, cellR = cellL + ppu_, cellT = -(1 - L.anchorY) * ppu_, cellB = cellT + ppu_;
        // Solid cells (one color, one frame, filling exactly their cell), and edge-lined ones: their
        // base joins the color runs, their lines (on the screen sides the cell's flips and rotation
        // put them) make runs of their own ...
        std::vector<uint32_t> color(L.cells.size(), 0);
        std::vector<std::array<uint32_t, 4>> lineColor(L.cells.size());
        std::vector<std::array<uint8_t, 4>> lineW(L.cells.size());
        for (size_t c = 0; c < L.cells.size(); c++) {
            const int v = L.cells[c];
            if (v < 0 || area.tiles[v].frames.size() != 1) continue;
            const int s = area.tiles[v].frames[0];
            const TileSprite& t = tileSprites_[s];
            const SpriteDef& S = area.sprites[s];
            if (t.offX != cellL || t.offY != cellT || t.w != ppu_ || t.h != ppu_) continue;
            if (S.solid) {
                color[c] = S.solid;
            } else if (S.edgedBase) {
                color[c] = S.edgedBase;
                const uint32_t flags = L.xform.empty() ? 0 : L.xform[c];
                for (int side = 0; side < 4; side++) {
                    if (!S.edgeWidth[side]) continue;
                    const int on = screenSide(side, flags);
                    lineColor[c][on] = S.edgeColor[side];
                    lineW[c][on] = S.edgeWidth[side];
                }
            }
        }
        // ... but not under another cell's sprite reaching past its own cell (a tall one): those stay
        // sprites, drawn in the cells' order.
        for (int cy = 0; cy < L.h; cy++)
            for (int cx = 0; cx < L.w; cx++) {
                const int v = L.cell(cx, cy);
                if (v < 0) continue;
                for (int s : area.tiles[v].frames) {
                    const TileSprite& t = tileSprites_[s];
                    const int left = (int)std::ceil((cellL - t.offX) / ppu_), right = (int)std::ceil((t.offX + t.w - cellR) / ppu_);
                    const int up = (int)std::ceil((cellT - t.offY) / ppu_), down = (int)std::ceil((t.offY + t.h - cellB) / ppu_);
                    for (int dy = -std::max(0, down); dy <= std::max(0, up); dy++)
                        for (int dx = -std::max(0, left); dx <= std::max(0, right); dx++) {
                            const int nx = cx + dx, ny = cy + dy;
                            if ((dx || dy) && nx >= 0 && nx < L.w && ny >= 0 && ny < L.h) color[(size_t)ny * L.w + nx] = 0;
                        }
                }
            }
        // Greedy rectangles: a run along a row, grown upward while the rows above match.
        std::vector<uint8_t>& used = inRect_[li];
        used.assign(L.cells.size(), 0);
        auto free = [&](int cx, int cy, uint32_t c) { return color[(size_t)cy * L.w + cx] == c && !used[(size_t)cy * L.w + cx]; };
        for (int cy = 0; cy < L.h; cy++)
            for (int cx = 0; cx < L.w; cx++) {
                const uint32_t c = color[(size_t)cy * L.w + cx];
                if (!c || used[(size_t)cy * L.w + cx]) continue;
                int w = 1;
                while (cx + w < L.w && free(cx + w, cy, c)) w++;
                int h = 1;
                for (bool rowOk = true; rowOk && cy + h < L.h; h += rowOk) {
                    for (int i = 0; i < w && rowOk; i++) rowOk = free(cx + i, cy + h, c);
                }
                for (int j = 0; j < h; j++)
                    for (int i = 0; i < w; i++) used[(size_t)(cy + j) * L.w + cx + i] = 1;
                solidRects_[li].push_back({(int16_t)cx, (int16_t)cy, (int16_t)w, (int16_t)h, c});
            }
        // The lines over them: runs of one color and width along a row (top / bottom) or a column.
        // (Only cells still in the runs (color[c]) give lines: an excluded cell draws its own sprite,
        // lines included, after the rects — so the gate saves draws, it isn't needed for the pixels.)
        for (int side = 0; side < 4; side++) {
            const bool alongRow = side < 2;
            using Line = std::pair<uint32_t, uint8_t>; // (color, width; width 0: none)
            const Line none{}; // (0, 0)
            auto line = [&](int cx, int cy) -> Line {
                const size_t c = (size_t)cy * L.w + cx;
                return color[c] && lineW[c][side] ? Line{lineColor[c][side], lineW[c][side]} : none;
            };
            for (int cy = 0; cy < L.h; cy++)
                for (int cx = 0; cx < L.w; cx++) {
                    const auto here = line(cx, cy);
                    if (!here.second) continue;
                    const Line before = alongRow ? (cx > 0 ? line(cx - 1, cy) : none) : (cy > 0 ? line(cx, cy - 1) : none);
                    if (before == here) continue; // (inside a run started earlier)
                    int n = 1;
                    while (alongRow ? cx + n < L.w && line(cx + n, cy) == here : cy + n < L.h && line(cx, cy + n) == here) n++;
                    solidRects_[li].push_back({(int16_t)cx, (int16_t)cy, (int16_t)(alongRow ? n : 1), (int16_t)(alongRow ? 1 : n),
                                               here.first, (uint8_t)(LINE_TOP + side), here.second});
                }
        }
    }
}

namespace {

// std::round's halves away from zero, without the library call (screen coordinates: small values).
float roundPx(float v) { return (float)(int)(v >= 0 ? v + 0.5f : v - 0.5f); }

} // namespace

void AreaView::layerRange(const Area& area, const Grid& grid, int layer, float& ax, float& ay, int& x0, int& x1, int& y0,
                          int& y1) const {
    const TileLayer& L = area.layers[layer];
    const LayerBounds& b = layerBounds_[layer];
    float baseX, baseY;
    grid.ownerBase(L.owner, baseX, baseY);
    // A cell is ppu_ px right per column, up per row, from cell (0, 0)'s anchor.
    ax = (baseX + L.originX + L.minX + L.anchorX - camX_) * ppu_ + screenW_ * 0.5f;
    ay = (camY_ - (baseY + L.originY + L.minY + L.anchorY)) * ppu_ + screenH_ * 0.5f;
    // (a cell more each way than can show: the per-cell check is exact)
    x0 = std::max(0, (int)std::floor((-b.right - ax) / ppu_));
    x1 = std::min(L.w - 1, (int)std::ceil((screenW_ - b.left - ax) / ppu_));
    y0 = std::max(0, (int)std::floor((ay + b.top - screenH_) / ppu_));
    y1 = std::min(L.h - 1, (int)std::ceil((ay + b.bottom) / ppu_));
}

void AreaView::drawLayer(const Area& area, const Grid& grid, int layer) {
    const TileLayer& L = area.layers[layer];
    float ax, ay;
    int x0, x1, y0, y1;
    layerRange(area, grid, layer, ax, ay, x0, x1, y0, y1);
    const float w = (float)screenW_, h = (float)screenH_;
    // Its solid runs first (nothing in the layer overlaps them), then its other cells in order.
    const float cellL = -L.anchorX * ppu_, cellT = -(1 - L.anchorY) * ppu_;
    for (const SolidRect& r : solidRects_[layer]) {
        if (r.cx > x1 || r.cx + r.w - 1 < x0 || r.cy > y1 || r.cy + r.h - 1 < y0) continue;
        const float px = roundPx(ax + r.cx * ppu_ + cellL), py = roundPx(ay - (r.cy + r.h - 1) * ppu_ + cellT);
        float rx = px, ry = py, rw = r.w * ppu_, rh = r.h * ppu_;
        if (r.part == LINE_TOP) rh = r.lineW;
        else if (r.part == LINE_BOTTOM) ry += rh - r.lineW, rh = r.lineW;
        else if (r.part == LINE_LEFT) rw = r.lineW;
        else if (r.part == LINE_RIGHT) rx += rw - r.lineW, rw = r.lineW;
        if (rx >= w || ry >= h || rx + rw <= 0 || ry + rh <= 0) continue;
        plat::drawRect(rx, ry, rw, rh, r.color);
        spriteCount_++;
        tileCount_++;
    }
    const std::vector<uint8_t>& inRect = inRect_[layer];
    for (int cy = y0; cy <= y1; cy++) {
        const int16_t* row = &L.cells[(size_t)cy * L.w];
        const uint8_t* skip = &inRect[(size_t)cy * L.w];
        const uint8_t* xform = L.xform.empty() ? nullptr : &L.xform[(size_t)cy * L.w];
        const float rowY = ay - cy * ppu_;
        for (int cx = x0; cx <= x1; cx++) {
            const int v = row[cx];
            if (v < 0 || skip[cx]) continue;
            const int s = tileFrame_[v];
            if (s < 0) continue;
            const TileSprite& t = tileSprites_[s];
            const float px = roundPx(ax + cx * ppu_ + t.offX), py = roundPx(rowY + t.offY);
            if (px >= w || py >= h || px + t.w <= 0 || py + t.h <= 0) continue;
            const uint32_t flags = xform ? xform[cx] : 0;
            if (t.tex)
                plat::drawSprite(t.tex, t.x, t.y, t.w, t.h, px, py, flags);
            else
                sheets_->draw(area, s, px, py, flags);
            spriteCount_++;
            tileCount_++;
        }
    }
}

bool AreaView::spriteRect(const Area& area, const SortedSprite& sp, float& px, float& py) const {
    if (sp.sprite == NULL_SPRITE) return false; // (a clip's null key)
    assert(sp.sprite >= 0);
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
