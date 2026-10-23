// The top screen's overlays over the world view, in the original's canvas order: the cutscene skip
// prompt (DollyCanvas), then EffectCanvas: the hint box (game/hints), ItemPickupEffect's "<item>
// Acquired!", UIEffects' black and white panels (the original's acquired text is above the panels;
// the two never meet). Drawn at 1x: the world's pixel size, as the original's sit over its 320x180
// view. (Next: area titles.)
#pragma once

#include <string>
#include <vector>

#include "game/render/font.h"

class MenuSkin;
class World;

// Everything above the world view on the top screen (the skin's fonts).
void drawTopOverlays(const World& world, const MenuSkin& skin);
// The skip prompt (Skip Prompt: "Skip" over a bar filling 0-1) in the bottom-right of a screen
// `screenW` x `screenH`.
void drawSkipPrompt(const Font& font, int screenW, int screenH, float fill);
// `text` word-wrapped to lines at most `maxWidth` px wide at `scale`x (greedy, on spaces; a word
// wider than that gets a line of its own).
std::vector<std::string> wrapLines(const Font& font, const std::string& text, int scale, int maxWidth);
