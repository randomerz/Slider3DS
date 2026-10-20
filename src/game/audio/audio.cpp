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

void Audio::startThread() {
    if (!threaded_) threaded_ = plat::startAudioThread(&Audio::serviceThread, this);
}

void Audio::stopThread() {
    if (!threaded_) return;
    plat::stopAudioThread();
    threaded_ = false;
}

void Audio::setListener(const Listener& listener) {
    if (!threaded_) return applyListener(listener);
    plat::lockAudio();
    inbox_.listener = listener;
    plat::unlockAudio();
}

void Audio::run(const std::vector<AudioCue>& cues) {
    if (!threaded_) return applyCues(cues);
    plat::lockAudio();
    inbox_.cues.insert(inbox_.cues.end(), cues.begin(), cues.end());
    plat::unlockAudio();
}

void Audio::setVolumes(const Settings& settings) {
    const Volumes v{settings.masterVolume, settings.sfxVolume, settings.musicVolume, settings.ambienceVolume};
    if (!threaded_) return applyVolumes(v);
    plat::lockAudio();
    inbox_.volumes = v;
    plat::unlockAudio();
}

void Audio::setPaused(bool paused) {
    if (!threaded_) return applyPaused(paused);
    plat::lockAudio();
    inbox_.paused = paused;
    plat::unlockAudio();
}

Audio::ThreadLoad Audio::threadLoad() const {
    if (!threaded_) return {};
    plat::lockAudio();
    ThreadLoad load = load_;
    plat::unlockAudio();
    load.running = true;
    load.core = plat::audioThreadCore();
    load.underruns = plat::audioUnderruns();
    return load;
}

double Audio::threadBusyBetween(double t0, double t1) const {
    if (!threaded_) return 0;
    plat::lockAudio();
    const double ms = busyLog_.between(t0, t1);
    plat::unlockAudio();
    return ms;
}

// The audio thread's (never use a ProfScope in here or below: the profiler is the game thread's,
// and its stolen-time query takes plat::lockAudio).
void Audio::service() {
    const double start = plat::nowMs();
    if (loadStart_ == 0) loadStart_ = start;
    plat::lockAudio();
    if (start - loadStart_ >= 1000) { // (a second's busy shares)
        const double span = start - loadStart_;
        load_.total = (float)(busyMs_ / span);
        load_.streams = (float)(streamMs_ / span);
        load_.clips = (float)(clipMs_ / span);
        load_.io = (float)(ioMs_ / span);
        loadStart_ = start;
        busyMs_ = streamMs_ = clipMs_ = ioMs_ = 0;
    }
    taken_.swap(inbox_.cues); // (taken_ is empty: applied last time)
    const Listener listener = inbox_.listener;
    const Volumes volumes = inbox_.volumes;
    const bool paused = inbox_.paused;
    plat::unlockAudio();
    applyVolumes(volumes);
    applyPaused(paused);
    applyListener(listener);
    applyCues(taken_);
    taken_.clear();
    fill();
    streamMs_ += mixer_.takeStreamMs();
    const double clipStart = plat::nowMs();
    bank_.decodeSome(DECODE_BUDGET_MS);
    const double end = plat::nowMs();
    clipMs_ += end - clipStart;
    busyMs_ += end - start;
    ioMs_ += plat::takeFileIoMs();
    float bgmS, bgmLoopS;
    bgmPosition(bgmS, bgmLoopS);
    plat::lockAudio();
    busyLog_.add(start, end);
    load_.bgmS = bgmS;
    load_.bgmLoopS = bgmLoopS;
    plat::unlockAudio();
}

