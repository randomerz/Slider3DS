// Headless platform backend for tests: files come from in-memory fixtures or the asset root,
// sheets are named placeholders, and draw calls and audio blocks are recorded instead of played
// (Ogg decoding is the PC build's: src/platform/stb/ogg_stb.cpp).
#include "test_platform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

#include "platform/platform.h"

namespace plat {
struct Texture {
    std::string name;
};
} // namespace plat

namespace {
std::map<std::string, std::string> g_files;
std::map<std::string, std::string> g_userFiles;
std::vector<DrawCall> g_draws;
int g_audioWanted = 0;
bool g_audioAvailable = true;
std::vector<int16_t> g_audio;
bool g_audioThreadOn = false;
double g_nowMs = 0;
void (*g_audioService)(void*) = nullptr;
void* g_audioUser = nullptr;
int g_assetReadsToFail = 0;
} // namespace

void testFailAssetReads(int reads) { g_assetReadsToFail = reads; }

void testSetFile(const std::string& path, const std::string& contents) { g_files[path] = contents; }
void testClearFiles() { g_files.clear(); }
std::map<std::string, std::string>& testUserFiles() { return g_userFiles; }
const std::vector<DrawCall>& testDrawCalls() { return g_draws; }
void testResetDrawCalls() { g_draws.clear(); }
void testSetAudioWanted(int blocks) {
    g_audioWanted = blocks;
    g_audio.clear();
}
const std::vector<int16_t>& testAudioQueued() { return g_audio; }
void testSetAudioAvailable(bool available) { g_audioAvailable = available; }
void testSetAudioThread(bool on) { g_audioThreadOn = on; }
void testSetNowMs(double ms) { g_nowMs = ms; }
void testRunAudioThread() {
    if (g_audioService) g_audioService(g_audioUser);
}

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

// Asset files: read whole through readFile (fixtures included); testFailAssetReads makes reads fail.
struct AssetFile {
    std::string data;
};

AssetFile* openAsset(const char* path) {
    AssetFile* file = new AssetFile;
    if (readFile(path, file->data)) return file;
    delete file;
    return nullptr;
}

void closeAsset(AssetFile* file) { delete file; }

int64_t assetSize(const AssetFile* file) { return (int64_t)file->data.size(); }

int readAsset(AssetFile* file, int64_t offset, void* out, int bytes) {
    const int64_t size = (int64_t)file->data.size();
    if (g_assetReadsToFail > 0) {
        g_assetReadsToFail--;
        return 0;
    }
    if (offset < 0 || offset >= size) return 0;
    const int n = (int)std::min<int64_t>(bytes, size - offset);
    memcpy(out, file->data.data() + offset, n);
    return n;
}

bool readUserFile(const char* name, std::string& out) {
    auto it = g_userFiles.find(name);
    if (it == g_userFiles.end()) return false;
    out = it->second;
    return true;
}
bool writeUserFile(const char* name, const std::string& data) {
    g_userFiles[name] = data;
    return true;
}
bool removeUserFile(const char* name) {
    g_userFiles.erase(name);
    return true;
}

Texture* loadSheet(const char* name) { return new Texture{name}; }
void freeSheet(Texture* tex) { delete tex; }

void beginFrame() {}
void beginScreen(Screen, uint32_t) {}
void drawRect(float x, float y, float w, float h, uint32_t color) {
    // (A rect's size goes in sw/sh, so draw lists and goldens record it.)
    g_draws.push_back({"", 0, 0, (int)std::lround(w), (int)std::lround(h), x, y, w, h, color});
}
void drawLine(float x0, float y0, float x1, float y1, float width, uint32_t color) {
    // (sheet "line": from (x, y) to (sx, sy), rounded; sw = width in 1/100 px)
    const int w100 = (int)std::lround(width * 100);
    g_draws.push_back({"line", (int)std::lround(x1), (int)std::lround(y1), w100, 0, x0, y0, (float)w100, 0, color});
}
void drawCircle(float cx, float cy, float r, uint32_t color) {
    // (sheet "circle": centered on (x, y); sw = the radius in 1/100 px)
    const int r100 = (int)std::lround(r * 100);
    g_draws.push_back({"circle", 0, 0, r100, 0, cx, cy, (float)r100, 0, color});
}
void drawSprite(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, uint32_t flags, int scale, uint32_t fill) {
    g_draws.push_back({tex->name, sx, sy, sw, sh, x, y, (float)(sw * scale), (float)(sh * scale), flags, fill});
}
void drawSpriteStretched(Texture* tex, int sx, int sy, int sw, int sh, float x, float y, float w, float h) {
    g_draws.push_back({tex->name, sx, sy, sw, sh, x, y, w, h, 0});
}
void endFrame() {}
FrameStats lastFrameStats() { return {}; }
double nowMs() { return g_nowMs; }

bool audioAvailable() { return g_audioAvailable; }
int audioThreadCore() { return -1; }
int audioUnderruns() { return 0; }
bool startAudioThread(void (*service)(void*), void* user) {
    if (!g_audioThreadOn) return false; // (most tests mix by hand)
    g_audioService = service;
    g_audioUser = user;
    return true;
}
void stopAudioThread() { g_audioService = nullptr; }
void lockAudio() {}
void unlockAudio() {}
int audioBlocksWanted() { return g_audioWanted; }
void queueAudioBlock(const int16_t* stereo) {
    g_audio.insert(g_audio.end(), stereo, stereo + AUDIO_BLOCK * 2);
    if (g_audioWanted > 0) g_audioWanted--;
}

} // namespace plat
