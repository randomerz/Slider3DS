// The textures of an Area's sprite sheets, and drawing an exported sprite (Area::sprites) by index.
// Shared by everything that draws area content (AreaView, ArtifactUI).
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

namespace plat { struct Texture; }

class SpriteSheets {
public:
    SpriteSheets() = default;
    SpriteSheets(const SpriteSheets&) = delete;
    SpriteSheets& operator=(const SpriteSheets&) = delete;
    ~SpriteSheets() { unload(); }

    bool load(const Area& area); // false (and nothing kept) if a sheet fails to load
    void unload();

    // Draws sprite `index` of `area` with its top-left at screen pixel (x, y), rounded, magnified
    // by a whole `scale` (an outline sprite as rectangles).
    void draw(const Area& area, int index, float x, float y, uint32_t flags = 0, int scale = 1) const;

private:
    std::vector<plat::Texture*> sheets_;
};
