#include "game/dev/profiler.h"

#include <algorithm>

#include "platform/platform.h"

namespace {

float g_ms[(int)ProfSection::Count] = {};
int g_current = -1; // the innermost open scope's section
double g_mark = 0;  // when time last started counting toward it
double (*g_busyBetween)(double, double, void*) = nullptr;
void* g_busyUser = nullptr;

// Counts the time since the mark toward the open section (less what another thread on this core
// took: AudioThread's) and restarts the mark.
void charge() {
    const double now = plat::nowMs();
    const double stolen = g_busyBetween && g_mark > 0 ? std::min(g_busyBetween(g_mark, now, g_busyUser), now - g_mark) : 0;
    if (g_current >= 0) g_ms[g_current] += (float)(now - g_mark - stolen);
    g_ms[(int)ProfSection::AudioThread] += (float)stolen;
    g_mark = now;
}

} // namespace

namespace prof {

void beginFrame() {
    for (float& m : g_ms) m = 0;
}

float ms(ProfSection s) { return g_ms[(int)s]; }

void setStolenTime(double (*busyBetween)(double, double, void*), void* user) {
    g_busyBetween = busyBetween;
    g_busyUser = user;
}

} // namespace prof

ProfScope::ProfScope(ProfSection s) : outer_(g_current) {
    charge();
    g_current = (int)s;
}

ProfScope::~ProfScope() {
    charge();
    g_current = outer_;
}
