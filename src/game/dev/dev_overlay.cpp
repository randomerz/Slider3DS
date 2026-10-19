#include "game/dev/dev_overlay.h"

#include <algorithm>
#include <cstdio>

#include "game/dev/debug_text.h"

namespace {

constexpr float SMOOTHING = 1.0f / 30.0f; // exponential average over ~half a second at 60 fps
constexpr int TEXT_SCALE = 2;
constexpr float PANEL_Y = 126, PANEL_H = 110, BOOT_H = 8; // (the boot line above the panel)
// The graph: a column per frame, 1.5 px per ms (50 px: 33.3 ms).
constexpr float GRAPH_X = 10, GRAPH_Y = 130, GRAPH_H = 50, COL_W = 3, GRAPH_MS_PX = 1.5f;
// The rows below it: the CPU stack, GPU, then the legend's two rows.
constexpr float ROW_Y[4] = {184, 196, 208, 220}, ROW_H = 10, BAR_X = 10, MS_PX = 10, TEXT_X = 190;
constexpr int WORST_FRAMES = 60; // MAX: the worst over the last second

const char* const LABELS[DevOverlay::SECTIONS] = {"LOG", "PHY", "NPC", "ANI", "FX", "AUD", "DRW"};
const uint32_t COLORS[DevOverlay::SECTIONS] = {
    plat::rgba(0x60, 0xd0, 0x70), plat::rgba(0xe0, 0xa0, 0x40), plat::rgba(0xd0, 0x70, 0xc0),
    plat::rgba(0xe0, 0xe0, 0x60), plat::rgba(0x60, 0xd0, 0xd0), plat::rgba(0xa0, 0x70, 0xf0),
    plat::rgba(0x60, 0x90, 0xe0),
};
const uint32_t WHITE = plat::rgba(0xff, 0xff, 0xff), RED = plat::rgba(0xe0, 0x50, 0x50);

float total(const float (&ms)[DevOverlay::SECTIONS]) {
    float t = 0;
    for (float m : ms) t += m;
    return t;
}

// One stack of section times from (x, y) upward (`up`) or rightward, `pxPerMs` long per ms, at most
// `maxLen` long.
void drawStack(float x, float y, float thickness, const float (&ms)[DevOverlay::SECTIONS], float pxPerMs, float maxLen,
               bool up) {
    float at = 0;
    for (int s = 0; s < DevOverlay::SECTIONS && at < maxLen; s++) {
        const float len = std::min(ms[s] * pxPerMs, maxLen - at);
        if (len < 0.5f) continue;
        if (up)
            plat::drawRect(x, y - at - len, thickness, len, COLORS[s]);
        else
            plat::drawRect(x + at, y, len, thickness, COLORS[s]);
        at += len;
    }
}

} // namespace

void DevOverlay::record(const plat::FrameStats& stats, int sprites, float audioThreadLoad) {
    audioLoad_ = audioThreadLoad;
    float* frame = history_[next_];
    for (int s = 0; s < (int)ProfSection::Count; s++) frame[s] = prof::ms((ProfSection)s);
    frame[SECTIONS - 1] = stats.cpuMs;
    for (int s = 0; s < SECTIONS; s++) avg_[s] += (frame[s] - avg_[s]) * SMOOTHING;
    next_ = (next_ + 1) % HISTORY;
    gpuMs_ += (stats.gpuMs - gpuMs_) * SMOOTHING;
    if (stats.frameMs > 0) frameMs_ += (stats.frameMs - frameMs_) * SMOOTHING;
    sprites_ += (sprites - sprites_) * SMOOTHING;
}

void DevOverlay::setBoot(float init, float area, float sheets, float ui, float audio) {
    snprintf(boot_, sizeof boot_, "BOOT %.1f IN%.1f AR%.1f SH%.1f UI%.1f AU%.1f", init + area + sheets + ui + audio, init,
             area, sheets, ui, audio);
}

void DevOverlay::draw() const {
    char text[32];
    plat::drawRect(4, PANEL_Y - BOOT_H, 312, PANEL_H + BOOT_H, plat::rgba(0x1a, 0x1a, 0x1a, 0xc0));
    const float afterBoot = drawDebugText(BAR_X, PANEL_Y - BOOT_H + 2, boot_, WHITE, 1);
    snprintf(text, sizeof text, "AUDIO THREAD %.0f%%", audioLoad_ * 100);
    drawDebugText(afterBoot + 16, PANEL_Y - BOOT_H + 2, text, COLORS[(int)ProfSection::Audio], 1);

    // The graph, oldest column on the left.
    float worst = 0;
    for (int i = 0; i < HISTORY; i++) {
        const auto& frame = history_[(next_ + i) % HISTORY];
        drawStack(GRAPH_X + i * COL_W, GRAPH_Y + GRAPH_H, COL_W, frame, GRAPH_MS_PX, GRAPH_H, true);
        if (i >= HISTORY - WORST_FRAMES) worst = std::max(worst, total(frame));
    }
    const float graphW = HISTORY * COL_W;
    plat::drawRect(GRAPH_X, GRAPH_Y + GRAPH_H - 16.7f * GRAPH_MS_PX, graphW, 1, RED);
    plat::drawRect(GRAPH_X, GRAPH_Y + GRAPH_H - 33.3f * GRAPH_MS_PX, graphW, 1, plat::rgba(0xe0, 0x50, 0x50, 0x80));

    // CPU: the averaged stack.
    drawStack(BAR_X, ROW_Y[0], ROW_H, avg_, MS_PX, TEXT_X - 2 * BAR_X, false);
    plat::drawRect(BAR_X + 16.7f * MS_PX, ROW_Y[0] - 2, 1, ROW_H + 4, RED);
    snprintf(text, sizeof text, "CPU %.1f", total(avg_));
    const float after = drawDebugText(TEXT_X, ROW_Y[0], text, WHITE, TEXT_SCALE);
    snprintf(text, sizeof text, "%.0fFPS", frameMs_ > 0 ? 1000.0f / frameMs_ : 0.0f);
    drawDebugText(after + 4 * TEXT_SCALE, ROW_Y[0], text, WHITE, TEXT_SCALE);

    // GPU, and the worst frame.
    const uint32_t gray = plat::rgba(0xa0, 0xa0, 0xa0);
    plat::drawRect(BAR_X, ROW_Y[1], std::min(gpuMs_ * MS_PX, TEXT_X - 2 * BAR_X), ROW_H, gray);
    snprintf(text, sizeof text, "GPU %.1f", gpuMs_);
    const float afterGpu = drawDebugText(TEXT_X, ROW_Y[1], text, gray, TEXT_SCALE);
    snprintf(text, sizeof text, "MAX %.0f", worst);
    drawDebugText(afterGpu + 4 * TEXT_SCALE, ROW_Y[1], text, worst > 16.7f ? RED : WHITE, TEXT_SCALE);

    // The legend: four entries a row, the last slot SPR.
    for (int i = 0; i <= SECTIONS; i++) {
        const float x = BAR_X + (i % 4) * 76.0f, y = ROW_Y[2 + i / 4];
        if (i < SECTIONS) {
            snprintf(text, sizeof text, "%s %.1f", LABELS[i], avg_[i]);
            drawDebugText(x, y, text, COLORS[i], TEXT_SCALE);
        } else {
            snprintf(text, sizeof text, "SPR %.0f", sprites_);
            drawDebugText(x, y, text, sprites_ > plat::MAX_DRAWS_PER_FRAME * 3 / 4 ? RED : gray, TEXT_SCALE);
        }
    }
}
