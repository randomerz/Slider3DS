// The new game's intro (the original's IntroCutscene scene: CutsceneManager), on the top screen at 1x:
// seven images, each with its caption typed below it (assets/ui/intro.json, by tools/make_ui.py). Per
// image the canvas fades in over FADE_TIME; TYPE_DELAY in, the caption types a character every
// CHAR_TIME; typed, it holds HOLD_TIME, then fades out over FADE_TIME; after the last one, the game
// starts (the Village). Any key or a touch (AdvanceCutscene) cuts the current step short: the fade
// in, the typing (it shows whole), the hold, then fades out twice as fast; pressed during the fade
// out, the next image comes at once. The background is the camera's #1a1a1a; the bottom screen
// stays dark (the original has one screen). The Frontend plays it (game/app/frontend).
#pragma once

#include <string>
#include <vector>

#include "game/render/font.h"
#include "platform/platform.h"

// The intro's images and captions (assets/ui/intro.json + its sheet), loaded only while it plays.
class IntroArt {
public:
    struct Slide {
        int x = 0, y = 0, w = 0, h = 0; // its image in the sheet
        std::string text;               // its caption ("\n": the original's line breaks)
    };

    IntroArt() = default;
    IntroArt(const IntroArt&) = delete;
    IntroArt& operator=(const IntroArt&) = delete;
    ~IntroArt() { unload(); }

    bool load(std::string* error = nullptr);
    void unload();
    bool loaded() const { return sheet_ != nullptr; }

    const std::vector<Slide>& slides() const { return slides_; }
    int imageY() const { return imageY_; }      // px the images' centers sit above the screen's middle
    int textW() const { return textW_; }        // the captions' box: its width,
    int textBottom() const { return textBottom_; } // its bottom, px above the screen's bottom
    void drawImage(int slide, int cx, int cy, float alpha) const; // 1x, centered on (cx, cy)

private:
    plat::Texture* sheet_ = nullptr;
    std::vector<Slide> slides_;
    int imageY_ = 0, textW_ = 0, textBottom_ = 0;
};

class IntroCutscene {
public:
    static constexpr float FADE_TIME = 1;     // FadeIn / FadeOut: the canvas' alpha, 1 a second
    static constexpr float TYPE_DELAY = 0.2f; // scrolltext waits before typing
    static constexpr float CHAR_TIME = 0.06f; // TMPTextTyper's textSpeed: GameSettings.textSpeed (0.03) x 2
    static constexpr float HOLD_TIME = 3;     // WaitToAdvance
    static constexpr int LINE_STEP = 14;      // px between caption lines (PixelTitanZero 16, TMP lineSpacing -15)

    void start(const IntroArt& art); // (its captions' lengths: the typing)
    // One frame. `pressed`: a key or a touch went down. True once the last image has faded out.
    bool update(bool pressed, float dt);
    bool done() const { return slide_ >= (int)lengths_.size(); }

    int slide() const { return slide_; }
    float alpha() const { return alpha_; }
    int shownChars() const { return chars_; } // of the caption, counting spaces and line breaks
    void drawTop(const IntroArt& art, const Font& caption) const;

private:
    enum class Step { Typing, Holding, FadingOut };
    void next();

    std::vector<int> lengths_; // per slide: its caption's characters
    int slide_ = 0;
    Step step_ = Step::Typing;
    float alpha_ = 0;
    bool fadingIn_ = false;
    bool skip_ = false;      // CutsceneManager.skipImages: a press, until a step uses it
    bool fastFade_ = false;  // speedFadeOut
    float t_ = 0;            // s into the current step
    int chars_ = 0;
};
