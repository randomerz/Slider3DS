// The dev profiler (game/dev/profiler): nested scopes count their own time; another thread's time on
// the same core (setStolenTime) comes out of the scope it interrupted and goes to AudioThread.
#include "game/audio/busy_log.h"
#include "game/dev/profiler.h"
#include "test.h"
#include "test_platform.h"

namespace {

// Half of any span was "the other thread's".
double half(double t0, double t1, void*) { return (t1 - t0) / 2; }

} // namespace

TEST(profiler_nested_scopes_count_their_own_time) {
    prof::setEnabled(true);
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
    prof::setEnabled(false);
    testSetNowMs(0);
}

TEST(profiler_off_counts_nothing) {
    // Off (release, or the overlay hidden): scopes don't touch the clock or the times.
    prof::setEnabled(false);
    testSetNowMs(1500);
    prof::beginFrame();
    {
        const ProfScope logic(ProfSection::Logic);
        testSetNowMs(1510);
    }
    CHECK(prof::ms(ProfSection::Logic) == 0);
    testSetNowMs(0);
}

TEST(profiler_takes_stolen_time_out_of_the_scope_it_interrupted) {
    prof::setEnabled(true);
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
    // (read with the hook still set: the times resolve as they're read)
    CHECK(prof::ms(ProfSection::Draw) == 2 && prof::ms(ProfSection::Logic) == 0);
    CHECK(prof::ms(ProfSection::AudioThread) == 5);
    prof::setStolenTime(nullptr, nullptr);
    prof::setEnabled(false);
    testSetNowMs(0);
}

namespace {

// More than any span: the profiler must not take out more than the span itself.
double greedy(double t0, double t1, void*) { return (t1 - t0) * 3; }

} // namespace

TEST(profiler_never_takes_more_than_the_span_and_skips_long_gaps) {
    prof::setEnabled(true);
    testSetNowMs(3000);
    { const ProfScope prime(ProfSection::Logic); }
    prof::setStolenTime(&greedy, nullptr);
    prof::beginFrame();
    {
        const ProfScope draw(ProfSection::Draw);
        testSetNowMs(3004);
    }
    CHECK(prof::ms(ProfSection::Draw) == 0 && prof::ms(ProfSection::AudioThread) == 4);
    // Back from a long pause (HOME): the gap charges nothing stolen (the log doesn't reach back).
    prof::beginFrame();
    testSetNowMs(3004 + prof::MAX_STOLEN_SPAN_MS + 500);
    {
        const ProfScope logic(ProfSection::Logic);
        testSetNowMs(3004 + prof::MAX_STOLEN_SPAN_MS + 502);
    }
    CHECK(prof::ms(ProfSection::AudioThread) == 2 && prof::ms(ProfSection::Logic) == 0);
    prof::setStolenTime(nullptr, nullptr);
    prof::setEnabled(false);
    testSetNowMs(0);
}

namespace {

int g_queries = 0;
double counted(double, double, void*) {
    g_queries++;
    return 0;
}

} // namespace

TEST(profiler_asks_for_stolen_time_only_when_the_times_are_read) {
    // The stolen-time query (the audio thread's lock) runs as ms() is read, not at each edge: its
    // cost never lands in a section.
    prof::setEnabled(true);
    testSetNowMs(4000);
    { const ProfScope prime(ProfSection::Logic); }
    prof::setStolenTime(&counted, nullptr);
    prof::beginFrame();
    g_queries = 0;
    for (int i = 0; i < 20; i++) {
        const ProfScope tiles(ProfSection::DrawTiles);
        testSetNowMs(4001 + i);
    }
    CHECK(g_queries == 0);
    CHECK(prof::ms(ProfSection::DrawTiles) == 20 && g_queries > 0);
    prof::setStolenTime(nullptr, nullptr);
    prof::setEnabled(false);
    testSetNowMs(0);
}

TEST(busy_log_sums_the_spans_inside_an_interval) {
    BusyLog log;
    CHECK(log.between(0, 100) == 0 && log.oldest() == 0);
    log.add(10, 14);
    log.add(20, 22);
    log.add(30, 35);
    CHECK(log.between(0, 100) == 11);  // (all three)
    CHECK(log.between(12, 31) == 5);   // (2 of the first, the second, 1 of the third: clipped)
    CHECK(log.between(15, 19) == 0);   // (between spans)
    CHECK(log.between(40, 50) == 0);   // (after them all)
    // The ring keeps the latest SIZE spans: older ones are gone.
    for (int i = 0; i < BusyLog::SIZE; i++) log.add(100 + 10 * i, 101 + 10 * i);
    CHECK(log.between(0, 99) == 0 && log.oldest() == 100);
    CHECK(log.between(0, 1e9) == BusyLog::SIZE);
}
