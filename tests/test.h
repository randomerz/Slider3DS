// Tiny test framework for the headless PC suite (slider3ds_tests).
//   TEST(name) { CHECK(cond); check(cond, "message"); }
// Tests self-register; run all with slider3ds_tests.exe, or only those whose name contains
// a substring with slider3ds_tests.exe <substring>. Exit code 0 = all passed.
#pragma once

#include <string>

#include "game/data/area.h"

struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)());
};

#define TEST(name)                                              \
    static void test_##name();                                  \
    static TestRegistrar registrar_##name(#name, test_##name);  \
    static void test_##name()

// Records a check in the running test; failures are printed with `what`.
void check(bool ok, const std::string& what);
#define CHECK(cond) check((cond), #cond)

// The real Village export, loaded once (a failed load fails the calling test and returns an
// empty Area).
const Area& villageArea();
