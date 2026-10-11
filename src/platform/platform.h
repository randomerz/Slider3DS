// Platform layer: the only interface game code uses for rendering, input and lifecycle.
// Implemented by src/platform/ctr (3DS, citro2d) and src/platform/sdl (PC, SDL2).
#pragma once

#include <cstdint>

namespace plat {

enum class Screen { Top, Bottom };

constexpr int TOP_W = 400;
constexpr int TOP_H = 240;
constexpr int BOTTOM_W = 320;
constexpr int BOTTOM_H = 240;

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

bool init();
void shutdown();
bool mainLoop(); // false when the OS/window asks the app to quit

Input pollInput();

void beginFrame();
void beginScreen(Screen screen, uint32_t clearColor);
void drawRect(float x, float y, float w, float h, uint32_t color);
void endFrame();

} // namespace plat
