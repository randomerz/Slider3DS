// 3DS audio: ndsp plays the game's mixed blocks on one stereo channel (a ring of wave buffers in
// linear memory, kept QUEUE_BLOCKS ahead); Ogg Vorbis (the rare clips) decodes with libvorbisidec
// (Tremor) straight from RomFS, and the streams' asset files are read from it a piece at a time (both
// timed: takeFileIoMs). ndspInit needs the DSP firmware (sdmc:/3ds/dspfirm.cdc, dumped by DSP1): without it
// the game plays silently.
#include "platform/ctr/audio_ctr.h"
#include "platform/platform.h"

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace plat {

namespace {

constexpr int RING = 8;         // wave buffers
constexpr int QUEUE_BLOCKS = 6; // queued ahead, ~96 ms (room for the audio thread to fall behind a little)
constexpr int CHANNEL = 0;
constexpr size_t FILE_BUFFER = 64 * 1024;

bool g_ready = false;
int16_t* g_pcm = nullptr; // RING blocks, linear memory (the DSP reads it)
ndspWaveBuf g_bufs[RING];
int g_next = 0; // the next buffer to fill
bool g_started = false; // a block was queued (the audio thread's)
std::atomic<int> g_underruns{0}; // (read by the game thread)

bool queued(const ndspWaveBuf& b) { return b.status == NDSP_WBUF_QUEUED || b.status == NDSP_WBUF_PLAYING; }

// The audio thread.
constexpr int THREAD_STACK = 128 * 1024; // (Tremor decodes on it: its alloca's)
constexpr s64 SERVICE_NS = 4000000;      // between services (a block is 16 ms)
Thread g_thread = nullptr;
volatile bool g_running = false;
void (*g_service)(void*) = nullptr;
void* g_user = nullptr;
LightLock g_lock;

void audioThread(void*) {
    while (g_running) {
        g_service(g_user);
        svcSleepThread(SERVICE_NS);
    }
}

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
    LightLock_Init(&g_lock);
    g_ready = true;
}

void exitAudio() {
    stopAudioThread();
    if (!g_ready) return;
    ndspChnReset(CHANNEL);
    ndspExit();
    linearFree(g_pcm);
    g_ready = false;
}

bool audioAvailable() { return g_ready; }

int audioUnderruns() { return g_underruns.load(); }

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
    bool playing = false; // (nothing queued or playing: it ran dry, this block starts after a gap)
    for (const ndspWaveBuf& w : g_bufs) playing = playing || queued(w);
    if (g_started && !playing) g_underruns++;
    memcpy(b.data_pcm16, stereo, AUDIO_BLOCK * 2 * sizeof(int16_t));
    DSP_FlushDataCache(b.data_pcm16, AUDIO_BLOCK * 2 * sizeof(int16_t));
    ndspChnWaveBufAdd(CHANNEL, &b);
    g_started = true;
    g_next = (g_next + 1) % RING;
}

int g_core = -1;

bool startAudioThread(void (*service)(void*), void* user) {
    if (!g_ready || g_thread) return false;
    g_service = service;
    g_user = user;
    g_running = true;
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    if (n3ds) {
        g_thread = threadCreate(audioThread, nullptr, THREAD_STACK, priority - 1, 2, false);
        g_core = 2;
    }
    if (!g_thread) { // (it sleeps between services: the game gets the core back)
        g_thread = threadCreate(audioThread, nullptr, THREAD_STACK, priority - 1, 0, false);
        g_core = 0;
    }
    if (!g_thread) {
        printf("No audio thread: audio mixes on the main loop\n");
        g_running = false;
        g_core = -1;
    }
    return g_thread != nullptr;
}

int audioThreadCore() { return g_core; }

void stopAudioThread() {
    if (!g_thread) return;
    g_running = false;
    threadJoin(g_thread, U64_MAX);
    threadFree(g_thread);
    g_thread = nullptr;
    g_core = -1;
}

void lockAudio() {
    if (g_ready) LightLock_Lock(&g_lock);
}

void unlockAudio() {
    if (g_ready) LightLock_Unlock(&g_lock);
}

// The file's reads and seeks, timed (Tremor reads through these: the IO share of the audio thread).
double g_fileIoMs = 0;

size_t timedRead(void* out, size_t size, size_t count, void* file) {
    const u64 t = svcGetSystemTick();
    const size_t n = fread(out, size, count, (FILE*)file);
    g_fileIoMs += (svcGetSystemTick() - t) * 1000.0 / SYSCLOCK_ARM11;
    return n;
}

int timedSeek(void* file, ogg_int64_t offset, int whence) {
    const u64 t = svcGetSystemTick();
    const int r = fseek((FILE*)file, (long)offset, whence);
    g_fileIoMs += (svcGetSystemTick() - t) * 1000.0 / SYSCLOCK_ARM11;
    return r;
}

int closeFile(void* file) { return fclose((FILE*)file); }
long tellFile(void* file) { return ftell((FILE*)file); }

struct Ogg {
    OggVorbis_File vf;
    int channels = 0, rate = 0;
    int64_t frames = 0;
};

Ogg* openOgg(const char* path) {
    FILE* f = fopen((std::string("romfs:/") + path).c_str(), "rb");
    if (!f) return nullptr;
    Ogg* ogg = new Ogg;
    setvbuf(f, nullptr, _IOFBF, FILE_BUFFER); // (Tremor reads in small pieces: fewer trips to the FS service)
    const ov_callbacks io = {timedRead, timedSeek, closeFile, tellFile};
    if (ov_open_callbacks(f, &ogg->vf, nullptr, 0, io) != 0) { // (on success the decoder owns the file)
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

struct AssetFile {
    FILE* f = nullptr;
    int64_t size = 0, pos = 0; // (pos: where the stdio position is, so a sequential read seeks nothing)
};

AssetFile* openAsset(const char* path) {
    FILE* f = fopen((std::string("romfs:/") + path).c_str(), "rb");
    if (!f) return nullptr;
    setvbuf(f, nullptr, _IOFBF, FILE_BUFFER);
    AssetFile* file = new AssetFile{f};
    if (timedSeek(f, 0, SEEK_END) == 0) file->size = ftell(f);
    timedSeek(f, 0, SEEK_SET);
    return file;
}

void closeAsset(AssetFile* file) {
    if (!file) return;
    fclose(file->f);
    delete file;
}

int64_t assetSize(const AssetFile* file) { return file->size; }

int readAsset(AssetFile* file, int64_t offset, void* out, int bytes) {
    if (offset != file->pos && timedSeek(file->f, offset, SEEK_SET) != 0) return 0;
    const int n = (int)timedRead(out, 1, bytes, file->f);
    file->pos = offset + n;
    return n;
}

double takeFileIoMs() {
    const double ms = g_fileIoMs;
    g_fileIoMs = 0;
    return ms;
}

} // namespace plat
