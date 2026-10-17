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
    // Not read yet (the options menu shows and saves them): Settings.ScreenVisualEffects (flashes),
    // BigTextEnabled, and the volumes (Master, SFX "Sound", Music, Ambience) for when audio lands.
    float screenEffects = 1.0f;
    bool bigText = false;
    float masterVolume = 0.5f, sfxVolume = 0.5f, musicVolume = 0.5f, ambienceVolume = 0.5f;
    // (A new setting: add it to FIELDS in settings.cpp too, or it's neither saved nor compared;
    // a size check there catches a forgotten one.)

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
