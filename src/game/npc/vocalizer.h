// SliderVocalization for the port: an NPC's line read out in its voice (VoiceDef), composed as
// Unity's VocalizableParagraph / SentenceVocalizer / WordVocalizer / PhonemeClusterVocalizer do:
//   - clauses: text up to , . ! ? and the spaces after it (a last stretch without either isn't read);
//   - words: runs of letters / digits (a digit reads as a random vowel); a clause's punctuation
//     sets its intonation (? rises — falls, mostly, after who / what / ...; . ! fall, short ones
//     rise half the time; , stays flat);
//   - a word's vowel clusters get stress at random; it sounds its stressed ones (else its first),
//     each a syllable of `duration` a letter (shorter on a high word: `speedup`), its pitch gliding
//     from the last word's end through the word's peak to its base; words go low / high at random
//     (a clause's first and last word by its length and intonation);
//   - each clause ends in its gap (waited out times the last vowel's openness).
// The line's emote adds its modifier (pitch, volume, duration, intonation). Not ported (the
// port's simplifications): the vowel formants as they glide (a syllable plays the rendered clip of
// its first vowel: audio.json's voices), per-clause voices (none in the Village). The stressed
// vowels' modifiers are left out as Unity applies them only after the timing and pitches are fixed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/area.h"

struct VoiceStep {
    bool sound = false; // a syllable (else a pause)
    float duration = 0; // s it lasts: the syllable, or the pause's wait
    char vowel = 'a';   // the syllable's clip: 'a', 'e', 'i', 'o' or 'u'
    float pitchStart = 1, pitchMid = 1, pitchEnd = 1; // over its first half, then its second
};

struct VoiceLine {
    std::vector<VoiceStep> steps;
    float totalDuration = 0; // the narration uninterrupted (what paces the typing: DisplaySentence)
    float volume = 1;        // linear: the preset's volume and dB adjustment (its emote's included)
};

// UnityEngine.Random's part: a small deterministic source.
class VoiceRandom {
public:
    explicit VoiceRandom(uint32_t seed = 0x9e3779b9u) : s_(seed ? seed : 1) {}
    float value(); // [0, 1)

private:
    uint32_t s_;
};

// `text`: what's typed (tags already gone); `emote`: the line's NPCEmotes value;
// `textSpeedMultiplier`: vocalized / typed length (1: the same text).
VoiceLine vocalize(const VoiceDef& voice, int emote, const std::string& text, float textSpeedMultiplier,
                   VoiceRandom& random);

// The clauses SentenceVocalizer.Parse reads (its regex, [^.,!?]+[.,!?\s]+(?=[^.,!?\s]|$)).
std::vector<std::string> voiceClauses(const std::string& text);
