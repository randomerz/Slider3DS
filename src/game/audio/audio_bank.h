// The exported sounds: assets/audio/audio.json + clips.pcm + its Ogg files, from tools/make_audio.py
// (the FMOD events the port plays, rendered; its docstring has the format). The clips played all the
// time (footsteps, slides, clicks, the voices) are raw PCM, read in one go at load. The rare ones are
// Ogg, not even opened at load (Tremor's setup per file costs an Old 3DS a lot): the audio thread
// decodes them a slice at a time after it (decodeSome: the title screen's intro gives it seconds; a
// clip becomes playable once whole), or the rest at once if one plays before that (decode). Music
// and ambience stream from their files as they play.
#pragma once

#include <cstdint>
#include <functional> // (std::less<>)
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "game/data/curve.h"
#include "platform/platform.h"

// The buses under Master whose volumes are the player's (Settings).
enum class Bus { Sfx, Music, Ambience };

struct Clip {
    std::string path;     // its Ogg (checked when decoded: mono, at plat::AUDIO_RATE, `frames` long); empty: raw
    int64_t frames = 0;   // its length
    mutable std::vector<int16_t> pcm; // its frames (an Ogg's: empty until AudioBank decodes it)
};

// An AudioManager sound.
struct SoundDef {
    std::vector<Clip> clips;  // a multi instrument's playlist: one picked at random
    float gain = 1;           // linear (the event's master volume)
    float pitchRandom = 0;    // semitones: a random pitch within +- half of it
    bool pitchParam = false;  // the "Pitch" parameter (0.5-2) sets the playback rate
    bool spatial = false;     // positioned by its emitter (min / max distance, linear-squared rolloff)
    float minDistance = 1, maxDistance = 20;
    bool canPause = true;     // held while the game is paused
};

// An AudioManager music / ambience track.
struct StreamDef {
    struct Jump {
        int64_t at = 0, to = 0; // frames: on playing up to `at`, continue from `to`...
        std::string param;      // ...always (empty), or while this global parameter is in [min, max]
        float min = 0, max = 0;
    };
    std::string file; // under audio/ (plat::AUDIO_RATE)
    int channels = 2; // its file's: 2, or 1 (played the same on both sides)
    int64_t frames = 0; // its file's length (the file is checked against both as it opens)
    Bus bus = Bus::Music;
    float gain = 1;
    float attack = 0, release = 0; // s, in from / out to silence (on the bank's AHDSR shape; 0: at once)
    int64_t length = 0;            // playing up to it ends the track (past it: crossfades, by jumps)
    std::vector<Jump> jumps;       // in order of `at`; the first that applies wins
    // Its volume by a global parameter (none if empty): dB at each value, in order (measured).
    std::string volumeParam;
    std::vector<std::pair<float, float>> volumeLevels;
};

// FMOD's AHDSR shape (measured: tools/fmod_measure.py): dB at each 5% of a release, 0 to -80.
struct AhdsrShape {
    static constexpr int POINTS = 21;
    float db[POINTS] = {};
    float release(float x) const; // dB below the start at `x` (0-1) of the release
    float attack(float x) const { return release(1 - x); } // dB below full at `x` of the attack
};

// An NPC voice's synth (the vocalizer's), rendered with its effects (tools/make_audio.py).
struct VoiceSynth {
    static constexpr const char* VOWELS = "aeiou";
    bool held = false;          // loops while a syllable lasts: clips[i] for VOWELS[i]; else one at random
    std::vector<Clip> clips;
    int loopStart = 0, loopEnd = 0; // held: frames
    float gain = 1;
    bool pitched = false;       // "Pitch" changes it
    bool spatial = false;
    float minDistance = 1, maxDistance = 20;
};

// FMOD's spatializer as Slider's listener hears it (measured: tools/fmod_measure.py): a mono sound
// splits over the two sides at constant power, `gain` in all, at an angle (45 deg centered, 90 hard
// right) that follows its azimuth (its side over the listener's height).
struct SpatialModel {
    float gain = 1.6847f;
    std::vector<std::pair<float, float>> pan = {{0.0f, 45.0f}, {90.0f, 90.0f}}; // [azimuth, angle] deg, rising
    float angle(float azimuthDeg) const;                             // (by |azimuth|; mirrored left)
};

// IndoorMusic3Eq: the music bus's 3-band EQ while the player is in a house (fitted to FMOD's ThreeEQ
// as the modifier sets it: tools/fmod_measure.py).
struct IndoorEq {
    float lowHz = 280, highHz = 1650; // crossovers
    float lowDb = 0, highDb = 0;      // below / above them
};

class AudioBank {
public:
    // audio/audio.json and the raw clips. False (and `error`) if anything is missing or doesn't
    // match its entry: they're part of the build. (An Ogg clip is checked when it's decoded.)
    bool load(std::string* error = nullptr);
    void unload();
    AudioBank() = default;
    AudioBank(const AudioBank&) = delete;
    AudioBank& operator=(const AudioBank&) = delete;
    ~AudioBank() { unload(); }

    const SoundDef* sound(std::string_view name) const;
    const StreamDef* music(std::string_view name) const;
    const StreamDef* ambience(std::string_view name) const;
    const VoiceSynth* voice(std::string_view event) const;
    // Decodes `clip` (one of the bank's) if it isn't yet.
    void decode(const Clip& clip);
    // Decodes Ogg clips not yet decoded for about `budgetMs` — at least a slice (DECODE_SLICE
    // frames; a clip's first also opens it) — false once all are.
    bool decodeSome(double budgetMs);
    static constexpr int DECODE_SLICE = 4096;
    const Clip* decoding() const { return partialOgg_ ? undecoded_.back() : nullptr; } // (half decoded; tests)
    void decodeAll() { while (decodeSome(1e9)) {} }
    // AudioManager.soundDampenCurve: DampenMusic's shape, 1 -> 0 -> 1 over its length.
    const std::vector<CurveKey>& dampenCurve() const { return dampenCurve_; }
    const AhdsrShape& ahdsr() const { return ahdsr_; }
    const IndoorEq& indoorEq() const { return indoorEq_; }
    const SpatialModel& spatial() const { return spatial_; }

private:
    IndoorEq indoorEq_;
    SpatialModel spatial_;
    AhdsrShape ahdsr_;
    std::vector<CurveKey> dampenCurve_;
    std::vector<const Clip*> undecoded_; // (the maps' nodes don't move; the last one is decoding)
    plat::Ogg* partialOgg_ = nullptr;    // the last undecoded clip's, open while it's decoding
    std::vector<int16_t> partial_;       // its frames so far (moved into the clip when whole)
    int64_t partialDone_ = 0;
    std::map<std::string, SoundDef, std::less<>> sounds_;
    std::map<std::string, StreamDef, std::less<>> music_, ambience_;
    std::map<std::string, VoiceSynth, std::less<>> voices_;
};
