#include "game/ui/menu_skin.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"
#include "game/ui/palette.h"

namespace {

constexpr int S = MenuSkin::UI_SCALE;

// UI Slider's colors: its bar (Background, 0.4 gray) and the fill up to the handle (0.6 gray). The
// bar is the slider's middle 3 px; the handle covers its full height.
constexpr uint32_t SLIDER_BAR = plat::rgba(0x66, 0x66, 0x66);
constexpr uint32_t SLIDER_FILL = plat::rgba(0x99, 0x99, 0x99);
// Text colors: Menu Button's label (m_fontColor 0.102, on its light face), titles and the selected
// option (0.902), option labels (UI Slider / Label Toggle Pair "Label": 0.6).
constexpr uint32_t TEXT_DARK = palette::DARK;
constexpr uint32_t TEXT_LIGHT = palette::OFF_WHITE;
constexpr uint32_t TEXT_GRAY = palette::GRAY;
// Button24's bottom 2 px are its shaded edge: labels center on the face above it.
constexpr int BUTTON_EDGE = 2;
constexpr int HANDLE_W = 3; // ui_slider's handles
constexpr int BRACKET_GAP = 1; // px between a selected button's sides and its brackets at rest

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
    read("bracketLeft", bracketLeft_);
    read("bracketRight", bracketRight_);
    read("bracketPressedLeft", bracketPressedLeft_);
    read("bracketPressedRight", bracketPressedRight_);
    read("toggle", toggle_);
    read("toggleSelected", toggleSelected_);
    read("toggleCheck", toggleCheck_);
    read("sliderHandle", handle_);
    read("sliderHandleSelected", handleSelected_);
    const char* states[3] = {"", "Pressed", "Selected"};
    for (int i = 0; i < 3; i++) {
        read((std::string("card") + states[i]).c_str(), card_[i]);
        read((std::string("back") + states[i]).c_str(), back_[i]);
        read((std::string("trash") + states[i]).c_str(), trash_[i]);
    }
    read("cardBack", cardBack_);
    read("frameLight", frame_[(int)Frame::Light]);
    read("frameDark", frame_[(int)Frame::Dark]);
    // The widgets are their sprites' sizes (one grid: no scaling).
    auto is = [](const Sprite& s, int w, int h) { return s.w == w && s.h == h; };
    // The buttons alike; each pair of brackets mirror images of one size.
    if (ok && (!is(buttonPressed_, button_.w, button_.h) || !is(bracketRight_, bracketLeft_.w, bracketLeft_.h) ||
               !is(bracketPressedRight_, bracketPressedLeft_.w, bracketPressedLeft_.h))) {
        ok = false;
        if (error) *error = "ui/menu.json: the button sprites' sizes don't match";
    }
    if (ok && !(is(handle_, HANDLE_W, SLIDER_H) && is(handleSelected_, HANDLE_W, SLIDER_H) &&
                is(toggle_, TOGGLE_SIZE, TOGGLE_SIZE) && is(toggleSelected_, TOGGLE_SIZE, TOGGLE_SIZE) &&
                is(toggleCheck_, TOGGLE_SIZE, TOGGLE_SIZE))) {
        ok = false;
        if (error) *error = "ui/menu.json: the slider handle / toggle sprites aren't their widgets' sizes";
    }
    // Each sprite-swapped widget's looks are one size.
    for (const Sprite* set : {card_, back_, trash_})
        if (ok && !(is(set[1], set[0].w, set[0].h) && is(set[2], set[0].w, set[0].h))) {
            ok = false;
            if (error) *error = "ui/menu.json: a card / icon's states differ in size";
        }
    if (ok && !is(frame_[1], frame_[0].w, frame_[0].h)) {
        ok = false;
        if (error) *error = "ui/menu.json: the bracket frames differ in size";
    }
    if (ok && !is(cardBack_, card_[0].w, card_[0].h)) {
        ok = false;
        if (error) *error = "ui/menu.json: the card background isn't the card's size";
    }
    if (!ok) return false;
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) {
        if (error) *error = "can't load the menu sheet " + doc["sheet"].asString();
        return false;
    }
    return font_.load("fonts/menu.json", error);
}

void MenuSkin::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
    font_.unload();
}

void MenuSkin::draw(const Sprite& s, int x, int y) const {
    plat::drawSprite(sheet_, s.x, s.y, s.w, s.h, (float)((x + shiftX_) * S), (float)((y + shiftY_) * S), 0, S);
}

