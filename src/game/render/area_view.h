// Draws a World's area (tile layers, active world sprites as scripts left their SpriteRenderers, the
// player, NPCs' dialogue and message pings, knot lines, particles) through the platform layer.
//
// Per frame: begin() sets the camera, then draw() draws everything visible in Unity's order:
// sorting layer, then order in layer, then (GraphicsSettings custom sort axis (0,1,0)) higher sort
// point y first.
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game/data/area.h"
#include "game/npc/dialogue_text.h"
#include "game/render/font.h"
#include "game/grid/grid.h"
#include "game/render/sprite_sheets.h"
#include "game/world/world.h"

// A sprite ready to draw: its pivot lands on world (x, y); sorted by (sortLayer, order, sortY).
struct SortedSprite {
    int sprite = -1; // Area::sprites index
    float x = 0, y = 0, sortY = 0;
    int sortLayer = 0, order = 0;
    uint32_t flags = 0; // plat::DrawFlags
    uint32_t fill = 0;  // a solid color over its shape (RGBA), 0 = none
};

// `p` placed relative to (baseX, baseY), showing its animation frame `animTime` seconds in, with
// `flipFlags` toggling its flips (e.g. the player facing left).
SortedSprite placeSprite(const Area& area, const SpritePlacement& p, float baseX, float baseY, float animTime,
                         uint32_t flipFlags = 0);

class AreaView {
public:
    // World point (camX, camY) lands at the screen center; the camera snaps to whole pixels.
    // `time` (seconds) drives animated tiles and sprites. Inactive STiles are drawn when showInactiveTiles.
    void begin(const Area& area, const SpriteSheets& sheets, float camX, float camY, int screenW, int screenH, float time,
               bool showInactiveTiles);
    // The font NPC dialogue is drawn in (none: dialogue isn't drawn).
    void setFont(const Font* font) { font_ = font; }
    // Draws the visible tile layers, active world sprites (NPCs' pings among them), the player and NPCs' emotes and dialogue
    // with minSortLayer <= sorting layer value <= maxSortLayer, sorted together.
    void draw(const World& world, int minSortLayer = INT_MIN, int maxSortLayer = INT_MAX);

    // World -> screen pixel (top-left origin) with the current snapped camera.
    void worldToScreen(float wx, float wy, float& sx, float& sy) const;

    // The animation time world sprite `ws` (index `index` in Area::worldSprites) is at, at `time`.
    static float animTime(const Area& area, const WorldSprite& ws, size_t index, float time);

    int lastSpriteCount() const { return spriteCount_; } // sprites drawn since begin()
    int lastTileCount() const { return tileCount_; }     // of them, tile layer cells

private:
    enum class ItemKind : uint8_t { Layer, Sprite, Player, Emote, Dialogue, Text, Line, Particle, Pickup }; // tie order at equal sort keys
    struct Item {
        ItemKind kind;
        // Layer: Area::layers; Sprite: Area::worldSprites; Player: insertion order; Emote, Dialogue:
        // Area::npcs; Text: texts_; Line: lines_; Particle: World::particles; Pickup: 0 = its circle,
        // 1 = its item (drawn as a sprite)
        int index;
        SortedSprite sprite; // sort keys (all kinds) and what to draw (sprites)
    };
    struct LineDraw {
        float x0, y0, x1, y1, width; // world units
        uint32_t color;
    };

    void drawLayer(const Area& area, const Grid& grid, int layer); // Area::layers index
    void prepareTiles(const Area& area); // tileSprites_ / layerBounds_ / tileFrame_, once per Area
    void drawSprite(const Area& area, const SortedSprite& sp); // if on screen
    void drawDialogue(const World& world, int npc);
    void drawWorldText(const World& world, int i); // texts_[i]
    // `text` laid out as `glyphs` in a box whose bottom-left is at screen (sx, sy): every backing,
    // then the letters; `typer` (null = all shown, still) reveals and waves them.
    void drawGlyphs(const DialogueText& text, const std::vector<GlyphPlace>& glyphs, float sx, float sy,
                    const Typer* typer);
    void drawLine(const LineDraw& l);
    void drawPickupCircle(const World& world); // ItemPickupEffect's mask (its dark panel inside it)
    void drawParticle(const Area& area, const Particles::Particle& p);
    float particleReach(const ParticlesDef& d) const; // culling: screen px it reaches from its position
    // Screen top-left of `sp`; false if it is off screen.
    bool spriteRect(const Area& area, const SortedSprite& sp, float& px, float& py) const;

