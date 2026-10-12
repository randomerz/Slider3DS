#include "game/dev_overlay.h"

#include <algorithm>
#include <cstdio>

#include "game/debug_text.h"

namespace {

constexpr float SMOOTHING = 1.0f / 30.0f; // exponential average over ~half a second at 60 fps
constexpr float MS_PX = 10.0f;            // bar length per ms
constexpr float ROW_Y[3] = {196, 210, 224}, ROW_H = 10, BAR_X = 10, TEXT_X = 190;
constexpr int TEXT_SCALE = 2;

} // namespace

void DevOverlay::record(const plat::FrameStats& stats, float updateMs, int sprites) {
    cpuMs_ += (updateMs + stats.cpuMs - cpuMs_) * SMOOTHING;
    gpuMs_ += (stats.gpuMs - gpuMs_) * SMOOTHING;
    if (stats.frameMs > 0) frameMs_ += (stats.frameMs - frameMs_) * SMOOTHING;
    sprites_ += (sprites - sprites_) * SMOOTHING;
}

void DevOverlay::draw() const {
    const bool nearBudget = sprites_ > plat::MAX_DRAWS_PER_FRAME * 3 / 4;
    const uint32_t green = plat::rgba(0x60, 0xd0, 0x70), blue = plat::rgba(0x60, 0x90, 0xe0);
    const uint32_t gray = nearBudget ? plat::rgba(0xe0, 0x50, 0x50) : plat::rgba(0xa0, 0xa0, 0xa0);
    char text[32];
    plat::drawRect(4, ROW_Y[0] - 4, 312, ROW_Y[2] + ROW_H - ROW_Y[0] + 8, plat::rgba(0x1a, 0x1a, 0x1a, 0xc0));

    plat::drawRect(BAR_X, ROW_Y[0], cpuMs_ * MS_PX, ROW_H, green);
    snprintf(text, sizeof text, "CPU %.1fMS", cpuMs_);
    const float after = drawDebugText(TEXT_X, ROW_Y[0], text, green, TEXT_SCALE);
    snprintf(text, sizeof text, "%.0fFPS", frameMs_ > 0 ? 1000.0f / frameMs_ : 0.0f);
    drawDebugText(after + 4 * TEXT_SCALE, ROW_Y[0], text, plat::rgba(0xff, 0xff, 0xff), TEXT_SCALE);

    plat::drawRect(BAR_X, ROW_Y[1], gpuMs_ * MS_PX, ROW_H, blue);
    snprintf(text, sizeof text, "GPU %.1fMS", gpuMs_);
    drawDebugText(TEXT_X, ROW_Y[1], text, blue, TEXT_SCALE);

    plat::drawRect(BAR_X, ROW_Y[2], std::min(sprites_ / 20.0f, TEXT_X - 2 * BAR_X), ROW_H, gray);
    snprintf(text, sizeof text, "SPR %.0f", sprites_);
    drawDebugText(TEXT_X, ROW_Y[2], text, gray, TEXT_SCALE);

    plat::drawRect(BAR_X + 16.7f * MS_PX, ROW_Y[0] - 2, 1, ROW_Y[2] + ROW_H - ROW_Y[0] + 4, plat::rgba(0xe0, 0x50, 0x50));
}
