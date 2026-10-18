#include "game/ui/title_screen.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "game/data/json.h"
#include "game/render/font.h"
#include "game/ui/palette.h"

namespace {

// OpenCutscene's waits after its first frame, and what follows each.
constexpr float SHAKE_RISE_AT = 1, SHAKE_AT = 3, PROMPT_AT = 4; // s (1, then 2, then 1)
constexpr float SHAKE_RISE_DURATION = 2.1f, SHAKE_RISE_AMOUNT = 0.1f;
constexpr float SHAKE_DURATION = 0.25f, SHAKE_AMOUNT = 0.2f;
// MainMenuBGStars: a star stays on up to MAX_TIME_ON s (at least half that), off up to MAX_TIME_OFF.
constexpr float MAX_TIME_ON = 50, MAX_TIME_OFF = 1;
constexpr float PX_PER_UNIT = 16;

std::vector<CurveKey> readKeys(const Json& j) {
    std::vector<CurveKey> keys;
    for (const Json& k : j.items()) keys.push_back({k.at(0).asFloat(), k.at(1).asFloat(), k.at(2).asFloat(), k.at(3).asFloat()});
    return keys;
}

// Where a sprite whose center (pivot) is world px (x, y) goes on the 400x240 top screen.
void drawAt(const TitleArt& art, int sprite, float x, float y, float camX, float camY) {
    const TitleArt::Sprite& s = art.sprites[sprite];
    const float left = std::round(plat::TOP_W / 2.0f + x - camX - s.pivotX);
    const float top = std::round(plat::TOP_H / 2.0f - (y - camY) - (s.h - s.pivotY));
    plat::drawSprite(art.sheet, s.x, s.y, s.w, s.h, left, top);
}

int frameAt(const TitleArt::Image& im, float t) {
    if (im.frames.size() <= 1 || im.frameTime <= 0) return im.frames.empty() ? -1 : im.frames[0];
    return im.frames[(size_t)(t / im.frameTime) % im.frames.size()];
}

} // namespace

