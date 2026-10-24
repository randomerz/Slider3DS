// AudioManager for the port: plays the game's cues (game/audio/audio_cues) from the exported bank
// (game/audio/audio_bank) through the Mixer, at the player's volumes (Settings: Master and the SFX
// / Music / Ambience buses, linear as FMOD's Bus.setVolume), and feeds the platform. Main owns it:
// load, then startThread; every frame the listener, the Frontend's cues, then pump. Unloaded, or
// without an audio output, it plays nothing.
//   Threads: with the platform's audio thread running, the setters only hand their values over
//   (the inbox, under plat::lockAudio) and the thread applies them, mixes and decodes (service);
//   pump does nothing. Without one (tests, no output) they apply at once and pump mixes.
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
#include "game/audio/busy_log.h"
#include "game/audio/mixer.h"
#include "game/data/rng.h"

struct Settings;

class Audio {
public:
    static constexpr float LISTENER_Z = 10; // AudioManager.ZLevel
    static constexpr float INDOOR_Y = -75;  // SoundWrapper.IsActuallyIndoor: at or below it, in a house
    static constexpr float INDOOR_MUTE_DB = 5; // AudioManager.indoorMuteDB
    static constexpr double DECODE_BUDGET_MS = 2; // per service, on decoding the bank's Ogg clips
    static constexpr float VOICE_RELEASE = 0.1f; // a held synth's End: its last 0.1 s fading as a syllable stops

    bool load(std::string* error = nullptr); // the bank (see AudioBank::load)
    void unload();                           // (stops the thread first)
    void startThread();                      // (after load; nothing if the platform has none)
    void stopThread();

    void setListener(const Listener& listener); // (in a house: the indoor music EQ)
    void run(const std::vector<AudioCue>& cues);
    void setVolumes(const Settings& settings);
    void setPaused(bool paused); // spatial sounds hold (the pause menu; AudioManager.SetPaused)
    // Without the thread: mixes and queues as many blocks as the platform wants now, then decodes a
    // little.
    void pump();

    // What a music / ambience track plays on (0: not playing), the mixer (tests).
    Mixer::Voice music(std::string_view name) const;
    Mixer::Voice ambience(std::string_view name) const;
    const Mixer& mixer() const { return mixer_; }
    Mixer& mixer() { return mixer_; }
    float musicDampening() const; // the music bus's dip now, 0-1 (1: none)
    // The audio thread's busy share of its time over about the last second, 0-1 (dev overlay; all
    // 0: no thread): in all, decoding streams, decoding clips (the rest: mixing, cues), and the part
    // of those spent reading the files (io). Wall time: waits (file reads, locks) count as busy.
    struct ThreadLoad {
        bool running = false;
        float total = 0, streams = 0, clips = 0, io = 0;
        int core = -1; // (-1: unknown)
        int underruns = 0; // the output ran dry (plat::audioUnderruns)
        // The music playing: its position and where it loops back (s in its timeline; -1: none).
        // Past its loop point it's in the baked crossfade, which counts on from the loop point.
        float bgmS = -1, bgmLoopS = -1;
    };
    ThreadLoad threadLoad() const;
    // ms the audio thread was busy within [t0, t1] (plat::nowMs times; its last BusyLog::SIZE
    // services; wall time): the profiler's stolen time when it shares the game's core.
    double threadBusyBetween(double t0, double t1) const;
    static double threadBusyCallback(double t0, double t1, void* self) { // (prof::setStolenTime's)
        return static_cast<const Audio*>(self)->threadBusyBetween(t0, t1);
    }
    // The gains a spatial sound gets at (x, y) (each side; the event's volume not included).
    void spatialGains(const SoundDef& s, bool positioned, float x, float y, bool alwaysOutdoor, float& left,
                      float& right) const;
    // Mixes one block (pump's step; tests mix through it so the dampening advances).
    void mixBlock(int16_t* out);
    ~Audio() { stopThread(); }

private:
    // ThreadLoad's bgmS / bgmLoopS: reads the mixer, so the audio thread's only (service).
    void bgmPosition(float& at, float& loop) const;
    struct Dampen {
        float amount, length, t;
        int key;
    };
    struct Volumes {
        float master = 1, sfx = 1, music = 1, ambience = 1;
    };
    // What the game thread hands the audio thread (the latest of each; cues in order).
    struct Inbox {
        std::vector<AudioCue> cues;
        Listener listener;
        Volumes volumes;
        bool paused = false;
    };
    static void serviceThread(void* self) { static_cast<Audio*>(self)->service(); }
    void service(); // the thread's: the inbox, then mix what's wanted, then decode
    void fill();    // mixes and queues the blocks the platform wants
    void applyListener(const Listener& listener);
    void applyCues(const std::vector<AudioCue>& cues);
    void applyVolumes(const Volumes& v);
    void applyPaused(bool paused);
    void play(const AudioCue& cue);
    void playVoice(const AudioCue& cue);
    void stopVoices(int key); // key 0: all of them
    void playMusic(const AudioCue& cue);
    void playAmbience(const AudioCue& cue);
    float random01() { return rng_.unit(); } // [0, 1)

    AudioBank bank_;
    Mixer mixer_; // (after the bank: its voices point into it)
    std::map<std::string, Mixer::Voice, std::less<>> music_, ambience_;
    std::vector<Dampen> dampens_;
    std::vector<std::pair<int, Mixer::Voice>> voices_; // speaker, syllable
    Listener listener_;
    float musicVolume_ = 1; // the setting (the bus also carries the dampening)
    bool paused_ = false;
    Rng rng_{0x2545f491u};
    bool threaded_ = false;
    Inbox inbox_;                        // (guarded by plat::lockAudio)
    std::vector<AudioCue> taken_;        // the audio thread's: the cues taken from the inbox
    double loadStart_ = 0, busyMs_ = 0, streamMs_ = 0, clipMs_ = 0, ioMs_ = 0; // the audio thread's: this second's
    ThreadLoad load_;                    // (guarded by plat::lockAudio) the last second's
    BusyLog busyLog_;                    // (guarded by plat::lockAudio) the latest services
};
