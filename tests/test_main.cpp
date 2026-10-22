#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "test.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

int g_checks = 0, g_failures = 0;

} // namespace

TestRegistrar::TestRegistrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }

void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) {
        g_failures++;
        printf("    FAIL  %s\n", what.c_str());
    }
}

const Area& villageArea() {
    static Area area;
    static bool loaded = false, failed = false;
    static std::string error;
    if (!loaded && !failed) {
        loaded = loadArea("village/village.json", area, &error);
        failed = !loaded;
    }
    check(loaded, "load village/village.json: " + error);
    return area;
}

void forEachCall(const Area& area, const std::function<void(const CallDef&)>& f) {
    auto all = [&](const std::vector<CallDef>& calls) {
        for (const CallDef& c : calls) f(c);
    };
    for (const ConditionCheckerDef& c : area.conditionCheckers) {
        all(c.conditionals.onSuccess);
        all(c.conditionals.onFail);
    }
    for (const InteractionDef& i : area.interactions) all(i.onSuccess);
    for (const TriggerEventDef& t : area.triggerEvents) {
        all(t.onEnter);
        all(t.onExit);
    }
    for (const CollectibleDef& c : area.collectibles) all(c.onCollect);
    for (const ItemDef& it : area.items) {
        all(it.onPickUp);
        all(it.onDrop);
    }
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds) {
            all(c.onEnter);
            all(c.onExhausted);
            for (const DialogueDef& d : c.dialogue) {
                all(d.onStart);
                all(d.onEnd);
            }
        }
}

int main(int argc, char** argv) {
#ifdef _MSC_VER
    // A failed debug assertion (e.g. vector subscript) prints and aborts instead of opening a dialog.
    for (int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;
    for (const TestCase& t : registry()) {
        if (filter && !strstr(t.name, filter)) continue;
        int checks = g_checks, failures = g_failures;
        t.fn();
        run++;
        bool ok = g_failures == failures;
        if (!ok) failedTests++;
        printf("%s  %s (%d checks)\n", ok ? "ok  " : "FAIL", t.name, g_checks - checks);
    }
    printf("%s: %d tests, %d checks, %d failed check%s\n", g_failures ? "FAILED" : "OK", run, g_checks, g_failures,
           g_failures == 1 ? "" : "s");
    return (g_failures || run == 0) ? 1 : 0;
}
