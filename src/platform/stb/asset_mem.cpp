// plat::openAsset and friends for the PC build: the file is read whole through plat::readFile and
// served from memory (the tests have their own, in tests/test_platform.cpp, which can fail reads).
#include <algorithm>
#include <cstring>
#include <string>

#include "platform/platform.h"

namespace plat {

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
    if (offset < 0 || offset >= size) return 0;
    const int n = (int)std::min<int64_t>(bytes, size - offset);
    memcpy(out, file->data.data() + offset, n);
    return n;
}

} // namespace plat
