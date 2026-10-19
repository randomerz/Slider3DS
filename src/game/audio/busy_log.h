// The audio thread's latest busy spans (plat::nowMs times, wall clock: waits on file reads and
// locks included), for the dev profiler's stolen time (game/dev/profiler: ATH). A ring; Audio keeps
// it under plat::lockAudio.
#pragma once

#include <algorithm>

class BusyLog {
public:
    static constexpr int SIZE = 64; // spans remembered (services: ~0.25 s)

    void add(double start, double end) {
        spans_[next_] = {start, end};
        next_ = (next_ + 1) % SIZE;
        count_ = std::min(count_ + 1, SIZE);
    }

    // ms of the spans within [t0, t1]: newest first, stopping at the first that ended before t0
    // (spans come in order), so a frame's queries read only its few.
    double between(double t0, double t1) const {
        double ms = 0;
        for (int k = 1; k <= count_; k++) {
            const Span& s = spans_[(next_ - k + SIZE) % SIZE];
            if (s.end < t0) break;
            ms += std::max(0.0, std::min(s.end, t1) - std::max(s.start, t0));
        }
        return ms;
    }

    // The span the ring still covers: from its oldest start (0: empty).
    double oldest() const { return count_ ? spans_[(next_ - count_ + SIZE) % SIZE].start : 0; }

private:
    struct Span {
        double start = 0, end = 0;
    };
    Span spans_[SIZE];
    int next_ = 0, count_ = 0;
};
