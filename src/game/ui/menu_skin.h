// The original's menu widgets, for the port's menus: its buttons (Menu Button: Button24, 9-sliced,
// sprite swap), sliders (UI Slider) and toggles (UI Toggle), with TinyUnicode text (a port choice:
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

    enum class Button { Normal, Selected, Pressed }; // Selected = the menu's cursor (keyboard selection)

    MenuSkin() = default;
    MenuSkin(const MenuSkin&) = delete;
    MenuSkin& operator=(const MenuSkin&) = delete;
    ~MenuSkin() { unload(); }

    bool load(std::string* error = nullptr);
    void unload();

    // A page title (light), centered on (cx, cy).
    void drawTitle(std::string_view text, int cx, int cy) const;
    // A button filling the box (its label dark, centered on its face).
    void drawButton(int x, int y, int w, int h, std::string_view label, Button state) const;
    // An option's label, left-aligned at x, centered on cy: gray, light when selected.
    void drawLabel(std::string_view text, int x, int cy, bool selected) const;
    // A slider (w x SLIDER_H at (x, y)) at `value` 0-1; `selected` swaps its handle.
    void drawSlider(int x, int y, int w, float value, bool selected) const;
    // The value a touch at screen px `screenX` sets on a slider at UI x `x`, `w` wide (0-1, clamped).
    static float sliderValueAt(float screenX, int x, int w);
    // A toggle (TOGGLE_SIZE square at (x, y)); `selected` swaps its background.
    void drawToggle(int x, int y, bool on, bool selected) const;

private:
    struct Sprite {
        int x = 0, y = 0, w = 0, h = 0;
        int left = 0, top = 0, right = 0, bottom = 0; // 9-slice borders
    };

    // A 9-slice filling the box: corners 1:1, edges and middle stretched.
    void drawNineSlice(const Sprite& s, int x, int y, int w, int h) const;
    void draw(const Sprite& s, int x, int y) const;
    // `text` in `font`, its capitals' middle on cy (rounded down to the grid), pen at x.
    static void drawText(const Font& font, std::string_view text, int x, int cy);

    plat::Texture* sheet_ = nullptr;
    Sprite button_, buttonPressed_, buttonSelected_, toggle_, toggleSelected_, toggleCheck_, handle_, handleSelected_;
    Font dark_, light_, label_;
};
