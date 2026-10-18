// What the game asks of AudioManager, queued as data for the app's Audio (game/audio/audio): game
// logic stays free of the mixer, and tests read what was asked. The World queues its own
// (World::audio); the Frontend gathers them each frame with the title's and the menus'.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Where the sounds are heard from (AudioManager.UpdateCameraPosition).
struct Listener {
    float x = 0, y = 0;     // world units: the camera's target
    bool indoor = false;    // the player is in a house (Player.isInHouse)
    float housingOffset = 0; // SGrid.housingOffset: a house interior's y offset
};

struct AudioCue {
    enum class Kind { Play, PlayMusic, StopMusic, PlayAmbience, StopAmbience, SetParameter, DampenMusic };
    Kind kind;
    std::string name;    // AudioManager's sound / music / ambience name, or the parameter's
    float value = 0;     // SetParameter: the value; Play: the "Pitch" parameter (0: the event's own);
                         // DampenMusic: the volume it dips to
    bool restart = true; // PlayMusic: restart the track if it's already playing
    float volume = 1;    // Play: SoundWrapper.WithVolume; DampenMusic: the length, s
    bool positioned = false; // Play: at (x, y) (WithAttachmentToTransform), else at the listener
    float x = 0, y = 0;      // world units
    bool alwaysOutdoor = false; // Play: IndoorStatus.AlwaysOutdoor (else by its y: a house below)
};

class AudioCues {
public:
    // AudioManager.Play / PlayWithVolume / PlayWithPitch (a spatial sound at the listener).
    void play(std::string_view sound, float volume = 1, float pitch = 0) {
        AudioCue c{AudioCue::Kind::Play, std::string(sound), pitch};
        c.volume = volume;
        add(std::move(c));
    }
    // A sound attached to something at (x, y) (AudioManager.Play(name, transform)).
    void playAt(std::string_view sound, float x, float y, float volume = 1, float pitch = 0, bool alwaysOutdoor = false) {
        AudioCue c{AudioCue::Kind::Play, std::string(sound), pitch};
        c.volume = volume;
        c.positioned = true;
        c.x = x;
        c.y = y;
        c.alwaysOutdoor = alwaysOutdoor;
        add(std::move(c));
    }
    // AudioManager.PlayMusic (stopOtherTracks: the other music stops).
    void playMusic(std::string_view name, bool restart = true) {
        add({AudioCue::Kind::PlayMusic, std::string(name), 0, restart});
    }
    void stopMusic(std::string_view name) { add({AudioCue::Kind::StopMusic, std::string(name)}); }
    void playAmbience(std::string_view name) { add({AudioCue::Kind::PlayAmbience, std::string(name)}); }
    void stopAmbience(std::string_view name) { add({AudioCue::Kind::StopAmbience, std::string(name)}); }
    // AudioManager.SetGlobalParameter.
    void setParameter(std::string_view name, float value) {
        add({AudioCue::Kind::SetParameter, std::string(name), value});
    }
    // AudioManager.DampenMusic: the music dips to `amount` and comes back over `length` s.
    void dampenMusic(float amount, float length) {
        AudioCue c{AudioCue::Kind::DampenMusic, "", amount};
        c.volume = length;
        add(std::move(c));
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
