// AudioManager for the port: plays the game's cues (game/audio/audio_cues) from the exported bank
// (game/audio/audio_bank) through the Mixer, at the player's volumes (Settings: Master and the SFX
// / Music / Ambience buses, linear as FMOD's Bus.setVolume), and feeds the platform. Main owns it:
// load at start; every frame the listener, the Frontend's cues, then pump. Unloaded, or without an
// audio output, it plays nothing.
//   Spatial sounds: FMOD's spatializer as AudioManager sets it up — the listener 10 units off the
//   plane above the camera's target (ZLevel; distance attenuation from there too), linear-squared
//   rolloff between the event's min and max distance, panned by the side the source is on; a
//   sound in a house heard outside (or the other way) is moved by the housing offset and turned
//   down (indoorMuteDB). Each is placed once, as it starts (the port's sounds are short).
//   Music: PlayMusic stops the other tracks; DampenMusic dips the music bus along the bank's curve
//   (the lowest of those running; held while paused).
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

    bool load(std::string* error = nullptr); // the bank (see AudioBank::load)
    void unload();

    void setListener(const Listener& listener) { listener_ = listener; }
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
    };
    void play(const AudioCue& cue);
    void playMusic(const AudioCue& cue);
    void playAmbience(const AudioCue& cue);
    float random01(); // [0, 1)

    AudioBank bank_;
    Mixer mixer_; // (after the bank: its voices point into it)
    std::map<std::string, Mixer::Voice, std::less<>> music_, ambience_;
    std::vector<Dampen> dampens_;
    Listener listener_;
    float musicVolume_ = 1; // the setting (the bus also carries the dampening)
    bool paused_ = false;
    uint32_t rng_ = 0x2545f491;
};
