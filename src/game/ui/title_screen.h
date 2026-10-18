// The title screen's top screen: MainMenu.unity's title art (exported by the Unity exporter's
// Slider3DSTitleExporter to assets/ui/title.json, its sprites cut by tools/make_ui.py), at 1x like
// the world. MainMenuManager.Start / OpenCutscene, step by step:
//   the stars twinkle (MainMenuBGStars: each on for 25-50 s, then off for 0.5-1 s), the giant stars
//   and the meteor loop, all hidden (ParalaxBG's CanvasGroup at alpha 0: the empty background);
//   each letter of the logo bobs on its own SinWaveAnimation; the player idles;
//   0-3 s: the logo slides in from the left (TitleSlideIn); 1 s: the camera shake starts rising
//   (ShakeIncrease 2.1 s to 0.1); 3 s: a shake (0.25 s, 0.2); 4 s: "[press any key to begin]"
//   fades in over 2 s.
// From the second frame (the listener's), any button or touch starts the main menu (StartMainMenu):
// the shakes stop, the stars and images fade in (mainMenuBackgroundAnimator's isVisible), the logo
// and the player rise (TitleRiseUp / MainMenuPlayerUp), the prompt fades out (if shown) and the
// bottom screen's menu (game/ui/title_menu) slides up from below (MainMenuButtonsUp: menuDrop).
// Not ported: the parallax with the mouse (no pointer on the top screen), the language selector.
// A port addition: the stars and images are spread from the original's 320x180 view over the
// 400x240 top screen, with extra stars in the gaps; the logo sits lower (logoDown) and the player
// stays centered (playerRises false: the original's room for its buttons, here on the bottom
// screen) (assets/ui/title_extra.json, tools/make_title_extra.py: it applies the same layout to
// keep its stars clear).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/curve.h"
#include "game/fx/screen_effects.h"
#include "platform/platform.h"

class Font;

// The loaded title art (read-only, like Area). Positions in px, y up, from the screen's center.
struct TitleArt {
    struct Sprite {
        int x = 0, y = 0, w = 0, h = 0; // on the sheet (top-left origin)
        float pivotX = 0, pivotY = 0;   // px from its bottom-left
    };
    struct Star {
        float x, y;
        int on, off; // sprites (SpriteSwapper)
    };
    struct Image {
        float x, y;
        std::vector<int> frames; // one: a still image
        float frameTime = 0;
    };
    struct Letter {
        int sprite;
        float a, b, c, d; // SinWaveAnimation: y offset a sin(b t + c) + d (px)
    };

    plat::Texture* sheet = nullptr;
    std::vector<Sprite> sprites;
    uint32_t background = 0;
    std::vector<Star> stars;
    std::vector<Image> images;
    std::vector<CurveKey> backgroundFade; // the stars' and images' alpha from StartMainMenu on (0 before)
    std::vector<CurveKey> menuSlide;      // the bottom menu's y from StartMainMenu on, screen heights up
    struct {
        float originX = 0, originY = 0, restY = 0;
        std::vector<Letter> letters;
        std::vector<CurveKey> slideIn, riseX, riseY; // anchored x / y over time
        float slideInLength = 0;
    } logo;
    struct {
        Image image;
        std::vector<CurveKey> riseY;
        bool rises = true; // (title_extra.json's playerRises)
    } player;
    struct {
        std::string text;
        float x = 0, y = 0;
        uint32_t color = 0;
        std::vector<CurveKey> fade; // AnyKeyFadeIn's alpha over time
        float fadeOutSpeed = 1;     // AnyKeyFadeOut: that curve backwards from its end, this many times as fast
    } prompt;

    TitleArt() = default;
    TitleArt(const TitleArt&) = delete;
    TitleArt& operator=(const TitleArt&) = delete;
    ~TitleArt() { unload(); }
    bool load(std::string* error = nullptr); // ui/title.json + ui/title_sheet.json + its sheet
    void unload();
};

class TitleScreen {
public:
    // StartMainMenu's LockoutSelectablesInCurrentMenu: the menu ignores input this long, s. TitleArt
    // won't load a menuSlide longer than it: the menu's hit boxes stay where it comes to rest.
    static constexpr float MENU_LOCKOUT = 1;

    // The scene loads (MainMenuManager.Start): the intro from its start.
    void start(const TitleArt& art);
    // One frame (the game steps a fixed 60 fps): `in` is the frame's input (any button or touch
    // starts the menu), `noise` the camera shake's (MainMenu's camera has the Player's CM 2D Shake:
    // the area's), `screenShake` the setting. True on the frame the menu starts (that input is used up).
    bool update(const CameraShakeDef& noise, const plat::Input& in, float dt, float screenShake);
    bool menuShown() const { return upAt_ >= 0; } // StartMainMenu has run
    void draw(const TitleArt& art, const Font& font) const;

    float time() const { return time_; } // s since start
    float promptAlpha(const TitleArt& art) const; // "[press any key to begin]"'s, 0-1
    float backgroundAlpha(const TitleArt& art) const; // the stars' and images', 0-1
    // How far below its place the bottom menu is, screen heights: 1 before StartMainMenu
    // (MainMenuButtonsHidden), then menuSlide's keys.
    float menuDrop(const TitleArt& art) const;

private:
    struct StarState {
        bool enabled = false, showsOn = true; // (the authored sprite is the "on" one)
        float timeTillSwap = 0;
    };
    float random(float lo, float hi); // Random.Range

    float time_ = 0;
    float upAt_ = -1;      // when StartMainMenu ran, -1 = not yet
    bool cutscene_ = true; // OpenCutscene still running (StopAllCoroutines ends it)
    int step_ = 0;         // OpenCutscene's next wait: 0 the 1 s, 1 the 2 s, 2 the 1 s, 3 done
    float promptShownAt_ = -1, promptHiddenAt_ = -1;
    std::vector<StarState> stars_;
    ScreenEffects shake_;
    uint32_t seed_ = 1;
};
