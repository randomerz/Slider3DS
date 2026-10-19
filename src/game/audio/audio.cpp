#include "game/audio/audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/save/settings.h"
#include "platform/platform.h"

namespace {

constexpr float BLOCK_S = plat::AUDIO_BLOCK / (float)plat::AUDIO_RATE;

} // namespace

bool Audio::load(std::string* error) {
    unload();
    if (!bank_.load(error)) return false;
    mixer_.setAhdsr(&bank_.ahdsr());
    return true;
}

void Audio::setListener(const Listener& listener) {
    // SetListenerIsIndoor: IndoorMusic3Eq in a house (its parameters on the music bus's EQ).
    if (listener.indoor != listener_.indoor) {
        const IndoorEq& eq = bank_.indoorEq();
        const auto gain = [](float db) { return std::pow(10.0f, db / 20); };
        mixer_.setMusicEq(eq.lowHz, eq.highHz, listener.indoor ? gain(eq.lowDb) : 1, listener.indoor ? gain(eq.highDb) : 1);
    }
    listener_ = listener;
}

void Audio::unload() {
    mixer_.stopAll();
    voices_.clear();
    music_.clear();
    ambience_.clear();
    dampens_.clear();
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
        case AudioCue::Kind::PlayAmbience: playAmbience(c); break;
        case AudioCue::Kind::StopAmbience:
            if (auto it = ambience_.find(c.name); it != ambience_.end()) mixer_.stop(it->second);
            break;
        case AudioCue::Kind::SetParameter: mixer_.setParameter(c.name, c.value); break;
        case AudioCue::Kind::DampenMusic:
            if (c.key != 0)
                dampens_.erase(std::remove_if(dampens_.begin(), dampens_.end(), [&](const Dampen& d) { return d.key == c.key; }),
                               dampens_.end());
            if (c.length > 0) dampens_.push_back({c.value, c.length, 0, c.key});
            break;
        case AudioCue::Kind::PlayVoice: playVoice(c); break;
        case AudioCue::Kind::StopVoice:
            stopVoices(c.key);
            dampens_.erase(std::remove_if(dampens_.begin(), dampens_.end(), [&](const Dampen& d) { return d.key == c.key; }),
                           dampens_.end()); // (StopDampen<VocalizableParagraph>: its talking's dip)
            break;
        }
    }
}

void Audio::spatialGains(const SoundDef& s, bool positioned, float x, float y, bool alwaysOutdoor, float& left,
                         float& right) const {
    // At the listener unless placed; one in a house heard from outside (or the other way) is moved
    // by the housing offset into the listener's frame, and turned down.
    const float sx = positioned ? x : listener_.x;
    float sy = positioned ? y : listener_.y;
    float gain = 1;
    const bool indoor = !alwaysOutdoor && positioned && sy <= INDOOR_Y;
    if (positioned && indoor != listener_.indoor) {
        sy += listener_.indoor ? listener_.housingOffset : -listener_.housingOffset;
        gain = std::pow(2.0f, -INDOOR_MUTE_DB / 6); // (AudioManager's dB: 6 per doubling)
    }
    const float dx = sx - listener_.x, dy = sy - listener_.y;
    const float d = std::sqrt(dx * dx + dy * dy + LISTENER_Z * LISTENER_Z);
    if (d > s.minDistance) { // linear squared, silent at the max distance
        const float k = std::max(0.0f, (s.maxDistance - d) / (s.maxDistance - s.minDistance));
        gain *= k * k;
    }
    // Panned by the side (constant power): straight below the listener, centered.
    const float pan = dx / std::sqrt(dx * dx + LISTENER_Z * LISTENER_Z); // -1 left .. 1 right
    const float angle = (pan + 1) * 0.25f * 3.14159265f;
    left = gain * std::cos(angle);
    right = gain * std::sin(angle);
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
    float left, right;
    if (s->spatial) {
        spatialGains(*s, cue.positioned, cue.x, cue.y, cue.alwaysOutdoor, left, right);
    } else { // 2D: a mono source at -3 dB a side (constant power, centered)
        left = right = std::sqrt(0.5f);
    }
    const float g = s->gain * cue.volume;
    // Unity's SetPaused reaches only managed (spatial) instances: 2D one-shots play on.
    mixer_.playClip(clip, Bus::Sfx, rate, left * g, right * g, s->canPause && s->spatial);
}

