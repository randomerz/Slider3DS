#include "game/dev/dev_overlay.h"

#include <algorithm>
#include <cstdio>

#include "game/dev/debug_text.h"

namespace {

constexpr float SMOOTHING = 1.0f / 30.0f; // exponential average over ~half a second at 60 fps
constexpr int TEXT_SCALE = 2;
constexpr float PANEL_Y = 112, PANEL_H = 126, BOOT_H = 24; // (the audio thread's, the boot's and the worst frame's lines above the panel)
// The graph: a column per frame, 1.5 px per ms (50 px: 33.3 ms).
constexpr float GRAPH_X = 10, GRAPH_Y = 116, GRAPH_H = 50, COL_W = 3, GRAPH_MS_PX = 1.5f;
// The rows below it: the CPU stack, GPU, then the legend's three rows.
constexpr float ROW_Y[5] = {170, 182, 196, 208, 220}, ROW_H = 10, BAR_X = 10, MS_PX = 10, TEXT_X = 190;
constexpr int WORST_FRAMES = 60; // MAX: the worst over the last second
constexpr int SECTIONS = DevOverlay::SECTIONS;

using S = ProfSection;
constexpr uint32_t bit(S s) { return 1u << (int)s; }
constexpr uint32_t ALL = (1u << SECTIONS) - 1;
constexpr uint32_t WAIT = bit(S::Wait);
constexpr uint32_t DRAWING =
    bit(S::Draw) | bit(S::DrawList) | bit(S::DrawSort) | bit(S::DrawTiles) | bit(S::DrawObjects) | bit(S::DrawBottom);
static_assert(SECTIONS <= 32, "sections are a bit mask");

// A legend row: the sections it sums.
struct Row {
    const char* label;
    uint32_t color;
    uint32_t sections;
};
constexpr int MAX_ROWS = 11;
struct PageDef {
    const char* name;
    int rows;
    Row row[MAX_ROWS];
    int counts; // after the rows in the legend: SPR, then (DRAW) CEL
};
constexpr uint32_t WAIT_COLOR = plat::rgba(0x58, 0x58, 0x68);
constexpr PageDef PAGE_DEFS[DevOverlay::PAGES] = {
    {"FRAME", 11, {
        {"LOG", plat::rgba(0x60, 0xd0, 0x70), bit(S::Logic)},
        {"PHY", plat::rgba(0xe0, 0xa0, 0x40), bit(S::Physics)},
        {"NPC", plat::rgba(0xd0, 0x70, 0xc0), bit(S::Npcs)},
        {"ANI", plat::rgba(0xe0, 0xe0, 0x60), bit(S::Anim)},
        {"VFX", plat::rgba(0x60, 0xd0, 0xd0), bit(S::Fx)},
        {"AUD", plat::rgba(0xa0, 0x70, 0xf0), bit(S::Audio)},
        {"DRW", plat::rgba(0x60, 0x90, 0xe0), DRAWING},
        {"DEV", plat::rgba(0x80, 0x80, 0x80), bit(S::Dev)},
        {"END", plat::rgba(0x40, 0x60, 0xb0), bit(S::FrameEnd)},
        {"ATH", plat::rgba(0xf0, 0x60, 0xa0), bit(S::AudioThread)},
        {"WAI", WAIT_COLOR, WAIT},
    }, 1},
    {"DRAW", 8, {
        {"LST", plat::rgba(0x60, 0xd0, 0x70), bit(S::DrawList)},
        {"SRT", plat::rgba(0xe0, 0xe0, 0x60), bit(S::DrawSort)},
        {"TIL", plat::rgba(0x60, 0x90, 0xe0), bit(S::DrawTiles)},
        {"OBJ", plat::rgba(0xe0, 0xa0, 0x40), bit(S::DrawObjects)},
        {"BOT", plat::rgba(0xa0, 0x70, 0xf0), bit(S::DrawBottom)},
        {"DRW", plat::rgba(0x40, 0x60, 0xb0), bit(S::Draw)},
        {"OTH", plat::rgba(0x80, 0x80, 0x80), ALL & ~DRAWING & ~WAIT},
        {"WAI", WAIT_COLOR, WAIT},
    }, 2},
};
constexpr int LEGEND_COLS = 4, LEGEND_SLOTS = 3 * LEGEND_COLS; // (ROW_Y[2..4])

// Every page: its rows split the sections, each counted once, WAI last (the stack's top, left out
// of the frame's work).
constexpr bool pagesSplitSections() {
    for (const PageDef& p : PAGE_DEFS) {
        uint32_t seen = 0;
        for (int r = 0; r < p.rows; r++) {
            if (seen & p.row[r].sections) return false;
            seen |= p.row[r].sections;
        }
        if (seen != ALL || p.row[p.rows - 1].sections != WAIT || p.rows + p.counts > LEGEND_SLOTS) return false;
    }
    return true;
}

const uint32_t WHITE = plat::rgba(0xff, 0xff, 0xff), RED = plat::rgba(0xe0, 0x50, 0x50), GRAY = plat::rgba(0xa0, 0xa0, 0xa0);

// A frame's (or the average's) ms by the page's rows.
void rowsOf(const float (&ms)[SECTIONS], const PageDef& page, float (&out)[MAX_ROWS]) {
    for (int r = 0; r < page.rows; r++) {
        out[r] = 0;
        for (int s = 0; s < SECTIONS; s++)
            if (page.row[r].sections & (1u << s)) out[r] += ms[s];
    }
}

// The frame's work: every section but WAI (the wait for the display's pace pads any frame to a refresh).
float busy(const float (&ms)[SECTIONS]) {
    float t = 0;
    for (int s = 0; s < SECTIONS; s++)
        if (!(WAIT & (1u << s))) t += ms[s];
    return t;
}

// One stack of the first `rows` rows from (x, y) upward (`up`) or rightward, `pxPerMs` long per
// ms, at most `maxLen` long.
void drawStack(float x, float y, float thickness, const PageDef& page, const float (&rowMs)[MAX_ROWS], int rows,
               float pxPerMs, float maxLen, bool up) {
    float at = 0;
    for (int r = 0; r < rows && at < maxLen; r++) {
        const float len = std::min(rowMs[r] * pxPerMs, maxLen - at);
        if (len < 0.5f) continue;
        if (up)
            plat::drawRect(x, y - at - len, thickness, len, page.row[r].color);
        else
            plat::drawRect(x + at, y, len, thickness, page.row[r].color);
        at += len;
    }
}

} // namespace

