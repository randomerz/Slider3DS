// 3DS backend: libctru + citro2d.
#include "platform/platform.h"
#include "platform/ctr/dev_update.h"

#include <3ds.h>
#include <citro2d.h>

namespace plat {
namespace {

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
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    return true;
}

void shutdown() {
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

void beginFrame() { C3D_FrameBegin(C3D_FRAME_SYNCDRAW); }

void beginScreen(Screen screen, uint32_t clearColor) {
    C3D_RenderTarget* t = screen == Screen::Top ? g_top : g_bottom;
    C2D_TargetClear(t, toC2D(clearColor));
    C2D_SceneBegin(t);
}

void drawRect(float x, float y, float w, float h, uint32_t color) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, toC2D(color));
}

void endFrame() { C3D_FrameEnd(0); }

} // namespace plat
