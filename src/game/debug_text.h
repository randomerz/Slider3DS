// A tiny 3x5 pixel font drawn with rects, for dev overlays (digits, '.', '-', ':', space and a few
// capitals: C F G M P R S U). Unknown characters draw as blanks. Not the game's text.
#pragma once

#include <cstdint>

// Draws `text` with its top-left at (x, y); each glyph pixel is `scale` screen pixels and glyphs
// advance 4 * scale. Returns the x after the last glyph.
float drawDebugText(float x, float y, const char* text, uint32_t color, int scale = 1);
