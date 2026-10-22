// PC backend: SDL2. One window shows both screens stacked like a 3DS:
// top 400x240, bottom 320x240 centered beneath it. Mouse = stylus on the bottom screen.
//
// Keys (Citra/Azahar defaults): arrows = D-pad, X = A, Z = B, S = X, A = Y,
// Q = L, W = R, Enter = START, Backspace = SELECT.
//
// Audio: SDL's queue (SDL_QueueAudio) at AUDIO_RATE, kept AUDIO_QUEUE_BLOCKS ahead, filled by the
// audio thread (an SDL thread); no device (or the screenshot hook below) = silent.
//
// Dev hook: if SLIDER3DS_SCREENSHOT=<file.bmp> is set, the window is saved there after
// SCREENSHOT_FRAME frames and the app quits (lets tools verify rendering headlessly), without audio.
#include "platform/platform.h"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifndef ASSET_ROOT
#define ASSET_ROOT "assets"
#endif

namespace plat {

struct Texture {
    SDL_Texture* tex;
    SDL_Texture* white; // the same shapes in solid white (drawSprite's fill tints it)
};

namespace {

constexpr int SCREENSHOT_FRAME = 30;
int g_frame = 0;
Uint64 g_frameStart = 0;
FrameStats g_stats;

constexpr int WINDOW_SCALE = 2;
constexpr int LOGICAL_W = TOP_W;
constexpr int LOGICAL_H = TOP_H + BOTTOM_H;
constexpr int BOTTOM_X = (TOP_W - BOTTOM_W) / 2;
constexpr int BOTTOM_Y = TOP_H;

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_AudioDeviceID g_audio = 0;
constexpr int AUDIO_QUEUE_BLOCKS = 6; // ~96 ms
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
    if (k[SDL_SCANCODE_T])         b |= BTN_UP; // (Citra's defaults: T/G/F/H = D-pad, arrows = Circle Pad)
    if (k[SDL_SCANCODE_G])         b |= BTN_DOWN;
    if (k[SDL_SCANCODE_F])         b |= BTN_LEFT;
    if (k[SDL_SCANCODE_H])         b |= BTN_RIGHT;
    if (k[SDL_SCANCODE_UP])        b |= BTN_CPAD_UP;
    if (k[SDL_SCANCODE_DOWN])      b |= BTN_CPAD_DOWN;
    if (k[SDL_SCANCODE_LEFT])      b |= BTN_CPAD_LEFT;
    if (k[SDL_SCANCODE_RIGHT])     b |= BTN_CPAD_RIGHT;
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
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED); // paced in beginFrame()
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(g_renderer, LOGICAL_W, LOGICAL_H);
    SDL_RenderSetIntegerScale(g_renderer, SDL_TRUE);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND); // rects and lines with alpha blend (as citro2d's)
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
        return false;
    }
    if (!std::getenv("SLIDER3DS_SCREENSHOT")) {
        SDL_AudioSpec want{};
        want.freq = AUDIO_RATE;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = AUDIO_BLOCK;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0 || !(g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0)))
            SDL_Log("No audio: %s", SDL_GetError()); // (plays on silently)
        else
            SDL_PauseAudioDevice(g_audio, 0);
    }
    return true;
}

void shutdown() {
    stopAudioThread();
    if (g_audio) SDL_CloseAudioDevice(g_audio);
    IMG_Quit();
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

namespace {

// SDL's pref path for the player's files (created on first use), "" if unavailable.
const std::string& userDir() {
    static const std::string dir = [] {
        char* p = SDL_GetPrefPath("boomo", "Slider3DS");
        std::string s = p ? p : "";
        SDL_free(p);
        return s;
    }();
    return dir;
}

bool readWhole(const std::string& path, std::string& out) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) return false;
    out.assign(static_cast<const char*>(data), size);
    SDL_free(data);
    return true;
}

} // namespace

bool readUserFile(const char* name, std::string& out) {
    if (userDir().empty()) return false;
    const std::string path = userDir() + name;
    return readWhole(path, out) || readWhole(path + ".tmp", out); // (a write cut off before its rename)
}

bool writeUserFile(const char* name, const std::string& data) {
    if (userDir().empty()) return false;
    const std::string path = userDir() + name, tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool written = fwrite(data.data(), 1, data.size(), f) == data.size();
    if (fclose(f) != 0 || !written) {
        remove(tmp.c_str());
        return false;
    }
    remove(path.c_str()); // (Windows' rename won't replace a file)
    return rename(tmp.c_str(), path.c_str()) == 0;
}

bool removeUserFile(const char* name) {
    if (userDir().empty()) return false;
    const std::string path = userDir() + name;
    remove((path + ".tmp").c_str());
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return true;
    fclose(f);
    return remove(path.c_str()) == 0;
}

