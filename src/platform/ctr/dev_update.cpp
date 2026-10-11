// Dev-only wireless updater. Holding SELECT at launch downloads the latest build from the
// dev PC (DEV_UPDATE_URL, set by the Makefile from DEV_HOST) and replaces DEV_UPDATE_PATH.
// The 3DS initiates the connection, so this works when the PC can't reach the 3DS
// (e.g. the 3DS is behind a second router's NAT and 3dslink fails).
#ifdef DEV_UPDATE_URL

#include "platform/ctr/dev_update.h"

#include <3ds.h>
#include <cstdio>
#include <cstring>

#define DEV_UPDATE_PATH "sdmc:/3ds/slider3ds.3dsx"

namespace plat {
namespace {

bool download(const char* url, const char* outPath) {
    httpcContext ctx;
    if (R_FAILED(httpcOpenContext(&ctx, HTTPC_METHOD_GET, url, 1))) {
        printf("Can't open HTTP context\n");
        return false;
    }
    httpcAddRequestHeaderField(&ctx, "User-Agent", "slider3ds-dev");

    u32 status = 0;
    if (R_FAILED(httpcBeginRequest(&ctx)) || R_FAILED(httpcGetResponseStatusCode(&ctx, &status))) {
        printf("Request failed - is the PC server running?\n");
        httpcCloseContext(&ctx);
        return false;
    }
    if (status != 200) {
        printf("HTTP status %lu\n", status);
        httpcCloseContext(&ctx);
        return false;
    }

    FILE* f = fopen(outPath, "wb");
    if (!f) {
        printf("Can't write %s\n", outPath);
        httpcCloseContext(&ctx);
        return false;
    }

    static u8 buf[64 * 1024];
    u32 total = 0, got = 0;
    bool magicOk = false;
    Result res;
    do {
        res = httpcDownloadData(&ctx, buf, sizeof(buf), &got);
        if (total == 0 && got >= 4) magicOk = memcmp(buf, "3DSX", 4) == 0;
        fwrite(buf, 1, got, f);
        total += got;
    } while (res == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING);
    fclose(f);
    httpcCloseContext(&ctx);

    if (R_FAILED(res) || !magicOk) {
        printf(R_FAILED(res) ? "Download error %08lx\n" : "Not a 3dsx file\n", res);
        return false;
    }
    printf("Downloaded %lu bytes\n", total);
    return true;
}

} // namespace

void runDevUpdate() {
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    printf("Slider3DS dev updater\n\nGET %s\n\n", DEV_UPDATE_URL);

    httpcInit(0);
    const char* tmpPath = DEV_UPDATE_PATH ".tmp";
    bool ok = download(DEV_UPDATE_URL, tmpPath);
    httpcExit();

    if (ok) {
        remove(DEV_UPDATE_PATH);
        ok = rename(tmpPath, DEV_UPDATE_PATH) == 0;
        printf(ok ? "Updated %s\nRelaunch from the Homebrew Launcher.\n"
                  : "Rename to %s failed\n", DEV_UPDATE_PATH);
    } else {
        remove(tmpPath);
        printf("Update failed; existing build untouched.\n");
    }

    printf("\nPress START to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    gfxExit();
}

} // namespace plat

#endif // DEV_UPDATE_URL
