// Screen-level effects scripts ask for, for the frontend to show: UIEffects' black fade and white
// flash, SceneChanger leaving the area, and CameraShake (the camera's Cinemachine noise, whose
// amplitude the strongest running shake sets each frame). Part of World.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

// What runs when a screen effect gets somewhere (UIEffects' callbacks; World::effectDone runs it):
// a fade to black's end (FadeToBlack's callback), a white flash's peak (FlashWhite's callbackMiddle).
enum class EffectDone : uint8_t { None, DollyEnd, VillageShuffle };

// The callbacks due after one ScreenEffects::update.
struct EffectCallbacks {
    EffectDone fadeEnd = EffectDone::None;
    EffectDone flashMiddle = EffectDone::None;
};

class ScreenEffects {
public:
    void reset(); // nothing showing; the noise from its start

    // UIEffects' black panel: FadeFromBlack (black to clear) and FadeToBlack (clear to black, then
    // `done`), each over area.fadeDuration, counted from the frame after `frame` (World::frame when
    // it starts: wait.h's rule); the panel turns off at the end (the callback usually fades back in
    // that frame). A new fade cancels a running one, callback and all.
    void fadeFromBlack(uint32_t frame) { startBlack(1, 0, EffectDone::None, frame); }
    void fadeToBlack(EffectDone done, uint32_t frame) { startBlack(0, 1, done, frame); }
    float black() const { return black_; } // the black panel's alpha
    // UIEffects.FlashWhite: white up over flashDuration / 2 (then `middle`), then back. A new flash
    // cancels a running one and its callback.
    void flashWhite(EffectDone middle = EffectDone::None);
    // CameraShake.Shake: `amount` falling to 0 over `duration` s.
    void shake(float duration, float amount);
    // One frame: the fade and flash advance; CameraShake.Update picks the strongest shake (times the
    // screenShake setting), then the camera's noise (CinemachineBasicMultiChannelPerlin) moves by it.
    // Returns the callbacks now due (for World to run). `frame`: World::frame.
    EffectCallbacks update(const Area& area, float dt, float screenShake, uint32_t frame);

    float whiteAlpha(const Area& area) const; // the white panel's alpha
    float shakeGain() const { return gain_; } // the noise's amplitude gain this frame
    // Where the shake puts the camera, relative to where it follows (world units).
    float shakeX() const { return shakeX_; }
    float shakeY() const { return shakeY_; }

    bool leftArea = false; // SceneChanger.ChangeScenes ran (the port ends at the Village)

private:
    struct Shake {
        float duration, start, end, elapsed; // CameraShake.CameraShakeData (no delay: Shake's)
    };
    float random(); // [0, 1)
    void startBlack(float from, float to, EffectDone done, uint32_t frame);

    bool blackOn_ = false;
    float black_ = 0, blackFrom_ = 0, blackTo_ = 0, blackT_ = 0;
    uint32_t blackSince_ = 0;
    EffectDone blackDone_ = EffectDone::None;

    float flashT_ = 0;
    bool flashFresh_ = false; // started this frame (scripts run before update)
    EffectDone flashMiddle_ = EffectDone::None;
    enum class Flash : uint8_t { None, Up, Down } flash_ = Flash::None;
    std::vector<Shake> shakes_;
    float gain_ = 0, noiseTime_ = 0, shakeX_ = 0, shakeY_ = 0;
    float offsetX_ = 0, offsetY_ = 0; // the noise's random time offsets (Random.Range(-1000, 1000))
    uint32_t rng_ = 0x9e3779b9u;
};
