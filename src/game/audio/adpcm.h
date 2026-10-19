// The streams' files (.adp, tools/adpcm.py documents the format): IMA ADPCM in self-contained
// 64-frame packets, read from the asset file a few packets at a time and decoded as they play (a few
// adds per sample: Vorbis cost an Old 3DS ~15% of a core per stream). Seeks are exact: the packet
// holding the frame, decoded up to it.
#pragma once

#include <cstdint>
#include <vector>

namespace plat {
struct AssetFile;
}

namespace adpcm {

constexpr int PACKET_FRAMES = 64;
constexpr int PACKET_BYTES = 36; // per channel: s16 predictor, u8 step index, u8 0, 32 bytes of nibbles
constexpr int HEADER_BYTES = 16;

// One channel's packet → PACKET_FRAMES samples, every `stride`th of `out`.
void decodePacket(const uint8_t* in, int16_t* out, int stride);

} // namespace adpcm

class AdpcmStream {
public:
    AdpcmStream() = default;
    AdpcmStream(const AdpcmStream&) = delete;
    AdpcmStream& operator=(const AdpcmStream&) = delete;
    ~AdpcmStream() { close(); }

    bool open(const char* path); // false: missing, not this format, or cut short
    void close();
    int channels() const { return channels_; }
    int rate() const { return rate_; }
    int64_t frames() const { return frames_; }

    // Up to `frames` frames (interleaved int16) from the current position; fewer only at the end.
    int read(int16_t* out, int frames);
    bool seek(int64_t frame); // false: past the end

private:
    bool load(int64_t packet); // decodes the PACKETS_PER_LOAD from `packet` into decoded_

    static constexpr int PACKETS_PER_LOAD = 16; // (1024 frames a read from the file)
    plat::AssetFile* file_ = nullptr;
    int channels_ = 0, rate_ = 0;
    int64_t frames_ = 0;
    int64_t pos_ = 0;              // the next frame read() returns
    int64_t loadedFrom_ = -1;      // the first frame in decoded_ (-1: none)
    int loadedFrames_ = 0;
    std::vector<uint8_t> raw_;     // the packets as read
    std::vector<int16_t> decoded_; // their frames, interleaved
};
