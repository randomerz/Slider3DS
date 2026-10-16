// An AnimationCurve as the exporter samples it (21 values evenly over 0..1, interpolated between
// them), and Mathf's clamped lerp, for the motions ported from Unity (item pick-ups and drops, Chad's
// jump).
#pragma once

#include <algorithm>

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

namespace mathf {
// Mathf.Lerp: t clamped to 0..1 (a curve that overshoots 1 doesn't carry the motion past b).
inline float lerp(float a, float b, float t) { return a + (b - a) * std::min(std::max(t, 0.0f), 1.0f); }
} // namespace mathf
