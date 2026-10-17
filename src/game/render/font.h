// The game's bitmap fonts, baked from the original TTFs by tools/make_font.py (assets/fonts/<name>.json
// + its sheet): the dialogue's (TinyUnicode letters over a PixelUnicodeFill backing) and the menus'
// (TinyUnicode, drawn in a color), both 16 px per em.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace plat { struct Texture; }

// A glyph image in the font's sheet: its top-left lands at (pen x + ox, baseline + oy), px, y down.
// w == 0: nothing to draw.
struct GlyphRect {
    int x = 0, y = 0, w = 0, h = 0, ox = 0, oy = 0;
};

struct Glyph {
    int advance = 0;      // px
    GlyphRect text, fill; // the letter, and its dark backing (all backings are drawn before letters)
};

struct FontDef {
    std::string sheet;  // sprite sheet name (plat::loadSheet)
    int ppem = 16;      // px per em (dialogue: = per world unit at TMP fontSize 1)
    int descent = 0;    // px below the baseline
    std::vector<Glyph> glyphs; // by character code (ASCII)

    // Character `c`'s glyph; characters the font lacks show as '?'.
    const Glyph& glyph(char c) const;
};

bool loadFont(const char* path, FontDef& out, std::string* error = nullptr);

// A loaded font with its sheet texture, for drawing.
class Font {
public:
    Font() = default;
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    ~Font() { unload(); }

    bool load(const char* path, std::string* error = nullptr);
    void unload();

    const FontDef& def() const { return def_; }
    // Draws `r` with its top-left at screen pixel (x, y), rounded, `scale`x; a non-zero `color`
    // (RGBA) replaces the baked one (the glyph's shape in that color).
    void draw(const GlyphRect& r, float x, float y, int scale = 1, uint32_t color = 0) const;
    // One line of plain text (no markup or wrapping) at `scale`x: its pen starts at x, its baseline
    // at y; backings under letters, like the dialogue; `color` as for draw (letters only).
    // textWidth = its advance, px.
    void drawText(std::string_view text, float x, float baseline, int scale = 1, uint32_t color = 0) const;
    int textWidth(std::string_view text, int scale = 1) const;

private:
    FontDef def_;
    plat::Texture* texture_ = nullptr;
};
