// The original's menu widgets, for the port's menus: its buttons (Menu Button: Button24, 9-sliced,
// sprite swap; selected: brackets beside it, the user's art), sliders
// (UI Slider) and toggles (UI Toggle); the title menu's profile cards, bracket frames (keyboard keys,
// the name field) and icons (back, trash); with TinyUnicode text (a port choice:
// it reads better on the 3DS than the original's menu fonts). Sprites cut by tools/make_ui.py
// (assets/ui/menu.json), fonts baked by tools/make_font.py.
//
// One pixel grid: every coordinate here is a whole UI pixel, and art and text alike draw at
// UI_SCALE (as the Artifact does), so a sprite's pixels and the letters' are the same size.
#pragma once

#include <string>
#include <string_view>

#include "game/render/font.h"
#include "platform/platform.h"

class MenuSkin {
public:
    static constexpr int UI_SCALE = 2; // screen px per UI px
    // Widget sizes, UI px: the slider is its handle's height (3x5 at 1x; the original stretches it
    // to 2x), the toggle its sprites' size (8x8 at 1x).
    static constexpr int SLIDER_H = 5, TOGGLE_SIZE = 8;
    static constexpr int PRESS_DROP = 1; // the label follows Button24's pressed face, 1 px lower
    static constexpr int CARD_EDGE = 2;  // a card's face leaves its background's bottom 2 px showing

    // A sprite-swapped widget's look (Button transition: normal, pressed, highlighted).
    enum class State { Normal, Pressed, Selected };
    enum class Icon { Back, Trash };
    enum class Frame { Light, Dark }; // MainMenuButton20's bracket frames (_0, _2)

    MenuSkin() = default;
    MenuSkin(const MenuSkin&) = delete;
    MenuSkin& operator=(const MenuSkin&) = delete;
    ~MenuSkin() { unload(); }

    bool load(std::string* error = nullptr);
    void unload();

    // A page title (light), centered on (cx, cy).
    void drawTitle(std::string_view text, int cx, int cy) const;
    // A button filling the box (its label dark, centered on its face): `pressed` sinks its face;
    // `selected` (the menu's cursor) brackets its sides, `bracketOut` px further out than at rest
    // (the bounce), a pressed one's brackets sunk with the face. A disabled one's label is gray
    // (Continue without a profile).
    void drawButton(int x, int y, int w, int h, std::string_view label, bool selected, bool pressed,
                    int bracketOut = 0, bool enabled = true) const;
    // An option's label, left-aligned at x, centered on cy: gray, light when selected.
    void drawLabel(std::string_view text, int x, int cy, bool selected) const;
    // A slider (w x SLIDER_H at (x, y)) at `value` 0-1; `selected` swaps its handle.
    void drawSlider(int x, int y, int w, float value, bool selected) const;
    // The value a touch at UI x `touchX` (fractional) sets on a slider at x, `w` wide (0-1, clamped):
    // the handle's middle follows it.
    static float sliderValueAt(float touchX, int x, int w);
    // The ending's art (assets/ui/end.json: EndOfDemo's background), 1x with its top-left at
    // screen px (x, y) — the top screen's scale, not the menus' grid.
    void drawEndArt(int x, int y) const;
    // Its TinyUnicode font (the top screen's HUD draws it at 1x).
    const Font& font() const { return font_; }
    // A toggle (TOGGLE_SIZE square at (x, y)); `selected` swaps its background.
    void drawToggle(int x, int y, bool on, bool selected) const;
    // A profile card filling the box: its background, and its face (pressed: 1 px lower; selected:
    // outlined). Its contents are the caller's (drawText), PRESS_DROP lower when pressed.
    void drawCard(int x, int y, int w, int h, State state) const;
    // A bracket frame filling the box ("[ ]").
    void drawFrame(Frame frame, int x, int y, int w, int h) const;
    // An icon button at (x, y), its sprite's size (iconSize).
    void drawIcon(Icon icon, int x, int y, State state) const;
    void iconSize(Icon icon, int& w, int& h) const;
    // `text` in `color`, its capitals' middle on cy (rounded down to the grid), pen at x.
    void drawText(std::string_view text, int x, int cy, uint32_t color) const;
    int textWidth(std::string_view text) const { return font_.textWidth(text); }

    // While it lives, everything the skin draws is moved (dx, dy) UI px (a whole panel sliding in;
    // on the grid, as everything else).
    class Shift {
    public:
        Shift(const MenuSkin& skin, int dx, int dy) : skin_(skin), oldX_(skin.shiftX_), oldY_(skin.shiftY_) {
            skin.shiftX_ += dx;
            skin.shiftY_ += dy;
        }
        ~Shift() {
            skin_.shiftX_ = oldX_;
            skin_.shiftY_ = oldY_;
        }
        Shift(const Shift&) = delete;
        Shift& operator=(const Shift&) = delete;

    private:
        const MenuSkin& skin_;
        int oldX_, oldY_;
    };

private:
    struct Sprite {
        int x = 0, y = 0, w = 0, h = 0;
        int left = 0, top = 0, right = 0, bottom = 0; // 9-slice borders
    };

    // A 9-slice filling the box: corners 1:1, edges and middle stretched.
    void drawNineSlice(const Sprite& s, int x, int y, int w, int h) const;
    void draw(const Sprite& s, int x, int y) const;
    void fillRect(int x, int y, int w, int h, uint32_t color) const;

    plat::Texture* sheet_ = nullptr;
    plat::Texture* endSheet_ = nullptr;
    Sprite endArt_;
    Sprite button_, buttonPressed_, bracketLeft_, bracketRight_, bracketPressedLeft_, bracketPressedRight_, toggle_, toggleSelected_, toggleCheck_, handle_, handleSelected_;
    Sprite card_[3], cardBack_, frame_[2], back_[3], trash_[3]; // ([State] / [Frame])
    Font font_;
    mutable int shiftX_ = 0, shiftY_ = 0; // (Shift: a draw-time offset, not the skin's look)
};
