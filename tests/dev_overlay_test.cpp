// The dev overlay (game/dev/dev_overlay): both pages draw every character they use and stay on the
// bottom screen, with numbers as wide as they get.
#include <string>

#include "game/dev/debug_text.h"
#include "game/dev/dev_overlay.h"
#include "test.h"
#include "test_platform.h"

TEST(dev_overlay_pages_fit_the_bottom_screen_and_the_font) {
    // A frame with every section at a long time (worst-case widths: "WAI 99.9").
    prof::setEnabled(true);
    testSetNowMs(1000);
    { const ProfScope prime(ProfSection::Logic); }
    prof::beginFrame();
    double t = 1000;
    for (int s = 0; s < (int)ProfSection::Count; s++) {
        const ProfScope scope((ProfSection)s);
        t += 99.9;
        testSetNowMs(t);
    }
    Audio::ThreadLoad load;
    load.running = true;
    load.core = -1;
    load.total = load.streams = load.clips = load.io = 1;
    load.underruns = 99999;
    load.bgmS = load.bgmLoopS = 599;
    DevOverlay overlay;
    overlay.setBoot(99.9f, 9.9f, 9.9f, 9.9f, 9.9f, 9.9f);
    for (int i = 0; i < 60; i++) overlay.record({99.9f, 16.7f}, 9999, 9999, load);
    prof::setEnabled(false);
    testSetNowMs(0);

    for (int page = 0; page < DevOverlay::PAGES; page++) {
        const int missing = debugTextMissing();
        testResetDrawCalls();
        overlay.draw((DevOverlay::Page)page);
        check(debugTextMissing() == missing, "page " + std::to_string(page + 1) + " draws every character");
        bool inside = !testDrawCalls().empty();
        for (const DrawCall& d : testDrawCalls())
            inside = inside && d.x >= 0 && d.y >= 0 && d.x + d.w <= plat::BOTTOM_W && d.y + d.h <= plat::BOTTOM_H;
        check(inside, "page " + std::to_string(page + 1) + " stays on the bottom screen");
    }
    // (and a character the font lacks is counted)
    const int missing = debugTextMissing();
    drawDebugText(0, 0, "Q", 0);
    CHECK(debugTextMissing() == missing + 1);
}
