// The top screen's overlays over the world view, in the original's canvas order: the cutscene skip
// prompt (DollyCanvas), then EffectCanvas: the hint box (game/hints), ItemPickupEffect's "<item>
// Acquired!", UIEffects' black and white panels (the original's acquired text is above the panels;
// the two never meet). Drawn at 1x: the world's pixel size, as the original's sit over its 320x180
// view. (Next: area titles.)
#pragma once

#include "game/render/font.h"

class World;

// Everything above the world view on the top screen: `menuFont` the skip prompt's (the menus' TinyUnicode),
// `caption` the hint's and the pick-up's (PixelTitanZero: MenuSkin loads both).
void drawTopOverlays(const World& world, const Font& menuFont, const Font& caption);
// The skip prompt (Skip Prompt: "Skip" over a bar filling 0-1) in the bottom-right of a screen
// `screenW` x `screenH`.
void drawSkipPrompt(const Font& font, int screenW, int screenH, float fill);