bool readFile(const char* path, std::string& out) {
    std::string full = std::string(ASSET_ROOT) + "/" + path;
    size_t size = 0;
    void* data = SDL_LoadFile(full.c_str(), &size);
    if (!data) {
        SDL_Log("readFile(%s) failed: %s", full.c_str(), SDL_GetError());
        return false;
    }
    out.assign(static_cast<const char*>(data), size);
    SDL_free(data);
    return true;
}

Texture* loadSheet(const char* name) {
    std::string path = std::string(ASSET_ROOT) + "/sheets/" + name + ".png";
    SDL_Surface* loaded = IMG_Load(path.c_str());
    SDL_Surface* surface = loaded ? SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0) : nullptr;
    SDL_FreeSurface(loaded);
    if (!surface) {
        SDL_Log("loadSheet(%s) failed: %s", path.c_str(), IMG_GetError());
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_renderer, surface);
    // White silhouette: every pixel's color white, its alpha kept.
    SDL_LockSurface(surface);
    for (int y = 0; y < surface->h; y++) {
        Uint8* row = (Uint8*)surface->pixels + y * surface->pitch;
        for (int x = 0; x < surface->w; x++) row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = 255;
    }
    SDL_UnlockSurface(surface);
    SDL_Texture* white = SDL_CreateTextureFromSurface(g_renderer, surface);
    SDL_FreeSurface(surface);
    if (!tex || !white) {
        SDL_Log("loadSheet(%s) failed: %s", path.c_str(), SDL_GetError());
        SDL_DestroyTexture(tex);
        SDL_DestroyTexture(white);
        return nullptr;
    }
    for (SDL_Texture* t : {tex, white}) {
        SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return new Texture{tex, white};
}

void freeSheet(Texture* tex) {
    if (!tex) return;
    SDL_DestroyTexture(tex->tex);
    SDL_DestroyTexture(tex->white);
    delete tex;
}

// The game steps a fixed 1/60 s per frame (like the 3DS's 60 Hz vblank), so hold the PC loop to
// 60 Hz whatever the monitor's refresh rate: sleep, then spin the last ~2 ms, toward an absolute
// schedule. If we fall far behind (breakpoint, window drag), resync instead of fast-forwarding.
static void paceFrame() {
    static Uint64 next = 0;
    const Uint64 freq = SDL_GetPerformanceFrequency(), period = freq / 60;
    Uint64 now = SDL_GetPerformanceCounter();
    if (next == 0 || now > next + 4 * period) next = now;
    next += period;
    while ((now = SDL_GetPerformanceCounter()) < next) {
        Uint64 msLeft = (next - now) * 1000 / freq;
        if (msLeft > 2) SDL_Delay((Uint32)(msLeft - 2));
    }
}

void beginFrame() {
    paceFrame(); // (here, not in endFrame: endFrame is timed by the dev profiler)
    const Uint64 now = SDL_GetPerformanceCounter();
    if (g_frameStart) g_stats.frameMs = (float)((now - g_frameStart) * 1000.0 / SDL_GetPerformanceFrequency());
    g_frameStart = now;
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

void drawLine(float x0, float y0, float x1, float y1, float width, uint32_t color) {
    const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0) return;
    const float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f; // half-width across
    const SDL_Color c{(Uint8)(color >> 24), (Uint8)(color >> 16), (Uint8)(color >> 8), (Uint8)color};
    const SDL_Vertex v[4] = {{{x0 + nx, y0 + ny}, c, {0, 0}}, {{x1 + nx, y1 + ny}, c, {0, 0}},
                             {{x1 - nx, y1 - ny}, c, {0, 0}}, {{x0 - nx, y0 - ny}, c, {0, 0}}};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(g_renderer, nullptr, v, 4, idx, 6);
}

void drawCircle(float cx, float cy, float r, uint32_t color) {
    if (r <= 0) return;
    // A fan of triangles from the center; enough segments that the edge stays round at any size.
    const int n = std::min(std::max((int)(r * 0.75f), 16), 256);
    const SDL_Color c{(Uint8)(color >> 24), (Uint8)(color >> 16), (Uint8)(color >> 8), (Uint8)color};
    std::vector<SDL_Vertex> v(n + 1);
    std::vector<int> idx(n * 3);
    v[0] = {{cx, cy}, c, {0, 0}};
    for (int i = 0; i < n; i++) {
        const float a = 6.2831853f * i / n;
        v[i + 1] = {{cx + r * std::cos(a), cy + r * std::sin(a)}, c, {0, 0}};
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = i + 1 < n ? i + 2 : 1;
    }
    SDL_RenderGeometry(g_renderer, nullptr, v.data(), n + 1, idx.data(), n * 3);
}