bool TitleArt::load(std::string* error) {
    unload();
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        unload();
        return false;
    };
    std::string text, sheetText;
    Json doc, sheetDoc;
    if (!plat::readFile("ui/title.json", text) || !Json::parse(text, doc, error)) return fail("can't read ui/title.json");
    if (!plat::readFile("ui/title_sheet.json", sheetText) || !Json::parse(sheetText, sheetDoc, error))
        return fail("can't read ui/title_sheet.json");
    // Sprites by name, as title.json names them.
    std::map<std::string, int> index;
    for (const auto& [name, s] : sheetDoc["sprites"].members()) {
        const Json& r = s["rect"];
        const Json& p = s["pivot"];
        if (r.size() != 4 || p.size() != 2) return fail("ui/title_sheet.json: bad sprite " + name);
        index[name] = (int)sprites.size();
        sprites.push_back({r.at(0).asInt(), r.at(1).asInt(), r.at(2).asInt(), r.at(3).asInt(), p.at(0).asFloat(), p.at(1).asFloat()});
    }
    bool ok = true;
    auto sprite = [&](const Json& name) {
        auto it = index.find(name.asString());
        if (it == index.end()) ok = false;
        return it == index.end() ? 0 : it->second;
    };
    auto image = [&](const Json& j) {
        Image im{j["pos"].at(0).asFloat(), j["pos"].at(1).asFloat(), {}, j["frameTime"].asFloat()};
        for (const Json& f : j["frames"].items()) im.frames.push_back(sprite(f));
        if (im.frames.empty()) ok = false;
        return im;
    };
    const Json& bg = doc["background"];
    background = plat::rgba((uint8_t)bg.at(0).asInt(), (uint8_t)bg.at(1).asInt(), (uint8_t)bg.at(2).asInt());
    for (const Json& s : doc["stars"].items())
        stars.push_back({s["pos"].at(0).asFloat(), s["pos"].at(1).asFloat(), sprite(s["on"]), sprite(s["off"])});
    for (const Json& i : doc["images"].items()) images.push_back(image(i));
    // The port's fit to the top screen (tools/make_title_extra.py): the background spread out to
    // 400x240, then the stars added in its gaps.
    std::string extraText;
    Json extra;
    if (!plat::readFile("ui/title_extra.json", extraText) || !Json::parse(extraText, extra, error))
        return fail("can't read ui/title_extra.json");
    const float spreadX = extra["spread"].at(0).asFloat(), spreadY = extra["spread"].at(1).asFloat();
    if (spreadX <= 0 || spreadY <= 0) return fail("ui/title_extra.json: bad spread");
    for (Star& s : stars) {
        s.x *= spreadX;
        s.y *= spreadY;
    }
    for (Image& im : images) {
        im.x *= spreadX;
        im.y *= spreadY;
    }
    for (const Json& s : extra["stars"].items())
        stars.push_back({s["pos"].at(0).asFloat(), s["pos"].at(1).asFloat(), sprite(s["on"]), sprite(s["off"])});
    const Json& l = doc["logo"];
    logo.originX = l["origin"].at(0).asFloat();
    logo.originY = l["origin"].at(1).asFloat();
    logo.restY = l["restY"].asFloat();
    for (const Json& le : l["letters"].items()) {
        const Json& s = le["sin"];
        logo.letters.push_back({sprite(le["sprite"]), s.at(0).asFloat(), s.at(1).asFloat(), s.at(2).asFloat(), s.at(3).asFloat()});
    }
    logo.slideIn = readKeys(l["slideIn"]);
    logo.riseX = readKeys(l["riseX"]);
    logo.riseY = readKeys(l["riseY"]);
    logo.slideInLength = l["slideInLength"].asFloat();
    player.image = image(doc["player"]);
    player.riseY = readKeys(doc["player"]["riseY"]);
    const Json& p = doc["prompt"];
    prompt.text = p["text"].asString();
    prompt.x = p["pos"].at(0).asFloat();
    prompt.y = p["pos"].at(1).asFloat();
    const Json& c = p["color"];
    prompt.color = plat::rgba((uint8_t)c.at(0).asInt(), (uint8_t)c.at(1).asInt(), (uint8_t)c.at(2).asInt());
    prompt.fade = readKeys(p["fade"]);
    prompt.fadeOutSpeed = p["fadeOutSpeed"].asFloat(1);
    if (!ok || logo.letters.empty() || logo.slideIn.empty() || logo.riseY.empty() || player.riseY.empty() ||
        prompt.fade.empty() || prompt.fadeOutSpeed <= 0)
        return fail("ui/title.json: a missing sprite, curve or timing");
    sheet = plat::loadSheet(sheetDoc["sheet"].asString().c_str());
    if (!sheet) return fail("can't load the title sheet " + sheetDoc["sheet"].asString());
    return true;
}

void TitleArt::unload() {
    if (sheet) plat::freeSheet(sheet);
    sheet = nullptr;
    sprites.clear();
    stars.clear();
    images.clear();
    logo.letters.clear();
}

float TitleScreen::random(float lo, float hi) {
    seed_ = seed_ * 1664525u + 1013904223u; // (an LCG: the twinkle only needs to look random)
    return lo + (hi - lo) * (float)(seed_ >> 8) / (float)(1u << 24);
}

void TitleScreen::start(const TitleArt& art) {
    *this = TitleScreen();
    stars_.resize(art.stars.size());
    for (StarState& s : stars_) s.timeTillSwap = random(0, MAX_TIME_ON); // (LightData's constructor)
    shake_.reset();
}

