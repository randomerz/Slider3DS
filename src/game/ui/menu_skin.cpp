#include "game/ui/menu_skin.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"

namespace {

// UI Slider: its bar (Background, 0.4 gray) and fill (0.6 gray) span the middle half of its height;
// the fill reaches from the left end to the handle's middle; the handle (6 px wide, the slider's
// height: its 3x5 sprite at 2x) slides between 2.5 px insets.
constexpr uint32_t SLIDER_BAR = plat::rgba(0x66, 0x66, 0x66);
constexpr uint32_t SLIDER_FILL = plat::rgba(0x99, 0x99, 0x99);
constexpr float HANDLE_INSET = 2.5f, HANDLE_W = 6;
constexpr float FILL_INSET = 5; // Fill Area's inset; the fill ends 5 px past the value's point in it
// UI Toggle's 8x8 sprites, drawn 2x in its box (Unity stretches them to 20 px, 2.5x).
constexpr int TOGGLE_SCALE = 2;

} // namespace

bool MenuSkin::load(std::string* error) {
    unload();
    std::string text;
    Json doc;
    if (!plat::readFile("ui/menu.json", text) || !Json::parse(text, doc, error)) {
        if (error && error->empty()) *error = "can't read ui/menu.json";
        return false;
    }
    const Json& sprites = doc["sprites"];
    bool ok = true;
    auto read = [&](const char* name, Sprite& s) {
        const Json& r = sprites[name]["rect"];
        const Json& b = sprites[name]["border"];
        if (r.size() != 4 || b.size() != 4) {
            ok = false;
            if (error) *error = std::string("ui/menu.json: no sprite ") + name;
            return;
        }
        s = {r.at(0).asInt(), r.at(1).asInt(), r.at(2).asInt(), r.at(3).asInt(),
             b.at(0).asInt(), b.at(1).asInt(), b.at(2).asInt(), b.at(3).asInt()};
    };
    read("button", button_);
    read("buttonSelected", buttonSelected_);
    read("toggle", toggle_);
    read("toggleSelected", toggleSelected_);
    read("toggleCheck", toggleCheck_);
    read("sliderHandle", handle_);
    read("sliderHandleSelected", handleSelected_);
    if (!ok) return false;
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) {
        if (error) *error = "can't load the menu sheet " + doc["sheet"].asString();
        return false;
    }
    return title_.load("fonts/menu_title.json", error) && buttonFont_.load("fonts/menu_button.json", error) &&
           label_.load("fonts/menu_label.json", error);
}

void MenuSkin::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
    title_.unload();
    buttonFont_.unload();
    label_.unload();
}

void MenuSkin::drawNineSlice(const Sprite& s, float x, float y, float w, float h) const {
    // Columns and rows: border, stretched middle, border.
    const int sxs[3] = {s.x, s.x + s.left, s.x + s.w - s.right};
    const int sws[3] = {s.left, s.w - s.left - s.right, s.right};
    const int sys[3] = {s.y, s.y + s.top, s.y + s.h - s.bottom};
    const int shs[3] = {s.top, s.h - s.top - s.bottom, s.bottom};
    const float xs[3] = {x, x + s.left, x + w - s.right};
    const float ws[3] = {(float)s.left, w - s.left - s.right, (float)s.right};
    const float ys[3] = {y, y + s.top, y + h - s.bottom};
    const float hs[3] = {(float)s.top, h - s.top - s.bottom, (float)s.bottom};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            if (sws[c] <= 0 || shs[r] <= 0 || ws[c] <= 0 || hs[r] <= 0) continue;
            if (ws[c] == sws[c] && hs[r] == shs[r])
                plat::drawSprite(sheet_, sxs[c], sys[r], sws[c], shs[r], xs[c], ys[r]);
            else
                plat::drawSpriteStretched(sheet_, sxs[c], sys[r], sws[c], shs[r], xs[c], ys[r], ws[c], hs[r]);
        }
}

void MenuSkin::drawScaled(const Sprite& s, float x, float y, int scale) const {
    plat::drawSprite(sheet_, s.x, s.y, s.w, s.h, x, y, 0, scale);
}

void MenuSkin::drawCentered(const Font& font, std::string_view text, float x, float cy) {
    const GlyphRect& cap = font.def().glyph('A').text;
    font.drawText(text, x, std::round(cy - (cap.oy + cap.h / 2.0f)));
}

void MenuSkin::drawTitle(std::string_view text, float cx, float cy) const {
    drawCentered(title_, text, std::round(cx - title_.textWidth(text) / 2.0f), cy);
}

void MenuSkin::drawButton(float x, float y, float w, float h, std::string_view label, Button state) const {
    drawNineSlice(state == Button::Normal ? button_ : buttonSelected_, x, y, w, h);
    const float drop = state == Button::Pressed ? (float)PRESS_DROP : 0.0f;
    drawCentered(buttonFont_, label, std::round(x + (w - buttonFont_.textWidth(label)) / 2.0f), y + h / 2.0f + drop);
}

void MenuSkin::drawLabel(std::string_view text, float x, float cy) const { drawCentered(label_, text, x, cy); }

void MenuSkin::drawSlider(float x, float y, float value, bool selected) const {
    value = std::clamp(value, 0.0f, 1.0f);
    const float barY = y + SLIDER_H / 4.0f, barH = SLIDER_H / 2.0f;
    plat::drawRect(x, barY, SLIDER_W, barH, SLIDER_BAR);
    plat::drawRect(x, barY, std::round(FILL_INSET + (SLIDER_W - 2 * FILL_INSET) * value), barH, SLIDER_FILL);
    const Sprite& handle = selected ? handleSelected_ : handle_;
    const float center = x + HANDLE_INSET + (SLIDER_W - 2 * HANDLE_INSET) * value;
    drawScaled(handle, std::round(center - HANDLE_W / 2), y, SLIDER_H / handle.h);
}

float MenuSkin::sliderValueAt(float px, float sliderX) {
    return std::clamp((px - sliderX - HANDLE_INSET) / (SLIDER_W - 2 * HANDLE_INSET), 0.0f, 1.0f);
}

void MenuSkin::drawToggle(float x, float y, bool on, bool selected) const {
    const float inset = (TOGGLE_SIZE - toggle_.w * TOGGLE_SCALE) / 2.0f;
    drawScaled(selected ? toggleSelected_ : toggle_, x + inset, y + inset, TOGGLE_SCALE);
    if (on) drawScaled(toggleCheck_, x + inset, y + inset, TOGGLE_SCALE);
}
