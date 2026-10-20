// Mixes what plays into the platform's blocks (plat::AUDIO_BLOCK stereo frames at AUDIO_RATE):
//   clips:   a sound's decoded clip at a playback rate (its pitch; linear interpolation), with a
//            gain per side;
//   streams: a music / ambience track decoded from its file (game/audio/adpcm) as it plays, following its jumps (the
//            global parameters decide the conditional ones), faded in / out by its AHDSR (on
//            FMOD's shape) and at the volume its parameter sets (an ambience's distance).
// Each through its bus's volume and the master's; the music bus can also run a 3-band EQ (the
// indoor one: below / above two crossovers turned down; flat, and skipped, at unity). Every gain
// change ramps across a block, so nothing clicks. Single-threaded: only Audio's thread (or the app's loop,
// without one) touches it.
#pragma once

#include <cstdint>
#include <functional> // (std::less<>)
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "game/audio/adpcm.h"
#include "game/audio/audio_bank.h"

// How a clip plays besides its rate and gains (a vocalizer syllable uses all of it).
struct ClipShape {
    float rateMid = 0, rateEnd = 0; // > 0: the rate glides to rateMid over the first half of
    float glide = 0;                //   `glide` s, then to rateEnd over the second
    int loopStart = -1, loopEnd = -1; // frames: on reaching loopEnd it goes on from loopStart
    float stopAfter = -1;             // s: then it fades out (as a stop())
    float fadeOut = 0;                // s a stop fades over (0: one block)
};

class Mixer {
public:
    using Voice = uint32_t; // 0 = none
    static constexpr int MAX_VOICES = 32; // (past it, new sounds are dropped)

    explicit Mixer(const AhdsrShape* ahdsr = nullptr) : ahdsr_(ahdsr) {} // (none: fades linear in dB)
    void setAhdsr(const AhdsrShape* ahdsr) { ahdsr_ = ahdsr; }
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;
    ~Mixer();

    // `clip` (which must outlive the voice) at `rate` (1 = as recorded), `left` / `right` gains.
    // `pausable`: held while paused (setPaused).
    Voice playClip(const Clip& clip, Bus bus, float rate, float left, float right, bool pausable,
                   const ClipShape& shape = {});
    // `def` (which must outlive the voice) from its start; 0 if its file won't open.
    Voice playStream(const StreamDef& def);
    // A stream fades out over its release, then ends; a clip over its shape's fadeOut (a stream
    // without a release, over the next block).
    void stop(Voice voice);
    void stopAll(); // everything, at once
    bool playing(Voice voice) const; // (false once it has ended, or for 0)

    void setBusVolume(Bus bus, float volume); // linear (FMOD's Bus.setVolume)
    void setMasterVolume(float volume);
    void setPaused(bool paused);
    // The music bus's EQ: linear gains below `lowHz` and above `highHz` (1, 1: off).
    void setMusicEq(float lowHz, float highHz, float lowGain, float highGain);
    void setParameter(std::string_view name, float value); // a global parameter (stream jumps)
    float parameter(std::string_view name) const;           // (0 if never set)

    // The next block (plat::AUDIO_BLOCK frames, interleaved L R).
    void mix(int16_t* out);

    int64_t streamPosition(Voice voice) const; // the frame a stream plays next (tests; -1: not a stream)
    int voices() const { return (int)(clips_.size() + streams_.size()); } // (ended ones go at the next block)
    // ms spent decoding streams since the last call (the audio thread's load breakdown).
    double takeStreamMs() {
        const double ms = streamMs_;
        streamMs_ = 0;
        return ms;
    }

private:
    struct ClipVoice {
        Voice id;
        const Clip* clip;
        Bus bus;
        double pos = 0;
        float rate, left, right;
        bool pausable;
        ClipShape shape;
        int64_t played = 0;    // output frames so far
        bool stopping = false; // fading out, then gone
        int fadeLeft = 0, fadeTotal = 0; // output frames
    };
    void stopClip(ClipVoice& c);
    struct StreamVoice {
        Voice id;
        const StreamDef* def;
        std::unique_ptr<AdpcmStream> file;
        int64_t pos = 0;
        bool checked = false;   // the jumps at `pos` were seen (or it landed there by one)
        float levelDb = -80;    // the AHDSR's level, dB
        float gainMixed = 0;    // its gain as the last block ended
        bool releasing = false;
        float releaseFrom = 0;  // the level the release started at
        float releaseT = 0;     // s into the release
        float attackT = 0;      // s into the attack
        bool ended = false;
        bool readFailed = false; // the last block's read came up short before the file's end
        int failures = 0;        // blocks in a row that did (a few: retried, the rest of each silent)
    };

    // A 2nd-order low / high pass (RBJ, Q = 1/sqrt 2), state per channel; tools/fmod_measure.py
    // fits the indoor EQ's gains to these exact filters.
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1[2] = {}, x2[2] = {}, y1[2] = {}, y2[2] = {};
        static Biquad pass(bool low, float hz);
        float run(float x, int ch) {
            const float y = b0 * x + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];
            x2[ch] = x1[ch];
            x1[ch] = x;
            y2[ch] = y1[ch];
            y1[ch] = y;
            return y;
        }
    };
    Voice nextId();
    bool full() const { return clips_.size() + streams_.size() >= MAX_VOICES; }
    int readStream(StreamVoice& s, int16_t* out, int frames); // fewer than asked: it ended
    float envelope(StreamVoice& s, float dt) const;            // its level after `dt` more seconds, linear
    float paramGain(const StreamDef& def) const;               // its volume parameter's, linear
    void closeStream(StreamVoice& s);

    std::vector<ClipVoice> clips_;
    std::vector<StreamVoice> streams_;
    std::map<std::string, float, std::less<>> params_;
    float bus_[3] = {1, 1, 1}, busMixed_[3] = {1, 1, 1}; // set / as the last block ended
    float master_ = 1, masterMixed_ = 1;
    bool paused_ = false;
    const AhdsrShape* ahdsr_ = nullptr;
    Biquad eqLow_, eqHigh_;
    float eqGain_[2] = {1, 1}, eqMixed_[2] = {1, 1}; // low, high: set / as the last block ended
    std::vector<float> musicAcc_;                     // the music bus's block, while its EQ runs
    Voice lastId_ = 0;
    std::vector<float> acc_;       // the block being mixed
    double streamMs_ = 0;
    std::vector<int16_t> decoded_; // a stream's frames for the block
};
