// Rendering goldens: AreaView draws fixed scenes through the recording test platform, and each
// scene's draw list (sheet, source rect, pixel position, flags; in order) is hashed and compared
// with tests/golden/render.txt. No GPU or window needed, so it's deterministic.
//
// After an intended rendering or export change, regenerate and review the diff:
//   SLIDER3DS_UPDATE_GOLDENS=1 build-pc/Debug/slider3ds_tests.exe render
// On a mismatch the actual draw list is written to <build>/test-out/<scene>.txt for inspection.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>

#include "game/area_view.h"
#include "game/artifact.h"
#include "game/grid.h"
#include "platform/platform.h"
#include "test.h"
#include "test_platform.h"

namespace {

const std::string GOLDEN_PATH = std::string(TESTS_DIR) + "/golden/render.txt";

std::string drawListText(const std::vector<DrawCall>& calls) {
    std::ostringstream out;
    for (const DrawCall& c : calls)
        out << c.sheet << ' ' << c.sx << ' ' << c.sy << ' ' << c.sw << ' ' << c.sh << ' ' << c.x << ' ' << c.y << ' '
            << c.flags << '\n';
    return out.str();
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : s) h = (h ^ ch) * 1099511628211ull;
    return h;
}

std::map<std::string, std::string> readGoldens() {
    std::map<std::string, std::string> g;
    std::ifstream in(GOLDEN_PATH);
    std::string name, value, line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        if (ls >> name >> value) g[name] = value;
    }
    return g;
}

struct Scene {
    const char* name;
    float camX, camY, time;
    bool showInactive;
    std::function<void(Grid&, Artifact&)> setup;
};

void allButNine(Grid& grid, Artifact&) {
    for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);
}

} // namespace

TEST(render_goldens) {
    const Area& area = village();
    AreaView view;
    check(view.loadSheets(area), "sheets load");

    const Scene scenes[] = {
        {"start_tile1", 0, 0, 0, false, nullptr},
        {"start_tile1_inactive_shown", 0, 0, 0, true, nullptr},
        {"default_house", 0.5f, -148.25f, 0, false, nullptr},
        {"all_grid_center", 17, 17, 0, false, allButNine},
        {"all_grid_center_animated", 17, 17, 1.37f, false, allButNine},
        {"all_beach", 52, 20, 0.5f, false, allButNine},
        {"all_top_left", -2, 36, 0, false, allButNine},
        {"mid_slide",
         17, 17, 0, false,
         [](Grid& grid, Artifact& artifact) {
             allButNine(grid, artifact);
             artifact.init(grid);
             artifact.tryMove(grid, 2, 1, 1, 1, 0); // tile 7 left into the empty middle
             for (int i = 0; i < 40; i++) artifact.update(grid, 1.0f / 60.0f, 0);
         }},
        {"shuffled",
         17, 17, 0, false,
         [](Grid& grid, Artifact& artifact) {
             allButNine(grid, artifact);
             artifact.setGrid(grid, "182_043_765");
         }},
    };

    const bool update = std::getenv("SLIDER3DS_UPDATE_GOLDENS") != nullptr;
    std::map<std::string, std::string> goldens = readGoldens();
    std::ostringstream fresh;
    fresh << "# Draw-list hashes per scene (tests/render_test.cpp): name count:fnv1a64\n";

    for (const Scene& sc : scenes) {
        Grid grid;
        grid.init(area);
        Artifact artifact;
        artifact.init(grid);
        if (sc.setup) sc.setup(grid, artifact);
        testResetDrawCalls();
        view.begin(area, sc.camX, sc.camY, plat::TOP_W, plat::TOP_H, sc.time, sc.showInactive);
        view.drawLayers(area, grid);
        const std::vector<DrawCall>& calls = testDrawCalls();
        const std::string text = drawListText(calls);
        char value[64];
        snprintf(value, sizeof value, "%zu:%016llx", calls.size(), (unsigned long long)fnv1a(text));
        fresh << sc.name << ' ' << value << '\n';

        check(calls.size() > 50, std::string(sc.name) + " draws something (" + std::to_string(calls.size()) + ")");
        if (update) continue;
        auto it = goldens.find(sc.name);
        bool same = it != goldens.end() && it->second == value;
        if (!same) {
            std::string out = std::string(TEST_OUT_DIR) + "/" + sc.name + ".txt";
            std::ofstream(out) << text;
            check(false, std::string(sc.name) + ": draw list " + value + " != golden " +
                             (it == goldens.end() ? "(missing)" : it->second) + "; actual list in " + out);
        }
    }
    view.unloadSheets();

    if (update) {
        std::ofstream(GOLDEN_PATH, std::ios::binary) << fresh.str();
        printf("    wrote %s\n", GOLDEN_PATH.c_str());
    }
}

TEST(render_layer_ranges_compose) {
    // Drawing sorting layer by sorting layer (as the entity pass will) gives exactly the same
    // draw list as drawing everything at once.
    const Area& area = village();
    AreaView view;
    check(view.loadSheets(area), "sheets load");
    Grid grid;
    grid.init(area);
    for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);

    testResetDrawCalls();
    view.begin(area, 17, 17, plat::TOP_W, plat::TOP_H, 0, false);
    view.drawLayers(area, grid);
    const std::string all = drawListText(testDrawCalls());
    const int allCount = view.lastSpriteCount();

    testResetDrawCalls();
    view.begin(area, 17, 17, plat::TOP_W, plat::TOP_H, 0, false);
    int layersSeen = 0, prev = INT_MIN;
    for (const TileLayer& L : area.layers) {
        if (L.sortLayer == prev && layersSeen > 0) continue;
        prev = L.sortLayer;
        view.drawLayers(area, grid, L.sortLayer, L.sortLayer);
        layersSeen++;
    }
    check(layersSeen > 1, "the export uses several sorting layers (" + std::to_string(layersSeen) + ")");
    check(drawListText(testDrawCalls()) == all && view.lastSpriteCount() == allCount,
          "per-sorting-layer draws == one full draw");
    view.unloadSheets();
}
