// Headless platform backend for tests: files come from in-memory fixtures or the asset root,
// sheets are named placeholders, and draw calls are recorded instead of rendered.
#include "test_platform.h"

#include <cstdio>
#include <map>

#include "platform/platform.h"

namespace plat {
struct Texture {
    std::string name;
};
} // namespace plat

namespace {
std::map<std::string, std::string> g_files;
std::vector<DrawCall> g_draws;
} // namespace

void testSetFile(const std::string& path, const std::string& contents) { g_files[path] = contents; }
void testClearFiles() { g_files.clear(); }
const std::vector<DrawCall>& testDrawCalls() { return g_draws; }
void testResetDrawCalls() { g_draws.clear(); }

namespace plat {

bool init() { return true; }
void shutdown() {}
bool mainLoop() { return true; }
Input pollInput() { return {}; }

bool readFile(const char* path, std::string& out) {
    auto it = g_files.find(path);
    if (it != g_files.end()) {
        out = it->second;
        return true;
    }
    std::string full = std::string(ASSET_ROOT) + "/" + path;
    FILE* f = fopen(full.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    out.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    size_t got = fread(&out[0], 1, out.size(), f);
    fclose(f);
    return got == out.size();
}

Texture* loadSheet(const char* name) { return new Texture{name}; }
void freeSheet(Texture* tex) { delete tex; }

void beginFrame() {}
void beginScreen(Screen, uint32_t) {}
void drawRect(float x, float y, float w, float h, uint32_t color) {
    g_draws.push_back({"", 0, 0, 0, 0, x, y, w, h, color});
}
void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags) {
    g_draws.push_back({tex->name, sx, sy, sw, sh, x, y, (float)sw, (float)sh, flags});
}
void endFrame() {}
FrameStats lastFrameStats() { return {}; }

} // namespace plat
