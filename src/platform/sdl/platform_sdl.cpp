// PC backend: SDL2. One window shows both screens stacked like a 3DS:
// top 400x240, bottom 320x240 centered beneath it. Mouse = stylus on the bottom screen.
//
// Keys (Citra/Azahar defaults): arrows = D-pad, X = A, Z = B, S = X, A = Y,
// Q = L, W = R, Enter = START, Backspace = SELECT.
#include "platform/platform.h"

#include <SDL.h>

namespace plat {
namespace {

constexpr int WINDOW_SCALE = 2;
constexpr int LOGICAL_W = TOP_W;
constexpr int LOGICAL_H = TOP_H + BOTTOM_H;
constexpr int BOTTOM_X = (TOP_W - BOTTOM_W) / 2;
constexpr int BOTTOM_Y = TOP_H;

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
bool g_quit = false;
uint32_t g_prevHeld = 0;

uint32_t readKeys() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    uint32_t b = 0;
    if (k[SDL_SCANCODE_X])         b |= BTN_A;
    if (k[SDL_SCANCODE_Z])         b |= BTN_B;
    if (k[SDL_SCANCODE_S])         b |= BTN_X;
    if (k[SDL_SCANCODE_A])         b |= BTN_Y;
    if (k[SDL_SCANCODE_Q])         b |= BTN_L;
    if (k[SDL_SCANCODE_W])         b |= BTN_R;
    if (k[SDL_SCANCODE_RETURN])    b |= BTN_START;
    if (k[SDL_SCANCODE_BACKSPACE]) b |= BTN_SELECT;
    if (k[SDL_SCANCODE_UP])        b |= BTN_UP;
    if (k[SDL_SCANCODE_DOWN])      b |= BTN_DOWN;
    if (k[SDL_SCANCODE_LEFT])      b |= BTN_LEFT;
    if (k[SDL_SCANCODE_RIGHT])     b |= BTN_RIGHT;
    return b;
}

void setColor(uint32_t c) {
    SDL_SetRenderDrawColor(g_renderer, c >> 24, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
}

} // namespace

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    g_window = SDL_CreateWindow("Slider3DS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                LOGICAL_W * WINDOW_SCALE, LOGICAL_H * WINDOW_SCALE,
                                SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(g_renderer, LOGICAL_W, LOGICAL_H);
    SDL_RenderSetIntegerScale(g_renderer, SDL_TRUE);
    return true;
}

void shutdown() {
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}

bool mainLoop() {
    SDL_Event e;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_QUIT) g_quit = true;
    return !g_quit;
}

Input pollInput() {
    Input in;
    in.held = readKeys();
    in.pressed = in.held & ~g_prevHeld;
    g_prevHeld = in.held;

    // Mouse coords arrive in logical (400x480) space thanks to SDL_RenderSetLogicalSize.
    int mx, my;
    if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        float lx, ly;
        SDL_RenderWindowToLogical(g_renderer, mx, my, &lx, &ly);
        int tx = int(lx) - BOTTOM_X, ty = int(ly) - BOTTOM_Y;
        if (tx >= 0 && tx < BOTTOM_W && ty >= 0 && ty < BOTTOM_H) {
            in.touching = true;
            in.touchX = tx;
            in.touchY = ty;
        }
    }
    return in;
}

void beginFrame() {
    SDL_RenderSetViewport(g_renderer, nullptr);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
}

void beginScreen(Screen screen, uint32_t clearColor) {
    SDL_Rect vp = screen == Screen::Top ? SDL_Rect{0, 0, TOP_W, TOP_H}
                                        : SDL_Rect{BOTTOM_X, BOTTOM_Y, BOTTOM_W, BOTTOM_H};
    SDL_RenderSetViewport(g_renderer, &vp);
    setColor(clearColor);
    SDL_Rect full{0, 0, vp.w, vp.h};
    SDL_RenderFillRect(g_renderer, &full);
}

void drawRect(float x, float y, float w, float h, uint32_t color) {
    setColor(color);
    SDL_FRect r{x, y, w, h};
    SDL_RenderFillRectF(g_renderer, &r);
}

void endFrame() { SDL_RenderPresent(g_renderer); }

} // namespace plat
