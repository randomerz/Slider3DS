// Platform layer: the only interface game code uses for rendering, input and lifecycle.
// Implemented by src/platform/ctr (3DS, citro2d) and src/platform/sdl (PC, SDL2).
#pragma once

#include <cstdint>
#include <string>

namespace plat {

enum class Screen { Top, Bottom };

constexpr int TOP_W = 400;
constexpr int TOP_H = 240;
constexpr int BOTTOM_W = 320;
constexpr int BOTTOM_H = 240;

// Sprites + rects per frame, both screens (the 3DS backend's C2D_Init budget). Draws past it are
// dropped silently by citro2d, so the dev overlay warns before getting close.
constexpr int MAX_DRAWS_PER_FRAME = 8192;

enum Button : uint32_t {
    BTN_A      = 1u << 0,
    BTN_B      = 1u << 1,
    BTN_X      = 1u << 2,
    BTN_Y      = 1u << 3,
    BTN_L      = 1u << 4,
    BTN_R      = 1u << 5,
    BTN_START  = 1u << 6,
    BTN_SELECT = 1u << 7,
    BTN_UP     = 1u << 8,
    BTN_DOWN   = 1u << 9,
    BTN_LEFT   = 1u << 10,
    BTN_RIGHT  = 1u << 11,
};

struct Input {
    uint32_t held = 0;    // buttons currently down
    uint32_t pressed = 0; // buttons that went down this frame
    bool touching = false;
    int touchX = 0;       // bottom-screen pixel coords
    int touchY = 0;
};

// Colors are 0xRRGGBBAA.
constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | a;
}

// Sprite draw flags. ROT90 rotates 90 degrees counter-clockwise (as seen on screen) and is
// applied before the flips, matching the exporter's "xform" bits.
enum DrawFlags : uint32_t {
    FLIP_X = 1u << 0,
    FLIP_Y = 1u << 1,
    ROT90  = 1u << 2,
};

struct Texture; // opaque, backend-specific

// Timing of the previous frame, for on-screen perf readouts.
struct FrameStats {
    float cpuMs = 0; // time spent between beginFrame() and endFrame()
    float gpuMs = 0; // GPU drawing time (0 where the backend can't measure it)
    float frameMs = 0; // time from the previous beginFrame() to the last one (1000 / fps)
};

bool init();
void shutdown();
bool mainLoop(); // false when the OS/window asks the app to quit

Input pollInput();

// Reads a data file relative to the asset root (PC: <repo>/assets/, 3DS: romfs:/).
bool readFile(const char* path, std::string& out);

// The player's own files (settings; later the save profile), by plain name. PC: SDL's pref path
// (%APPDATA%/boomo/Slider3DS/), 3DS: sdmc:/3ds/slider3ds/. writeUserFile replaces the whole file
// through a temporary, so a failed write keeps the old one.
bool readUserFile(const char* name, std::string& out);
bool writeUserFile(const char* name, const std::string& data);
// Deletes a user file (and a leftover temporary); true if none is left.
bool removeUserFile(const char* name);

// Loads sprite sheet `name` (PC: assets/sheets/<name>.png, 3DS: romfs:/gfx/<name>.t3x).
// Returns nullptr on failure.
Texture* loadSheet(const char* name);
void freeSheet(Texture* tex);

void beginFrame();
void beginScreen(Screen screen, uint32_t clearColor);
void drawRect(float x, float y, float w, float h, uint32_t color);
// A `width`-wide band along the segment (x0, y0)-(x1, y1), ending square at its endpoints (a
// two-point LineRenderer without caps). Colors with alpha < 255 blend, as for drawRect.
void drawLine(float x0, float y0, float x1, float y1, float width, uint32_t color);
// Draws the sw x sh pixel rect at (sx, sy) of `tex` (top-left origin) with its top-left at (x, y).
// Rotation is about the rect's center, so ROT90 suits square sprites (all tiles are 16x16).
// `scale` is a whole-pixel magnification (the drawn size is sw*scale x sh*scale). A non-zero `fill`
// (RGBA) draws the sprite's shape in that solid color instead (a white flash).
void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags = 0, int scale = 1,
                uint32_t fill = 0);
// Draws the sw x sh pixel rect at (sx, sy) of `tex` stretched to w x h at (x, y), point-sampled
// (a 9-slice's edges and middle).
void drawSpriteStretched(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, float w, float h);
void endFrame();
FrameStats lastFrameStats();

// Monotonic time in milliseconds (sub-millisecond resolution), for timing code.
double nowMs();

} // namespace plat
