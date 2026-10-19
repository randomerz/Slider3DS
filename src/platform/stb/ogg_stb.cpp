// plat::openOgg and friends on stb_vorbis (third_party/stb_vorbis.c), for the PC build and the
// headless tests: the file is read whole through plat::readFile and decoded from memory.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <string>

#include "platform/platform.h"

namespace plat {

struct Ogg {
    std::string data; // (stb_vorbis decodes from it: kept as long as the decoder)
    stb_vorbis* vorbis = nullptr;
    int channels = 0, rate = 0;
    int64_t frames = 0;
};

Ogg* openOgg(const char* path) {
    Ogg* ogg = new Ogg;
    int error = 0;
    if (readFile(path, ogg->data))
        ogg->vorbis = stb_vorbis_open_memory((const unsigned char*)ogg->data.data(), (int)ogg->data.size(), &error, nullptr);
    if (!ogg->vorbis) {
        delete ogg;
        return nullptr;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(ogg->vorbis);
    ogg->channels = info.channels;
    ogg->rate = (int)info.sample_rate;
    ogg->frames = stb_vorbis_stream_length_in_samples(ogg->vorbis);
    return ogg;
}

void closeOgg(Ogg* ogg) {
    if (!ogg) return;
    stb_vorbis_close(ogg->vorbis);
    delete ogg;
}

int oggChannels(const Ogg* ogg) { return ogg->channels; }
int oggRate(const Ogg* ogg) { return ogg->rate; }
int64_t oggFrames(const Ogg* ogg) { return ogg->frames; }

int readOgg(Ogg* ogg, int16_t* out, int frames) {
    int done = 0;
    while (done < frames) { // (a call stops at the end of a Vorbis frame)
        const int n = stb_vorbis_get_samples_short_interleaved(ogg->vorbis, ogg->channels, out + done * ogg->channels,
                                                               (frames - done) * ogg->channels);
        if (n <= 0) break;
        done += n;
    }
    return done;
}

bool seekOgg(Ogg* ogg, int64_t frame) { return stb_vorbis_seek(ogg->vorbis, (unsigned int)frame) != 0; }

double takeFileIoMs() { return 0; } // (not timed: PC files are read whole)

} // namespace plat
