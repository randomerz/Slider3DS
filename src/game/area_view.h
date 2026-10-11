// Draws an Area's tile layers through the platform layer.
#pragma once

#include <vector>

#include "game/area.h"

namespace plat { struct Texture; }

class AreaView {
public:
    bool loadSheets(const Area& area);
    void unloadSheets();

    // Draws the layers visible around world point (camX, camY), which lands at the screen center.
    // `time` (seconds) drives animated tiles. Inactive STiles are drawn when showInactiveTiles.
    void draw(const Area& area, float camX, float camY, int screenW, int screenH, float time,
              bool showInactiveTiles);

    int lastSpriteCount() const { return spriteCount_; }

private:
    std::vector<plat::Texture*> sheets_;
    int spriteCount_ = 0;
};
