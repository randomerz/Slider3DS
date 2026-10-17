#include "game/ui/menu_skin.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"

namespace {

// UI Slider: its bar (Background, 0.4 gray) and fill (0.6 gray) span the middle half of its height;
// the fill reaches from the left end to 5 px past the value's point; the handle (its 3x5 sprite at
// the slider's height) slides between 2.5 px insets. (UI px, times UI_SCALE.)
constexpr uint32_t SLIDER_BAR = plat::rgba(0x66, 0x66, 0x66);
constexpr uint32_t SLIDER_FILL = plat::rgba(0x99, 0x99, 0x99);
constexpr float HANDLE_INSET = 2.5f * MenuSkin::UI_SCALE;
constexpr float FILL_INSET = 5.0f * MenuSkin::UI_SCALE;

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
    // The handle fills the slider's height, the toggle's sprites its box: at whole-pixel scales.
    if (ok && (handle_.h <= 0 || SLIDER_H % handle_.h != 0 || handleSelected_.h != handle_.h || toggle_.w <= 0 ||
               TOGGLE_SIZE % toggle_.w != 0)) {
        ok = false;
        if (error) *error = "ui/menu.json: the slider handle / toggle sprites don't scale to their widgets";
    }
    if (!ok) return false;
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) {
        if (error) *error = "can't load the menu sheet " + doc["sheet"].asString();
        return false;
    }
    return text_.load("fonts/menu_text.json", error) && label_.load("fonts/menu_label.json", error);
}

void MenuSkin::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
    text_.unload();
    label_.unload();
}

void MenuSkin::drawNineSlice(const Sprite& s, float x, float y, float w, float h, int scale) const {
    // Columns and rows: border, stretched middle, border.
    const int sxs[3] = {s.x, s.x + s.left, s.x + s.w - s.right};
    const int sws[3] = {s.left, s.w - s.left - s.right, s.right};
    const int sys[3] = {s.y, s.y + s.top, s.y + s.h - s.bottom};
    const int shs[3] = {s.top, s.h - s.top - s.bottom, s.bottom};
    const float l = (float)(s.left * scale), r = (float)(s.right * scale);
    const float t = (float)(s.top * scale), b = (float)(s.bottom * scale);
    const float xs[3] = {x, x + l, x + w - r};
    const float ws[3] = {l, w - l - r, r};
    const float ys[3] = {y, y + t, y + h - b};
    const float hs[3] = {t, h - t - b, b};
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++) {
            if (sws[col] <= 0 || shs[row] <= 0 || ws[col] <= 0 || hs[row] <= 0) continue;
            if (ws[col] == sws[col] * scale && hs[row] == shs[row] * scale)
                plat::drawSprite(sheet_, sxs[col], sys[row], sws[col], shs[row], xs[col], ys[row], 0, scale);
            else
                plat::drawSpriteStretched(sheet_, sxs[col], sys[row], sws[col], shs[row], xs[col], ys[row], ws[col], hs[row]);
        }
}

void MenuSkin::drawScaled(const Sprite& s, float x, float y, int scale) const {
    plat::drawSprite(sheet_, s.x, s.y, s.w, s.h, x, y, 0, scale);
}

void MenuSkin::drawCentered(const Font& font, std::string_view text, float x, float cy, int scale) {
    const GlyphRect& cap = font.def().glyph('A').text;
    font.drawText(text, x, std::round(cy - (cap.oy + cap.h / 2.0f) * scale), scale);
}

void MenuSkin::drawTitle(std::string_view text, float cx, float cy) const {
    drawCentered(text_, text, std::round(cx - text_.textWidth(text, TITLE_SCALE) / 2.0f), cy, TITLE_SCALE);
}

void MenuSkin::drawButton(float x, float y, float w, float h, std::string_view label, Button state) const {
    drawNineSlice(state == Button::Normal ? button_ : buttonSelected_, x, y, w, h, UI_SCALE);
    const float drop = state == Button::Pressed ? (float)PRESS_DROP : 0.0f;
    drawCentered(text_, label, std::round(x + (w - text_.textWidth(label, TEXT_SCALE)) / 2.0f), y + h / 2.0f + drop,
                 TEXT_SCALE);
}

void MenuSkin::drawLabel(std::string_view text, float x, float cy) const { drawCentered(label_, text, x, cy, TEXT_SCALE); }

void MenuSkin::drawSlider(float x, float y, float value, bool selected) const {
    value = std::clamp(value, 0.0f, 1.0f);
    const float barY = y + SLIDER_H / 4.0f, barH = SLIDER_H / 2.0f;
    plat::drawRect(x, barY, SLIDER_W, barH, SLIDER_BAR);
    plat::drawRect(x, barY, std::round(FILL_INSET + (SLIDER_W - 2 * FILL_INSET) * value), barH, SLIDER_FILL);
    const Sprite& handle = selected ? handleSelected_ : handle_;
    const int scale = SLIDER_H / handle.h;
    const float center = x + HANDLE_INSET + (SLIDER_W - 2 * HANDLE_INSET) * value;
    drawScaled(handle, std::round(center - handle.w * scale / 2.0f), y, scale);
}

float MenuSkin::sliderValueAt(float px, float sliderX) {
    return std::clamp((px - sliderX - HANDLE_INSET) / (SLIDER_W - 2 * HANDLE_INSET), 0.0f, 1.0f);
}

void MenuSkin::drawToggle(float x, float y, bool on, bool selected) const {
    const int scale = TOGGLE_SIZE / toggle_.w; // (8 px sprites)
    drawScaled(selected ? toggleSelected_ : toggle_, x, y, scale);
    if (on) drawScaled(toggleCheck_, x, y, scale);
}
