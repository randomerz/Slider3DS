#include "game/audio/mixer.h"

#include <algorithm>
#include <cmath>

#include "platform/platform.h"

namespace {

constexpr float SILENT_DB = -80; // the AHDSR's floor (FMOD's -80 dB: off)
constexpr int N = plat::AUDIO_BLOCK;
constexpr float BLOCK_S = N / (float)plat::AUDIO_RATE;

float level(float db) { return db <= SILENT_DB ? 0 : std::pow(10.0f, db / 20); }

} // namespace

Mixer::~Mixer() { stopAll(); }

Mixer::Voice Mixer::nextId() {
    if (++lastId_ == 0) ++lastId_;
    return lastId_;
}

Mixer::Voice Mixer::playClip(const Clip& clip, Bus bus, float rate, float left, float right, bool pausable) {
    if (full() || clip.pcm.empty()) return 0;
    const Voice id = nextId();
    clips_.push_back({id, &clip, bus, 0, rate, left, right, pausable});
    return id;
}

Mixer::Voice Mixer::playStream(const StreamDef& def) {
    if (full()) return 0;
    plat::Ogg* ogg = plat::openOgg(("audio/" + def.file).c_str());
    if (!ogg) return 0;
    StreamVoice s;
    s.id = nextId();
    s.def = &def;
    s.ogg = ogg;
    s.levelDb = def.attack > 0 ? SILENT_DB : 0;
    s.gainMixed = level(s.levelDb);
    streams_.push_back(s);
    return s.id;
}

void Mixer::closeStream(StreamVoice& s) {
    plat::closeOgg(s.ogg);
    s.ogg = nullptr;
}

void Mixer::stop(Voice voice) {
    clips_.erase(std::remove_if(clips_.begin(), clips_.end(), [&](const ClipVoice& c) { return c.id == voice; }), clips_.end());
    for (StreamVoice& s : streams_) {
        if (s.id != voice || s.releasing) continue;
        if (s.def->release > 0) {
            s.releasing = true;
            s.releaseFrom = s.levelDb;
            s.releaseT = 0;
        } else {
            s.ended = true; // (dropped at the next block)
        }
    }
}

void Mixer::stopAll() {
    clips_.clear();
    for (StreamVoice& s : streams_) closeStream(s);
    streams_.clear();
}

bool Mixer::playing(Voice voice) const {
    if (voice == 0) return false;
    for (const ClipVoice& c : clips_)
        if (c.id == voice) return true;
    for (const StreamVoice& s : streams_)
        if (s.id == voice) return !s.ended;
    return false;
}

void Mixer::setBusVolume(Bus bus, float volume) { bus_[(int)bus] = volume; }
void Mixer::setMasterVolume(float volume) { master_ = volume; }
void Mixer::setPaused(bool paused) { paused_ = paused; }

void Mixer::setParameter(std::string_view name, float value) {
    auto it = params_.find(name);
    if (it == params_.end()) params_.emplace(std::string(name), value);
    else it->second = value;
}

float Mixer::parameter(std::string_view name) const {
    auto it = params_.find(name);
    return it == params_.end() ? 0 : it->second;
}

int64_t Mixer::streamPosition(Voice voice) const {
    for (const StreamVoice& s : streams_)
        if (s.id == voice) return s.pos;
    return -1;
}

float Mixer::envelope(StreamVoice& s, float dt) const {
    const StreamDef& d = *s.def;
    if (s.releasing) {
        s.releaseT += dt;
        s.levelDb = d.release > 0 ? s.releaseFrom + (SILENT_DB - s.releaseFrom) * std::min(s.releaseT / d.release, 1.0f) : SILENT_DB;
        if (s.levelDb <= SILENT_DB) s.ended = true; // (after this block, which ramps down to it)
    } else if (s.levelDb < 0) {
        s.attackT += dt;
        s.levelDb = d.attack > 0 ? SILENT_DB + -SILENT_DB * std::min(s.attackT / d.attack, 1.0f) : 0;
    }
    return level(s.levelDb);
}