void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags, int scale, uint32_t fill) {
    // SDL flips in texture space before rotating, so a flip that follows ROT90 is a rotation the
    // other way: F*R(a) = R(-a)*F. SDL angles are clockwise on screen.
    int flip = ((flags & FLIP_X) ? SDL_FLIP_HORIZONTAL : 0) | ((flags & FLIP_Y) ? SDL_FLIP_VERTICAL : 0);
    double angle = 0.0;
    if (flags & ROT90) {
        bool mirrored = ((flags & FLIP_X) != 0) != ((flags & FLIP_Y) != 0);
        angle = mirrored ? 90.0 : -90.0;
    }
    SDL_Rect src{sx, sy, sw, sh};
    SDL_FRect dst{x, y, (float)(sw * scale), (float)(sh * scale)};
    SDL_Texture* t = tex->tex;
    if (fill) {
        t = tex->white;
        SDL_SetTextureColorMod(t, (Uint8)(fill >> 24), (Uint8)(fill >> 16), (Uint8)(fill >> 8));
        SDL_SetTextureAlphaMod(t, (Uint8)fill);
    }
    SDL_RenderCopyExF(g_renderer, t, &src, &dst, angle, nullptr, (SDL_RendererFlip)flip);
}

void drawSpriteStretched(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, float w, float h) {
    SDL_Rect src{sx, sy, sw, sh};
    SDL_FRect dst{x, y, w, h};
    SDL_RenderCopyF(g_renderer, tex->tex, &src, &dst);
}

FrameStats lastFrameStats() { return g_stats; }

double nowMs() { return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }

void endFrame() {
    const char* shot = std::getenv("SLIDER3DS_SCREENSHOT");
    if (shot && ++g_frame == SCREENSHOT_FRAME) {
        SDL_RenderSetViewport(g_renderer, nullptr);
        int w, h;
        SDL_GetRendererOutputSize(g_renderer, &w, &h);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(g_renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        SDL_SaveBMP(s, shot);
        SDL_FreeSurface(s);
        g_quit = true;
    }
    SDL_RenderPresent(g_renderer);
}

bool audioAvailable() { return g_audio != 0; }

namespace {

SDL_Thread* g_audioThread = nullptr;
SDL_mutex* g_audioLock = nullptr;
SDL_atomic_t g_audioRunning;
void (*g_audioService)(void*) = nullptr;
void* g_audioUser = nullptr;

int audioThread(void*) {
    while (SDL_AtomicGet(&g_audioRunning)) {
        g_audioService(g_audioUser);
        SDL_Delay(4);
    }
    return 0;
}

} // namespace

bool startAudioThread(void (*service)(void*), void* user) {
    if (!g_audio || g_audioThread) return false;
    if (!g_audioLock && !(g_audioLock = SDL_CreateMutex())) return false;
    g_audioService = service;
    g_audioUser = user;
    SDL_AtomicSet(&g_audioRunning, 1);
    g_audioThread = SDL_CreateThread(audioThread, "audio", nullptr);
    return g_audioThread != nullptr;
}

int audioThreadCore() { return -1; }

void stopAudioThread() {
    if (!g_audioThread) return;
    SDL_AtomicSet(&g_audioRunning, 0);
    SDL_WaitThread(g_audioThread, nullptr);
    g_audioThread = nullptr;
}

void lockAudio() {
    if (g_audioLock) SDL_LockMutex(g_audioLock);
}

void unlockAudio() {
    if (g_audioLock) SDL_UnlockMutex(g_audioLock);
}

namespace {
bool g_audioStarted = false;
SDL_atomic_t g_audioUnderruns;
} // namespace

int audioUnderruns() { return SDL_AtomicGet(&g_audioUnderruns); }

int audioBlocksWanted() {
    if (!g_audio) return 0;
    const Uint32 bytes = SDL_GetQueuedAudioSize(g_audio);
    const int queued = (int)(bytes / (AUDIO_BLOCK * 2 * sizeof(int16_t)));
    return queued < AUDIO_QUEUE_BLOCKS ? AUDIO_QUEUE_BLOCKS - queued : 0;
}

void queueAudioBlock(const int16_t* stereo) {
    if (!g_audio) return;
    if (g_audioStarted && SDL_GetQueuedAudioSize(g_audio) == 0) SDL_AtomicIncRef(&g_audioUnderruns); // (ran dry: a gap)
    SDL_QueueAudio(g_audio, stereo, AUDIO_BLOCK * 2 * sizeof(int16_t));
    g_audioStarted = true;
}

} // namespace plat
