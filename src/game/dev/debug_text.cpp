#include "game/dev/debug_text.h"

#include "platform/platform.h"

namespace {

// Five rows of three bits each, top row first; bit 2 is the left column.
struct Glyph {
    char ch;
    uint8_t rows[5];
};

constexpr Glyph GLYPHS[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 7, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}}, {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}}, {'.', {0, 0, 0, 0, 2}}, {'-', {0, 0, 7, 0, 0}},
    {':', {0, 2, 0, 2, 0}}, {'C', {7, 4, 4, 4, 7}}, {'F', {7, 4, 7, 4, 4}}, {'G', {7, 4, 5, 5, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'P', {7, 5, 7, 4, 4}}, {'R', {7, 5, 6, 5, 5}}, {'S', {7, 4, 7, 1, 7}},
    {'U', {5, 5, 5, 5, 7}}, {'A', {7, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}}, {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 7, 4, 7}},
    {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}}, {'L', {4, 4, 4, 4, 7}}, {'N', {7, 5, 5, 5, 5}},
    {'O', {7, 5, 5, 5, 7}}, {'T', {7, 2, 2, 2, 2}}, {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
    {'Y', {5, 5, 7, 2, 2}}, {'V', {5, 5, 5, 5, 2}}, {'%', {5, 1, 2, 4, 5}}, {'/', {1, 1, 2, 4, 4}}, {'J', {1, 1, 1, 5, 7}},
};

const Glyph* findGlyph(char ch) {
    for (const Glyph& g : GLYPHS)
        if (g.ch == ch) return &g;
    return nullptr;
}

} // namespace

float drawDebugText(float x, float y, const char* text, uint32_t color, int scale) {
    for (; *text; text++, x += 4 * scale) {
        const Glyph* g = findGlyph(*text);
        if (!g) continue;
        for (int row = 0; row < 5; row++) // (a rect per run of lit pixels: the overlay draws ~1k glyph rows)
            for (int col = 0; col < 3;) {
                if (!(g->rows[row] & (4 >> col))) {
                    col++;
                    continue;
                }
                int end = col + 1;
                while (end < 3 && (g->rows[row] & (4 >> end))) end++;
                plat::drawRect(x + col * scale, y + row * scale, (float)((end - col) * scale), (float)scale, color);
                col = end;
            }
    }
    return x;
}
