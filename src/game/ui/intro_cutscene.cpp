#include "game/ui/intro_cutscene.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"
#include "game/ui/palette.h"

bool IntroArt::load(std::string* error) {
    unload();
    std::string text;
    Json doc;
    if (!plat::readFile("ui/intro.json", text) || !Json::parse(text, doc, error)) {
        if (error && error->empty()) *error = "can't read ui/intro.json";
        return false;
    }
    for (const Json& s : doc["slides"].items()) {
        const Json& r = s["rect"];
        Slide sl{r.at(0).asInt(-1), r.at(1).asInt(-1), r.at(2).asInt(), r.at(3).asInt(), s["text"].asString()};
        if (r.size() != 4 || sl.x < 0 || sl.y < 0 || sl.w <= 0 || sl.h <= 0 || sl.w > plat::TOP_W || sl.h > plat::TOP_H) {
            if (error) *error = "ui/intro.json: a bad slide rect";
            slides_.clear();
            return false;
        }
        slides_.push_back(std::move(sl));
    }
    const Json& box = doc["textBox"]; // [w, h, the middle's y above the bottom]
    imageY_ = doc["imageY"].asInt();
    textW_ = box.at(0).asInt();
    textBottom_ = (int)std::lround(box.at(2).asFloat() - box.at(1).asFloat() / 2);
    if (slides_.empty() || textW_ <= 0 || textW_ > plat::TOP_W || textBottom_ < 0) {
        if (error) *error = "ui/intro.json: no slides or a bad text box";
        slides_.clear();
        return false;
    }
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) {
        if (error) *error = "can't load the intro sheet " + doc["sheet"].asString();
        slides_.clear();
        return false;
    }
    return true;
}

void IntroArt::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
    slides_.clear();
}

void IntroArt::drawImage(int slide, int cx, int cy, float alpha) const {
    const Slide& s = slides_[slide];
    const uint8_t a = (uint8_t)std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255);
    plat::drawSpriteTinted(sheet_, s.x, s.y, s.w, s.h, (float)cx, (float)cy, 0, plat::rgba(255, 255, 255, a));
}

void IntroCutscene::start(const IntroArt& art) {
    lengths_.clear();
    for (const IntroArt::Slide& s : art.slides()) lengths_.push_back((int)s.text.size());
    slide_ = -1;
    next();
}

void IntroCutscene::next() {
    slide_++;
    step_ = Step::Typing;
    fadingIn_ = true;
    skip_ = fastFade_ = false;
    t_ = 0;
    chars_ = 0;
}

bool IntroCutscene::update(bool pressed, float dt) {
    if (done()) return true;
    if (pressed) skip_ = true;
    if (fadingIn_) { // FadeIn (a press leaves skipImages set: the typing takes it too)
        alpha_ = skip_ ? 1 : std::min(1.0f, alpha_ + dt / FADE_TIME);
        fadingIn_ = alpha_ < 1;
    }
    t_ += dt;
    switch (step_) {
    case Step::Typing: { // scrolltext
        if (t_ < TYPE_DELAY) break;
        const int length = lengths_[slide_];
        chars_ = skip_ ? length : std::min(length, 1 + (int)((t_ - TYPE_DELAY) / CHAR_TIME));
        if (chars_ >= length) {
            skip_ = false;
            step_ = Step::Holding;
            t_ = 0;
        }
        break;
    }
    case Step::Holding: // WaitToAdvance
        if (skip_ || t_ >= HOLD_TIME) {
            fastFade_ = skip_;
            skip_ = false;
            step_ = Step::FadingOut;
        }
        break;
    case Step::FadingOut: // FadeOut, then advanceImages
        alpha_ = skip_ ? 0 : std::max(0.0f, alpha_ - dt / FADE_TIME * (fastFade_ ? 2 : 1));
        if (alpha_ <= 0) next();
        break;
    }
    return done();
}

void IntroCutscene::drawTop(const IntroArt& art, const Font& caption) const {
    plat::beginScreen(plat::Screen::Top, palette::DARK); // (the camera's background)
    if (done() || alpha_ <= 0) return;
    art.drawImage(slide_, plat::TOP_W / 2, plat::TOP_H / 2 - art.imageY(), alpha_);
    // The caption: the original's lines word-wrapped to the box, centered, the last line's
    // descent on the box's bottom; typed in place (each line laid out whole).
    const std::string& text = art.slides()[slide_].text;
    std::vector<std::string> lines;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        for (std::string& l : caption.wrap(text.substr(at, end - at), 1, art.textW())) lines.push_back(std::move(l));
        at = end + 1;
    }
    const uint32_t color = palette::withAlpha(palette::OFF_WHITE, (uint8_t)std::lround(alpha_ * 255));
    const int last = plat::TOP_H - art.textBottom() - caption.def().descent;
    int left = chars_;
    for (int i = 0; i < (int)lines.size() && left > 0; i++) {
        const std::string& l = lines[i];
        const int x = (plat::TOP_W - caption.textWidth(l)) / 2;
        const int y = last - ((int)lines.size() - 1 - i) * LINE_STEP;
        caption.drawText(std::string_view(l).substr(0, (size_t)std::min<int>(left, (int)l.size())), (float)x, (float)y, 1, color);
        left -= (int)l.size() + 1; // (and the space or line break it ended at)
    }
}
