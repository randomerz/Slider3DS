// The vocalizer (game/npc/vocalizer): SentenceVocalizer's clauses, the words' syllables and pauses,
// the emote's modifier; and the bank's rendered synths, played as syllables.
#include <cmath>
#include <string>
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
