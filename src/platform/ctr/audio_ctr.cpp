// 3DS audio: ndsp plays the game's mixed blocks on one stereo channel (a ring of wave buffers in
// linear memory, kept QUEUE_BLOCKS ahead); Ogg Vorbis decodes with libvorbisidec (Tremor) straight
// from RomFS. ndspInit needs the DSP firmware (sdmc:/3ds/dspfirm.cdc, dumped by DSP1): without it
// the game plays silently.
#include "platform/ctr/audio_ctr.h"
#include "platform/platform.h"

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace plat {

namespace {

constexpr int RING = 8;         // wave buffers
constexpr int QUEUE_BLOCKS = 6; // queued ahead, ~96 ms (the game mixes on its main loop: room for a slow frame)
constexpr int CHANNEL = 0;

bool g_ready = false;
int16_t* g_pcm = nullptr; // RING blocks, linear memory (the DSP reads it)
ndspWaveBuf g_bufs[RING];
int g_next = 0; // the next buffer to fill

bool queued(const ndspWaveBuf& b) { return b.status == NDSP_WBUF_QUEUED || b.status == NDSP_WBUF_PLAYING; }

} // namespace

void initAudio() {
    if (R_FAILED(ndspInit())) {
        printf("No audio (ndspInit failed: is sdmc:/3ds/dspfirm.cdc there?)\n");
        return;
    }
    g_pcm = (int16_t*)linearAlloc(RING * AUDIO_BLOCK * 2 * sizeof(int16_t));
    if (!g_pcm) {
        ndspExit();
        return;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(CHANNEL);
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CHANNEL, AUDIO_RATE);
    ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {1, 1}; // front left / right at full
    ndspChnSetMix(CHANNEL, mix);
    memset(g_bufs, 0, sizeof g_bufs);
    for (int i = 0; i < RING; i++) {
        g_bufs[i].data_pcm16 = g_pcm + i * AUDIO_BLOCK * 2;
        g_bufs[i].nsamples = AUDIO_BLOCK;
    }
    g_ready = true;
}

void exitAudio() {
    if (!g_ready) return;
    ndspChnReset(CHANNEL);
    ndspExit();
    linearFree(g_pcm);
    g_ready = false;
}

bool audioAvailable() { return g_ready; }

int audioBlocksWanted() {
    if (!g_ready) return 0;
    int n = 0;
    for (const ndspWaveBuf& b : g_bufs) n += queued(b) ? 1 : 0;
    // (the next buffer must be free too: the ring is longer than the queue, so it is)
    return n < QUEUE_BLOCKS && !queued(g_bufs[g_next]) ? QUEUE_BLOCKS - n : 0;
}

void queueAudioBlock(const int16_t* stereo) {
    if (!g_ready) return;
    ndspWaveBuf& b = g_bufs[g_next];
    if (queued(b)) return; // (asked for more than audioBlocksWanted: dropped)
    memcpy(b.data_pcm16, stereo, AUDIO_BLOCK * 2 * sizeof(int16_t));
    DSP_FlushDataCache(b.data_pcm16, AUDIO_BLOCK * 2 * sizeof(int16_t));
    ndspChnWaveBufAdd(CHANNEL, &b);
    g_next = (g_next + 1) % RING;
}

struct Ogg {
    OggVorbis_File vf;
    int channels = 0, rate = 0;
    int64_t frames = 0;
};

Ogg* openOgg(const char* path) {
    FILE* f = fopen((std::string("romfs:/") + path).c_str(), "rb");
    if (!f) return nullptr;
    Ogg* ogg = new Ogg;
    if (ov_open(f, &ogg->vf, nullptr, 0) != 0) { // (on success the decoder owns the file)
        fclose(f);
        delete ogg;
        return nullptr;
    }
    const vorbis_info* info = ov_info(&ogg->vf, -1);
    ogg->channels = info->channels;
    ogg->rate = (int)info->rate;
    ogg->frames = ov_pcm_total(&ogg->vf, -1);
    return ogg;
}

void closeOgg(Ogg* ogg) {
    if (!ogg) return;
    ov_clear(&ogg->vf);
    delete ogg;
}

int oggChannels(const Ogg* ogg) { return ogg->channels; }
int oggRate(const Ogg* ogg) { return ogg->rate; }
int64_t oggFrames(const Ogg* ogg) { return ogg->frames; }

int readOgg(Ogg* ogg, int16_t* out, int frames) {
    const int frameBytes = ogg->channels * (int)sizeof(int16_t);
    int done = 0, section = 0;
    while (done < frames) { // (a call returns at most one Vorbis packet)
        const long n = ov_read(&ogg->vf, (char*)(out + done * ogg->channels), (frames - done) * frameBytes, &section);
        if (n <= 0) break; // (the end, or a damaged packet: stop there)
        done += (int)(n / frameBytes);
    }
    return done;
}

bool seekOgg(Ogg* ogg, int64_t frame) { return ov_pcm_seek(&ogg->vf, frame) == 0; }

} // namespace plat
