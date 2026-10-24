// The new game's intro (game/ui/intro_cutscene): CutsceneManager's timeline per image, what a press
// cuts short, and its assets.
#include <cmath>
#include <string>

#include "game/ui/intro_cutscene.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

void run(IntroCutscene& intro, int frames, bool press = false) {
    for (int n = 0; n < frames && !intro.done(); n++) intro.update(press, DT);
}

// Frames until `done` holds (at most `limit`).
template <typename F>
int until(IntroCutscene& intro, F done, int limit = 600) {
    int n = 0;
    while (!done() && n < limit) intro.update(false, DT), n++;
    return n;
}

} // namespace

TEST(intro_art_loads_seven_slides) {
    IntroArt art;
    std::string error;
    check(art.load(&error), "intro art loads " + error);
    CHECK(art.slides().size() == 7 && art.textW() == 300 && art.imageY() == 20 && art.textBottom() == 10);
    CHECK(art.slides()[0].text == "In ages past, man held the power to remake the world.");
    CHECK(art.slides()[6].w == 160 && art.slides()[6].h == 120 && art.slides()[2].text.find('\n') != std::string::npos);
}

TEST(intro_cutscene_plays_each_image_then_ends) {
    // Untouched: the image fades in over 1 s, the caption types from 0.2 s (a character every
    // 0.06 s), holds 3 s once typed, fades out over 1 s; then the next.
    IntroArt art;
    check(art.load(), "intro art");
    IntroCutscene intro;
    intro.start(art);
    CHECK(intro.slide() == 0 && intro.alpha() == 0 && intro.shownChars() == 0);
    run(intro, 30);
    CHECK(std::fabs(intro.alpha() - 0.5f) < 0.02f && intro.shownChars() > 0); // (typing since 0.2 s)
    const int length = (int)art.slides()[0].text.size();
    const int typed = 30 + until(intro, [&] { return intro.shownChars() >= length; });
    const int expected = (int)std::ceil((IntroCutscene::TYPE_DELAY + (length - 1) * IntroCutscene::CHAR_TIME) / DT);
    check(std::abs(typed - expected) <= 1, "typed at frame " + std::to_string(typed) + ", expected " + std::to_string(expected));
    const int held = until(intro, [&] { return intro.alpha() < 1; });
    check(std::abs(held - 180) <= 2, "held " + std::to_string(held) + " frames");
    const int fading = until(intro, [&] { return intro.slide() != 0; });
    check(std::abs(fading - 60) <= 2, "faded out over " + std::to_string(fading) + " frames");
    CHECK(intro.slide() == 1 && intro.alpha() == 0 && intro.shownChars() == 0);
    run(intro, 60 * 60); // (all seven)
    CHECK(intro.done() && intro.update(false, DT));
}

TEST(intro_cutscene_a_press_cuts_each_step_short) {
    IntroArt art;
    check(art.load(), "intro art");
    const int length = (int)art.slides()[0].text.size();
    // During the fade in: shown at once, and the caption whole as soon as it starts.
    IntroCutscene a;
    a.start(art);
    run(a, 3);
    a.update(true, DT);
    CHECK(a.alpha() == 1);
    run(a, 12); // (past 0.2 s)
    CHECK(a.shownChars() == length);
    // Typing: the caption whole; holding: the fade out, twice as fast.
    IntroCutscene b;
    b.start(art);
    run(b, 15);
    b.update(true, DT);
    CHECK(b.shownChars() == length);
    b.update(true, DT);
    const int fast = until(b, [&] { return b.slide() != 0; });
    check(std::abs(fast - 30) <= 2, "the fast fade took " + std::to_string(fast) + " frames");
    // While fading out: the next image at once.
    IntroCutscene c;
    c.start(art);
    run(c, 15);
    c.update(true, DT);
    c.update(true, DT);
    run(c, 5);
    CHECK(c.slide() == 0 && c.alpha() < 1);
    c.update(true, DT);
    CHECK(c.slide() == 1);
}
