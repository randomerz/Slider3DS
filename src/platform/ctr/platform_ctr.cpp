// 3DS backend: libctru + citro2d.
#include "platform/platform.h"
#include "platform/ctr/dev_update.h"

#include <3ds.h>
#include <citro2d.h>

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
constexpr size_t MAX_2D_OBJECTS = 8192;

C3D_RenderTarget* g_top = nullptr;
C3D_RenderTarget* g_bottom = nullptr;

u32 toC2D(uint32_t c) {
    return C2D_Color32(c >> 24, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
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
    C2D_Init(MAX_2D_OBJECTS);
    C2D_Prepare();
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    romfsInit();
    return true;
}

void shutdown() {
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

bool readFile(const char* path, std::string& out) {
    std::string full = std::string("romfs:/") + path;
    FILE* f = fopen(full.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    out.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    size_t got = fread(&out[0], 1, out.size(), f);
    fclose(f);
    return got == out.size();
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

void beginFrame() { C3D_FrameBegin(C3D_FRAME_SYNCDRAW); }

void beginScreen(Screen screen, uint32_t clearColor) {
    C3D_RenderTarget* t = screen == Screen::Top ? g_top : g_bottom;
    C2D_TargetClear(t, toC2D(clearColor));
    C2D_SceneBegin(t);
}

void drawRect(float x, float y, float w, float h, uint32_t color) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, toC2D(color));
}

void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags) {
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
    p.pos = {x + sw * 0.5f, y + sh * 0.5f, (flags & FLIP_X) ? -(float)sw : (float)sw,
             (flags & FLIP_Y) ? -(float)sh : (float)sh};
    p.center = {sw * 0.5f, sh * 0.5f};
    p.depth = 0.5f;
    p.angle = angle;
    C2D_DrawImage(img, &p, nullptr);
}

void endFrame() { C3D_FrameEnd(0); }

FrameStats lastFrameStats() { return {C3D_GetProcessingTime(), C3D_GetDrawingTime()}; }

} // namespace plat
