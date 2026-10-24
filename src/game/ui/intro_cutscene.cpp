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
    auto fail = [&](const std::string& why) {
        if (error) *error = "ui/intro.json: " + why;
        slides_.clear();
        return false;
    };
    if (!doc.has("slides") || !doc.has("imageY") || !doc.has("textBox") || !doc.has("wordSpacing") || !doc.has("sheet"))
        return fail("a key is missing");
    for (const Json& s : doc["slides"].items()) {
        const Json& r = s["rect"];
        Slide sl{r.at(0).asInt(-1), r.at(1).asInt(-1), r.at(2).asInt(), r.at(3).asInt(), s["text"].asString(), 0};
        if (!s.has("text") || r.size() != 4 || sl.x < 0 || sl.y < 0 || sl.w <= 0 || sl.h <= 0 || sl.w > plat::TOP_W ||
            sl.h > plat::TOP_H)
            return fail("a bad slide");
        for (char c : sl.text) {
            if ((unsigned char)c >= 0x80) return fail("a caption isn't ASCII");
            sl.visible += c != ' ' && c != '\n';
        }
        slides_.push_back(std::move(sl));
    }
    const Json& box = doc["textBox"]; // [w, h, the middle's y above the bottom]
    imageY_ = doc["imageY"].asInt();
    textW_ = box.at(0).asInt();
    textBottom_ = (int)std::lround(box.at(2).asFloat() - box.at(1).asFloat() / 2);
    wordSpacing_ = doc["wordSpacing"].asFloat();
    if (slides_.empty() || box.size() != 3 || textW_ <= 0 || textW_ > plat::TOP_W || textBottom_ < 0)
        return fail("no slides or a bad text box");
    sheet_ = plat::loadSheet(doc["sheet"].asString().c_str());
    if (!sheet_) return fail("can't load its sheet " + doc["sheet"].asString());
    lines_.assign(slides_.size(), {});
    laidOutWith_ = nullptr;
    return true;
}

void IntroArt::unload() {
    if (sheet_) plat::freeSheet(sheet_);
    sheet_ = nullptr;
    slides_.clear();
    lines_.clear();
    laidOutWith_ = nullptr;
}

int IntroArt::spaceWidth(const Font& font) const { return (int)std::lround(font.textWidth(" ") + wordSpacing_); }

const std::vector<IntroArt::Line>& IntroArt::lines(int slide, const Font& font) const {
    if (laidOutWith_ != &font) {
        lines_.assign(slides_.size(), {});
        laidOutWith_ = &font;
    }
    std::vector<Line>& out = lines_[slide];
    if (!out.empty()) return out;
    // The original's lines, each word-wrapped (greedy) to the box with the narrower spaces.
    const std::string& text = slides_[slide].text;
    const int space = spaceWidth(font);
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        Line line;
        size_t w = at;
        while (w < end) {
            size_t we = text.find(' ', w);
            if (we == std::string::npos || we > end) we = end;
            if (we > w) {
                const std::string word = text.substr(w, we - w);
                const int width = font.textWidth(word);
                if (!line.words.empty() && line.width + space + width > textW_) {
                    out.push_back(std::move(line));
                    line = Line();
                }
                line.width += (line.words.empty() ? 0 : space) + width;
                line.words.push_back(word);
            }
            w = we + 1;
        }
        out.push_back(std::move(line));
        at = end + 1;
    }
    return out;
}

void IntroArt::drawImage(int slide, int cx, int cy, float alpha) const {
    const Slide& s = slides_[slide];
    const uint8_t a = (uint8_t)std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255);
    plat::drawSpriteTinted(sheet_, s.x, s.y, s.w, s.h, (float)cx, (float)cy, 0, plat::rgba(255, 255, 255, a));
}

void IntroCutscene::start(const IntroArt& art) {
    lengths_.clear();
    for (const IntroArt::Slide& s : art.slides()) lengths_.push_back(s.visible);
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
    // The caption: centered, the last line's descent on the box's bottom; typed in place (each line
    // laid out whole), a visible character at a time.
    const std::vector<IntroArt::Line>& lines = art.lines(slide_, caption);
    const uint32_t color = palette::withAlpha(palette::OFF_WHITE, (uint8_t)std::lround(alpha_ * 255));
    const int space = art.spaceWidth(caption);
    const int last = plat::TOP_H - art.textBottom() - caption.def().descent;
    int left = chars_;
    for (int i = 0; i < (int)lines.size() && left > 0; i++) {
        int x = (plat::TOP_W - lines[i].width) / 2;
        const int y = last - ((int)lines.size() - 1 - i) * LINE_STEP;
        for (const std::string& word : lines[i].words) {
            if (left <= 0) break;
            const size_t shown = (size_t)std::min<int>(left, (int)word.size());
            caption.drawText(std::string_view(word).substr(0, shown), (float)x, (float)y, 1, color);
            left -= (int)shown;
            x += caption.textWidth(word) + space;
        }
    }
}
