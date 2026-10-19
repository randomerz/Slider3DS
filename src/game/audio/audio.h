// AudioManager for the port: plays the game's cues (game/audio/audio_cues) from the exported bank
// (game/audio/audio_bank) through the Mixer, at the player's volumes (Settings: Master and the SFX
// / Music / Ambience buses, linear as FMOD's Bus.setVolume), and feeds the platform. Main owns it:
// load at start; every frame the listener, the Frontend's cues, then pump. Unloaded, or without an
// audio output, it plays nothing.
//   2D sounds: full on both sides (FMOD's mono upmix, measured). Spatial sounds: FMOD's spatializer
//   as AudioManager sets it up — the listener 10 units off the plane above the camera's target
//   (ZLevel; distance attenuation from there too), linear-squared rolloff between the event's min
//   and max distance, split by side on the bank's measured SpatialModel; a
//   sound in a house heard outside (or the other way) is moved by the housing offset and turned
//   down (indoorMuteDB). Each is placed once, as it starts (the port's sounds are short).
//   Voices: a vocalizer's syllables (PlayVoice) on its rendered synth clips — the syllable's vowel,
//   its pitch as the playback rate (gliding), held in the clip's loop while it lasts, then faded
//   (the synth's End); StopVoice fades a speaker's out; pausing stops them all (ManagedInstance:
//   dialogue hard-stops on pause).
//   Music: PlayMusic stops the other tracks; DampenMusic dips the music bus along the bank's curve
//   (the lowest of those running; held while paused); in a house the music bus is muffled
//   (IndoorMusic3Eq: the bank's EQ).
#pragma once

#include <cstdint>
#include <functional> // (std::less<>)
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "game/audio/audio_bank.h"
#include "game/audio/audio_cues.h"
#include "game/audio/mixer.h"

struct Settings;

class Audio {
public:
    static constexpr float LISTENER_Z = 10; // AudioManager.ZLevel
    static constexpr float INDOOR_Y = -75;  // SoundWrapper.IsActuallyIndoor: at or below it, in a house
    static constexpr float INDOOR_MUTE_DB = 5; // AudioManager.indoorMuteDB
    static constexpr double DECODE_BUDGET_MS = 2; // per frame, on decoding the bank's clips
    static constexpr float VOICE_RELEASE = 0.1f; // a held synth's End: its last 0.1 s fading as a syllable stops

    bool load(std::string* error = nullptr); // the bank (see AudioBank::load)
    void unload();

    void setListener(const Listener& listener); // (in a house: the indoor music EQ)
    void run(const std::vector<AudioCue>& cues);
    void setVolumes(const Settings& settings);
    void setPaused(bool paused); // spatial sounds hold (the pause menu; AudioManager.SetPaused)
    // Mixes and queues as many blocks as the platform wants now.
    void pump();

    // What a music / ambience track plays on (0: not playing), the mixer (tests).
    Mixer::Voice music(std::string_view name) const;
    Mixer::Voice ambience(std::string_view name) const;
    const Mixer& mixer() const { return mixer_; }
    Mixer& mixer() { return mixer_; }
    float musicDampening() const; // the music bus's dip now, 0-1 (1: none)
    // The gains a spatial sound gets at (x, y) (each side; the event's volume not included).
    void spatialGains(const SoundDef& s, bool positioned, float x, float y, bool alwaysOutdoor, float& left,
                      float& right) const;
    // Mixes one block (pump's step; tests mix through it so the dampening advances).
    void mixBlock(int16_t* out);

private:
    struct Dampen {
        float amount, length, t;
        int key;
    };
    void play(const AudioCue& cue);
    void playVoice(const AudioCue& cue);
    void stopVoices(int key); // key 0: all of them
    void playMusic(const AudioCue& cue);
    void playAmbience(const AudioCue& cue);
    float random01(); // [0, 1)

    AudioBank bank_;
    Mixer mixer_; // (after the bank: its voices point into it)
    std::map<std::string, Mixer::Voice, std::less<>> music_, ambience_;
    std::vector<Dampen> dampens_;
    std::vector<std::pair<int, Mixer::Voice>> voices_; // speaker, syllable
    Listener listener_;
    float musicVolume_ = 1; // the setting (the bus also carries the dampening)
    bool paused_ = false;
    uint32_t rng_ = 0x2545f491;
};
