// The original's menu widgets, for the port's menus: its buttons (MainMenu Button Variant: a 9-sliced
// sprite swap), sliders (UI Slider) and toggles (UI Toggle), with TinyUnicode text (a port choice:
// it reads better on the 3DS than the original's PixelTitanZero). Sprites cut by tools/make_ui.py
// (assets/ui/menu.json), fonts baked by tools/make_font.py.
//
// The original's 320x180 UI is drawn at 1x; the 3DS's touch screen wants bigger targets, so the
// sprites draw at UI_SCALE (whole pixels, like the Artifact) and the text at 3-4x. Sizes and
// positions here are screen px.
#pragma once

#include <string>
#include <string_view>

#include "game/render/font.h"
#include "platform/platform.h"

class MenuSkin {
public:
    static constexpr int UI_SCALE = 2;                         // sprites' magnification
    static constexpr int TITLE_SCALE = 4, TEXT_SCALE = 3;      // text's
    static constexpr int SLIDER_W = 96, SLIDER_H = 10 * UI_SCALE; // UI Slider: 10 tall (narrower than its 80)
    static constexpr int TOGGLE_SIZE = 16 * UI_SCALE;          // UI Toggle as Label Toggle Pair sizes it
    static constexpr int PRESS_DROP = 2 * UI_SCALE;            // UIButtonTextEffect.amount: the label while pressed

    enum class Button { Normal, Selected, Pressed }; // Selected = the menu's cursor (keyboard selection)

    MenuSkin() = default;
    MenuSkin(const MenuSkin&) = delete;
    MenuSkin& operator=(const MenuSkin&) = delete;
    ~MenuSkin() { unload(); }

    bool load(std::string* error = nullptr);
    void unload();

    // A page title, centered on (cx, cy).
    void drawTitle(std::string_view text, float cx, float cy) const;
    // A button filling the box, its label centered.
    void drawButton(float x, float y, float w, float h, std::string_view label, Button state) const;
    // An option's label (the labels' gray), left-aligned at x, centered on cy.
    void drawLabel(std::string_view text, float x, float cy) const;
    // A slider (SLIDER_W x SLIDER_H at (x, y)) at `value` 0-1; `selected` swaps its handle.
    void drawSlider(float x, float y, float value, bool selected) const;
    // The value a touch at screen x sets on a slider at x (0-1, clamped).
    static float sliderValueAt(float px, float sliderX);
    // A toggle (TOGGLE_SIZE square at (x, y)); `selected` swaps its background.
    void drawToggle(float x, float y, bool on, bool selected) const;

private:
    struct Sprite {
        int x = 0, y = 0, w = 0, h = 0;
        int left = 0, top = 0, right = 0, bottom = 0; // 9-slice borders
    };

    // A 9-slice filling the box: corners at `scale`, edges and middle stretched.
    void drawNineSlice(const Sprite& s, float x, float y, float w, float h, int scale) const;
    void drawScaled(const Sprite& s, float x, float y, int scale) const;
    // `text` in `font` at `scale`, its capitals' middle on cy, pen at x.
    static void drawCentered(const Font& font, std::string_view text, float x, float cy, int scale);

    plat::Texture* sheet_ = nullptr;
    Sprite button_, buttonSelected_, toggle_, toggleSelected_, toggleCheck_, handle_, handleSelected_;
    Font text_, label_;
};
