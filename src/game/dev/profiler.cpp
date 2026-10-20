#include "game/dev/profiler.h"

#include <algorithm>
#include <cstdint>

#include "platform/platform.h"

namespace {

// A stretch of time toward one section (-1: none), as an edge logged it; resolved into g_ms (with
// the other thread's share taken out) only when the times are read, outside every scope.
struct Interval {
    double t0, t1;
    int8_t section;
};
constexpr int MAX_INTERVALS = 512; // (a frame's edges: ~2 per tile layer plus a handful)

bool g_enabled = false;
float g_ms[(int)ProfSection::Count] = {};
Interval g_intervals[MAX_INTERVALS];
int g_count = 0;
int g_current = -1; // the innermost open scope's section
double g_mark = 0;  // when time last started counting toward it
double (*g_busyBetween)(double, double, void*) = nullptr;
void* g_busyUser = nullptr;

// Counts the logged intervals toward their sections, less what another thread on this core took
// (AudioThread's).
void resolve() {
    for (int i = 0; i < g_count; i++) {
        const Interval& iv = g_intervals[i];
        const double span = iv.t1 - iv.t0;
        const double stolen = g_busyBetween && iv.t0 > 0 && span <= prof::MAX_STOLEN_SPAN_MS
                                  ? std::min(g_busyBetween(iv.t0, iv.t1, g_busyUser), span)
                                  : 0;
        if (iv.section >= 0) g_ms[iv.section] += (float)(span - stolen);
        g_ms[(int)ProfSection::AudioThread] += (float)stolen;
    }
    g_count = 0;
}

// Logs the time since the mark toward the open section and restarts the mark: a clock read.
void charge() {
    const double now = plat::nowMs();
    if (g_count == MAX_INTERVALS) resolve(); // (never in practice: a frame has far fewer edges)
    g_intervals[g_count++] = {g_mark, now, (int8_t)g_current};
    g_mark = now;
}

} // namespace

namespace prof {

void setEnabled(bool enabled) { g_enabled = enabled; }

void beginFrame() {
    for (float& m : g_ms) m = 0;
    g_count = 0;
}

float ms(ProfSection s) {
    if (g_count > 0) resolve();
    return g_ms[(int)s];
}

void setStolenTime(double (*busyBetween)(double, double, void*), void* user) {
    g_busyBetween = busyBetween;
    g_busyUser = user;
}

} // namespace prof

ProfScope::ProfScope(ProfSection s) : outer_(g_current), active_(g_enabled) {
    if (!active_) return;
    charge();
    g_current = (int)s;
}

ProfScope::~ProfScope() {
    if (!active_) return;
    charge();
    g_current = outer_;
}
