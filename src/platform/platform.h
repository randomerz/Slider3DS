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
    BTN_UP     = 1u << 8,  // the D-pad (the Artifact's cursor)
    BTN_DOWN   = 1u << 9,
    BTN_LEFT   = 1u << 10,
    BTN_RIGHT  = 1u << 11,
    BTN_CPAD_UP    = 1u << 12, // the Circle Pad (walking)
    BTN_CPAD_DOWN  = 1u << 13,
    BTN_CPAD_LEFT  = 1u << 14,
    BTN_CPAD_RIGHT = 1u << 15,
};
// Either pad: what the menus move by.
constexpr uint32_t DIR_UP = BTN_UP | BTN_CPAD_UP, DIR_DOWN = BTN_DOWN | BTN_CPAD_DOWN;
constexpr uint32_t DIR_LEFT = BTN_LEFT | BTN_CPAD_LEFT, DIR_RIGHT = BTN_RIGHT | BTN_CPAD_RIGHT;

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
// A filled circle of radius `r` centered on (cx, cy). Colors with alpha < 255 blend, as for drawRect.
void drawCircle(float cx, float cy, float r, uint32_t color);
// Draws the sw x sh pixel rect at (sx, sy) of `tex` (top-left origin) with its top-left at (x, y).
// Rotation is about the rect's center, so ROT90 suits square sprites (area loading refuses it on a
// tile that isn't: most tiles are 16x16, a few 16x32).
// `scale` is a whole-pixel magnification (the drawn size is sw*scale x sh*scale). A non-zero `fill`
// (RGBA) draws the sprite's shape in that solid color instead (a white flash).
void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags = 0, int scale = 1,
                uint32_t fill = 0);
// Draws the sw x sh pixel rect at (sx, sy) of `tex` at 1x centered on (cx, cy), turned `angle`
// radians counter-clockwise on screen, point-sampled, its colors multiplied by `tint` (RGBA: alpha
// fades it; white = as is) — textured particles.
void drawSpriteTinted(Texture* tex, int sx, int sy, int sw, int sh, float cx, float cy, float angle, uint32_t tint);
// Draws the sw x sh pixel rect at (sx, sy) of `tex` stretched to w x h at (x, y), point-sampled
// (a 9-slice's edges and middle).
void drawSpriteStretched(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, float w, float h);
void endFrame();
FrameStats lastFrameStats();

// Monotonic time in milliseconds (sub-millisecond resolution), for timing code.
double nowMs();

// ---- Audio ----
// The game mixes (game/audio) and the backend only plays: stereo int16 blocks at AUDIO_RATE, in
// order. audioBlocksWanted() is how many more blocks it wants now: it keeps a short queue (~100 ms)
// filled, so a frame that runs long doesn't starve it. Without an output (no device; a 3DS without
// the DSP firmware) audioAvailable() is false and nothing should play: voices would never end.
constexpr int AUDIO_RATE = 32000;
constexpr int AUDIO_BLOCK = 512; // frames per block (16 ms)
bool audioAvailable();
int audioBlocksWanted();
int audioUnderruns(); // gaps since start: blocks queued after the output had run dry (dev overlay)
void queueAudioBlock(const int16_t* stereo); // AUDIO_BLOCK frames, interleaved L R
// The audio thread: calls `service(user)` every few ms until stopped (it mixes what's wanted, so
// mixing and decoding never hold up a frame). 3DS: a New 3DS's third core, else the game's own
// core at a higher priority than the game (measured on an Old 3DS: the system core, even at an 80%
// share, ran it ~4x slower and the output ran dry); PC: an SDL thread. False (nothing started): no output or no threads (tests): the caller
// services audio itself. lockAudio / unlockAudio guard what the game thread and the audio thread
// share: hold it only briefly (handing over, never while mixing).
bool startAudioThread(void (*service)(void* user), void* user);
int audioThreadCore(); // the core it runs on (-1: none or unknown)
void stopAudioThread();
void lockAudio();
void unlockAudio();

// Ogg Vorbis files among the assets (paths as readFile's), decoded as they're read.
struct Ogg; // opaque, backend-specific
Ogg* openOgg(const char* path); // nullptr if missing or not Ogg Vorbis
void closeOgg(Ogg* ogg);
int oggChannels(const Ogg* ogg);
int oggRate(const Ogg* ogg);
int64_t oggFrames(const Ogg* ogg); // the length, in frames
// Decodes up to `frames` frames (interleaved int16) from the current position; returns how many
// (fewer only at the end, 0 there).
int readOgg(Ogg* ogg, int16_t* out, int frames);
bool seekOgg(Ogg* ogg, int64_t frame); // exact (the next read starts at that frame)

// Asset files read a piece at a time (streams; paths as readFile's).
struct AssetFile; // opaque, backend-specific
AssetFile* openAsset(const char* path); // nullptr if missing
void closeAsset(AssetFile* file);
int64_t assetSize(const AssetFile* file);
// Reads up to `bytes` bytes at `offset`; returns how many (fewer at the end, or on a read error).
int readAsset(AssetFile* file, int64_t offset, void* out, int bytes);

// ms the calling thread spent reading / seeking Ogg and asset files since its last call (the audio
// thread's IO share: dev overlay; 0 where the backend doesn't time it). One thread's: the audio thread's.
double takeFileIoMs();

} // namespace plat
