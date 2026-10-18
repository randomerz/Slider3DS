// AudioManager for the port: plays the game's cues (game/audio/audio_cues) from the exported bank
// (game/audio/audio_bank) through the Mixer, at the player's volumes (Settings: Master and the SFX
// / Music / Ambience buses, linear as FMOD's Bus.setVolume), and feeds the platform. Main owns it:
// load at start, run the Frontend's cues and pump every frame. Unloaded, it plays nothing.
#pragma once

#include <cstdint>
#include <functional>
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
    bool load(std::string* error = nullptr); // the bank (see AudioBank::load)
    void unload();

    void run(const std::vector<AudioCue>& cues);
    void setVolumes(const Settings& settings);
    void setPaused(bool paused); // pausable sounds hold (the pause menu; AudioManager.SetPaused)
    // Mixes and queues as many blocks as the platform wants now.
    void pump();

    // What a music track plays on (0: not playing), and the mixer (tests).
    Mixer::Voice music(std::string_view name) const;
    const Mixer& mixer() const { return mixer_; }
    Mixer& mixer() { return mixer_; }

private:
    void play(const AudioCue& cue);
    void playMusic(const AudioCue& cue);
    float random01(); // [0, 1)

    AudioBank bank_;
    Mixer mixer_; // (after the bank: its voices point into it)
    std::map<std::string, Mixer::Voice, std::less<>> music_;
    uint32_t rng_ = 0x2545f491;
};
