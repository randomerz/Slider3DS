// The vocalizer (game/npc/vocalizer): SentenceVocalizer's clauses, the words' syllables and pauses,
// the emote's modifier; and the bank's rendered synths, played as syllables.
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "game/audio/audio.h"
#include "game/npc/vocalizer.h"
#include "platform/platform.h"
#include "test.h"

namespace {

VoiceDef plainVoice() {
    VoiceDef v;
    v.present = true;
    v.event = "event:/Dialogue/romeo";
    v.pitch = 1;
    v.duration = 0.1f;
    v.clauseGap = 0.2f;
    v.emotes = {{}, {}, {0.15f, 5, -0.025f, -1}};
    return v;
}

} // namespace

TEST(vocalizer_reads_clauses_as_its_regex_does) {
    CHECK((voiceClauses("Hello there. How are you?") == std::vector<std::string>{"Hello there. ", "How are you?"}));
    CHECK((voiceClauses("Wait, what?!") == std::vector<std::string>{"Wait, ", "what?!"}));
    // A last stretch without punctuation or a space after it isn't read.
    CHECK((voiceClauses("Hello world") == std::vector<std::string>{"Hello "}));
    CHECK(voiceClauses("...").empty());
}

TEST(vocalizer_says_stressed_syllables_then_the_clause_gap) {
    VoiceRandom random;
    // "hi": one vowel cluster, one letter, a short word: stressed — one 'i' syllable a letter long,
    // then the gap (counted in full; waited times the last vowel's openness: 'i' is closed).
    const VoiceLine line = vocalize(plainVoice(), 0, "Hi!", 1, random);
    CHECK(line.steps.size() == 2 && line.steps[0].sound && line.steps[0].vowel == 'i');
    CHECK(std::fabs(line.steps[0].duration - 0.1f) < 1e-6f && !line.steps[1].sound && line.steps[1].duration == 0);
    CHECK(std::fabs(line.totalDuration - 0.3f) < 1e-6f && line.volume == 1);
    for (const VoiceStep& s : line.steps)
        CHECK(s.pitchStart >= 0.5f && s.pitchMid <= 2 && s.pitchEnd >= 0.5f);
    // "ah": 'a' is open: the gap is waited out in full.
    VoiceRandom r2;
    const VoiceLine open = vocalize(plainVoice(), 0, "Ah.", 1, r2);
    CHECK(open.steps.size() == 2 && std::fabs(open.steps[1].duration - 0.2f) < 1e-6f);
}

TEST(vocalizer_applies_the_emote_and_is_repeatable) {
    VoiceRandom a, b;
    const VoiceLine plain = vocalize(plainVoice(), 0, "Hi!", 1, a);
    const VoiceLine excited = vocalize(plainVoice(), 2, "Hi!", 1, b);
    // The emote's modifier: shorter syllables, louder.
    CHECK(std::fabs(excited.steps[0].duration - 0.075f) < 1e-6f && std::fabs(excited.volume - std::pow(10.0f, 0.25f)) < 1e-5f);
    CHECK(plain.steps[0].duration > excited.steps[0].duration);
    // The same seed reads a long line the same way twice.
    const std::string text = "Hello there, how are you doing today? I am fine.";
    VoiceRandom c, d;
    const VoiceLine x = vocalize(plainVoice(), 0, text, 1, c), y = vocalize(plainVoice(), 0, text, 1, d);
    CHECK(x.steps.size() == y.steps.size() && x.totalDuration == y.totalDuration && x.steps.size() > 6);
}

TEST(audio_voices_play_their_syllables_and_stop) {
    Audio audio;
    CHECK(audio.load());
    std::vector<int16_t> block(plat::AUDIO_BLOCK * 2);
    const int perSecond = plat::AUDIO_RATE / plat::AUDIO_BLOCK;
    AudioCues cues;
    // A 2 s syllable outlasts the clip: it holds in the loop, then fades out.
    cues.playVoice("event:/Dialogue/romeo", 1, 'a', 2, 1, 1.2f, 0.9f, 1, 0, 0);
    audio.run(cues.take());
    CHECK(audio.mixer().voices() == 1);
    for (int i = 0; i < perSecond * 18 / 10; i++) audio.mixBlock(block.data());
    CHECK(audio.mixer().voices() == 1); // (1.8 s: long past its clip, looping)
    for (int i = 0; i < perSecond / 2; i++) audio.mixBlock(block.data());
    CHECK(audio.mixer().voices() == 0); // (2 s, then its 0.1 s fade)
    // StopVoice fades the speaker's out; pausing stops them all.
    cues.playVoice("event:/Dialogue/romeo", 1, 'e', 5, 1, 1, 1, 1, 0, 0);
    cues.playVoice("event:/Dialogue/romeo", 2, 'o', 5, 1, 1, 1, 1, 0, 0);
    audio.run(cues.take());
    cues.stopVoice(1);
    audio.run(cues.take());
    for (int i = 0; i < perSecond / 4; i++) audio.mixBlock(block.data());
    CHECK(audio.mixer().voices() == 1);
    audio.setPaused(true);
    for (int i = 0; i < perSecond / 4; i++) audio.mixBlock(block.data());
    CHECK(audio.mixer().voices() == 0);
    audio.setPaused(false);
}

