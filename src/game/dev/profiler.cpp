#include "game/dev/profiler.h"

#include "platform/platform.h"

namespace {

float g_ms[(int)ProfSection::Count] = {};
int g_current = -1; // the innermost open scope's section
double g_mark = 0;  // when time last started counting toward it

// Counts the time since the mark toward the open section and restarts the mark.
void charge() {
    const double now = plat::nowMs();
    if (g_current >= 0) g_ms[g_current] += (float)(now - g_mark);
    g_mark = now;
}

} // namespace

namespace prof {

void beginFrame() {
    for (float& m : g_ms) m = 0;
}

float ms(ProfSection s) { return g_ms[(int)s]; }

} // namespace prof

ProfScope::ProfScope(ProfSection s) : outer_(g_current) {
    charge();
    g_current = (int)s;
}

ProfScope::~ProfScope() {
    charge();
    g_current = outer_;
}