void Audio::playVoice(const AudioCue& cue) {
    const VoiceSynth* v = bank_.voice(cue.name);
    if (!v) {
        printf("No voice %s\n", cue.name.c_str());
        return;
    }
    const Clip* clip;
    ClipShape shape;
    if (v->held) {
        const char* at = std::strchr(VoiceSynth::VOWELS, cue.vowel);
        clip = &v->clips[at && *at ? at - VoiceSynth::VOWELS : 0];
        shape.loopStart = v->loopStart;
        shape.loopEnd = v->loopEnd;
        shape.fadeOut = VOICE_RELEASE;
    } else {
        clip = &v->clips[std::min((size_t)(random01() * v->clips.size()), v->clips.size() - 1)];
    }
    shape.stopAfter = cue.duration;
    float rate = 1;
    if (v->pitched) {
        rate = cue.value;
        shape.rateMid = cue.pitchMid;
        shape.rateEnd = cue.pitchEnd;
        shape.glide = cue.duration;
    }
    float left = std::sqrt(0.5f), right = left;
    if (v->spatial) {
        SoundDef s;
        s.minDistance = v->minDistance;
        s.maxDistance = v->maxDistance;
        spatialGains(s, cue.positioned, cue.x, cue.y, false, left, right);
    }
    const float g = v->gain * cue.volume;
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [&](const auto& kv) { return !mixer_.playing(kv.second); }),
                  voices_.end());
    voices_.push_back({cue.key, mixer_.playClip(*clip, Bus::Sfx, rate, left * g, right * g, false, shape)});
}

void Audio::stopVoices(int key) {
    for (const auto& [k, voice] : voices_)
        if (key == 0 || k == key) mixer_.stop(voice);
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

void Audio::playAmbience(const AudioCue& cue) {
    const StreamDef* def = bank_.ambience(cue.name);
    if (!def) {
        printf("Couldn't find ambience of name: %s\n", cue.name.c_str());
        return;
    }
    if (auto it = ambience_.find(cue.name); it != ambience_.end()) mixer_.stop(it->second); // (the emitter restarts)
    ambience_[cue.name] = mixer_.playStream(*def);
}

Mixer::Voice Audio::music(std::string_view name) const {
    auto it = music_.find(name);
    return it != music_.end() && mixer_.playing(it->second) ? it->second : 0;
}

Mixer::Voice Audio::ambience(std::string_view name) const {
    auto it = ambience_.find(name);
    return it != ambience_.end() && mixer_.playing(it->second) ? it->second : 0;
}

void Audio::setVolumes(const Settings& settings) {
    mixer_.setMasterVolume(settings.masterVolume);
    mixer_.setBusVolume(Bus::Sfx, settings.sfxVolume);
    musicVolume_ = settings.musicVolume; // (with the dampening, per block)
    mixer_.setBusVolume(Bus::Ambience, settings.ambienceVolume);
}

void Audio::setPaused(bool paused) {
    if (paused && !paused_) stopVoices(0); // (ManagedInstance.SetPaused: dialogue stops for good)
    paused_ = paused;
    mixer_.setPaused(paused);
}

float Audio::musicDampening() const {
    if (paused_) return 1; // ("for accurate music volume adjustment, always use multiplier 1 while paused")
    float lowest = 1;
    for (const Dampen& d : dampens_)
        lowest = std::min(lowest, d.amount + (1 - d.amount) * evalKeys(bank_.dampenCurve(), d.t / d.length));
    return lowest;
}

void Audio::mixBlock(int16_t* out) {
    mixer_.setBusVolume(Bus::Music, musicVolume_ * musicDampening());
    mixer_.mix(out);
    if (paused_) return;
    for (Dampen& d : dampens_) d.t += BLOCK_S;
    dampens_.erase(std::remove_if(dampens_.begin(), dampens_.end(), [](const Dampen& d) { return d.t >= d.length; }),
                   dampens_.end());
}

void Audio::pump() {
    int16_t block[plat::AUDIO_BLOCK * 2];
    for (int n = plat::audioBlocksWanted(); n > 0; n--) {
        mixBlock(block);
        plat::queueAudioBlock(block);
    }
}

float Audio::random01() {
    rng_ ^= rng_ << 13; // (xorshift32)
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (rng_ >> 8) / 16777216.0f;
}
