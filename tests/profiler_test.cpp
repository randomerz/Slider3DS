// The dev profiler (game/dev/profiler): nested scopes count their own time; another thread's time on
// the same core (setStolenTime) comes out of the scope it interrupted and goes to AudioThread.
#include "game/dev/profiler.h"
#include "test.h"
#include "test_platform.h"

namespace {

// Half of any span was "the other thread's".
double half(double t0, double t1, void*) { return (t1 - t0) / 2; }

} // namespace

TEST(profiler_nested_scopes_count_their_own_time) {
    testSetNowMs(1000);
    prof::beginFrame();
    {
        const ProfScope logic(ProfSection::Logic);
        testSetNowMs(1002);
        {
            const ProfScope physics(ProfSection::Physics);
            testSetNowMs(1005);
        }
        testSetNowMs(1006);
    }
    CHECK(prof::ms(ProfSection::Logic) == 3 && prof::ms(ProfSection::Physics) == 3);
    CHECK(prof::ms(ProfSection::AudioThread) == 0);
    testSetNowMs(0);
}

TEST(profiler_takes_stolen_time_out_of_the_scope_it_interrupted) {
    testSetNowMs(2000);
    { const ProfScope prime(ProfSection::Logic); } // (the clock's mark: now, before the hook)
    prof::setStolenTime(&half, nullptr);
    prof::beginFrame();
    {
        const ProfScope draw(ProfSection::Draw);
        testSetNowMs(2004);
    }
    testSetNowMs(2010); // (between scopes: only the stolen part is counted, to AudioThread)
    {
        const ProfScope logic(ProfSection::Logic);
    }
    prof::setStolenTime(nullptr, nullptr);
    CHECK(prof::ms(ProfSection::Draw) == 2 && prof::ms(ProfSection::Logic) == 0);
    CHECK(prof::ms(ProfSection::AudioThread) == 5);
    testSetNowMs(0);
}
