#include "game/audio/audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/save/settings.h"
#include "platform/platform.h"

bool Audio::load(std::string* error) {
    unload();
    return bank_.load(error);
}

void Audio::unload() {
    mixer_.stopAll();
    music_.clear();
    bank_.unload();
}

void Audio::run(const std::vector<AudioCue>& cues) {
    // Without an output nothing mixes, so nothing would ever end: only the parameters are kept.
    const bool output = plat::audioAvailable();
    for (const AudioCue& c : cues) {
        if (!output && c.kind != AudioCue::Kind::SetParameter) continue;
        switch (c.kind) {
        case AudioCue::Kind::Play: play(c); break;
        case AudioCue::Kind::PlayMusic: playMusic(c); break;
        case AudioCue::Kind::StopMusic:
            if (auto it = music_.find(c.name); it != music_.end()) mixer_.stop(it->second);
            break;
        case AudioCue::Kind::SetParameter: mixer_.setParameter(c.name, c.value); break;
        }
    }
}

void Audio::play(const AudioCue& cue) {
    const SoundDef* s = bank_.sound(cue.name);
    if (!s) {
        printf("Sound %s not found\n", cue.name.c_str()); // (AudioManager.PickSound's error)
        return;
    }
    const Clip& clip = s->clips[std::min((size_t)(random01() * s->clips.size()), s->clips.size() - 1)];
    float rate = std::pow(2.0f, (random01() - 0.5f) * s->pitchRandom / 12);
    if (s->pitchParam && cue.value > 0) rate *= std::clamp(cue.value, 0.5f, 2.0f);
    // Centered (2D, or at the listener): a mono source at -3 dB a side (constant power).
    const float side = s->gain * std::sqrt(0.5f);
    // Unity's SetPaused reaches only managed (spatial) instances: 2D one-shots play on.
    mixer_.playClip(clip, Bus::Sfx, rate, side, side, s->canPause && s->spatial);
}

void Audio::playMusic(const AudioCue& cue) {
    const StreamDef* def = bank_.music(cue.name);
    if (!def) {
        printf("Couldn't find music of name: %s\n", cue.name.c_str());
        return;
    }
    if (!cue.restart && mixer_.playing(music(cue.name))) return;
    for (const auto& [name, voice] : music_) mixer_.stop(voice); // (stopOtherTracks: this one too, if restarted)
    music_[cue.name] = mixer_.playStream(*def);
}

Mixer::Voice Audio::music(std::string_view name) const {
    auto it = music_.find(name);
    return it != music_.end() && mixer_.playing(it->second) ? it->second : 0;
}

void Audio::setVolumes(const Settings& settings) {
    mixer_.setMasterVolume(settings.masterVolume);
    mixer_.setBusVolume(Bus::Sfx, settings.sfxVolume);
    mixer_.setBusVolume(Bus::Music, settings.musicVolume);
    mixer_.setBusVolume(Bus::Ambience, settings.ambienceVolume);
}

void Audio::setPaused(bool paused) { mixer_.setPaused(paused); }

void Audio::pump() {
    int16_t block[plat::AUDIO_BLOCK * 2];
    for (int n = plat::audioBlocksWanted(); n > 0; n--) {
        mixer_.mix(block);
        plat::queueAudioBlock(block);
    }
}

float Audio::random01() {
    rng_ ^= rng_ << 13; // (xorshift32)
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (rng_ >> 8) / 16777216.0f;
}
