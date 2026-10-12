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
#include "game/artifact_ui.h"
#include "game/artifact.h"
#include "game/grid.h"
#include "platform/platform.h"
#include "test.h"
#include "test_platform.h"

namespace {


std::string drawListText(const std::vector<DrawCall>& calls) {
    std::ostringstream out;
    for (const DrawCall& c : calls) {
        out << c.sheet << ' ' << c.sx << ' ' << c.sy << ' ' << c.sw << ' ' << c.sh << ' ' << c.x << ' ' << c.y << ' '
            << c.flags;
        if (!c.sheet.empty() && c.w != (float)c.sw) out << " x" << (int)(c.w / c.sw); // sprite scale, if not 1
        out << '\n';
    }
    return out.str();
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : s) h = (h ^ ch) * 1099511628211ull;
    return h;
}

// A golden file of draw-list hashes (tests/golden/<file>): compare() checks the draw calls
// recorded since the last testResetDrawCalls() against the named entry; with
// SLIDER3DS_UPDATE_GOLDENS set, the file is rewritten instead when the set is destroyed.
class GoldenSet {
public:
    explicit GoldenSet(const std::string& file) : path_(std::string(TESTS_DIR) + "/golden/" + file) {
        update_ = std::getenv("SLIDER3DS_UPDATE_GOLDENS") != nullptr;
        std::ifstream in(path_);
        std::string name, value, line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            if (ls >> name >> value) goldens_[name] = value;
        }
        fresh_ << "# Draw-list hashes per scene (tests/render_test.cpp): name count:fnv1a64\n";
    }
    ~GoldenSet() {
        if (!update_) return;
        std::ofstream(path_, std::ios::binary) << fresh_.str();
        printf("    wrote %s\n", path_.c_str());
    }

    void compare(const std::string& name, size_t minDraws) {
        const std::vector<DrawCall>& calls = testDrawCalls();
        const std::string text = drawListText(calls);
        char value[64];
        snprintf(value, sizeof value, "%zu:%016llx", calls.size(), (unsigned long long)fnv1a(text));
        fresh_ << name << ' ' << value << '\n';
        check(calls.size() >= minDraws, name + " draws something (" + std::to_string(calls.size()) + ")");
        if (update_) return;
        auto it = goldens_.find(name);
        if (it != goldens_.end() && it->second == value) return;
        std::string out = std::string(TEST_OUT_DIR) + "/" + name + ".txt";
        std::ofstream(out) << text;
        check(false, name + ": draw list " + value + " != golden " +
                         (it == goldens_.end() ? "(missing)" : it->second) + "; actual list in " + out);
    }

private:
    std::string path_;
    bool update_ = false;
    std::map<std::string, std::string> goldens_;
    std::ostringstream fresh_;
};

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
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");

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

    GoldenSet goldens("render.txt");
    for (const Scene& sc : scenes) {
        Grid grid;
        grid.init(area);
        Artifact artifact;
        artifact.init(grid);
        if (sc.setup) sc.setup(grid, artifact);
        testResetDrawCalls();
        view.begin(area, sheets, sc.camX, sc.camY, plat::TOP_W, plat::TOP_H, sc.time, sc.showInactive);
        view.drawLayers(area, grid);
        goldens.compare(sc.name, 50);
    }
}

TEST(render_artifact_goldens) {
    const Area& area = village();
    SpriteSheets sheets;
    check(sheets.load(area), "sheets load");
    GoldenSet goldens("artifact.txt");

    struct Setup {
        Grid grid;
        Artifact artifact;
        ArtifactUI ui;
        explicit Setup(bool allButNine) {
            grid.init(village());
            if (allButNine)
                for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);
            artifact.init(grid);
            ui.init(village(), artifact);
        }
        void touch(bool down, int x, int y, int frames = 1) {
            for (int i = 0; i < frames; i++) {
                plat::Input in;
                in.touching = down;
                float cx, cy;
                ui.slotCenter(village(), x, y, cx, cy);
                in.touchX = (int)cx + (down && i > 0 ? i : 0); // wiggle to start drags
                in.touchY = (int)cy;
                ui.update(in, village(), grid, artifact, 0, 1.0f / 60.0f);
                artifact.update(grid, 1.0f / 60.0f, 0);
            }
        }
        void render(const SpriteSheets& sheets) {
            testResetDrawCalls();
            ui.draw(village(), grid, artifact, sheets);
        }
    };

    {
        Setup s(false);
        s.render(sheets);
        goldens.compare("artifact_start", 11);
    }
    {
        Setup s(false);
        s.touch(true, 0, 0); // tap tile 1: selected, 4 options highlighted
        s.touch(false, 0, 0);
        s.render(sheets);
        goldens.compare("artifact_selected", 11);
    }
    {
        Setup s(true);
        s.touch(true, 2, 1); // tile 7, then the middle slot: moving (pressed, nudged)
        s.touch(false, 2, 1);
        s.touch(true, 1, 1);
        s.touch(false, 1, 1);
        s.touch(false, 0, 0, 4);
        s.render(sheets);
        goldens.compare("artifact_moving", 11);
    }
    {
        Setup s(true);
        s.touch(true, 1, 0, 6); // drag tile 5 ...
        s.touch(true, 1, 1, 2); // ... hovering over the empty middle
        s.render(sheets);
        goldens.compare("artifact_drag_hover", 11);
    }
}

TEST(render_layer_ranges_compose) {
    // Drawing sorting layer by sorting layer (as the entity pass will) gives exactly the same
    // draw list as drawing everything at once.
    const Area& area = village();
    SpriteSheets sheets;
    AreaView view;
    check(sheets.load(area), "sheets load");
    Grid grid;
    grid.init(area);
    for (int id = 1; id <= 8; id++) grid.setTileActive(id, true);

    testResetDrawCalls();
    view.begin(area, sheets, 17, 17, plat::TOP_W, plat::TOP_H, 0, false);
    view.drawLayers(area, grid);
    const std::string all = drawListText(testDrawCalls());
    const int allCount = view.lastSpriteCount();

    testResetDrawCalls();
    view.begin(area, sheets, 17, 17, plat::TOP_W, plat::TOP_H, 0, false);
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
}