int Mixer::readStream(StreamVoice& s, int16_t* out, int frames) {
    const StreamDef& d = *s.def;
    int done = 0;
    while (done < frames) {
        // The next frame where something happens: a jump, or the timeline's end (past it, in the
        // crossfades, only jumps lead on). One at `pos` counts once, and not where a jump landed.
        int64_t next = INT64_MAX;
        for (const StreamDef::Jump& j : d.jumps)
            if (j.at > s.pos || (j.at == s.pos && !s.checked)) {
                next = j.at;
                break;
            }
        if (d.length > s.pos || (d.length == s.pos && !s.checked)) next = std::min(next, d.length);
        if (next == s.pos) {
            s.checked = true;
            const StreamDef::Jump* jump = nullptr;
            for (const StreamDef::Jump& j : d.jumps) {
                if (j.at != s.pos) continue;
                const float v = parameter(j.param);
                if (j.param.empty() || (v >= j.min && v <= j.max)) {
                    jump = &j;
                    break;
                }
            }
            if (jump) {
                if (!plat::seekOgg(s.ogg, jump->to)) return done;
                s.pos = jump->to;
            } else if (s.pos == d.length) {
                return done; // (the event's end)
            }
            continue;
        }
        const int want = (int)std::min<int64_t>(frames - done, next - s.pos);
        const int got = plat::readOgg(s.ogg, out + done * 2, want);
        if (got > 0) {
            s.pos += got;
            s.checked = false;
            done += got;
        }
        if (got < want) return done; // (the file ended early)
    }
    return done;
}

void Mixer::mix(int16_t* out) {
    acc_.assign(N * 2, 0.0f);
    float from[3], to[3]; // each bus's gain (with the master's) at the block's start and end
    for (int b = 0; b < 3; b++) {
        from[b] = busMixed_[b] * masterMixed_;
        to[b] = bus_[b] * master_;
    }

    for (ClipVoice& c : clips_) {
        if (paused_ && c.pausable) continue;
        const std::vector<int16_t>& pcm = c.clip->pcm;
        const double last = (double)(pcm.size() - 1);
        const float g0 = from[(int)c.bus], g1 = to[(int)c.bus];
        for (int i = 0; i < N && c.pos <= last; i++, c.pos += c.rate) {
            const size_t k = (size_t)c.pos;
            const float f = (float)(c.pos - (double)k);
            const float s = k + 1 < pcm.size() ? pcm[k] + (pcm[k + 1] - pcm[k]) * f : pcm[k];
            const float g = g0 + (g1 - g0) * (i / (float)N);
            acc_[2 * i] += s * c.left * g;
            acc_[2 * i + 1] += s * c.right * g;
        }
    }
    clips_.erase(std::remove_if(clips_.begin(), clips_.end(),
                                [](const ClipVoice& c) { return c.pos > (double)(c.clip->pcm.size() - 1); }),
                 clips_.end());

    decoded_.resize(N * 2);
    for (StreamVoice& s : streams_) {
        if (s.ended) continue;
        const float e0 = s.gainMixed, e1 = envelope(s, BLOCK_S);
        s.gainMixed = e1;
        const int n = readStream(s, decoded_.data(), N);
        if (n < N) s.ended = true;
        const float g0 = from[(int)s.def->bus] * e0 * s.def->gain, g1 = to[(int)s.def->bus] * e1 * s.def->gain;
        for (int i = 0; i < n; i++) {
            const float g = g0 + (g1 - g0) * (i / (float)N);
            acc_[2 * i] += decoded_[2 * i] * g;
            acc_[2 * i + 1] += decoded_[2 * i + 1] * g;
        }
    }
    for (StreamVoice& s : streams_)
        if (s.ended) closeStream(s);
    streams_.erase(std::remove_if(streams_.begin(), streams_.end(), [](const StreamVoice& s) { return s.ended; }),
                   streams_.end());

    std::copy(std::begin(bus_), std::end(bus_), busMixed_);
    masterMixed_ = master_;
    for (int i = 0; i < N * 2; i++) out[i] = (int16_t)std::clamp(std::lround(acc_[i]), -32768L, 32767L);
}
