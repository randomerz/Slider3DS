#include "game/ui/end_screen.h"

#include <cmath>

#include "game/data/json.h"
#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"

namespace {

constexpr int S = MenuSkin::UI_SCALE;
constexpr int MIN_W = 320, MIN_H = 180; // EndOfDemo.unity's canvas
// Main Menu (its button), centered on the bottom screen like the pause menu's (UI px).
constexpr int BUTTON_W = 120, BUTTON_H = 18;
constexpr int BUTTON_X = (plat::BOTTOM_W / S - BUTTON_W) / 2, BUTTON_Y = (plat::BOTTOM_H / S - BUTTON_H) / 2;
constexpr int BUTTON = 0; // its MenuTap item

} // namespace

bool EndArt::load(std::string* error) {
    unload();
    std::string text;
    Json doc;
    if (!plat::readFile("ui/end.json", text) || !Json::parse(text, doc, error)) {
        if (error && error->empty()) *error = "can't read ui/end.json";
        return false;
    }
    const Json& r = doc["sprites"]["art"]["rect"];
    if (r.size() != 4) {
        if (error) *error = "ui/end.json: no sprite art";
        return false;
    }
    x_ = r.at(0).asInt(), y_ = r.at(1).asInt(), w_ = r.at(2).asInt(), h_ = r.at(3).asInt();
    if (x_ < 0 || y_ < 0 || w_ < MIN_W || h_ < MIN_H || w_ > plat::TOP_W || h_ > plat::TOP_H) {
        if (error) *error = "ui/end.json: the art is " + std::to_string(w_) + "x" + std::to_string(h_) + " at " +
                            std::to_string(x_) + "," + std::to_string(y_);
        return false;
    }
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) {
        if (error) *error = "can't load the end sheet " + doc["sheet"].asString();
        return false;
    }
    return true;
}

void EndArt::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
}

void EndArt::draw(int x, int y) const { plat::drawSprite(sheet_, x_, y_, w_, h_, (float)x, (float)y); }

EndScreen::Box EndScreen::buttonBox() { return {BUTTON_X * S, BUTTON_Y * S, BUTTON_W * S, BUTTON_H * S}; }

void EndScreen::start() {
    frames_ = 0;
    clicked_ = false;
    tap_.release();
    bounce_ = BracketBounce();
}

bool EndScreen::update(const plat::Input& in) {
    clicked_ = false;
    const bool ready = frames_ >= LOCKOUT_FRAMES;
    if (!ready) frames_++;
    const MenuTap::Frame f = tap_.update(in, S, [&](int x, int y) {
        return ready && x >= BUTTON_X && x < BUTTON_X + BUTTON_W && y >= BUTTON_Y && y < BUTTON_Y + BUTTON_H ? BUTTON : -1;
    });
    if (!ready) return false;
    bounce_.update(BUTTON);
    clicked_ = f.down == BUTTON || (in.pressed & plat::BTN_A);
    return f.tapped == BUTTON || (in.pressed & plat::BTN_A);
}

void EndScreen::drawTop(const EndArt& art) const {
    plat::beginScreen(plat::Screen::Top, palette::DARK); // (the art's own background around it)
    art.draw((plat::TOP_W - art.width()) / 2, (plat::TOP_H - art.height()) / 2);
    if (frames_ < FADE_FRAMES) // (from black)
        plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, (uint8_t)std::lround(255 - 255.0f * frames_ / FADE_FRAMES)));
}

void EndScreen::drawBottom(const MenuSkin& skin) const {
    plat::beginScreen(plat::Screen::Bottom, palette::DARK);
    if (frames_ < LOCKOUT_FRAMES) return;
    skin.drawButton(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, "Main Menu", true, tap_.held(BUTTON), bounce_.out());
}
