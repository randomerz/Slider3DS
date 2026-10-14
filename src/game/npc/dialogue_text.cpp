#include "game/npc/dialogue_text.h"

#include <algorithm>
#include <cmath>

namespace {

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
}

// TMPSpecialText.commands that aren't text replacements (effects over a range).
bool isEffect(const std::string& c) {
    return c == "jitter" || c == "jittery" || c == "shake" || c == "shaky" || c == "wave" || c == "wavy" || c == "type";
}

} // namespace

DialogueText parseDialogue(const std::string& raw, const GameState& state) {
    // DialogueDisplay.DisplaySentence: curly quotes and ellipses become ASCII. (Line ends: \n only.)
    std::string text = raw;
    replaceAll(text, "\xE2\x80\x98", "'"); // ‘
    replaceAll(text, "\xE2\x80\x99", "'"); // ’
    replaceAll(text, "\xE2\x80\xA6", "..."); // …
    replaceAll(text, "\r", "");

    // TMPSpecialText.ParseTextFirstFrame, pass 1: <var>name</var> / <string>name</string> -> the
    // profile's string.
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '<') continue;
        const size_t close = text.find('>', i);
        if (close == std::string::npos) continue;
        const std::string cmd = text.substr(i + 1, close - i - 1);
        if (cmd != "var" && cmd != "string") continue;
        const size_t end = text.find("</" + cmd + ">", i);
        if (end == std::string::npos) continue;
        const std::string name = text.substr(close + 1, end - close - 1);
        text = text.substr(0, i) + state.str(name) + text.substr(end + cmd.size() + 3);
    }

    // Pass 2: effect tags are removed, leaving [first, last] ranges of the cleaned text.
    DialogueText out;
    struct Range {
        std::string cmd;
        int first, last;
    };
    std::vector<Range> ranges;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '<') continue;
        const size_t close = text.find('>', i);
        if (close == std::string::npos) continue;
        const std::string cmd = text.substr(i + 1, close - i - 1);
        if (!isEffect(cmd)) continue;
        const size_t end = text.find("</" + cmd + ">", i);
        if (end == std::string::npos) continue;
        const std::string inner = text.substr(close + 1, end - close - 1);
        text = text.substr(0, i) + inner + text.substr(end + cmd.size() + 3);
        ranges.push_back({cmd, (int)i, (int)(i + inner.size()) - 1});
    }
    out.chars = text;
    out.wavy.assign(text.size(), 0);
    for (const Range& r : ranges) {
        if (r.cmd == "wavy" || r.cmd == "wave")
            for (int c = std::max(0, r.first); c <= r.last && c < (int)text.size(); c++) out.wavy[c] = 1;
        if (r.cmd == "type") {
            out.typeFirst = r.first;
            out.typeLast = r.last;
        }
        // jitter / shake: not used by the Village's dialogue (drawn still).
    }
    return out;
}

void layoutDialogue(const FontDef& font, const DialogueText& text, const NpcTextDef& box, float ppu,
                    std::vector<GlyphPlace>& out) {
    const std::string& s = text.chars;
    out.assign(s.size(), GlyphPlace());
    const float width = (box.maxX - box.minX) * ppu;
    const float charSpacing = box.charSpacing * ppu, wordSpacing = box.wordSpacing * ppu;
    auto advance = [&](char c) {
        return font.glyph(c).advance + charSpacing + (c == ' ' ? wordSpacing : 0.0f);
    };

    // Lines as [first, last) character ranges, with their pen positions from the line start.
    struct Line {
        size_t first, last;
        float width; // up to the end of its last visible character
    };
    std::vector<Line> lines;
    std::vector<float> pen(s.size(), 0.0f);
    size_t lineStart = 0, wordStart = 0;
    float x = 0, wordX = 0;
    auto endLine = [&](size_t end) {
        float w = 0;
        for (size_t c = lineStart; c < end; c++)
            if (glyphVisible(s[c])) w = pen[c] + font.glyph(s[c]).advance + charSpacing;
        lines.push_back({lineStart, end, w});
    };
    for (size_t c = 0; c < s.size(); c++) {
        if (s[c] == '\n') {
            pen[c] = x;
            endLine(c + 1);
            lineStart = wordStart = c + 1;
            x = wordX = 0;
            continue;
        }
        if (s[c] == ' ') {
            pen[c] = x;
            x += advance(' ');
            wordStart = c + 1;
            wordX = x;
            continue;
        }
        // A character past the box's edge wraps its word to a new line (unless the word started
        // the line: then the word breaks here).
        if (x + advance(s[c]) > width + 0.001f && c > lineStart) {
            const size_t breakAt = wordStart > lineStart ? wordStart : c;
            endLine(breakAt);
            const float shift = breakAt == c ? x : wordX;
            for (size_t k = breakAt; k < c; k++) pen[k] -= shift;
            x -= shift;
            lineStart = wordStart = breakAt;
            wordX = 0;
        }
        pen[c] = x;
        x += advance(s[c]);
    }
    if (lineStart < s.size() || lines.empty()) endLine(s.size());

    // Bottom-aligned: the last line's baseline is `descent` above the box bottom.
    const float lineAdvance = box.lineAdvance * ppu;
    for (size_t l = 0; l < lines.size(); l++) {
        const float baseline = font.descent + (float)(lines.size() - 1 - l) * lineAdvance;
        const float left = (width - lines[l].width) * 0.5f;
        for (size_t c = lines[l].first; c < lines[l].last; c++) out[c] = {left + pen[c], baseline};
    }
}

