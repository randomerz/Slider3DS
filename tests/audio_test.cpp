// Audio (game/audio): the exported bank, the mixer's streams (jumps, the timeline's end, the AHDSR)
// and clips (rate, gains, buses, pause), and AudioManager's music.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "game/audio/audio.h"
#include "game/audio/sounds.h"
#include "platform/platform.h"
#include "test.h"
#include "test_platform.h"

namespace {

constexpr int N = plat::AUDIO_BLOCK;
const char* const FILE_NAME = "music_main_menu.ogg"; // (any stereo stream will do)

// The file's frames from `from`, as decoded directly.
std::vector<int16_t> decoded(int64_t from, int frames) {
    std::vector<int16_t> out(frames * 2);
    plat::Ogg* ogg = plat::openOgg((std::string("audio/") + FILE_NAME).c_str());
    check(ogg && plat::seekOgg(ogg, from) && plat::readOgg(ogg, out.data(), frames) == frames, "the file decodes");
    plat::closeOgg(ogg);
    return out;
}

// `blocks` blocks of the mixer's output.
std::vector<int16_t> mixBlocks(Mixer& m, int blocks) {
    std::vector<int16_t> out(blocks * N * 2);
    for (int b = 0; b < blocks; b++) m.mix(out.data() + b * N * 2);
    return out;
}

bool same(const std::vector<int16_t>& a, int aFrom, const std::vector<int16_t>& b, int bFrom, int frames) {
    for (int i = 0; i < frames * 2; i++)
        if (a[aFrom * 2 + i] != b[bFrom * 2 + i]) return false;
    return true;
}

bool silent(const std::vector<int16_t>& a, int from, int frames) {
    for (int i = 0; i < frames * 2; i++)
        if (a[from * 2 + i] != 0) return false;
    return true;
}

StreamDef stream(int64_t length, std::vector<StreamDef::Jump> jumps) {
    StreamDef d;
    d.file = FILE_NAME;
    d.length = length;
    d.jumps = std::move(jumps);
    return d;
}

} // namespace

TEST(audio_bank_has_every_sound_the_port_plays) {
    AudioBank bank;
    std::string error;
    CHECK(bank.load(&error));
    if (!error.empty()) printf("  %s\n", error.c_str());
    for (const char* name : sounds::ALL) check(bank.sound(name) != nullptr, std::string("sound ") + name);
    for (const char* name : tracks::ALL_MUSIC) check(bank.music(name) != nullptr, std::string("music ") + name);
    // (UI Click: the "Pitch" parameter and a random pitch, as its FMOD event; not paused with the game)
    const SoundDef* click = bank.sound(sounds::UI_CLICK);
    CHECK(click && click->pitchParam && click->pitchRandom > 0 && !click->canPause && !click->clips[0].pcm.empty());
    // (the Village's music loops through a crossfade past its timeline, back near its start)
    const StreamDef* v = bank.music(tracks::VILLAGE);
    CHECK(v && v->attack == 1 && v->release == 1 && v->jumps.size() == 2 && v->jumps[1].at > v->length &&
          v->jumps[1].to < plat::AUDIO_RATE * 2);
    // (the title's music holds its intro until the menu opens: a jump while MainMenuActivated is 0)
    const StreamDef* mm = bank.music(tracks::MAIN_MENU);
    CHECK(mm && mm->jumps[0].param == audio_params::MAIN_MENU_ACTIVATED && mm->jumps[0].max == 0);
}

TEST(mixer_stream_jumps_and_ends_at_its_length) {
    Mixer m;
    const StreamDef d = stream(3000, {{1024, 2048}});
    const Mixer::Voice v = m.playStream(d);
    CHECK(v != 0 && m.playing(v));
    const std::vector<int16_t> out = mixBlocks(m, 4); // (2048 frames)
    // 0-1023 as decoded, then from 2048 up to the length (952 frames), then nothing: it ended.
    CHECK(same(out, 0, decoded(0, 1024), 0, 1024));
    CHECK(same(out, 1024, decoded(2048, 952), 0, 952));
    CHECK(silent(out, 1976, 2048 - 1976) && !m.playing(v));
}

