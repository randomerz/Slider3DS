// The top screen's overlays over the world view, in the original's canvas order: the cutscene skip
// prompt (DollyCanvas), then UIEffects' black and white panels (EffectCanvas, above it). Drawn at 1x:
// the world's pixel size, as the original's sit over its 320x180 view. (Next: the item pickup
// effect, trackers, area titles.)
#pragma once

#include "game/render/font.h"

class World;

// Everything above the world view on the top screen (`font`: the TinyUnicode menu font).
void drawTopOverlays(const World& world, const Font& font);
// The skip prompt (Skip Prompt: "Skip" over a bar filling 0-1) in the bottom-right of a screen
// `screenW` x `screenH`.
void drawSkipPrompt(const Font& font, int screenW, int screenH, float fill);
