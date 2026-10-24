// The textures of an Area's sprite sheets, and drawing an exported sprite (Area::sprites) by index.
// Shared by everything that draws area content (AreaView, ArtifactUI).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/area.h"

namespace plat { struct Texture; }

class SpriteSheets {
public:
    SpriteSheets() = default;
    SpriteSheets(const SpriteSheets&) = delete;
    SpriteSheets& operator=(const SpriteSheets&) = delete;
    ~SpriteSheets() { unload(); }

    // False (and nothing kept, `error` naming it) if a sheet fails to load.
    bool load(const Area& area, std::string* error = nullptr);
    void unload();

    // Draws sprite `index` of `area` with its top-left at screen pixel (x, y), rounded, magnified
    // by a whole `scale` (an outline sprite as rectangles).
    void draw(const Area& area, int index, float x, float y, uint32_t flags = 0, int scale = 1, uint32_t fill = 0) const;
    // Sprite `index` at 1x centered on (cx, cy), turned `angle` radians counter-clockwise, its colors
    // times `tint` (plat::drawSpriteTinted; not an outline sprite: the area refuses those as frames).
    void drawTinted(const Area& area, int index, float cx, float cy, float angle, uint32_t tint) const;
    plat::Texture* texture(int sheet) const { return sheets_[sheet]; } // (SpriteDef::sheet)
    bool loaded() const { return !sheets_.empty(); }

private:
    std::vector<plat::Texture*> sheets_;
};
