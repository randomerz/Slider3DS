// NPC dialogue text the way the original shows it (DialogueDisplay: TextMeshPro plus its helpers):
//   - parseDialogue: TMPSpecialText's tags (<var>/<string> replaced from the save profile, effect
//     tags <wavy>/<type>/... turned into character ranges);
//   - layoutDialogue: TMP's word-wrapped, centered, bottom-aligned layout in the display's box;
//   - Typer: TMPTextTyper typing it out (fading each character in) and the effects' clocks.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/area.h"
#include "game/font.h"
#include "game/game_state.h"

struct DialogueText {
    std::string chars;         // what TMP lays out: tags removed, <var>s replaced, curly quotes / ellipses ASCII
    std::vector<uint8_t> wavy; // per char: in a <wavy>/<wave> range
    int typeFirst = -1, typeLast = -1; // the <type> range (retyped in a loop), -1 = none
};

// TMP's characterInfo[i].isVisible: whitespace has no glyph (it's typed, but draws nothing).
inline bool glyphVisible(char c) { return c != ' ' && c != '\n' && c != '\t'; }

DialogueText parseDialogue(const std::string& text, const GameState& state);

// The pen position of a character on its line's baseline: px from the box's bottom-left, y up.
struct GlyphPlace {
    float x = 0, y = 0;
};

// Places every character of `text` in `box` (sizes in world units, `ppu` px each), like TMP:
// words wrap at the box width (a word wider than it gets its own line), lines are centered, and
// the last line's descender sits on the box bottom (text grows upward). A trailing line break adds
// no empty line (TMP aligns by the characters' descenders).
void layoutDialogue(const FontDef& font, const DialogueText& text, const NpcTextDef& box, float ppu,
                    std::vector<GlyphPlace>& out);

// TMPTextTyper with fadeTextIn (the NPC prefab's), and the TMPSpecialText effect clocks:
//   - every TEXT_SPEED s one more character starts fading in (in steps of 127 alpha: rollover
//     spread 2); typing finishes one step after the last character is opaque;
//   - <type>: once its characters are typed, they're retyped in a loop, one per TYPE_LOOP_SPEED s;
//   - <wavy>: characters bob by WAVE_AMPLITUDE (world units) on a sine stepping every WAVE_STEP s.
class Typer {
public:
    static constexpr float TEXT_SPEED = 0.03f;     // GameSettings.textSpeed
    static constexpr float TYPE_LOOP_SPEED = 0.2f; // NPC prefab TMPSpecialText.typeLoopSpeed
    static constexpr float TYPE_LOOP_PAUSE = 0.2f; // LoopType's wait once its range is typed (hard-coded)
    static constexpr float WAVE_STEP = 0.05f;
    static constexpr float WAVE_AMPLITUDE = 0.05f; // waveAmplitude 0.01 * 5
    static constexpr float WAVE_PERIOD = 0.5f;     // wavePeriod 1 * 0.5

    // Starts typing `text` (its first character starts fading in at once).
    void start(const DialogueText& text);
    // Advances the clocks by one frame. Unity's WaitForSeconds counts from the frame after it was
    // started, so the first tick after start() only marks that frame.
    void tick(float dt);
    // TMPTextTyper.TrySkipText: shows everything and finishes, if still typing.
    void skip();
    // Its object was deactivated: the coroutines stop for good (typing never finishes by itself,
    // effects freeze) until the next start(). skip() still works.
    void halt() { halted_ = true; }

    bool typing() const { return running_; }
    bool finished() const { return finished_; }
    // Character i is drawn: typed in (any alpha) and not hidden by the <type> loop.
    bool shown(int i) const;
    // <wavy> characters' vertical offset (world units, up).
    float waveOffset(int i) const;

private:
    void step(); // one iteration of TMPTextTyper.TypeStringFadeIn's loop

    std::vector<uint8_t> alpha_, visible_, wavy_;
    int typeFirst_ = -1, typeLast_ = -1;
    int start_ = 0, index_ = 0; // startingCharacterIndex, charIndex
    bool rangeMax_ = false, running_ = false, finished_ = false, fresh_ = false, halted_ = false;
    float wait_ = 0;        // since the last step
    float time_ = 0;        // since start (effects)
    float loopFrom_ = -1;   // time the <type> range was all typed, -1 = not yet
};
