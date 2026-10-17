// Screen-level effects scripts ask for, for the frontend to show: UIEffects' black fade and white
// flash, SceneChanger leaving the area, and CameraShake (the camera's Cinemachine noise, whose
// amplitude the strongest running shake sets each frame). Part of World.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

class ScreenEffects {
public:
    static constexpr float SHAKE_SETTING = 0.5f; // Settings.ScreenShake's default (SettingsManager)

    void reset(); // nothing showing; the noise from its start

    void fadeFromBlack() { fade = 1; } // UIEffectApplier.FadeFromBlack
    void flashWhite();                 // UIEffects.FlashWhite: white up over flashDuration / 2, then back
    // CameraShake.Shake: `amount` (times the setting) falling to 0 over `duration` s.
    void shake(float duration, float amount);
    // One frame: the fade and flash advance; CameraShake.Update picks the strongest shake, then the
    // camera's noise (CinemachineBasicMultiChannelPerlin) moves by it.
    void update(const Area& area, float dt);

    float whiteAlpha(const Area& area) const; // the white panel's alpha
    float shakeGain() const { return gain_; } // the noise's amplitude gain this frame
    // Where the shake puts the camera, relative to where it follows (world units).
    float shakeX() const { return shakeX_; }
    float shakeY() const { return shakeY_; }

    float fade = 0;        // FadeFromBlack: black overlay alpha, falls to 0 over area.fadeDuration
    bool leftArea = false; // SceneChanger.ChangeScenes ran (the port ends at the Village)

private:
    struct Shake {
        float duration, start, end, elapsed; // CameraShake.CameraShakeData (no delay: Shake's)
    };
    float random(); // [0, 1)

    float flashT_ = 0;
    bool flashFresh_ = false; // started this frame (scripts run before update)
    enum class Flash : uint8_t { None, Up, Down } flash_ = Flash::None;
    std::vector<Shake> shakes_;
    float gain_ = 0, noiseTime_ = 0, shakeX_ = 0, shakeY_ = 0;
    float offsetX_ = 0, offsetY_ = 0; // the noise's random time offsets (Random.Range(-1000, 1000))
    uint32_t rng_ = 0x9e3779b9u;
};
