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

// A keyframe of an AnimationCurve kept as authored: tangents, and weights (Keyframe.inWeight /
// outWeight; an unweighted side's is 1/3, which makes the segment Unity's plain Hermite).
struct CurveKey {
    static constexpr float INFINITE = 1e29f; // a tangent at least this big: the value holds until the next key
    static constexpr float UNWEIGHTED = 1.0f / 3;
    float t, v, in, out;
    float inW = UNWEIGHTED, outW = UNWEIGHTED;
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
        const float dt = b.t - a.t;
        if (a.outW == CurveKey::UNWEIGHTED && b.inW == CurveKey::UNWEIGHTED) {
            const float s = (t - a.t) / dt, s2 = s * s, s3 = s2 * s;
            return (2 * s3 - 3 * s2 + 1) * a.v + (s3 - 2 * s2 + s) * dt * a.out + (-2 * s3 + 3 * s2) * b.v + (s3 - s2) * dt * b.in;
        }
        // Weighted: a cubic Bezier in (time, value) whose inner points lie outW / inW of the segment's
        // length along each key's tangent; its time rises with the parameter (weights 0..1), so the
        // parameter for t is found by bisection.
        const float w0 = std::min(std::max(a.outW, 0.0f), 1.0f), w1 = std::min(std::max(b.inW, 0.0f), 1.0f);
        auto bezier = [](float p0, float p1, float p2, float p3, float s) {
            const float r = 1 - s;
            return r * r * r * p0 + 3 * r * r * s * p1 + 3 * r * s * s * p2 + s * s * s * p3;
        };
        const float x = (t - a.t) / dt;
        float lo = 0, hi = 1;
        for (int step = 0; step < 20; step++) {
            const float mid = (lo + hi) / 2;
            (bezier(0, w0, 1 - w1, 1, mid) < x ? lo : hi) = mid;
        }
        return bezier(a.v, a.v + w0 * dt * a.out, b.v - w1 * dt * b.in, b.v, (lo + hi) / 2);
    }
    return keys.back().v;
}

namespace mathf {
// Mathf.Lerp: t clamped to 0..1 (a curve that overshoots 1 doesn't carry the motion past b).
inline float lerp(float a, float b, float t) { return a + (b - a) * std::min(std::max(t, 0.0f), 1.0f); }
} // namespace mathf
