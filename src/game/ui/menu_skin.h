// The original's menu look, for the port's menus: its buttons (MainMenu Button Variant: a 9-sliced
// sprite swap), sliders (UI Slider), toggles (UI Toggle) and text (PixelTitanZero titles and
// buttons, PixelUnicode labels). Sprites cut by tools/make_ui.py (assets/ui/menu.json), fonts baked
// by tools/make_font.py. Drawn 1:1: the UI canvas is 320x180 px, the bottom screen 320x240.
#pragma once

#include <string>
#include <string_view>

#include "game/render/font.h"
#include "platform/platform.h"

class MenuSkin {
public:
    // Widget sizes as the prefabs have them, px.
    static constexpr int SLIDER_W = 80, SLIDER_H = 10; // UI Slider's "Slider"
    static constexpr int TOGGLE_SIZE = 20;             // UI Toggle
    static constexpr int PRESS_DROP = 2;               // UIButtonTextEffect.amount: the label while pressed

    enum class Button { Normal, Selected, Pressed }; // Selected = the menu's cursor (keyboard selection)

    MenuSkin() = default;
    MenuSkin(const MenuSkin&) = delete;
    MenuSkin& operator=(const MenuSkin&) = delete;
    ~MenuSkin() { unload(); }

    bool load(std::string* error = nullptr);
    void unload();

    // A panel title ("Paused Text": centered on (cx, cy)).
    void drawTitle(std::string_view text, float cx, float cy) const;
    // A button filling the box, its label centered.
    void drawButton(float x, float y, float w, float h, std::string_view label, Button state) const;
    // An option's label, left-aligned at x, centered on cy.
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

    void drawNineSlice(const Sprite& s, float x, float y, float w, float h) const;
    void drawScaled(const Sprite& s, float x, float y, int scale) const;
    // `text` at `font`, centered vertically on cy (its capitals' middle), pen at x.
    static void drawCentered(const Font& font, std::string_view text, float x, float cy);

    plat::Texture* sheet_ = nullptr;
    Sprite button_, buttonSelected_, toggle_, toggleSelected_, toggleCheck_, handle_, handleSelected_;
    Font title_, buttonFont_, label_;
};