void Typer::start(const DialogueText& text) {
    const size_t n = text.chars.size();
    alpha_.assign(n, 0);
    visible_.resize(n);
    for (size_t i = 0; i < n; i++) visible_[i] = glyphVisible(text.chars[i]);
    wavy_ = text.wavy;
    typeFirst_ = text.typeFirst;
    typeLast_ = text.typeLast;
    start_ = index_ = 0;
    rangeMax_ = false;
    running_ = true;
    finished_ = false;
    fresh_ = true;
    halted_ = false;
    wait_ = time_ = 0;
    loopFrom_ = -1;
    step(); // the coroutine's first iteration runs before its first wait
}

void Typer::step() {
    const int n = (int)alpha_.size();
    if (n == 0) return; // TMP waits for characters that never come: an empty line never finishes
    // As TypeStringFadeIn: fade in [startingCharacterIndex, charIndex]; done when every character
    // has passed (whitespace passes at once).
    const int first = start_;
    for (int i = first; i < index_ + 1; i++) {
        if (!visible_[i]) {
            start_++;
        } else {
            alpha_[i] = (uint8_t)std::min(255, alpha_[i] + 127);
            if (alpha_[i] == 255) start_++;
        }
        if (start_ == n) rangeMax_ = true;
    }
    if (index_ + 1 < n) index_++;
}

void Typer::tick(float dt) {
    if (halted_) return;
    if (fresh_) { // the frame start() ran in
        fresh_ = false;
        return;
    }
    time_ += dt;
    if (running_) {
        wait_ += dt;
        if (wait_ >= TEXT_SPEED - 1e-4f) {
            wait_ = 0;
            if (rangeMax_) {
                running_ = false;
                finished_ = true;
            } else {
                step();
            }
        }
    }
    if (typeFirst_ >= 0 && loopFrom_ < 0) {
        bool typed = true;
        for (int i = typeFirst_; i <= typeLast_ && i < (int)alpha_.size(); i++)
            typed = typed && (!visible_[i] || alpha_[i] > 0); // (TMP leaves whitespace opaque)
        if (typed) loopFrom_ = time_;
    }
}

void Typer::skip() {
    if (!running_) return;
    for (size_t i = 0; i < alpha_.size(); i++)
        if (visible_[i]) alpha_[i] = 255;
    running_ = false;
    finished_ = true;
}

bool Typer::shown(int i) const {
    if (i < 0 || i >= (int)alpha_.size() || alpha_[i] == 0) return false;
    if (i < typeFirst_ || i > typeLast_ || loopFrom_ < 0) return true;
    // LoopType: TYPE_LOOP_PAUSE after the range is typed, hide it all, then show one visible
    // character more every TYPE_LOOP_SPEED, then start over.
    const float t = time_ - loopFrom_ - TYPE_LOOP_PAUSE;
    if (t < 0) return true;
    int count = 0, rank = 0;
    for (int c = typeFirst_; c <= typeLast_ && c < (int)visible_.size(); c++) {
        if (!visible_[c]) continue;
        if (c < i) rank++;
        count++;
    }
    const int shownCount = (int)(t / TYPE_LOOP_SPEED) % (count + 1);
    return rank < shownCount;
}

float Typer::waveOffset(int i) const {
    if (i < 0 || i >= (int)wavy_.size() || !wavy_[i] || time_ <= 0) return 0;
    const int loop = (int)(time_ / WAVE_STEP);
    return WAVE_AMPLITUDE * std::sin(WAVE_PERIOD * (float)(loop - i));
}
