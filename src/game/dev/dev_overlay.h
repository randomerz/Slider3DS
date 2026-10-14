// Dev perf overlay for the bottom screen: one row per measure, each a bar plus its number, averaged
// over about half a second:
//   CPU ms (green): game update + building the frame's draw calls; FPS beside it
//   GPU ms (blue):  drawing (0 where the backend can't measure it)
//   SPR (gray):     sprites drawn on the top screen (1px = 20; red near the draw budget)
// Bars are 10px per ms; the red tick marks 16.7 ms (60 fps).
#pragma once

#include "platform/platform.h"

class DevOverlay {
public:
    // Once per frame: the previous frame's platform stats, this frame's update time, sprites drawn.
    void record(const plat::FrameStats& stats, float updateMs, int sprites);
    void draw() const;

private:
    float cpuMs_ = 0, gpuMs_ = 0, frameMs_ = 1000.0f / 60.0f, sprites_ = 0;
};
