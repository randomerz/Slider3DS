// Mixes what plays into the platform's blocks (plat::AUDIO_BLOCK stereo frames at AUDIO_RATE):
//   clips:   a sound's decoded clip at a playback rate (its pitch; linear interpolation), with a
//            gain per side;
//   streams: a music / ambience track decoded from its Ogg as it plays, following its jumps (the
//            global parameters decide the conditional ones) and faded in / out by its AHDSR.
// Each through its bus's volume and the master's. Every gain change ramps across a block, so
// nothing clicks. Single-threaded: the app mixes on its own loop (Audio::pump).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "game/audio/audio_bank.h"

namespace plat {
struct Ogg;
}

class Mixer {
public:
    using Voice = uint32_t; // 0 = none
    static constexpr int MAX_VOICES = 32; // (past it, new sounds are dropped)

    Mixer() = default;
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;
    ~Mixer();

    // `clip` (which must outlive the voice) at `rate` (1 = as recorded), `left` / `right` gains.
    // `pausable`: held while paused (setPaused).
    Voice playClip(const Clip& clip, Bus bus, float rate, float left, float right, bool pausable);
    // `def` (which must outlive the voice) from its start; 0 if its file won't open.
    Voice playStream(const StreamDef& def);
    // A stream fades out over its release, then ends; a clip ends at once.
    void stop(Voice voice);
    void stopAll(); // everything, at once
    bool playing(Voice voice) const; // (false once it has ended, or for 0)

    void setBusVolume(Bus bus, float volume); // linear (FMOD's Bus.setVolume)
    void setMasterVolume(float volume);
    void setPaused(bool paused);
    void setParameter(std::string_view name, float value); // a global parameter (stream jumps)
    float parameter(std::string_view name) const;           // (0 if never set)

    // The next block (plat::AUDIO_BLOCK frames, interleaved L R).
    void mix(int16_t* out);

    int64_t streamPosition(Voice voice) const; // the frame a stream plays next (tests; -1: not a stream)

private:
    struct ClipVoice {
        Voice id;
        const Clip* clip;
        Bus bus;
        double pos = 0;
        float rate, left, right;
        bool pausable;
    };
    struct StreamVoice {
        Voice id;
        const StreamDef* def;
        plat::Ogg* ogg;
        int64_t pos = 0;
        bool checked = false;   // the jumps at `pos` were seen (or it landed there by one)
        float levelDb = -80;    // the AHDSR's level, dB
        float gainMixed = 0;    // its gain as the last block ended
        bool releasing = false;
        float releaseFrom = 0;  // the level the release started at
        float releaseT = 0;     // s into the release
        float attackT = 0;      // s into the attack
        bool ended = false;
    };

    Voice nextId();
    bool full() const { return clips_.size() + streams_.size() >= MAX_VOICES; }
    int readStream(StreamVoice& s, int16_t* out, int frames); // fewer than asked: it ended
    float envelope(StreamVoice& s, float dt) const;            // its level after `dt` more seconds, linear
    void closeStream(StreamVoice& s);

    std::vector<ClipVoice> clips_;
    std::vector<StreamVoice> streams_;
    std::map<std::string, float, std::less<>> params_;
    float bus_[3] = {1, 1, 1}, busMixed_[3] = {1, 1, 1}; // set / as the last block ended
    float master_ = 1, masterMixed_ = 1;
    bool paused_ = false;
    Voice lastId_ = 0;
    std::vector<float> acc_;       // the block being mixed
    std::vector<int16_t> decoded_; // a stream's frames for the block
};