void Audio::bgmPosition(float& at, float& loop) const {
    at = loop = -1;
    for (const auto& [name, voice] : music_) {
        const int64_t pos = mixer_.streamPosition(voice);
        const StreamDef* def = bank_.music(name);
        if (pos < 0 || !def || !mixer_.playing(voice)) continue;
        // The loop point: the unconditional jump back to the start (or into the crossfade that does).
        int64_t loopAt = -1, into = -1;
        for (const StreamDef::Jump& j : def->jumps) {
            if (!j.param.empty() || j.at > def->length) continue;
            loopAt = j.at;
            into = j.to;
            break;
        }
        loop = loopAt < 0 ? -1 : loopAt / (float)plat::AUDIO_RATE;
        // (in the crossfade: the loop point plus how far into it)
        const int64_t t = pos >= def->length && loopAt >= 0 && into >= def->length ? loopAt + (pos - into) : pos;
        at = t / (float)plat::AUDIO_RATE;
        return;
    }
}

void Audio::applyListener(const Listener& listener) {
    // SetListenerIsIndoor: IndoorMusic3Eq in a house (its parameters on the music bus's EQ).
    if (listener.indoor != listener_.indoor) {
        const IndoorEq& eq = bank_.indoorEq();
        const auto gain = [](float db) { return std::pow(10.0f, db / 20); };
        mixer_.setMusicEq(eq.lowHz, eq.highHz, listener.indoor ? gain(eq.lowDb) : 1, listener.indoor ? gain(eq.highDb) : 1);
    }
    listener_ = listener;
}

void Audio::unload() {
    stopThread();
    mixer_.stopAll();
    voices_.clear();
    music_.clear();
    ambience_.clear();
    dampens_.clear();
    bank_.unload();
}

void Audio::applyCues(const std::vector<AudioCue>& cues) {
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
            // (its music dip runs out: DialogueDisplay keys it by itself, which StopDampen<VocalizableParagraph> never matches)
            stopVoices(c.key);
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
    // Split by its side at constant power, as FMOD's spatializer does (measured: the angle by the
    // azimuth; its height on screen only adds distance).
    const SpatialModel& model = bank_.spatial();
    const float azimuth = std::atan2(dx, LISTENER_Z) * 57.29578f;
    float angle = model.angle(azimuth) / 57.29578f;
    if (dx < 0) angle = 3.14159265f / 2 - angle;
    left = gain * model.gain * std::cos(angle);
    right = gain * model.gain * std::sin(angle);
}

void Audio::play(const AudioCue& cue) {
    const SoundDef* s = bank_.sound(cue.name);
    if (!s) {
        printf("Sound %s not found\n", cue.name.c_str()); // (AudioManager.PickSound's error)
        return;
    }
    const Clip& clip = s->clips[std::min((size_t)(random01() * s->clips.size()), s->clips.size() - 1)];
    bank_.decode(clip);
    float rate = std::pow(2.0f, (random01() - 0.5f) * s->pitchRandom / 12);
    if (s->pitchParam && cue.value > 0) rate *= std::clamp(cue.value, 0.5f, 2.0f);
    float left, right;
    if (s->spatial) {
        spatialGains(*s, cue.positioned, cue.x, cue.y, cue.alwaysOutdoor, left, right);
    } else { // 2D: a mono source at full on both sides (measured: FMOD's upmix)
        left = right = 1;
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
    bank_.decode(*clip);
    shape.stopAfter = cue.duration;
    float rate = 1;
    if (v->pitched) {
        rate = cue.value;
        shape.rateMid = cue.pitchMid;
        shape.rateEnd = cue.pitchEnd;
        shape.glide = cue.duration;
    }
    float left = 1, right = 1;
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

void Audio::applyVolumes(const Volumes& v) {
    mixer_.setMasterVolume(v.master);
    mixer_.setBusVolume(Bus::Sfx, v.sfx);
    musicVolume_ = v.music; // (with the dampening, per block)
    mixer_.setBusVolume(Bus::Ambience, v.ambience);
}

void Audio::applyPaused(bool paused) {
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
    if (threaded_) return; // (the thread does it)
    fill();
    if (plat::audioAvailable()) bank_.decodeSome(DECODE_BUDGET_MS); // (without an output nothing plays them)
}

void Audio::fill() {
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
