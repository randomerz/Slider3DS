#include "game/audio/adpcm.h"

#include <algorithm>
#include <cstring>

#include "platform/platform.h"

namespace adpcm {

namespace {

constexpr int16_t STEPS[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767};
constexpr int8_t INDEX_STEP[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

} // namespace

void decodePacket(const uint8_t* in, int16_t* out, int stride) {
    int pred = (int16_t)(in[0] | in[1] << 8);
    int index = std::min<int>(in[2], 88);
    for (int i = 0; i < PACKET_FRAMES; i++) {
        const int nibble = (in[4 + i / 2] >> ((i & 1) * 4)) & 15;
        const int step = STEPS[index];
        int diff = step >> 3;
        if (nibble & 4) diff += step;
        if (nibble & 2) diff += step >> 1;
        if (nibble & 1) diff += step >> 2;
        pred = std::clamp(nibble & 8 ? pred - diff : pred + diff, -32768, 32767);
        index = std::clamp(index + INDEX_STEP[nibble], 0, 88);
        out[i * stride] = (int16_t)pred;
    }
}

} // namespace adpcm

bool AdpcmStream::open(const char* path) {
    close();
    file_ = plat::openAsset(path);
    if (!file_) return false;
    uint8_t h[adpcm::HEADER_BYTES];
    const bool ok = plat::readAsset(file_, 0, h, sizeof h) == (int)sizeof h && memcmp(h, "SADP", 4) == 0 && h[4] == 1 &&
                    (h[5] == 1 || h[5] == 2);
    if (ok) {
        channels_ = h[5];
        frames_ = adpcm::le32(h + 8);
        rate_ = (int)adpcm::le32(h + 12);
    }
    const int64_t packets = (frames_ + adpcm::PACKET_FRAMES - 1) / adpcm::PACKET_FRAMES;
    if (!ok || plat::assetSize(file_) != adpcm::HEADER_BYTES + packets * channels_ * adpcm::PACKET_BYTES) {
        close();
        return false;
    }
    raw_.resize((size_t)PACKETS_PER_LOAD * channels_ * adpcm::PACKET_BYTES);
    decoded_.resize((size_t)PACKETS_PER_LOAD * adpcm::PACKET_FRAMES * channels_);
    return true;
}

void AdpcmStream::close() {
    if (file_) plat::closeAsset(file_);
    file_ = nullptr;
    channels_ = rate_ = 0;
    frames_ = pos_ = 0;
    loadedFrom_ = -1;
    loadedFrames_ = 0;
}

bool AdpcmStream::load(int64_t packet) {
    const int perPacket = channels_ * adpcm::PACKET_BYTES;
    const int got = plat::readAsset(file_, adpcm::HEADER_BYTES + packet * perPacket, raw_.data(),
                                    PACKETS_PER_LOAD * perPacket);
    const int packets = got / perPacket;
    for (int p = 0; p < packets; p++)
        for (int c = 0; c < channels_; c++)
            adpcm::decodePacket(raw_.data() + p * perPacket + c * adpcm::PACKET_BYTES,
                                decoded_.data() + p * adpcm::PACKET_FRAMES * channels_ + c, channels_);
    loadedFrom_ = packet * adpcm::PACKET_FRAMES;
    loadedFrames_ = (int)std::min<int64_t>(packets * adpcm::PACKET_FRAMES, frames_ - loadedFrom_);
    return loadedFrames_ > 0;
}

int AdpcmStream::read(int16_t* out, int frames) {
    int done = 0;
    while (done < frames && pos_ < frames_) {
        if (loadedFrom_ < 0 || pos_ < loadedFrom_ || pos_ >= loadedFrom_ + loadedFrames_) {
            if (!load(pos_ / adpcm::PACKET_FRAMES)) break; // (the file ended early: stop there)
        }
        const int at = (int)(pos_ - loadedFrom_);
        const int n = std::min(frames - done, loadedFrames_ - at);
        memcpy(out + done * channels_, decoded_.data() + at * channels_, (size_t)n * channels_ * sizeof(int16_t));
        done += n;
        pos_ += n;
    }
    return done;
}

bool AdpcmStream::seek(int64_t frame) {
    if (!file_ || frame < 0 || frame > frames_) return false;
    pos_ = frame; // (decoded from its packet as read() needs it)
    return true;
}