TEST(mixer_conditional_jump_reads_the_global_parameter) {
    Mixer m;
    const StreamDef d = stream(100000, {{1024, 0, "P", 0, 0}});
    const Mixer::Voice v = m.playStream(d);
    std::vector<int16_t> out = mixBlocks(m, 4);
    CHECK(same(out, 1024, decoded(0, 1024), 0, 1024)); // (P is 0: back to the start at 1024)
    CHECK(m.streamPosition(v) == 1024);
    // Landing on a jump's frame doesn't fire it; playing up to it does. With P = 1 it plays on.
    m.setParameter("P", 1);
    out = mixBlocks(m, 2);
    CHECK(m.streamPosition(v) == 2048 && same(out, 0, decoded(1024, 1024), 0, 1024));
}

TEST(mixer_attack_fades_in_and_release_fades_out) {
    Mixer m;
    StreamDef d = stream(100000, {});
    d.attack = d.release = 2 * N / (float)plat::AUDIO_RATE; // two blocks each
    const Mixer::Voice v = m.playStream(d);
    const std::vector<int16_t> out = mixBlocks(m, 4);
    const std::vector<int16_t> ref = decoded(0, 4 * N);
    CHECK(out[0] == 0 && out[1] == 0); // (from silence)
    CHECK(same(out, 2 * N, ref, 2 * N, 2 * N)); // (at full once the attack is over)
    m.stop(v);
    CHECK(m.playing(v)); // (fading out)
    mixBlocks(m, 2);
    CHECK(!m.playing(v));
}

TEST(mixer_clip_plays_at_its_rate_through_its_bus) {
    Mixer m;
    m.setBusVolume(Bus::Sfx, 0.5f);
    std::vector<int16_t> scratch(N * 2);
    m.mix(scratch.data()); // (the bus volume settles: changes ramp across a block)
    Clip clip;
    for (int i = 0; i < 4 * N; i++) clip.pcm.push_back((int16_t)(i * 8));
    m.playClip(clip, Bus::Sfx, 2, 1, 0.5f, true);
    std::vector<int16_t> out = mixBlocks(m, 1);
    bool ok = true;
    for (int i = 0; i < N; i++) ok = ok && out[2 * i] == i * 16 / 2 && out[2 * i + 1] == i * 16 / 4;
    CHECK(ok); // (every other sample: rate 2; left at the bus's half, right at a quarter)
    // Paused, a pausable clip holds; on resuming it goes on from there.
    m.setPaused(true);
    out = mixBlocks(m, 1);
    CHECK(silent(out, 0, N));
    m.setPaused(false);
    out = mixBlocks(m, 1);
    CHECK(out[0] == 2 * N * 8 / 2); // (sample 2N, at the bus's half)
    out = mixBlocks(m, 1);
    CHECK(silent(out, 0, N)); // (it ended at the clip's end)
}

TEST(audio_plays_music_as_audio_manager) {
    Audio audio;
    CHECK(audio.load());
    AudioCues cues;
    cues.playMusic(tracks::MAIN_MENU, false);
    audio.run(cues.take());
    const Mixer::Voice title = audio.music(tracks::MAIN_MENU);
    CHECK(title != 0);
    // Not restarted while it plays (restartTrackIfPlaying: false)...
    cues.playMusic(tracks::MAIN_MENU, false);
    audio.run(cues.take());
    CHECK(audio.music(tracks::MAIN_MENU) == title);
    // ...and another track stops it (stopOtherTracks; no release: at once).
    cues.playMusic(tracks::VILLAGE);
    audio.run(cues.take());
    std::vector<int16_t> block(N * 2);
    audio.mixer().mix(block.data());
    CHECK(audio.music(tracks::MAIN_MENU) == 0 && audio.music(tracks::VILLAGE) != 0);
    // A parameter goes to the mixer; an unknown sound is left out.
    cues.setParameter(audio_params::MAIN_MENU_ACTIVATED, 1);
    cues.play("No Such Sound");
    audio.run(cues.take());
    CHECK(audio.mixer().parameter(audio_params::MAIN_MENU_ACTIVATED) == 1);
    // pump mixes what the platform asks for.
    testSetAudioWanted(3);
    audio.pump();
    CHECK(testAudioQueued().size() == 3u * N * 2);
    testSetAudioWanted(0);
}

