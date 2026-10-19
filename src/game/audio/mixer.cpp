#include "game/audio/mixer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

Mixer::Voice Mixer::playClip(const Clip& clip, Bus bus, float rate, float left, float right, bool pausable,
                            const ClipShape& shape) {
    if (full() || clip.pcm.empty()) return 0;
    ClipVoice c{nextId(), &clip, bus, 0, rate, left, right, pausable, shape};
    clips_.push_back(c);
    return c.id;
}

void Mixer::stopClip(ClipVoice& c) {
    if (c.stopping) return;
    c.stopping = true;
    c.fadeTotal = c.fadeLeft = std::max(N, (int)(c.shape.fadeOut * plat::AUDIO_RATE));
}

Mixer::Voice Mixer::playStream(const StreamDef& def) {
    if (full()) return 0;
    plat::Ogg* ogg = plat::openOgg(("audio/" + def.file).c_str());
    if (!ogg) return 0;
    if (plat::oggChannels(ogg) != def.channels || plat::oggRate(ogg) != plat::AUDIO_RATE || plat::oggFrames(ogg) != def.frames) {
        printf("audio/%s doesn't match its entry\n", def.file.c_str());
        plat::closeOgg(ogg);
        return 0;
    }
    StreamVoice s;
    s.id = nextId();
    s.def = &def;
    s.ogg = ogg;
    s.levelDb = def.attack > 0 ? SILENT_DB : 0;
    s.gainMixed = level(s.levelDb) * paramGain(def);
    streams_.push_back(s);
    return s.id;
}

void Mixer::closeStream(StreamVoice& s) {
    plat::closeOgg(s.ogg);
    s.ogg = nullptr;
}

void Mixer::stop(Voice voice) {
    for (ClipVoice& c : clips_)
        if (c.id == voice) stopClip(c);
    for (StreamVoice& s : streams_) {
        if (s.id != voice || s.releasing) continue;
        s.releasing = true; // (no release: down to silence over the next block, so it doesn't click)
        s.releaseFrom = s.levelDb;
        s.releaseT = 0;
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
        if (c.id == voice) return !c.stopping;
    for (const StreamVoice& s : streams_)
        if (s.id == voice) return !s.ended;
    return false;
}

void Mixer::setBusVolume(Bus bus, float volume) { bus_[(int)bus] = volume; }

Mixer::Biquad Mixer::Biquad::pass(bool low, float hz) {
    const float w = 2 * 3.14159265f * hz / plat::AUDIO_RATE, alpha = std::sin(w) / (2 * std::sqrt(0.5f)), c = std::cos(w);
    const float a0 = 1 + alpha;
    Biquad q;
    q.b0 = (low ? (1 - c) / 2 : (1 + c) / 2) / a0;
    q.b1 = (low ? 1 - c : -(1 + c)) / a0;
    q.b2 = q.b0;
    q.a1 = -2 * c / a0;
    q.a2 = (1 - alpha) / a0;
    return q;
}

void Mixer::setMusicEq(float lowHz, float highHz, float lowGain, float highGain) {
    if (eqGain_[0] == 1 && eqGain_[1] == 1 && eqMixed_[0] == 1 && eqMixed_[1] == 1) { // (off: fresh filters)
        eqLow_ = Biquad::pass(true, lowHz);
        eqHigh_ = Biquad::pass(false, highHz);
    }
    eqGain_[0] = lowGain;
    eqGain_[1] = highGain;
}
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
    // Along FMOD's AHDSR shape (a release from where it was: that many dB below it).
    auto shape = [&](float x) { return ahdsr_ ? ahdsr_->release(x) : SILENT_DB * x; };
    if (s.releasing) {
        s.releaseT += dt;
        const float x = d.release > 0 ? std::min(s.releaseT / d.release, 1.0f) : 1;
        s.levelDb = x >= 1 ? SILENT_DB : std::max(s.releaseFrom + shape(x), SILENT_DB);
        if (s.levelDb <= SILENT_DB) s.ended = true; // (after this block, which ramps down to it)
    } else if (s.levelDb < 0) {
        s.attackT += dt;
        const float x = d.attack > 0 ? std::min(s.attackT / d.attack, 1.0f) : 1;
        s.levelDb = x >= 1 ? 0 : shape(1 - x);
    }
    return level(s.levelDb);
}

