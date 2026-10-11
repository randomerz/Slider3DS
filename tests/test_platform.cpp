// Minimal platform layer for headless tests: only file access is needed.
#include <cstdio>

#include "platform/platform.h"

namespace plat {

bool readFile(const char* path, std::string& out) {
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

} // namespace plat
