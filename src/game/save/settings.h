// The player's preferences (Unity's SettingsManager), apart from the save profile: main owns them
// (loaded at start, kept in their own file, outliving any World); World reads them, the options
// menu (game/ui/pause_menu) changes them. Defaults are SettingsManager's.
#pragma once

#include <string>
#include <string_view>

struct Settings {
    static constexpr const char* FILE_NAME = "settings.txt"; // a user file (plat::readUserFile)
    static constexpr int SHAKE_STEPS = 10; // the options menu's screen shake slider: 0-100% in 10% steps

    // A port feature (the user's "solve mode"): a tap on an Artifact tile with a single move option
    // moves it, as the save's forceAutoMove<Area> does during the final puzzle. Unity registers
    // Settings.AutoMove (default off) but never reads it.
    bool autoMove = false;
    // Settings.ScreenShake, 0-1: scales CameraShake's amounts.
    float screenShake = 0.5f;

    // screenShake = `value` clamped to 0-1, snapped to the nearest step (so it saves and reads back exactly).
    void setScreenShake(float value);
    int screenShakeStep() const; // 0..SHAKE_STEPS

    bool operator==(const Settings& o) const { return autoMove == o.autoMove && screenShake == o.screenShake; }
    bool operator!=(const Settings& o) const { return !(*this == o); }

    // One "key=value" line per setting. Parsing keeps the defaults for missing, unknown or bad
    // lines (an older or hand-edited file still loads) and clamps values into range (and onto steps).
    std::string serialize() const;
    static Settings parse(std::string_view text);

    // The saved settings (defaults if there are none yet); save() writes them, true on success.
    static Settings load();
    bool save() const;
};