TEST(mixer_stream_landing_on_its_length_plays_on) {
    // As both real tracks loop: a jump lands exactly on `length` (where the crossfade starts) and
    // the track goes on from there, out of the crossfade and back.
    Mixer m;
    const StreamDef d = stream(2048, {{1024, 2048}, {2560, 0}});
    const Mixer::Voice v = m.playStream(d);
    const std::vector<int16_t> out = mixBlocks(m, 4);
    CHECK(same(out, 1024, decoded(2048, 512), 0, 512)); // (landed on the length: no end)
    CHECK(same(out, 1536, decoded(0, 512), 0, 512));    // (out of the "crossfade", back to the start)
    CHECK(m.playing(v) && m.streamPosition(v) == 512);
}

TEST(mixer_stop_without_a_release_fades_over_a_block) {
    Mixer m;
    const StreamDef d = stream(100000, {});
    const Mixer::Voice v = m.playStream(d);
    mixBlocks(m, 1);
    m.stop(v);
    const std::vector<int16_t> out = mixBlocks(m, 1);
    const std::vector<int16_t> ref = decoded(N, N);
    // Down to silence across the block (no click), then gone.
    CHECK(out[0] == ref[0] && out[1] == ref[1]);
    CHECK(std::abs(out[2 * (N - 1)]) <= std::abs(ref[2 * (N - 1)]) / 64 + 1);
    CHECK(!m.playing(v) && m.voices() == 0);
    // A clip too.
    Clip clip;
    clip.pcm.assign(4 * N, 1000);
    const Mixer::Voice c = m.playClip(clip, Bus::Sfx, 1, 1, 1, false);
    m.stop(c);
    const std::vector<int16_t> fade = mixBlocks(m, 1);
    CHECK(fade[0] == 1000 && fade[2 * (N - 1)] < 10 && m.voices() == 0);
}

TEST(audio_without_an_output_plays_nothing) {
    // Nothing would mix, so nothing would ever end: no voices pile up, the parameters still apply.
    Audio audio;
    CHECK(audio.load());
    testSetAudioAvailable(false);
    AudioCues cues;
    for (int i = 0; i < 40; i++) cues.play(sounds::UI_CLICK);
    cues.playMusic(tracks::MAIN_MENU);
    cues.setParameter(audio_params::MAIN_MENU_ACTIVATED, 1);
    audio.run(cues.take());
    CHECK(audio.mixer().voices() == 0 && audio.mixer().parameter(audio_params::MAIN_MENU_ACTIVATED) == 1);
    testSetAudioAvailable(true);
}

TEST(audio_spatial_sounds_fall_off_pan_and_cross_the_house_line) {
    Audio audio;
    CHECK(audio.load());
    SoundDef s;
    s.spatial = true;
    s.minDistance = 1;
    s.maxDistance = 20;
    Listener l;
    l.x = 10;
    l.y = 5;
    l.housingOffset = -150;
    audio.setListener(l);
    float left, right;
    // At the listener: 10 units below it (ZLevel), centered; linear squared from 1 to 20.
    audio.spatialGains(s, false, 0, 0, false, left, right);
    const float k = (20 - 10) / 19.0f;
    CHECK(std::fabs(left - right) < 1e-5f && std::fabs(left * left + right * right - k * k * k * k) < 1e-4f);
    // To the right: louder right; past the max distance: silent.
    audio.spatialGains(s, true, 15, 5, false, left, right);
    CHECK(right > left && left > 0);
    audio.spatialGains(s, true, 40, 5, false, left, right);
    CHECK(left == 0 && right == 0);
    // In a house (y <= -75) heard from outside: moved up by the housing offset, and quieter.
    float l0, r0;
    audio.spatialGains(s, true, 10, 5, false, l0, r0);
    audio.spatialGains(s, true, 10, 5 - 150, false, left, right);
    CHECK(std::fabs(left - l0 * std::pow(2.0f, -5 / 6.0f)) < 1e-5f);
    // ...unless it's always outdoors (the slides): then it's far below, silent.
    audio.spatialGains(s, true, 10, 5 - 150, true, left, right);
    CHECK(left == 0);
}

