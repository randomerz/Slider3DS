// Test-only controls of the headless platform backend (tests/test_platform.cpp).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// In-memory files that readFile() serves before the asset root (for synthetic fixtures).
void testSetFile(const std::string& path, const std::string& contents);
void testClearFiles();
// plat::loadSheet of the sheet named `name` fails (a missing texture); "" = none fails (the default).
void testFailSheet(const std::string& name);
// The next `reads` plat::readAsset calls fail (read nothing), as a storage error would.
void testFailAssetReads(int reads);
// The player's files (readUserFile / writeUserFile), in memory, by name.
std::map<std::string, std::string>& testUserFiles();

// Every drawSprite/drawRect/drawLine/drawCircle since the last reset, in call order.
struct DrawCall {
    std::string sheet; // empty for rects
    int sx, sy, sw, sh; // rects: 0, 0, then their size
    float x, y, w, h;
    uint32_t flags; // sprites: DrawFlags; rects: color
    uint32_t fill = 0; // sprites: drawSprite's fill (0 = none)
    // drawSpriteTinted: (x, y) = its unturned top-left; its angle (radians, counter-clockwise) and tint.
    float angle = 0;
    uint32_t tint = 0; // 0 = not a tinted draw
};
const std::vector<DrawCall>& testDrawCalls();
void testResetDrawCalls();

// Audio: audioBlocksWanted() answers `blocks` (0 until set); queued blocks are kept, in order
// (interleaved stereo, plat::AUDIO_BLOCK frames each), until the next set.
void testSetAudioWanted(int blocks);
void testSetAudioAvailable(bool available); // (true until set)
const std::vector<int16_t>& testAudioQueued();
// The audio thread: off until set (startAudioThread fails); on, it starts "running" but never runs
// by itself — testRunAudioThread calls its service once, as the thread would.
void testSetAudioThread(bool on);
// plat::nowMs (0 until set: the clock stands still; set it back to 0 after).
void testSetNowMs(double ms);
void testRunAudioThread();
