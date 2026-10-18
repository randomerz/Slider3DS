#include "game/fx/screen_effects.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "game/data/curve.h"

namespace {

// Gradient noise in 0..1 around 0.5, smooth, like Mathf.PerlinNoise(x, 0) (not its exact values:
// the noise starts at a random offset anyway).
float perlin(float x) {
    auto grad = [](int i) {
        uint32_t h = (uint32_t)i * 0x27d4eb2du;
        h ^= h >> 15;
        h *= 0x85ebca6bu;
        h ^= h >> 13;
        return (float)(h & 0xffff) / 32767.5f - 1.0f; // -1..1
    };
    const float fl = std::floor(x);
    const int i = (int)fl;
    const float f = x - fl, u = f * f * f * (f * (f * 6 - 15) + 10);
    const float a = grad(i) * f, b = grad(i + 1) * (f - 1);
    return 0.5f + a + (b - a) * u;
}

// NoiseSettings.NoiseParams.GetValueAt.
float noiseAt(float frequency, float amplitude, bool constant, float time, float offset) {
    const float t = frequency * time + offset;
    if (constant) return std::cos(t * 2 * 3.14159265f) * amplitude * 0.5f;
    return (perlin(t) - 0.5f) * amplitude;
}

} // namespace

float ScreenEffects::random() {
    rng_ ^= rng_ << 13; // xorshift32
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float)(rng_ >> 8) / (float)(1u << 24);
}

void ScreenEffects::reset() {
    *this = ScreenEffects();
    offsetX_ = random() * 2000 - 1000;
    offsetY_ = random() * 2000 - 1000;
}

void ScreenEffects::startBlack(float from, float to, EffectDone done, uint32_t frame) {
    blackOn_ = true; // (FadeCoroutine: the start alpha this frame)
    black_ = blackFrom_ = from;
    blackTo_ = to;
    blackT_ = 0;
    blackDone_ = done;
    blackSince_ = frame;
}

void ScreenEffects::flashWhite(EffectDone middle) {
    flashMiddle_ = middle;
    flash_ = Flash::Up; // (FlashCoroutine: alpha 0 this frame, t counts from the next)
    flashT_ = 0;
    flashFresh_ = true;
}

void ScreenEffects::shakeIncrease(float duration, float amount) { shakes_.push_back({duration, 0, amount, 0}); }

void ScreenEffects::shake(float duration, float amount) {
    shakes_.push_back({duration, amount, 0, 0});
}

EffectCallbacks ScreenEffects::update(const Area& area, float dt, float screenShake, uint32_t frame) {
    EffectCallbacks done;
    if (blackOn_ && frame != blackSince_) {
        blackT_ += dt;
        if (area.fadeDuration <= 0 || blackT_ >= area.fadeDuration) {
            blackOn_ = false; // (disableAtEnd)
            black_ = 0;
            done.fadeEnd = std::exchange(blackDone_, EffectDone::None);
        } else {
            black_ = blackFrom_ + (blackTo_ - blackFrom_) * (blackT_ / area.fadeDuration);
        }
    }
    // FlashCoroutine: t climbs to flashDuration / 2, then falls back below 0.
    if (flash_ != Flash::None && !std::exchange(flashFresh_, false)) {
        if (flash_ == Flash::Up) {
            flashT_ += dt;
            if (flashT_ >= area.flashDuration / 2) { // (the peak: callbackMiddle)
                flash_ = Flash::Down;
                done.flashMiddle = std::exchange(flashMiddle_, EffectDone::None);
            }
        } else {
            flashT_ -= dt;
            if (flashT_ < 0) flash_ = Flash::None;
        }
    }
    updateShake(area.cameraShake, dt, screenShake);
    return done;
}

void ScreenEffects::updateShake(const CameraShakeDef& cs, float dt, float screenShake) {
    // CameraShake.FindMaxShake: finished shakes go, the strongest of the rest is the gain.
    gain_ = 0;
    for (size_t i = 0; i < shakes_.size();) {
        Shake& s = shakes_[i];
        if (s.elapsed > s.duration) {
            shakes_.erase(shakes_.begin() + (long)i);
            continue;
        }
        gain_ = std::max(gain_, screenShake * mathf::lerp(s.start, s.end, s.duration > 0 ? s.elapsed / s.duration : 1));
        s.elapsed += dt;
        i++;
    }
    // CinemachineBasicMultiChannelPerlin: the channels' sum, times the gain (camera-relative x / y;
    // the 2D camera isn't rotated).
    noiseTime_ += dt * cs.frequencyGain;
    shakeX_ = shakeY_ = 0;
    if (gain_ <= 0) return;
    for (const CameraShakeDef::Channel& ch : cs.position) {
        shakeX_ += noiseAt(ch.frequency[0], ch.amplitude[0], ch.constant[0], noiseTime_, offsetX_);
        shakeY_ += noiseAt(ch.frequency[1], ch.amplitude[1], ch.constant[1], noiseTime_, offsetY_);
    }
    shakeX_ *= gain_;
    shakeY_ *= gain_;
}

float ScreenEffects::whiteAlpha(const Area& area) const {
    if (flash_ == Flash::None || area.flashDuration <= 0) return 0;
    return std::min(std::max(flashT_ / (area.flashDuration / 2), 0.0f), 1.0f);
}
