// Tiny test framework for the headless PC suite (slider3ds_tests).
//   TEST(name) { CHECK(cond); check(cond, "message"); }
// Tests self-register; run all with slider3ds_tests.exe, or only those whose name contains
// a substring with slider3ds_tests.exe <substring>. Exit code 0 = all passed.
#pragma once

#include <functional>
#include <string>

#include "game/data/area.h"
#include "game/fx/item_pickup.h"

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
// Frames after the one a collectible is touched in that it's surely collected by: ItemPickupEffect's
// DoOnCollect comes 0.75 s in (counted from the next frame), plus a frame for rounding.
constexpr int PICKUP_FRAMES = (int)(ItemPickup::TEXT_AFTER * 60) + 2;
// Every exported call in `area` (conditionals, interactions, triggers, collectibles, items, NPCs).
void forEachCall(const Area& area, const std::function<void(const CallDef&)>& f);