TEST(audio_dampens_the_music_and_brings_it_back) {
    Audio audio;
    CHECK(audio.load());
    AudioCues cues;
    cues.dampenMusic(0.2f, 2);
    audio.run(cues.take());
    std::vector<int16_t> block(N * 2);
    const int blocks = (int)(1 / (N / (float)plat::AUDIO_RATE)); // ~1 s: the curve's bottom
    for (int i = 0; i < blocks; i++) audio.mixBlock(block.data());
    CHECK(std::fabs(audio.musicDampening() - 0.2f) < 0.02f);
    audio.setPaused(true);
    CHECK(audio.musicDampening() == 1); // (paused: none...)
    for (int i = 0; i < blocks; i++) audio.mixBlock(block.data());
    audio.setPaused(false);
    CHECK(std::fabs(audio.musicDampening() - 0.2f) < 0.02f); // (...and it held where it was)
    for (int i = 0; i < blocks + 2; i++) audio.mixBlock(block.data());
    CHECK(audio.musicDampening() == 1);
}

TEST(audio_bank_has_the_areas_pick_up) {
    AudioBank bank;
    CHECK(bank.load() && bank.sound(villageArea().name + " Pick Up") != nullptr); // (ItemPickupEffect's name)
}

TEST(mixer_stream_volume_follows_its_parameter) {
    // An ambience's distance curve: 0 dB at 0, -20 dB at 10 (linear in dB between the levels).
    Mixer m;
    StreamDef d = stream(100000, {});
    d.volumeParam = "Dist";
    d.volumeLevels = {{0.0f, 0.0f}, {10.0f, -20.0f}};
    m.setParameter("Dist", 10);
    m.playStream(d);
    mixBlocks(m, 1);
    const std::vector<int16_t> out = mixBlocks(m, 1), ref = decoded(N, N);
    int peak = 0, refPeak = 0;
    for (int i = 0; i < N * 2; i++) {
        peak = std::max(peak, std::abs((int)out[i]));
        refPeak = std::max(refPeak, std::abs((int)ref[i]));
    }
    CHECK(refPeak > 1000 && std::abs(peak - refPeak / 10) <= 2); // (-20 dB: a tenth)
}

TEST(mixer_attack_follows_fmods_shape) {
    // The measured AHDSR shape: an attack is the release mirrored (dB below full at each point).
    AhdsrShape shape;
    for (int i = 0; i < AhdsrShape::POINTS; i++) shape.db[i] = -80.0f * i / (AhdsrShape::POINTS - 1);
    CHECK(shape.release(0) == 0 && shape.release(1) == -80 && std::fabs(shape.release(0.5f) + 40) < 1e-4f);
    CHECK(std::fabs(shape.attack(0.25f) - shape.release(0.75f)) < 1e-6f);
    AudioBank bank;
    CHECK(bank.load() && bank.ahdsr().db[0] == 0 && bank.ahdsr().db[AhdsrShape::POINTS - 1] == -80);
}

namespace {
double energy(const std::vector<int16_t>& x) {
    double e = 0;
    for (int16_t v : x) e += (double)v * v;
    return e;
}
} // namespace

TEST(mixer_music_eq_cuts_and_goes_flat_again) {
    Mixer m;
    const StreamDef d = stream(1000000, {});
    m.playStream(d);
    m.setMusicEq(280, 1650, 0.5f, 0.2f);
    mixBlocks(m, 1); // (the gains ramp in)
    const std::vector<int16_t> cut = mixBlocks(m, 2);
    CHECK(energy(cut) < 0.8 * energy(decoded(N, 2 * N)));
    // Back to flat: one block's ramp, then exactly the stream again (the filters are skipped).
    m.setMusicEq(280, 1650, 1, 1);
    mixBlocks(m, 1);
    const std::vector<int16_t> flat = mixBlocks(m, 1);
    CHECK(same(flat, 0, decoded(4 * N, N), 0, N));
}

TEST(audio_muffles_the_music_in_a_house) {
    // IndoorMusic3Eq: the same music, quieter in its lows and highs while the listener is indoors.
    std::vector<int16_t> out[2];
    for (int indoor = 0; indoor < 2; indoor++) {
        Audio audio;
        CHECK(audio.load());
        Listener l;
        l.indoor = indoor != 0;
        audio.setListener(l);
        AudioCues cues;
        cues.playMusic(tracks::VILLAGE);
        audio.run(cues.take());
        std::vector<int16_t> block(N * 2);
        for (int i = 0; i < 160; i++) { // (past its fade-in)
            audio.mixBlock(block.data());
            if (i >= 100) out[indoor].insert(out[indoor].end(), block.begin(), block.end());
        }
    }
    CHECK(energy(out[1]) < 0.9 * energy(out[0]) && energy(out[1]) > 0);
}
