// The player's preferences (Unity's SettingsManager), apart from the save profile: main owns them
// (loaded at start, kept in their own file, outliving any World); World reads them, the options
// menu (game/ui/pause_menu) changes them. Defaults are SettingsManager's.
#pragma once

#include <string>
#include <string_view>

struct Settings {
    static constexpr const char* FILE_NAME = "settings.txt"; // a user file (plat::readUserFile)
    static constexpr int STEPS = 10; // every 0-1 setting moves in 10% steps (the options menu's sliders)

    // A port feature (the user's "solve mode"): a tap on an Artifact tile with a single move option
    // moves it, as the save's forceAutoMove<Area> does during the final puzzle. Unity registers
    // Settings.AutoMove (default off) but never reads it.
    bool autoMove = false;
    // Settings.ScreenShake, 0-1: scales CameraShake's amounts.
    float screenShake = 0.5f;
    // Settings.ScreenVisualEffects, 0-1: UIEffects' white flashes peak at it (game/ui/hud).
    float screenEffects = 1.0f;
    // BigTextEnabled: kept in the file, but neither shown nor read (the options menu hides it: its
    // 1.5x dialogue would break the one pixel scale; user's call).
    bool bigText = false;
    // The buses' volumes, 0-1, linear (AudioManager.SetMasterVolume / SFX "Sound" / Music /
    // Ambience): game/audio's Audio::setVolumes. Port: max by default (Unity: 0.5), as the 3DS has a volume slider.
    float masterVolume = 1, sfxVolume = 1, musicVolume = 1, ambienceVolume = 1;
    // A port addition: the pause menu's Cheats page works only with this on (game/world/cheats), and
    // so do dev builds' SELECT shortcuts (the perf overlay, noclip).
    bool cheats = false;
    // (A new setting: add it to FIELDS in settings.cpp too, or it's neither saved nor compared; a
    // member count there stops the build until it is.)

    // `value` clamped to 0-1 and snapped to the nearest step (so it saves and reads back exactly).
    static float snap(float value);
    void setScreenShake(float value) { screenShake = snap(value); }

    bool operator==(const Settings& o) const;
    bool operator!=(const Settings& o) const { return !(*this == o); }

    // One "key=value" line per setting. Parsing keeps the defaults for missing, unknown or bad
    // lines (an older or hand-edited file still loads) and clamps values into range (and onto steps).
    std::string serialize() const;
    static Settings parse(std::string_view text);

    // The saved settings (defaults if there are none yet); save() writes them, true on success.
    static Settings load();
    bool save() const;
};
