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

private:
    enum class ItemKind : uint8_t { Layer, Sprite, Player, Emote, Dialogue, Line, Particle }; // tie order at equal sort keys
    struct Item {
        ItemKind kind;
        // Layer: Area::layers; Sprite: Area::worldSprites; Player: insertion order; Emote, Dialogue:
        // Area::npcs; Line: lines_; Particle: World::particles
        int index;
        SortedSprite sprite; // sort keys (all kinds) and what to draw (sprites)
    };
    struct LineDraw {
        float x0, y0, x1, y1, width; // world units
        uint32_t color;
    };

    void drawLayer(const Area& area, const Grid& grid, const TileLayer& L);
    void drawSprite(const Area& area, const SortedSprite& sp); // if on screen
    void drawDialogue(const World& world, int npc);
    void drawLine(const LineDraw& l);
    void drawParticle(const Particles::Particle& p);
    // Screen top-left of `sp`; false if it is off screen.
    bool spriteRect(const Area& area, const SortedSprite& sp, float& px, float& py) const;

    const SpriteSheets* sheets_ = nullptr; // set by begin()
    const Font* font_ = nullptr;
    float camX_ = 0, camY_ = 0, ppu_ = 16, time_ = 0;
    int screenW_ = 0, screenH_ = 0;
    bool showInactive_ = false;
    int spriteCount_ = 0;
    std::vector<Item> items_; // reused every frame
    std::vector<LineDraw> lines_; // this frame's knot lines
    struct Layout {
        uint32_t textVersion = 0; // Npcs::textVersion it was laid out for (0 = none)
        std::vector<GlyphPlace> glyphs;
    };
    std::vector<Layout> layouts_; // per NPC: its line's layout, redone when the line changes
};