static_assert(pagesSplitSections(), "each page's rows split the profiler's sections (WAI last) and fit the legend");

void DevOverlay::record(const plat::FrameStats& stats, int sprites, int tiles, const Audio::ThreadLoad& audioThread) {
    audio_ = audioThread;
    float* frame = history_[next_];
    for (int s = 0; s < SECTIONS; s++) frame[s] = prof::ms((ProfSection)s);
    for (int s = 0; s < SECTIONS; s++) avg_[s] += (frame[s] - avg_[s]) * SMOOTHING;
    next_ = (next_ + 1) % HISTORY;
    gpuMs_ += (stats.gpuMs - gpuMs_) * SMOOTHING;
    if (stats.frameMs > 0) frameMs_ += (stats.frameMs - frameMs_) * SMOOTHING;
    sprites_ += (sprites - sprites_) * SMOOTHING;
    tiles_ += (tiles - tiles_) * SMOOTHING;
}

void DevOverlay::setBoot(float init, float area, float sheets, float ui, float audio, float game) {
    snprintf(boot_, sizeof boot_, "BOOT %.1f IN%.1f AR%.1f SH%.1f UI%.1f AU%.1f GA%.1f", init + area + sheets + ui + audio + game,
             init, area, sheets, ui, audio, game);
}