    const SpriteSheets* sheets_ = nullptr; // set by begin()
    const Font* font_ = nullptr;
    float camX_ = 0, camY_ = 0, ppu_ = 16, time_ = 0;
    int screenW_ = 0, screenH_ = 0;
    bool showInactive_ = false;
    int spriteCount_ = 0, tileCount_ = 0;
    std::vector<Item> items_; // reused every frame
    std::vector<LineDraw> lines_; // this frame's knot lines
    struct Layout {
        uint32_t textVersion = 0; // Npcs::textVersion it was laid out for (0 = none)
        std::vector<GlyphPlace> glyphs;
    };
    std::vector<Layout> layouts_; // per NPC: its line's layout, redone when the line changes
    std::vector<WorldText> texts_; // this frame's world texts
    struct TextLayout {
        const TextBoxDef* box = nullptr; // what it was laid out for
        const char* text = nullptr;
        DialogueText parsed;
        std::vector<GlyphPlace> glyphs;
    };
    std::vector<TextLayout> textLayouts_; // per texts_ entry, redone when its box or text changes

    // Tile cells, the bulk of a frame's sprites (~5 us each on an Old 3DS with the general path):
    // what each needs is worked out once per Area, so a cell is a few adds and one plat::drawSprite.
    struct TileSprite {
        plat::Texture* tex = nullptr; // null: an outline sprite (drawn the general way)
        int16_t x = 0, y = 0, w = 0, h = 0; // its rect in the sheet
        float offX = 0, offY = 0; // screen px from the cell's anchor point to its top-left (pivot on the anchor)
    };
    struct LayerBounds { // the layer's sprites' extents around a cell's anchor, screen px (for culling)
        float left = 0, right = 0, top = 0, bottom = 0;
    };
    const Area* tilesFor_ = nullptr;        // the Area and sheets these were prepared for
    const SpriteSheets* tileSheets_ = nullptr;
    std::vector<TileSprite> tileSprites_;   // per Area::sprites
    std::vector<LayerBounds> layerBounds_;  // per Area::layers
    std::vector<int> tileFrame_;            // per Area::tiles: its sprite this frame (-1: draws nothing)
    std::vector<int> animatedTiles_;        // tiles with several frames (tileFrame_ set each begin())
    // Most cells are one solid color (SpriteDef::solid: ~3/4 of the Village's), or a solid base with
    // edge lines (SpriteDef::edgedBase: the sliders' outlines): each layer's are merged into
    // rectangles per color — the bases first, then the lines, as runs along rows / columns — drawn
    // before its other cells. A cell joins only if its sprite fills exactly its cell and no other
    // cell's sprite in the layer reaches over it, so the pixels match.
    enum : uint8_t { FULL = 0, LINE_TOP, LINE_BOTTOM, LINE_LEFT, LINE_RIGHT }; // SolidRect::part (screen sides)
    struct SolidRect {
        int16_t cx, cy, w, h; // cells [cx, cx + w) x [cy, cy + h)
        uint32_t color;
        uint8_t part = FULL;  // the cells' whole squares, or a line `lineW` px thick along that side of them
        uint8_t lineW = 0;
    };
    std::vector<std::vector<SolidRect>> solidRects_; // per layer
    std::vector<std::vector<uint8_t>> inRect_;       // per layer, per cell: drawn by a SolidRect
    float spriteReach_ = 0; // screen px: the farthest any sprite's pixels lie from its pivot (world sprites' early cull)
    // The screen position of cell (0, 0)'s anchor and the cells whose sprites can reach the screen
    // (x0 > x1 or y0 > y1: none).
    void layerRange(const Area& area, const Grid& grid, int layer, float& ax, float& ay, int& x0, int& x1, int& y0,
                    int& y1) const;
};
