// What the game asks of AudioManager, queued as data for the app's Audio (game/audio/audio): game
// logic stays free of the mixer, and tests read what was asked. The World queues its own
// (World::audio); the Frontend gathers them each frame with the title's and the menus'.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct AudioCue {
    enum class Kind { Play, PlayMusic, StopMusic, SetParameter };
    Kind kind;
    std::string name;    // AudioManager's sound / music name, or the parameter's
    float value = 0;     // SetParameter: the value; Play: the "Pitch" parameter (0: the event's own)
    bool restart = true; // PlayMusic: restart the track if it's already playing
};

class AudioCues {
public:
    // AudioManager.Play / PlayWithPitch.
    void play(std::string_view sound, float pitch = 0) { add({AudioCue::Kind::Play, std::string(sound), pitch}); }
    // AudioManager.PlayMusic (stopOtherTracks: the other music stops).
    void playMusic(std::string_view name, bool restart = true) {
        add({AudioCue::Kind::PlayMusic, std::string(name), 0, restart});
    }
    void stopMusic(std::string_view name) { add({AudioCue::Kind::StopMusic, std::string(name)}); }
    // AudioManager.SetGlobalParameter.
    void setParameter(std::string_view name, float value) {
        add({AudioCue::Kind::SetParameter, std::string(name), value});
    }

    // Moves `other`'s cues after these.
    void takeFrom(AudioCues& other) {
        for (AudioCue& c : other.cues_) cues_.push_back(std::move(c));
        other.cues_.clear();
    }
    std::vector<AudioCue> take() { return std::exchange(cues_, {}); }
    const std::vector<AudioCue>& pending() const { return cues_; }

private:
    void add(AudioCue cue) { cues_.push_back(std::move(cue)); }
    std::vector<AudioCue> cues_;
};
