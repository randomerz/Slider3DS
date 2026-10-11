// Area data loaded from the Unity export (assets/<area>/<area>.json; format documented in the
// exporter, Slider repo: Assets/Scripts/Editor/Export3DS/Slider3DSExporter.cs).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SpriteDef {
    int sheet;
    int x, y, w, h;       // pixel rect in the sheet, top-left origin
    float pivotX, pivotY; // pixels from the sprite's bottom-left
    float ppu;
};

struct TileDef {
    std::vector<int> frames; // SpriteDef indices; empty = occupies the cell but draws nothing
    float fps = 0.0f;
};

struct TileLayer {
    std::string path;
    int owner = 0; // STile islandId, 0 = world
    bool active = true;
    float originX = 0, originY = 0; // world units; relative to the owner STile's position
    float anchorX = 0.5f, anchorY = 0.5f;
    int sortLayer = 0, order = 0;
    int minX = 0, minY = 0, w = 0, h = 0;
    std::vector<int16_t> cells; // TileDef index, -1 empty; row-major from (minX, minY), y up
    std::vector<uint8_t> xform; // plat::DrawFlags per cell; empty = none

    int cell(int cx, int cy) const { return cells[cy * w + cx]; }
};

struct STileDef {
    int islandId;
    std::string name;
    int x, y, width;
    bool active;
};

struct Area {
    int ppu = 16;
    int gridW = 0, gridH = 0;
    std::string targetGrid;
    std::vector<STileDef> stiles;
    std::vector<TileLayer> layers; // drawn layers only, in draw order
    std::vector<TileDef> tiles;
    std::vector<SpriteDef> sprites;
    std::vector<std::string> sheetNames;

    const STileDef* stile(int islandId) const;
};

// Loads e.g. "village/village.json" through plat::readFile.
bool loadArea(const char* path, Area& out, std::string* error = nullptr);
