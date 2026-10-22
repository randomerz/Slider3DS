// 3DS backend: libctru + citro2d (audio: audio_ctr.cpp).
#include "platform/platform.h"
#include "platform/ctr/audio_ctr.h"
#include "platform/ctr/dev_update.h"

#include <3ds.h>
#include <citro2d.h>

#include <sys/stat.h>

#include <cmath>
#include <cstdio>

namespace plat {

struct Texture {
    C2D_SpriteSheet sheet;
    C2D_Image image; // the whole sheet
    float invW, invH;
};

namespace {

// Each sprite is one citro2d object; a screenful of tile layers is a few thousand.

C3D_RenderTarget* g_top = nullptr;
C3D_RenderTarget* g_bottom = nullptr;

u32 toC2D(uint32_t c) {
    return C2D_Color32(c >> 24, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
}

constexpr const char* USER_DIR = "sdmc:/3ds/slider3ds/";

bool readWhole(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    out.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    size_t got = fread(&out[0], 1, out.size(), f);
    fclose(f);
    return got == out.size();
}

uint32_t mapKeys(u32 k) {
    uint32_t b = 0;
    if (k & KEY_A)      b |= BTN_A;
    if (k & KEY_B)      b |= BTN_B;
    if (k & KEY_X)      b |= BTN_X;
    if (k & KEY_Y)      b |= BTN_Y;
    if (k & KEY_L)      b |= BTN_L;
    if (k & KEY_R)      b |= BTN_R;
    if (k & KEY_START)  b |= BTN_START;
    if (k & KEY_SELECT) b |= BTN_SELECT;
    if (k & KEY_UP)     b |= BTN_UP;    // KEY_UP = D-pad | circle pad
    if (k & KEY_DOWN)   b |= BTN_DOWN;
    if (k & KEY_LEFT)   b |= BTN_LEFT;
    if (k & KEY_RIGHT)  b |= BTN_RIGHT;
    return b;
}

} // namespace

bool init() {
#ifdef DEV_UPDATE_URL
    hidScanInput();
    if (hidKeysHeld() & KEY_SELECT) {
        runDevUpdate();
        return false;
    }
#endif
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(MAX_DRAWS_PER_FRAME);
    C2D_Prepare();
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    romfsInit();
    initAudio();
    return true;
}

void shutdown() {
    exitAudio();
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

bool mainLoop() { return aptMainLoop(); }

Input pollInput() {
    hidScanInput();
    Input in;
    in.held = mapKeys(hidKeysHeld());
    in.pressed = mapKeys(hidKeysDown());
    if (hidKeysHeld() & KEY_TOUCH) {
        touchPosition t;
        hidTouchRead(&t);
        in.touching = true;
        in.touchX = t.px;
        in.touchY = t.py;
    }
    return in;
}

bool readFile(const char* path, std::string& out) { return readWhole(std::string("romfs:/") + path, out); }

bool readUserFile(const char* name, std::string& out) {
    const std::string path = std::string(USER_DIR) + name;
    return readWhole(path, out) || readWhole(path + ".tmp", out); // (a write cut off before its rename)
}

bool writeUserFile(const char* name, const std::string& data) {
    mkdir("sdmc:/3ds", 0777); // (usually there already)
    mkdir("sdmc:/3ds/slider3ds", 0777);
    const std::string path = std::string(USER_DIR) + name, tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool written = fwrite(data.data(), 1, data.size(), f) == data.size();
    if (fclose(f) != 0 || !written) {
        remove(tmp.c_str());
        return false;
    }
    remove(path.c_str()); // (FAT's rename won't replace a file)
    return rename(tmp.c_str(), path.c_str()) == 0;
}

bool removeUserFile(const char* name) {
    const std::string path = std::string(USER_DIR) + name;
    remove((path + ".tmp").c_str());
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return true;
    fclose(f);
    return remove(path.c_str()) == 0;
}

Texture* loadSheet(const char* name) {
    std::string path = std::string("romfs:/gfx/") + name + ".t3x";
    C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(path.c_str());
    if (!sheet) return nullptr;
    Texture* t = new Texture;
    t->sheet = sheet;
    t->image = C2D_SpriteSheetGetImage(sheet, 0);
    C3D_TexSetFilter(t->image.tex, GPU_NEAREST, GPU_NEAREST);
    t->invW = 1.0f / t->image.tex->width;
    t->invH = 1.0f / t->image.tex->height;
    return t;
}

void freeSheet(Texture* tex) {
    if (!tex) return;
    C2D_SpriteSheetFree(tex->sheet);
    delete tex;
}

static u64 g_lastFrameTick = 0;
static float g_frameMs = 0;

void beginFrame() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    const u64 now = svcGetSystemTick();
    if (g_lastFrameTick) g_frameMs = (float)((now - g_lastFrameTick) * 1000.0 / SYSCLOCK_ARM11);
    g_lastFrameTick = now;
}

void beginScreen(Screen screen, uint32_t clearColor) {
    C3D_RenderTarget* t = screen == Screen::Top ? g_top : g_bottom;
    C2D_TargetClear(t, toC2D(clearColor));
    C2D_SceneBegin(t);
}

void drawRect(float x, float y, float w, float h, uint32_t color) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, toC2D(color));
}