TEST(audio_bank_has_every_npc_voice) {
    AudioBank bank;
    CHECK(bank.load());
    for (const NpcDef& n : villageArea().npcs)
        check(!n.voice.present || bank.voice(n.voice.event) != nullptr, n.name + "'s voice " + n.voice.event);
}

namespace {

VoiceDef shapedVoice() {
    VoiceDef v = plainVoice();
    v.up = 0.2f;
    v.down = 0.2f;
    v.wordIntonation = 0.1f;
    return v;
}

// The last syllable of a line.
VoiceStep lastSyllable(const VoiceLine& line) {
    VoiceStep last;
    for (const VoiceStep& s : line.steps)
        if (s.sound) last = s;
    return last;
}

} // namespace

TEST(vocalizer_question_rises_to_its_peak_and_an_override_moves_only_the_peak) {
    // "Is it?": a question (not a who / what ...) rises: the last word goes high (x1.1), its peak at
    // the raised pitch (x1.2), ending at its base.
    VoiceRandom a;
    const VoiceStep up = lastSyllable(vocalize(shapedVoice(), 0, "Is it?", 1, a));
    CHECK(std::fabs(up.pitchMid - 1.2f * 1.1f) < 1e-5f && std::fabs(up.pitchEnd - 1.1f) < 1e-5f);
    // A flat override: the peak no longer rises, the word stays high (the clause's own intonation).
    VoiceDef flat = shapedVoice();
    flat.intonation = 0;
    VoiceRandom b;
    const VoiceStep f = lastSyllable(vocalize(flat, 0, "Is it?", 1, b));
    CHECK(std::fabs(f.pitchMid - 1.1f) < 1e-5f && std::fabs(f.pitchEnd - 1.1f) < 1e-5f);
}

TEST(vocalizer_intonation_odds_are_unitys) {
    // "Who is it?" falls 3 times in 4 (the last word low: ends at x0.9); a 2-word "." clause rises
    // half the time.
    int down = 0, rise = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        VoiceRandom r(seed * 2654435761u);
        down += lastSyllable(vocalize(shapedVoice(), 0, "Who is it?", 1, r)).pitchEnd < 1 ? 1 : 0;
        VoiceRandom r2(seed * 40503u + 7);
        rise += lastSyllable(vocalize(shapedVoice(), 0, "It is.", 1, r2)).pitchEnd > 1 ? 1 : 0;
    }
    CHECK(down > 260 && down < 340);
    CHECK(rise > 160 && rise < 240);
}

TEST(vocalizer_numbers_its_clauses) {
    VoiceRandom r;
    const VoiceLine line = vocalize(plainVoice(), 0, "Hi there. Bye now!", 1, r);
    CHECK(!line.steps.empty() && line.steps.front().clause == 0 && line.steps.back().clause == 1);
}

TEST(mixer_clip_loops_and_glides) {
    // A 400 Hz tone, 0.5 s, looping its last 0.4 s.
    Clip tone;
    for (int i = 0; i < plat::AUDIO_RATE / 2; i++) tone.pcm.push_back((int16_t)(8000 * std::sin(2 * 3.14159265 * 400 * i / plat::AUDIO_RATE)));
    tone.frames = (int64_t)tone.pcm.size();
    Mixer m;
    ClipShape shape;
    shape.loopStart = plat::AUDIO_RATE / 10;
    shape.loopEnd = (int)tone.pcm.size();
    shape.stopAfter = 2;
    shape.rateMid = 2; // (from 1 to 2 over the first second, then held at 2)
    shape.rateEnd = 2;
    shape.glide = 2;
    m.playClip(tone, Bus::Sfx, 1, 1, 1, false, shape);
    std::vector<int16_t> block(plat::AUDIO_BLOCK * 2);
    auto crossings = [&](int blocks) {
        int n = 0;
        double energy = 0;
        for (int b = 0; b < blocks; b++) {
            m.mix(block.data());
            for (int i = 1; i < plat::AUDIO_BLOCK; i++) {
                n += (block[2 * i - 2] < 0) != (block[2 * i] < 0) ? 1 : 0;
                energy += (double)block[2 * i] * block[2 * i];
            }
        }
        return std::make_pair(n, energy);
    };
    const int perTenth = plat::AUDIO_RATE / plat::AUDIO_BLOCK / 10;
    const auto first = crossings(perTenth);           // (0-0.1 s: ~400 Hz)
    crossings(perTenth * 14);                         // (to 1.5 s: well past the clip, looping)
    const auto late = crossings(perTenth);            // (1.5-1.6 s: ~800 Hz, still sounding)
    CHECK(late.second > 0.5 * first.second);
    CHECK(late.first > 1.7 * first.first && late.first < 2.3 * first.first);
}
