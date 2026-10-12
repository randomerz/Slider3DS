#include <cstdio>
#include <cstring>
#include <vector>

#include "test.h"

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

const Area& village() {
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

const ObjectStates& villageObjects() {
    static ObjectStates objects;
    static bool init = false;
    if (!init) {
        objects.init(village());
        init = true;
    }
    return objects;
}

int main(int argc, char** argv) {
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
