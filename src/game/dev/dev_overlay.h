// Dev perf overlay for the bottom screen, in pages (SELECT cycles off → FRAME → DRAW → off):
//   - a history graph of the last frames: one column per frame, its time stacked by the page's rows
//     (each row a group of game/dev/profiler's sections), WAI on top (plat::beginFrame's wait for
//     the display's pace and the GPU, so a column is about the whole frame: only the loop's own
//     edges, overlay.record and plat::mainLoop, go uncounted), lines at 16.7 ms (60 fps) and 33.3 ms
//     (30 fps), so spikes and what caused them show;
//   - CPU: the same stack averaged over about half a second without WAI (its end vs the 16.7 tick =
//     the headroom), its total and the FPS;
//   - GPU (drawing; 0 where the backend can't measure it);
//   - a legend: each row's averaged ms in its color, then the page's counts;
//   - above the panel: the audio thread (its core (C0 the game's, C2 a New 3DS's), its busy share and
//     how much of it went to decoding streams (STR) and clips (CLP) — the rest mixes —, how much of
//     it read files (IO; wall time: waits count as busy), DRY, the gaps after the output ran dry
//     (stutters), and BGM, the music's position / where it loops back (m:ss; in the loop's
//     crossfade it counts on past it)); the boot (total s, then each step's: IN platform init, AR
//     the area, SH sprite sheets, UI fonts + menus + title art, AU audio, GA settings + the
//     Frontend: the World, the save profiles); the worst frame of the last second by its work (WAI
//     left out): its ms, its biggest row's ms, and UP: the row furthest over its own average (+ms).
// Pages:
//   FRAME: LOG PHY NPC ANI VFX AUD DRW (all the drawing) DEV END ATH (the audio thread's time
//     taken from the game's core) WAI, and SPR, the top screen's sprites (red near the draw
//     budget). (AUD is only the main loop's part: handing the cues over.)
//   DRAW: the drawing apart — LST (gathering what the top screen draws), SRT (sorting it), TIL (the
//     tile layers' cells), OBJ (the rest of it: sprites, text, lines, particles), BOT (the bottom
//     screen), DRW (the rest: the title screen, the top overlays, starting the screens), OTH (all
//     else but WAI), WAI; and SPR, of them CEL (tile cells).
#pragma once

#include "game/audio/audio.h"
#include "game/dev/profiler.h"
#include "platform/platform.h"

class DevOverlay {
public:
    enum class Page { Frame, Draw };
    static constexpr int PAGES = 2;
    static constexpr int SECTIONS = (int)ProfSection::Count;
    static constexpr int HISTORY = 100; // frames in the graph

    // Once per frame, after it (its draws submitted): the profiler's times, the platform's stats,
    // the top screen's sprites (and, of them, tile cells).
    void record(const plat::FrameStats& stats, int sprites, int tiles, const Audio::ThreadLoad& audioThread);
    void draw(Page page) const;
    void setBoot(float init, float area, float sheets, float ui, float audio, float game); // s each

private:
    float avg_[SECTIONS] = {};
    float history_[HISTORY][SECTIONS] = {};
    int next_ = 0; // the oldest column (the next one written)
    char boot_[64] = {};
    Audio::ThreadLoad audio_;
    float gpuMs_ = 0, frameMs_ = 1000.0f / 60.0f, sprites_ = 0, tiles_ = 0;
};
