// Dev perf overlay for the bottom screen:
//   - a history graph of the last frames: one column per frame, CPU time stacked by
//     game/dev/profiler's sections (DRW building the draws, DEV this overlay, END submitting the
//     frame, ATH the audio thread's time taken from the game's core, WAI on top: plat::beginFrame's
//     wait for the display's pace and the GPU, so a column is the whole frame), lines at 16.7 ms
//     (60 fps) and 33.3 ms (30 fps), so spikes and what caused them show;
//   - CPU: the same stack averaged over about half a second, its total (WAI left out) and the FPS;
//   - GPU (drawing; 0 where the backend can't measure it);
//   - a legend: each section's averaged ms in its color, and SPR, sprites drawn on the top screen
//     (red near the draw budget);
//   - the boot above it: total s, then each step's (IN platform init, AR the area, SH sprite
//     sheets, UI fonts + menus + title art, AU audio, GA settings + the
//     Frontend: the World, the save profiles); above that the audio thread: its core (C0 the
//     game's, C2 a New 3DS's), its busy share and how much of it went to
//     decoding streams (STR) and clips (CLP) — the rest mixes —, how much of it read files (IO;
//     wall time: waits count as busy), and DRY, the gaps after the output ran dry (stutters);
//     above the panel too, the worst frame of the last second and its biggest section but WAI (AUD in the legend is only the main
//     loop's part: handing the cues over).
#pragma once

#include "game/audio/audio.h"
#include "game/dev/profiler.h"
#include "platform/platform.h"

class DevOverlay {
public:
    static constexpr int SECTIONS = (int)ProfSection::Count;
    static constexpr int HISTORY = 100; // frames in the graph

    // Once per frame, after it (its draws submitted): the profiler's times, the platform's stats,
    // sprites drawn.
    void record(const plat::FrameStats& stats, int sprites, const Audio::ThreadLoad& audioThread);
    void draw() const;
    void setBoot(float init, float area, float sheets, float ui, float audio, float game); // s each

private:
    float avg_[SECTIONS] = {};
    float history_[HISTORY][SECTIONS] = {};
    int next_ = 0; // the oldest column (the next one written)
    char boot_[64] = {};
    Audio::ThreadLoad audio_;
    float gpuMs_ = 0, frameMs_ = 1000.0f / 60.0f, sprites_ = 0;
};