void MenuSkin::fillRect(int x, int y, int w, int h, uint32_t color) const {
    plat::drawRect((float)((x + shiftX_) * S), (float)((y + shiftY_) * S), (float)(w * S), (float)(h * S), color);
}

void MenuSkin::drawNineSlice(const Sprite& s, int x, int y, int w, int h) const {
    // Columns and rows: border, stretched middle, border.
    const int sxs[3] = {s.x, s.x + s.left, s.x + s.w - s.right};
    const int sws[3] = {s.left, s.w - s.left - s.right, s.right};
    const int sys[3] = {s.y, s.y + s.top, s.y + s.h - s.bottom};
    const int shs[3] = {s.top, s.h - s.top - s.bottom, s.bottom};
    x += shiftX_;
    y += shiftY_;
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

void MenuSkin::drawText(std::string_view text, int x, int cy, uint32_t color) const {
    const GlyphRect& cap = font_.def().glyph('A').text;
    const int baseline = cy - (int)std::floor(cap.oy + cap.h / 2.0f);
    font_.drawText(text, (float)((x + shiftX_) * S), (float)((baseline + shiftY_) * S), S, color);
}

void MenuSkin::drawTitle(std::string_view text, int cx, int cy) const {
    drawText(text, cx - font_.textWidth(text) / 2, cy, TEXT_LIGHT);
}

void MenuSkin::drawButton(int x, int y, int w, int h, std::string_view label, bool selected, bool pressed,
                          int bracketOut, bool enabled) const {
    drawNineSlice(pressed ? buttonPressed_ : button_, x, y, w, h);
    if (selected) {
        // At rest BRACKET_GAP px from the sides, 1 px past the face's top and bottom (a pressed
        // face starts 1 px lower).
        const Sprite& l = pressed ? bracketPressedLeft_ : bracketLeft_;
        const Sprite& r = pressed ? bracketPressedRight_ : bracketRight_;
        const int top = pressed ? y : y - 1, height = pressed ? h + 1 : h + 2;
        drawNineSlice(l, x - l.w - BRACKET_GAP - bracketOut, top, l.w, height);
        drawNineSlice(r, x + w + BRACKET_GAP + bracketOut, top, r.w, height);
    }
    const int drop = pressed ? PRESS_DROP : 0;
    drawText(label, x + (w - font_.textWidth(label)) / 2, y + (h - BUTTON_EDGE) / 2 + drop, enabled ? TEXT_DARK : TEXT_GRAY);
}

void MenuSkin::drawLabel(std::string_view text, int x, int cy, bool selected) const {
    drawText(text, x, cy, selected ? TEXT_LIGHT : TEXT_GRAY);
}

void MenuSkin::drawSlider(int x, int y, int w, float value, bool selected) const {
    const Sprite& handle = selected ? handleSelected_ : handle_;
    const int handleX = x + (int)std::lround((w - handle.w) * std::clamp(value, 0.0f, 1.0f));
    fillRect(x, y + 1, w, SLIDER_H - 2, SLIDER_BAR);
    fillRect(x, y + 1, handleX - x + handle.w / 2, SLIDER_H - 2, SLIDER_FILL);
    draw(handle, handleX, y);
}

float MenuSkin::sliderValueAt(float touchX, int x, int w) {
    return std::clamp((touchX - x - HANDLE_W / 2.0f) / (w - HANDLE_W), 0.0f, 1.0f);
}

void MenuSkin::drawCard(int x, int y, int w, int h, State state) const {
    drawNineSlice(cardBack_, x, y, w, h);
    drawNineSlice(card_[(int)state], x, y, w, h);
}

void MenuSkin::drawFrame(Frame frame, int x, int y, int w, int h) const { drawNineSlice(frame_[(int)frame], x, y, w, h); }

void MenuSkin::drawIcon(Icon icon, int x, int y, State state) const {
    draw((icon == Icon::Back ? back_ : trash_)[(int)state], x, y);
}

void MenuSkin::iconSize(Icon icon, int& w, int& h) const {
    const Sprite& s = (icon == Icon::Back ? back_ : trash_)[0];
    w = s.w;
    h = s.h;
}

void MenuSkin::drawToggle(int x, int y, bool on, bool selected) const {
    draw(selected ? toggleSelected_ : toggle_, x, y);
    if (on) draw(toggleCheck_, x, y);
}