void DevOverlay::draw(Page which) const {
    const PageDef& page = PAGE_DEFS[(int)which];
    const int work = page.rows - 1; // (the rows but WAI)
    char text[32];
    plat::drawRect(4, PANEL_Y - BOOT_H, 312, PANEL_H + BOOT_H, plat::rgba(0x1a, 0x1a, 0x1a, 0xc0));
    char line[96];
    int n = snprintf(line, sizeof line, "AUDIO THREAD C%d %.0f%%  STR %.0f%%  CLP %.0f%%  IO %.0f%%  DRY %d", audio_.core,
                     audio_.total * 100, audio_.streams * 100, audio_.clips * 100, audio_.io * 100, audio_.underruns);
    if (audio_.bgmS >= 0) { // the music's position / its loop point, m:ss
        const int at = (int)audio_.bgmS, loop = (int)audio_.bgmLoopS;
        n += snprintf(line + n, sizeof line - n, "  BGM %d:%02d", at / 60, at % 60);
        if (loop >= 0) snprintf(line + n, sizeof line - n, "/%d:%02d", loop / 60, loop % 60);
    }
    drawDebugText(BAR_X, PANEL_Y - BOOT_H + 2, audio_.running ? line : "AUDIO THREAD NONE", plat::rgba(0xf0, 0x60, 0xa0), 1);
    drawDebugText(BAR_X, PANEL_Y - BOOT_H + 10, boot_, WHITE, 1);

    // The graph, oldest column on the left.
    float worst = 0, rowMs[MAX_ROWS];
    int worstRow = 0; // (the worst frame's biggest but WAI)
    for (int i = 0; i < HISTORY; i++) {
        const auto& frame = history_[(next_ + i) % HISTORY];
        rowsOf(frame, page, rowMs);
        drawStack(GRAPH_X + i * COL_W, GRAPH_Y + GRAPH_H, COL_W, page, rowMs, page.rows, GRAPH_MS_PX, GRAPH_H, true);
        if (i >= HISTORY - WORST_FRAMES && busy(frame) > worst) { // (by its work: WAI left out)
            worst = busy(frame);
            worstRow = (int)(std::max_element(rowMs, rowMs + work) - rowMs);
        }
    }
    const float graphW = HISTORY * COL_W;
    plat::drawRect(GRAPH_X, GRAPH_Y + GRAPH_H - 16.7f * GRAPH_MS_PX, graphW, 1, RED);
    plat::drawRect(GRAPH_X, GRAPH_Y + GRAPH_H - 33.3f * GRAPH_MS_PX, graphW, 1, plat::rgba(0xe0, 0x50, 0x50, 0x80));

    // CPU: the averaged stack without WAI, so its end against the 16.7 tick is the headroom.
    rowsOf(avg_, page, rowMs);
    drawStack(BAR_X, ROW_Y[0], ROW_H, page, rowMs, work, MS_PX, TEXT_X - 2 * BAR_X, false);
    plat::drawRect(BAR_X + 16.7f * MS_PX, ROW_Y[0] - 2, 1, ROW_H + 4, RED);
    snprintf(text, sizeof text, "CPU %.1f", busy(avg_));
    const float after = drawDebugText(TEXT_X, ROW_Y[0], text, WHITE, TEXT_SCALE);
    snprintf(text, sizeof text, "%.0fFPS", frameMs_ > 0 ? 1000.0f / frameMs_ : 0.0f);
    drawDebugText(after + 4 * TEXT_SCALE, ROW_Y[0], text, WHITE, TEXT_SCALE);

    // GPU.
    plat::drawRect(BAR_X, ROW_Y[1], std::min(gpuMs_ * MS_PX, TEXT_X - 2 * BAR_X), ROW_H, GRAY);
    snprintf(text, sizeof text, "GPU %.1f", gpuMs_);
    drawDebugText(TEXT_X, ROW_Y[1], text, GRAY, TEXT_SCALE);
    // The worst frame, the line above the panel: its ms and biggest row; the page's name at its end.
    snprintf(text, sizeof text, "WORST FRAME %.1fMS", worst);
    const float afterMax = drawDebugText(BAR_X, PANEL_Y - BOOT_H + 18, text, worst > 16.7f ? RED : WHITE, 1);
    if (worst > 0) drawDebugText(afterMax + 4, PANEL_Y - BOOT_H + 18, page.row[worstRow].label, page.row[worstRow].color, 1);
    snprintf(text, sizeof text, "PAGE %d/%d %s", (int)which + 1, DevOverlay::PAGES, page.name);
    drawDebugText(250, PANEL_Y - BOOT_H + 18, text, WHITE, 1);

    // The legend: four entries a row, the page's counts after its rows.
    for (int i = 0; i < page.rows + page.counts; i++) {
        const float x = BAR_X + (i % LEGEND_COLS) * 76.0f, y = ROW_Y[2 + i / LEGEND_COLS];
        if (i < page.rows) {
            snprintf(text, sizeof text, "%s %.1f", page.row[i].label, rowMs[i]);
            drawDebugText(x, y, text, page.row[i].color, TEXT_SCALE);
        } else if (i == page.rows) {
            snprintf(text, sizeof text, "SPR %.0f", sprites_);
            drawDebugText(x, y, text, sprites_ > plat::MAX_DRAWS_PER_FRAME * 3 / 4 ? RED : GRAY, TEXT_SCALE);
        } else {
            snprintf(text, sizeof text, "CEL %.0f", tiles_);
            drawDebugText(x, y, text, GRAY, TEXT_SCALE);
        }
    }
}
