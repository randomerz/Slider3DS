// The player's preferences (Unity's SettingsManager), apart from the save profile: World holds them
// for the systems that read them; the frontend sets them (an options menu later; dev: L + SELECT
// toggles autoMove). Defaults are SettingsManager's.
#pragma once

struct Settings {
    // A port feature (the user's "solve mode"): a tap on an Artifact tile with a single move option
    // moves it, as the save's forceAutoMove<Area> does during the final puzzle. Unity registers
    // Settings.AutoMove (default off) but never reads it.
    bool autoMove = false;
    // Settings.ScreenShake: scales CameraShake's amounts.
    float screenShake = 0.5f;
};
