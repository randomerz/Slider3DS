// Hello-both-screens: D-pad moves a square on the top screen, touch paints on the bottom.
// START quits.
#include "platform/platform.h"

int main(int, char**) {
    if (!plat::init()) return 1;

    float x = plat::TOP_W / 2.0f, y = plat::TOP_H / 2.0f;
    const float size = 16.0f; // one Slider world unit at 16 px/unit

    while (plat::mainLoop()) {
        plat::Input in = plat::pollInput();
        if (in.pressed & plat::BTN_START) break;

        if (in.held & plat::BTN_LEFT)  x -= 2;
        if (in.held & plat::BTN_RIGHT) x += 2;
        if (in.held & plat::BTN_UP)    y -= 2;
        if (in.held & plat::BTN_DOWN)  y += 2;

        plat::beginFrame();

        plat::beginScreen(plat::Screen::Top, plat::rgba(0x3b, 0x7d, 0x4f));
        plat::drawRect(x - size / 2, y - size / 2, size, size, plat::rgba(0xf4, 0xe0, 0x9c));

        plat::beginScreen(plat::Screen::Bottom, plat::rgba(0x2a, 0x2a, 0x3a));
        plat::drawRect(4, 4, 8, 8, plat::rgba(0xff, 0xff, 0xff)); // corner marker: (0,0)
        if (in.touching)
            plat::drawRect(in.touchX - 6, in.touchY - 6, 12, 12, plat::rgba(0xe0, 0x60, 0x60));

        plat::endFrame();
    }

    plat::shutdown();
    return 0;
}
