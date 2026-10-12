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

// Collision tilemap: cells index Area::shapes. Only solid (non-trigger, enabled) maps are kept.
enum class CollisionMode { Always, OnFoot, OnWater }; // WaterLandColliderManager toggles the latter two

struct CollisionLayer {
    std::string path;
    int owner = 0;
    bool active = true; // authored state; ignored for OnFoot/OnWater
    CollisionMode mode = CollisionMode::Always;
    float originX = 0, originY = 0;
    int minX = 0, minY = 0, w = 0, h = 0;
    std::vector<int16_t> cells; // shape index, -1 = none

    int cell(int cx, int cy) const { return cells[cy * w + cx]; }
};

// One polygon, flat x0,y0,x1,y1,... in cell units with (0,0) at the cell's min corner.
using Polygon = std::vector<float>;

struct Spawn {
    std::string name;
    int owner; // STile islandId (position relative to its center), 0 = world
    float x, y;
    bool inBoat;
};

struct STileDef {
    int islandId;
    std::string name;
    int x, y, width; // grid slot; the tile is centered on (x * width, y * width)
    bool active;
    int startX = 0, startY = 0; // slot and active state in the export (new game)
    bool startActive = false;
    // Runtime state, owned by Grid: x, y, active above; the drawn/physical position in slots
    // (differs from x, y while sliding), and a counter that switches the tile's colliders off while > 0
    // (STile.SetSliderCollider, used for tiles in a move the player isn't riding).
    float posX = 0, posY = 0;
    int collidersOff = 0;
};

// Temporary wall segment (world units): the edge colliders that fence a slide (Grid).
struct Wall { float x0, y0, x1, y1; };

struct Area {
    int ppu = 16;
    int gridW = 0, gridH = 0;
    std::string targetGrid;
    int housingOffset = 0; // y offset of each STile's house interior
    std::vector<STileDef> stiles;
    std::vector<Spawn> spawns;
    std::vector<CollisionLayer> collision;
    std::vector<std::vector<Polygon>> shapes; // collision shapes (each a list of polygons)
    std::vector<TileLayer> layers; // drawn layers only, in draw order
    std::vector<TileDef> tiles;
    std::vector<SpriteDef> sprites;
    std::vector<std::string> sheetNames;
    std::vector<Wall> borderWalls; // runtime, set by Grid

    const STileDef* stile(int islandId) const;
    const Spawn* spawn(const char* name) const;
    // World position of an owner's origin: an STile's (animated) center, or (0,0) for the world.
    void ownerBase(int owner, float& x, float& y) const;
};

// Loads e.g. "village/village.json" through plat::readFile.
bool loadArea(const char* path, Area& out, std::string* error = nullptr);
