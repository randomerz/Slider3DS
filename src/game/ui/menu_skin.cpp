#include "game/ui/menu_skin.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"

namespace {

constexpr int S = MenuSkin::UI_SCALE;

// UI Slider's colors: its bar (Background, 0.4 gray) and the fill up to the handle (0.6 gray). The
// bar is the slider's middle 3 px; the handle covers its full height.
constexpr uint32_t SLIDER_BAR = plat::rgba(0x66, 0x66, 0x66);
constexpr uint32_t SLIDER_FILL = plat::rgba(0x99, 0x99, 0x99);
// Button24's bottom 2 px are its shaded edge: labels center on the face above it.
constexpr int BUTTON_EDGE = 2;
constexpr int HANDLE_W = 3; // ui_slider's handles

void rect(int x, int y, int w, int h, uint32_t color) {
    plat::drawRect((float)(x * S), (float)(y * S), (float)(w * S), (float)(h * S), color);
}

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
    read("buttonPressed", buttonPressed_);
    read("buttonSelected", buttonSelected_);
    read("toggle", toggle_);
    read("toggleSelected", toggleSelected_);
    read("toggleCheck", toggleCheck_);
    read("sliderHandle", handle_);
    read("sliderHandleSelected", handleSelected_);
    // The widgets are their sprites' sizes (one grid: no scaling).
    if (ok && (handle_.h != SLIDER_H || handleSelected_.h != SLIDER_H || handle_.w != HANDLE_W ||
               handleSelected_.w != HANDLE_W || toggle_.w != TOGGLE_SIZE)) {
        ok = false;
        if (error) *error = "ui/menu.json: the slider handle / toggle sprites aren't their widgets' sizes";
    }
    if (!ok) return false;
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) {
        if (error) *error = "can't load the menu sheet " + doc["sheet"].asString();
        return false;
    }
    return dark_.load("fonts/menu_dark.json", error) && light_.load("fonts/menu_light.json", error) &&
           label_.load("fonts/menu_label.json", error);
}

void MenuSkin::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
    dark_.unload();
    light_.unload();
    label_.unload();
}

void MenuSkin::draw(const Sprite& s, int x, int y) const {
    plat::drawSprite(sheet_, s.x, s.y, s.w, s.h, (float)(x * S), (float)(y * S), 0, S);
}

void MenuSkin::drawNineSlice(const Sprite& s, int x, int y, int w, int h) const {
    // Columns and rows: border, stretched middle, border.
    const int sxs[3] = {s.x, s.x + s.left, s.x + s.w - s.right};
    const int sws[3] = {s.left, s.w - s.left - s.right, s.right};
    const int sys[3] = {s.y, s.y + s.top, s.y + s.h - s.bottom};
    const int shs[3] = {s.top, s.h - s.top - s.bottom, s.bottom};
    const int xs[3] = {x, x + s.left, x + w - s.right};
    const int ws[3] = {s.left, w - s.left - s.right, s.right};
    const int ys[3] = {y, y + s.top, y + h - s.bottom};
    const int hs[3] = {s.top, h - s.top - s.bottom, s.bottom};
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++) {
            if (sws[col] <= 0 || shs[row] <= 0 || ws[col] <= 0 || hs[row] <= 0) continue;
            if (ws[col] == sws[col] && hs[row] == shs[row])
                plat::drawSprite(sheet_, sxs[col], sys[row], sws[col], shs[row], (float)(xs[col] * S), (float)(ys[row] * S), 0, S);
            else
                plat::drawSpriteStretched(sheet_, sxs[col], sys[row], sws[col], shs[row], (float)(xs[col] * S),
                                          (float)(ys[row] * S), (float)(ws[col] * S), (float)(hs[row] * S));
        }
}

void MenuSkin::drawText(const Font& font, std::string_view text, int x, int cy) {
    const GlyphRect& cap = font.def().glyph('A').text;
    const int baseline = cy - (int)std::floor(cap.oy + cap.h / 2.0f);
    font.drawText(text, (float)(x * S), (float)(baseline * S), S);
}

void MenuSkin::drawTitle(std::string_view text, int cx, int cy) const {
    drawText(light_, text, cx - light_.textWidth(text) / 2, cy);
}

void MenuSkin::drawButton(int x, int y, int w, int h, std::string_view label, Button state) const {
    drawNineSlice(state == Button::Pressed ? buttonPressed_ : state == Button::Selected ? buttonSelected_ : button_, x, y, w, h);
    const int drop = state == Button::Pressed ? PRESS_DROP : 0;
    drawText(dark_, label, x + (w - dark_.textWidth(label)) / 2, y + (h - BUTTON_EDGE) / 2 + drop);
}

void MenuSkin::drawLabel(std::string_view text, int x, int cy, bool selected) const {
    drawText(selected ? light_ : label_, text, x, cy);
}

void MenuSkin::drawSlider(int x, int y, int w, float value, bool selected) const {
    const Sprite& handle = selected ? handleSelected_ : handle_;
    const int handleX = x + (int)std::lround((w - handle.w) * std::clamp(value, 0.0f, 1.0f));
    rect(x, y + 1, w, SLIDER_H - 2, SLIDER_BAR);
    rect(x, y + 1, handleX - x + handle.w / 2, SLIDER_H - 2, SLIDER_FILL);
    draw(handle, handleX, y);
}

float MenuSkin::sliderValueAt(float screenX, int x, int w) {
    // The handle's middle follows the stylus.
    return std::clamp((screenX / S - x - HANDLE_W / 2.0f) / (w - HANDLE_W), 0.0f, 1.0f);
}

void MenuSkin::drawToggle(int x, int y, bool on, bool selected) const {
    draw(selected ? toggleSelected_ : toggle_, x, y);
    if (on) draw(toggleCheck_, x, y);
}
