// AnimationCurves as exported: sampled (21 values evenly over 0..1, interpolated between them: item
// pick-ups and drops, Chad's jump) or as keys (Animator clips' transform curves), and Mathf's clamped lerp.
#pragma once

#include <algorithm>
#include <vector>

struct SampledCurve {
    static constexpr int SAMPLES = 21;
    float s[SAMPLES] = {};
    // AnimationCurve.Evaluate(t), t clamped to 0..1.
    float eval(float t) const {
        const float x = std::min(std::max(t, 0.0f), 1.0f) * (SAMPLES - 1);
        const int i = std::min((int)x, SAMPLES - 2);
        return s[i] + (s[i + 1] - s[i]) * (x - (float)i);
    }
};

// A keyframe of an AnimationCurve kept as authored (Unity's unweighted Hermite between keys).
struct CurveKey {
    static constexpr float INFINITE = 1e29f; // a tangent at least this big: the value holds until the next key
    float t, v, in, out;
};

// AnimationCurve.Evaluate on its keys (clamped to the first / last key's value outside them).
inline float evalKeys(const std::vector<CurveKey>& keys, float t) {
    if (keys.empty()) return 0;
    if (t <= keys.front().t) return keys.front().v;
    for (size_t i = 1; i < keys.size(); i++) {
        const CurveKey& a = keys[i - 1];
        const CurveKey& b = keys[i];
        if (t >= b.t) continue;
        if (a.out >= CurveKey::INFINITE || a.out <= -CurveKey::INFINITE || b.in >= CurveKey::INFINITE || b.in <= -CurveKey::INFINITE)
            return a.v; // stepped
        const float dt = b.t - a.t, s = (t - a.t) / dt, s2 = s * s, s3 = s2 * s;
        return (2 * s3 - 3 * s2 + 1) * a.v + (s3 - 2 * s2 + s) * dt * a.out + (-2 * s3 + 3 * s2) * b.v + (s3 - s2) * dt * b.in;
    }
    return keys.back().v;
}

namespace mathf {
// Mathf.Lerp: t clamped to 0..1 (a curve that overshoots 1 doesn't carry the motion past b).
inline float lerp(float a, float b, float t) { return a + (b - a) * std::min(std::max(t, 0.0f), 1.0f); }
} // namespace mathf