void drawLine(float x0, float y0, float x1, float y1, float width, uint32_t color) {
    const u32 c = toC2D(color);
    C2D_DrawLine(x0, y0, c, x1, y1, c, width, 0.5f);
}

void drawCircle(float cx, float cy, float r, uint32_t color) { C2D_DrawCircleSolid(cx, cy, 0.5f, r, toC2D(color)); }

void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags, int scale, uint32_t fill) {
    // Texture v runs bottom-up (top = 1 - y/H). Don't flip by swapping UVs: citro2d reads a
    // subtexture with top < bottom as a rotated atlas entry. A negative draw size flips instead,
    // in texture space before the rotation, so a flip that follows ROT90 is a rotation the
    // other way: F*R(a) = R(-a)*F.
    Tex3DS_SubTexture sub = {(u16)sw, (u16)sh, sx * tex->invW, 1.0f - sy * tex->invH,
                             (sx + sw) * tex->invW, 1.0f - (sy + sh) * tex->invH};
    C2D_Image img = {tex->image.tex, &sub};

    float angle = 0.0f; // citro2d: positive = clockwise on screen
    if (flags & ROT90) {
        bool mirrored = ((flags & FLIP_X) != 0) != ((flags & FLIP_Y) != 0);
        angle = mirrored ? (float)M_PI_2 : -(float)M_PI_2;
    }
    C2D_DrawParams p = {};
    const float w = (float)(sw * scale), h = (float)(sh * scale);
    p.pos = {x + w * 0.5f, y + h * 0.5f, (flags & FLIP_X) ? -w : w, (flags & FLIP_Y) ? -h : h};
    p.center = {w * 0.5f, h * 0.5f};
    p.depth = 0.5f;
    p.angle = angle;
    if (fill) { // the texture's alpha, the fill's color
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(fill >> 24, (fill >> 16) & 0xff, (fill >> 8) & 0xff, fill & 0xff), 1.0f);
        C2D_DrawImage(img, &p, &tint);
        return;
    }
    C2D_DrawImage(img, &p, nullptr);
}

void drawSpriteStretched(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, float w, float h) {
    Tex3DS_SubTexture sub = {(u16)sw, (u16)sh, sx * tex->invW, 1.0f - sy * tex->invH,
                             (sx + sw) * tex->invW, 1.0f - (sy + sh) * tex->invH};
    C2D_Image img = {tex->image.tex, &sub};
    C2D_DrawParams p = {};
    p.pos = {x, y, w, h};
    p.depth = 0.5f;
    C2D_DrawImage(img, &p, nullptr);
}

void endFrame() { C3D_FrameEnd(0); }

double nowMs() { return svcGetSystemTick() * 1000.0 / SYSCLOCK_ARM11; }

FrameStats lastFrameStats() { return {C3D_GetDrawingTime(), g_frameMs}; }

} // namespace plat