bool TitleScreen::update(const CameraShakeDef& noise, const plat::Input& in, float dt, float screenShake) {
    // OpenCutscene: from its second frame any key starts the menu (InputSystem.onAnyButtonPress).
    const bool starts = !menuShown() && time_ > 0 && (in.pressed != 0 || in.touching);
    if (starts) {
        upAt_ = time_; // StartMainMenu: StopAllCoroutines, StopShake, isUp, the prompt hides
        cutscene_ = false;
        shake_.stopShake();
        if (promptShownAt_ >= 0) promptHiddenAt_ = time_;
    }
    if (cutscene_) {
        if (step_ == 0 && time_ >= SHAKE_RISE_AT) {
            shake_.shakeIncrease(SHAKE_RISE_DURATION, SHAKE_RISE_AMOUNT);
            step_++;
        } else if (step_ == 1 && time_ >= SHAKE_AT) {
            shake_.shake(SHAKE_DURATION, SHAKE_AMOUNT);
            step_++;
        } else if (step_ == 2 && time_ >= PROMPT_AT) {
            promptShownAt_ = time_;
            step_++;
        }
    }
    // MainMenuBGStars.Update.
    for (StarState& s : stars_) {
        s.timeTillSwap -= dt;
        if (s.timeTillSwap > 0) continue;
        s.enabled = !s.enabled;
        s.showsOn = s.enabled;
        s.timeTillSwap = s.enabled ? random(MAX_TIME_ON / 2, MAX_TIME_ON) : random(MAX_TIME_OFF / 2, MAX_TIME_OFF);
    }
    shake_.updateShake(noise, dt, screenShake);
    time_ += dt;
    return starts;
}

float TitleScreen::promptAlpha(const TitleArt& art) const {
    if (promptShownAt_ < 0) return 0;
    const std::vector<CurveKey>& fade = art.prompt.fade;
    if (promptHiddenAt_ >= 0) // AnyKeyFadeOut: the fade-in clip backwards from its end, fadeOutSpeed times as fast
        return std::max(0.0f, evalKeys(fade, fade.back().t - (time_ - promptHiddenAt_) * art.prompt.fadeOutSpeed));
    return std::min(1.0f, evalKeys(fade, time_ - promptShownAt_));
}

void TitleScreen::draw(const TitleArt& art, const Font& font) const {
    plat::beginScreen(plat::Screen::Top, art.background);
    const float camX = shake_.shakeX() * PX_PER_UNIT, camY = shake_.shakeY() * PX_PER_UNIT;
    for (size_t i = 0; i < art.stars.size(); i++)
        drawAt(art, stars_[i].showsOn ? art.stars[i].on : art.stars[i].off, art.stars[i].x, art.stars[i].y, camX, camY);
    for (const TitleArt::Image& im : art.images) drawAt(art, frameAt(im, time_), im.x, im.y, camX, camY);
    // The player, then the logo over it (the canvas draws after the sprites).
    const float rise = menuShown() ? time_ - upAt_ : -1;
    const TitleArt::Image& p = art.player.image;
    drawAt(art, frameAt(p, time_), p.x, p.y + (rise >= 0 ? evalKeys(art.player.riseY, rise) : 0), camX, camY);
    float lx, ly;
    if (rise >= 0) {
        lx = evalKeys(art.logo.riseX, rise);
        ly = evalKeys(art.logo.riseY, rise);
    } else {
        lx = time_ < art.logo.slideInLength ? evalKeys(art.logo.slideIn, time_) : 0; // (TitleIn: x 0)
        ly = art.logo.restY;
    }
    for (const TitleArt::Letter& l : art.logo.letters)
        drawAt(art, l.sprite, art.logo.originX + lx, art.logo.originY + ly + l.a * std::sin(l.b * time_ + l.c) + l.d, camX, camY);
    const float alpha = promptAlpha(art);
    if (alpha > 0) {
        const GlyphRect& cap = font.def().glyph('A').text;
        const float cy = plat::TOP_H / 2.0f - (art.prompt.y - camY);
        const float baseline = std::round(cy - (cap.oy + cap.h / 2.0f));
        const float x = std::round(plat::TOP_W / 2.0f + art.prompt.x - camX - font.textWidth(art.prompt.text) / 2.0f);
        font.drawText(art.prompt.text, x, baseline, 1, palette::withAlpha(art.prompt.color, (uint8_t)std::lround(alpha * 255)));
    }
}
