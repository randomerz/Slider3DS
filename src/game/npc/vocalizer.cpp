#include "game/npc/vocalizer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

bool isPunct(char c) { return c == '.' || c == ',' || c == '!' || c == '?'; }
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
bool isAlnum(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
bool isVowel(char c) { return std::strchr("aeiouy", c) != nullptr && c != 0; }

// WordVocalizer.vowelDescriptionTable's openness.
float openness(char v) {
    switch (v) {
    case 'a': return 1.0f;
    case 'e': return 0.3f;
    case 'o': return 0.3f;
    default: return 0.0f; // i, u, y
    }
}

enum class Intonation { Flat, Up, Down };

struct Word {
    std::string chars;                // lowercased (digits as random vowels)
    std::vector<std::string> spoken;  // its stressed vowel clusters (else its first)
};

// WordVocalizer.MakeSpokenVocalizer: the clusters, stress placed (PlaceStress).
Word spokenWord(const std::string& raw, VoiceRandom& random) {
    static const char VOWELS[] = "aeiouy";
    Word w;
    for (char c : raw) {
        if (c >= '0' && c <= '9') c = VOWELS[std::min((int)(random.value() * 6), 5)];
        w.chars += (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    std::vector<std::string> vowelClusters;
    bool lastVowel = false;
    for (size_t i = 0; i < w.chars.size(); i++) {
        const bool vowel = isVowel(w.chars[i]);
        if (vowel && (i == 0 || !lastVowel)) vowelClusters.emplace_back();
        if (vowel) vowelClusters.back() += w.chars[i];
        lastVowel = vowel;
    }
    if (vowelClusters.empty()) vowelClusters.push_back(std::string(1, VOWELS[std::min((int)(random.value() * 6), 5)]));
    std::vector<uint8_t> stressed(vowelClusters.size(), 0);
    const int n = (int)vowelClusters.size();
    if (n == 1) {
        stressed[0] = vowelClusters[0].size() == 1 && w.chars.size() < 3;
    } else if (n == 2) {
        stressed[random.value() < 0.6f ? 0 : 1] = 1;
    } else {
        const int count = std::max(1, (int)std::nearbyint(random.value() * n / 2.0f)); // (Mathf.RoundToInt: to even)
        std::vector<int> pool(n);
        for (int i = 0; i < n; i++) pool[i] = i;
        for (int k = 0; k < count && !pool.empty(); k++) {
            const int idx = (int)std::floor(random.value() * pool.size());
            stressed[pool[idx]] = 1;
            if (idx != (int)pool.size() - 1) pool.erase(pool.begin() + idx + 1); // (its neighbour can't be stressed too)
            pool.erase(pool.begin() + idx);
        }
    }
    for (int i = 0; i < n; i++)
        if (stressed[i]) w.spoken.push_back(vowelClusters[i]);
    if (w.spoken.empty()) w.spoken.push_back(vowelClusters[0]);
    return w;
}

} // namespace

float VoiceRandom::value() {
    s_ ^= s_ << 13; // (xorshift32)
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return (s_ >> 8) / 16777216.0f;
}

std::vector<std::string> voiceClauses(const std::string& s) {
    // [^.,!?]+ greedy, then [.,!?\s]+ greedy, then (?=[^.,!?\s]|$) — which the greedy run always
    // meets; when the first part reaches the end, it gives back up to a whitespace.
    std::vector<std::string> out;
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        size_t aMax = i;
        while (aMax < n && !isPunct(s[aMax])) aMax++;
        size_t end = 0;
        for (size_t a = aMax; a > i && end == 0; a--) {
            size_t b = a;
            while (b < n && (isPunct(s[b]) || isSpace(s[b]))) b++;
            if (b > a) end = b;
        }
        if (end == 0) {
            i++;
            continue;
        }
        out.push_back(s.substr(i, end - i));
        i = end;
    }
    return out;
}

VoiceLine vocalize(const VoiceDef& voice, int emote, const std::string& text, float textSpeedMultiplier,
                   VoiceRandom& random) {
    // The preset with the emote's modifier (VocalizerParameters.ModifyWith).
    VoiceDef p = voice;
    if (emote >= 0 && emote < (int)voice.emotes.size()) {
        const VoiceDef::Mod& m = voice.emotes[emote];
        p.pitch += m.pitch;
        p.volumeDb += m.volumeDb;
        p.duration += m.duration;
        if (m.intonation >= 0) p.intonation = m.intonation;
    }
    VoiceLine line;
    line.volume = p.volume * std::pow(10.0f, p.volumeDb / 20);
    const float mult = textSpeedMultiplier > 0 ? textSpeedMultiplier : 1;
    bool low = false; // VocalRandomizationContext.isCurrentWordLow (carries over)
    float lastVowelOpenness = 0; // VocalizationContext.vowelOpenness (the pauses wait times it)
    for (const std::string& clause : voiceClauses(text)) {
        if (clause.size() <= 1) continue;
        // SentenceVocalizer: its words and punctuation.
        std::vector<Word> words;
        char punctuation = 0;
        std::vector<std::pair<size_t, size_t>> runs;
        for (size_t i = 0; i < clause.size();) {
            if (!isAlnum(clause[i])) {
                i++;
                continue;
            }
            size_t j = i;
            while (j < clause.size() && isAlnum(clause[j])) j++;
            runs.push_back({i, j});
            i = j;
        }
        if (runs.empty()) continue;
        for (size_t r = 0; r < runs.size(); r++) {
            const size_t next = r + 1 < runs.size() ? runs[r + 1].first : clause.size();
            for (size_t k = runs[r].second; k < next; k++)
                if (isPunct(clause[k])) {
                    punctuation = clause[k];
                    break;
                }
            words.push_back(spokenWord(clause.substr(runs[r].first, runs[r].second - runs[r].first), random));
        }
        if (punctuation == 0) punctuation = '.';
        const int wordCount = (int)words.size() + 1; // (and the clause's ending pause)
        Intonation intonation = Intonation::Flat;
        if (punctuation == '?') {
            intonation = Intonation::Up;
            static const char* QUESTIONS[] = {"who", "what", "when", "where", "why", "how"};
            for (const char* q : QUESTIONS)
                if (words[0].chars.rfind(q, 0) == 0 && random.value() < 0.75f) {
                    intonation = Intonation::Down;
                    break;
                }
        } else if (punctuation == '.' || punctuation == '!') {
            intonation = wordCount <= 3 && random.value() < 0.5f ? Intonation::Up : Intonation::Down;
        }
        if (p.intonation >= 0) intonation = (Intonation)p.intonation;
        const float intonated = intonation == Intonation::Up ? p.pitch * (1 + p.up)
                                : intonation == Intonation::Down ? p.pitch * (1 - p.down) : p.pitch;

        // RandomizeVocalization, word by word (PreRandomize sets each word's pitch frame).
        for (size_t w = 0; w <= words.size(); w++) {
            const bool first = w == 0, last = w + 1 == words.size(), pause = w == words.size();
            if (first) {
                low = random.value() > (wordCount <= 3 ? 0.75f : 0.25f);
            } else if (last) {
                if (intonation == Intonation::Up) low = false;
                else if (intonation == Intonation::Down) low = true;
            } else {
                low = low ? !(random.value() < p.lowToHigh) : random.value() < p.highToLow;
            }
            if (pause) { // PauseVocalizer: the clause gap, waited out times the vowel openness
                VoiceStep s;
                s.duration = p.clauseGap / mult * lastVowelOpenness;
                line.steps.push_back(s);
                line.totalDuration += p.clauseGap / mult;
                break;
            }
            const float base = p.pitch, peak = last ? intonated : p.pitch;
            float lastPitch = base;
            for (const std::string& cluster : words[w].spoken) { // PhonemeClusterVocalizer
                const float each = p.duration * (low ? 1 : 1 - p.speedup) / mult;
                const float k = low ? 1 - p.wordIntonation : 1 + p.wordIntonation;
                auto jitter = [&] { return p.syllables ? 1.0f : 1 + (random.value() - 0.5f) * 0.1f; };
                VoiceStep s;
                s.sound = true;
                s.duration = each * cluster.size();
                s.vowel = cluster[0] == 'y' ? 'i' : cluster[0];
                s.pitchStart = std::clamp(lastPitch, 0.5f, 2.0f);
                s.pitchMid = std::clamp(peak * k * jitter(), 0.5f, 2.0f);
                s.pitchEnd = std::clamp(base * k * jitter(), 0.5f, 2.0f);
                lastPitch = s.pitchEnd;
                line.steps.push_back(s);
                line.totalDuration += s.duration;
                lastVowelOpenness = p.syllables ? openness(cluster.back() == 'y' ? 'i' : cluster.back()) : 0.5f;
            }
        }
    }
    return line;
}
