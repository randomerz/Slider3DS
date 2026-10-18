// The exported sounds: assets/audio/audio.json + its Ogg files, from tools/make_audio.py (the FMOD
// events the port plays, rendered; its docstring has the format). A sound's clips are decoded whole
// at load (they're short); music and ambience stream from their files as they play.
#pragma once

#include <cstdint>
#include <functional> // (std::less<>)
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "game/data/curve.h"

// The buses under Master whose volumes are the player's (Settings).
enum class Bus { Sfx, Music, Ambience };

struct Clip {
    std::vector<int16_t> pcm; // mono, plat::AUDIO_RATE
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
    std::string file; // under audio/ (stereo, plat::AUDIO_RATE)
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

class AudioBank {
public:
    // audio/audio.json, every sound's clips decoded. False (and `error`) if anything is missing or
    // doesn't match its entry: they're part of the build.
    bool load(std::string* error = nullptr);
    void unload();

    const SoundDef* sound(std::string_view name) const;
    const StreamDef* music(std::string_view name) const;
    const StreamDef* ambience(std::string_view name) const;
    // AudioManager.soundDampenCurve: DampenMusic's shape, 1 -> 0 -> 1 over its length.
    const std::vector<CurveKey>& dampenCurve() const { return dampenCurve_; }
    const AhdsrShape& ahdsr() const { return ahdsr_; }

private:
    AhdsrShape ahdsr_;
    std::vector<CurveKey> dampenCurve_;
    std::map<std::string, SoundDef, std::less<>> sounds_;
    std::map<std::string, StreamDef, std::less<>> music_, ambience_;
};
