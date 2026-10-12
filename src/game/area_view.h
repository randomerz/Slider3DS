// Draws an Area (tile layers and world sprites) through the platform layer.
//
// Per frame: begin() sets the camera, then draw() draws everything visible in Unity's order:
// sorting layer, then order in layer, then (GraphicsSettings custom sort axis (0,1,0)) higher sort
// point y first. Sprites the caller owns (the player) are passed to draw() and sorted in too.
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game/area.h"
#include "game/grid.h"
#include "game/object_states.h"
#include "game/sprite_sheets.h"

// A sprite drawn at a world position (its pivot lands on (x, y)) and sorted among the area's.
struct SortedSprite {
    int sprite = -1; // Area::sprites index
    float x = 0, y = 0, sortY = 0;
    int sortLayer = 0, order = 0;
    uint32_t flags = 0; // plat::DrawFlags
};

class AreaView {
public:
    // World point (camX, camY) lands at the screen center; the camera snaps to whole pixels.
    // `time` (seconds) drives animated tiles and sprites. Inactive STiles are drawn when showInactiveTiles.
    void begin(const Area& area, const SpriteSheets& sheets, float camX, float camY, int screenW, int screenH, float time,
               bool showInactiveTiles);
    // Draws the visible tile layers, active world sprites and `extras` with minSortLayer <= sorting
    // layer value <= maxSortLayer, sorted together.
    void draw(const Area& area, const Grid& grid, const ObjectStates& objects, const SortedSprite* extras = nullptr,
              size_t extraCount = 0, int minSortLayer = INT_MIN, int maxSortLayer = INT_MAX);

    // World -> screen pixel (top-left origin) with the current snapped camera.
    void worldToScreen(float wx, float wy, float& sx, float& sy) const;

    // The sprite world sprite `ws` (index `index` in Area::worldSprites) shows at `time`.
    static int worldSpriteFrame(const Area& area, const WorldSprite& ws, size_t index, float time);

    int lastSpriteCount() const { return spriteCount_; } // sprites drawn since begin()

private:
    struct Item {
        int sortLayer, order;
        float sortY;
        uint8_t kind; // 0 tile layer, 1 world sprite, 2 extra
        int index;
        int sprite;   // world sprite / extra: the frame to draw
        float x, y;   // world sprite / extra: pivot position (world)
        uint32_t flags;
    };

    void drawLayer(const Area& area, const Grid& grid, const TileLayer& L);
    // Draws `sprite` with its pivot at world (x, y) if on screen.
    void drawSprite(const Area& area, int sprite, float x, float y, uint32_t flags);
    // Screen top-left of `sprite` with its pivot at world (x, y); false if it is off screen.
    bool spriteRect(const Area& area, int sprite, float x, float y, uint32_t flags, float& px, float& py) const;

    const SpriteSheets* sheets_ = nullptr; // set by begin()
    float camX_ = 0, camY_ = 0, ppu_ = 16, time_ = 0;
    int screenW_ = 0, screenH_ = 0;
    bool showInactive_ = false;
    int spriteCount_ = 0;
    std::vector<Item> items_; // reused every frame
};
