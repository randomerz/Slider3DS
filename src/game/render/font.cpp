#include "game/render/font.h"

#include <cmath>

#include "game/data/json.h"
#include "platform/platform.h"

const Glyph& FontDef::glyph(char c) const {
    const unsigned u = (unsigned char)c;
    if (u < glyphs.size() && (glyphs[u].advance > 0 || glyphs[u].text.w > 0)) return glyphs[u];
    static const Glyph none;
    return (unsigned)'?' < glyphs.size() ? glyphs['?'] : none;
}

static GlyphRect readRect(const Json& j) {
    GlyphRect r;
    if (j.size() == 6) r = {j.at(0).asInt(), j.at(1).asInt(), j.at(2).asInt(), j.at(3).asInt(), j.at(4).asInt(), j.at(5).asInt()};
    return r;
}

bool loadFont(const char* path, FontDef& out, std::string* error) {
    std::string text;
    Json doc;
    if (!plat::readFile(path, text) || !Json::parse(text, doc, error)) {
        if (error && error->empty()) *error = std::string("can't read ") + path;
        return false;
    }
    out = FontDef();
    out.sheet = doc["sheet"].asString();
    out.ppem = doc["ppem"].asInt(16);
    out.descent = doc["descent"].asInt();
    for (const Json& g : doc["glyphs"].items()) {
        const int c = g["c"].asInt(-1);
        if (c < 0 || c > 127) continue; // the port's text is ASCII
        if ((int)out.glyphs.size() <= c) out.glyphs.resize(c + 1);
        out.glyphs[c] = {g["adv"].asInt(), readRect(g["text"]), readRect(g["fill"])};
    }
    if (out.sheet.empty() || out.glyphs.empty()) {
        if (error) *error = std::string(path) + ": no sheet or glyphs";
        return false;
    }
    return true;
}

bool Font::load(const char* path, std::string* error) {
    unload();
    if (!loadFont(path, def_, error)) return false;
    texture_ = plat::loadSheet(def_.sheet.c_str());
    if (!texture_ && error) *error = "can't load font sheet " + def_.sheet;
    return texture_ != nullptr;
}

void Font::unload() {
    if (texture_) plat::freeSheet(texture_);
    texture_ = nullptr;
}

void Font::draw(const GlyphRect& r, float x, float y, int scale, uint32_t color) const {
    if (r.w > 0) plat::drawSprite(texture_, r.x, r.y, r.w, r.h, std::round(x), std::round(y), 0, scale, color);
}

void Font::drawText(std::string_view text, float x, float baseline, int scale, uint32_t color) const {
    for (bool letters : {false, true}) {
        float pen = x;
        for (char c : text) {
            const Glyph& g = def_.glyph(c);
            const GlyphRect& r = letters ? g.text : g.fill;
            draw(r, pen + r.ox * scale, baseline + r.oy * scale, scale, letters ? color : 0);
            pen += g.advance * scale;
        }
    }
}

std::vector<std::string> Font::wrap(std::string_view text, int scale, int maxWidth) const {
    std::vector<std::string> lines;
    std::string line;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find(' ', at);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view word = text.substr(at, end - at);
        at = end + 1;
        if (word.empty()) continue; // (runs of spaces: one)
        std::string joined = line.empty() ? std::string(word) : line + " " + std::string(word);
        if (!line.empty() && textWidth(joined, scale) > maxWidth) {
            lines.push_back(std::move(line));
            line = std::string(word);
        } else {
            line = std::move(joined);
        }
    }
    lines.push_back(std::move(line));
    return lines;
}

int Font::textWidth(std::string_view text, int scale) const {
    int w = 0;
    for (char c : text) w += def_.glyph(c).advance;
    return w * scale;
}