float Mixer::paramGain(const StreamDef& d) const {
    if (d.volumeParam.empty()) return 1;
    const auto& l = d.volumeLevels;
    const float v = parameter(d.volumeParam);
    if (v <= l.front().first) return level(l.front().second);
    for (size_t i = 1; i < l.size(); i++)
        if (v <= l[i].first)
            return level(l[i - 1].second + (l[i].second - l[i - 1].second) * (v - l[i - 1].first) / (l[i].first - l[i - 1].first));
    return level(l.back().second);
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
        const int got = plat::readOgg(s.ogg, out + done * d.channels, want);
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
        const ClipShape& sh = c.shape;
        if (sh.stopAfter >= 0 && c.played >= (int64_t)(sh.stopAfter * plat::AUDIO_RATE)) stopClip(c);
        // The rate this block (a glide moves it at block steps: syllables last several).
        float rate = c.rate;
        if (sh.glide > 0) {
            const float t = c.played / (float)plat::AUDIO_RATE, half = sh.glide / 2;
            rate = t < half ? c.rate + (sh.rateMid - c.rate) * (t / half)
                   : t < sh.glide ? sh.rateMid + (sh.rateEnd - sh.rateMid) * ((t - half) / half) : sh.rateEnd;
        }
        float fade0 = 1, fade1 = 1;
        if (c.stopping) {
            fade0 = c.fadeLeft / (float)c.fadeTotal;
            c.fadeLeft = std::max(0, c.fadeLeft - N);
            fade1 = c.fadeLeft / (float)c.fadeTotal;
        }
        const std::vector<int16_t>& pcm = c.clip->pcm;
        const double last = (double)(pcm.size() - 1);
        const bool loops = sh.loopEnd > sh.loopStart && sh.loopStart >= 0;
        const float g0 = from[(int)c.bus] * fade0, g1 = to[(int)c.bus] * fade1;
        for (int i = 0; i < N; i++, c.pos += rate) {
            // (a loop window ending with the clip: its last sample's interval wraps too)
            if (loops && (c.pos >= sh.loopEnd || c.pos > last)) c.pos -= sh.loopEnd - sh.loopStart;
            if (c.pos > last) break;
            const size_t k = (size_t)c.pos;
            const float f = (float)(c.pos - (double)k);
            const float s = k + 1 < pcm.size() ? pcm[k] + (pcm[k + 1] - pcm[k]) * f : pcm[k];
            const float g = g0 + (g1 - g0) * (i / (float)N);
            acc_[2 * i] += s * c.left * g;
            acc_[2 * i + 1] += s * c.right * g;
        }
        c.played += N;
    }
    clips_.erase(std::remove_if(clips_.begin(), clips_.end(),
                                [&](const ClipVoice& c) { // (a looping one ends only by fading out)
                                    const bool loops = c.shape.loopEnd > c.shape.loopStart && c.shape.loopStart >= 0;
                                    return (!loops && c.pos > (double)(c.clip->pcm.size() - 1)) ||
                                           (c.stopping && c.fadeLeft <= 0 && !(paused_ && c.pausable));
                                }),
                 clips_.end());

    // The music bus through its EQ: only while it isn't flat (or is ramping to flat).
    const bool eq = eqGain_[0] != 1 || eqGain_[1] != 1 || eqMixed_[0] != 1 || eqMixed_[1] != 1;
    if (eq) musicAcc_.assign(N * 2, 0.0f);
    decoded_.resize(N * 2);
    for (StreamVoice& s : streams_) {
        if (s.ended) continue;
        const float e0 = s.gainMixed, e1 = envelope(s, BLOCK_S) * paramGain(*s.def);
        s.gainMixed = e1;
        const double decodeStart = plat::nowMs();
        const int n = readStream(s, decoded_.data(), N);
        streamMs_ += plat::nowMs() - decodeStart;
        if (n < N) s.ended = true;
        const float g0 = from[(int)s.def->bus] * e0 * s.def->gain, g1 = to[(int)s.def->bus] * e1 * s.def->gain;
        std::vector<float>& into = eq && s.def->bus == Bus::Music ? musicAcc_ : acc_;
        if (s.def->channels == 1) {
            for (int i = 0; i < n; i++) {
                const float x = decoded_[i] * (g0 + (g1 - g0) * (i / (float)N));
                into[2 * i] += x;
                into[2 * i + 1] += x;
            }
        } else {
            for (int i = 0; i < n; i++) {
                const float g = g0 + (g1 - g0) * (i / (float)N);
                into[2 * i] += decoded_[2 * i] * g;
                into[2 * i + 1] += decoded_[2 * i + 1] * g;
            }
        }
    }
    if (eq) {
        for (int i = 0; i < N; i++) {
            const float k = i / (float)N;
            const float gl = eqMixed_[0] + (eqGain_[0] - eqMixed_[0]) * k, gh = eqMixed_[1] + (eqGain_[1] - eqMixed_[1]) * k;
            for (int ch = 0; ch < 2; ch++) {
                const float x = musicAcc_[2 * i + ch];
                const float low = eqLow_.run(x, ch), rest = x - low, high = eqHigh_.run(rest, ch);
                acc_[2 * i + ch] += low * gl + (rest - high) + high * gh;
            }
        }
        eqMixed_[0] = eqGain_[0];
        eqMixed_[1] = eqGain_[1];
    }
    for (StreamVoice& s : streams_)
        if (s.ended) closeStream(s);
    streams_.erase(std::remove_if(streams_.begin(), streams_.end(), [](const StreamVoice& s) { return s.ended; }),
                   streams_.end());

    std::copy(std::begin(bus_), std::end(bus_), busMixed_);
    masterMixed_ = master_;
    for (int i = 0; i < N * 2; i++) out[i] = (int16_t)std::clamp(std::lround(acc_[i]), -32768L, 32767L);
}
