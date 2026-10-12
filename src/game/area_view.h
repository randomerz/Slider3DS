// Draws an Area's tile layers through the platform layer.
//
// Per frame: begin() sets the camera, then drawLayers() draws the tile layers and static sprites
// of a range of sorting layers (in Unity's order: sorting layer, then order in layer). Things drawn between
// calls (y-sorted entities later) land between those sorting layers, like Unity's renderers.
#pragma once

#include <climits>
#include <vector>

#include "game/area.h"
#include "game/grid.h"

namespace plat { struct Texture; }

class AreaView {
public:
    bool loadSheets(const Area& area);
    void unloadSheets();

    // World point (camX, camY) lands at the screen center; the camera snaps to whole pixels.
    // `time` (seconds) drives animated tiles. Inactive STiles are drawn when showInactiveTiles.
    void begin(const Area& area, float camX, float camY, int screenW, int screenH, float time,
               bool showInactiveTiles);
    // Draws the visible tile layers and static sprites with minSortLayer <= sortingLayerValue <=
    // maxSortLayer. At equal (sorting layer, order) static sprites go first.
    void drawLayers(const Area& area, const Grid& grid, int minSortLayer = INT_MIN, int maxSortLayer = INT_MAX);

    // World -> screen pixel (top-left origin) with the current snapped camera.
    void worldToScreen(float wx, float wy, float& sx, float& sy) const;

    int lastSpriteCount() const { return spriteCount_; } // sprites drawn since begin()

private:
    void drawStatic(const Area& area, const Grid& grid, const StaticSprite& st);

    std::vector<plat::Texture*> sheets_;
    float camX_ = 0, camY_ = 0, ppu_ = 16, time_ = 0;
    int screenW_ = 0, screenH_ = 0;
    bool showInactive_ = false;
    int spriteCount_ = 0;
};
