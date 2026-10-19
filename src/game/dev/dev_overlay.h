// Dev perf overlay for the bottom screen:
//   - a history graph of the last frames: one column per frame, CPU time stacked by section
//     (game/dev/profiler's, plus DRW), lines at 16.7 ms (60 fps) and 33.3 ms (30 fps), so spikes
//     and what caused them show;
//   - CPU: the same stack averaged over about half a second, its total and the FPS;
//   - GPU (drawing; 0 where the backend can't measure it) and MAX, the worst frame's CPU over the
//     last second;
//   - a legend: each section's averaged ms in its color, and SPR, sprites drawn on the top screen
//     (red near the draw budget);
//   - the boot above it: total s, then each step's (IN platform init, AR the area, SH sprite
//     sheets, UI fonts + menus + title art, AU audio, GA settings + the
//     Frontend: the World, the save profiles); then the audio thread's busy share (AUD
//     above is only the main loop's part: handing the cues over).
// DRW = the platform's time between beginFrame and endFrame (building and submitting the draws),
// from the frame before.
#pragma once

#include "game/dev/profiler.h"
#include "platform/platform.h"

class DevOverlay {
public:
    static constexpr int SECTIONS = (int)ProfSection::Count + 1; // the profiler's, then DRW
    static constexpr int HISTORY = 100;                          // frames in the graph

    // Once per frame, after its update: the profiler's times, the previous frame's platform stats,
    // sprites drawn.
    void record(const plat::FrameStats& stats, int sprites, float audioThreadLoad);
    void draw() const;
    void setBoot(float init, float area, float sheets, float ui, float audio, float game); // s each

private:
    float avg_[SECTIONS] = {};
    float history_[HISTORY][SECTIONS] = {};
    int next_ = 0; // the oldest column (the next one written)
    char boot_[64] = {};
    float audioLoad_ = 0;
    float gpuMs_ = 0, frameMs_ = 1000.0f / 60.0f, sprites_ = 0;
};
