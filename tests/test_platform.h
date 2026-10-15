// Test-only controls of the headless platform backend (tests/test_platform.cpp).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// In-memory files that readFile() serves before the asset root (for synthetic fixtures).
void testSetFile(const std::string& path, const std::string& contents);
void testClearFiles();

// Every drawSprite/drawRect/drawLine since the last reset, in call order.
struct DrawCall {
    std::string sheet; // empty for rects
    int sx, sy, sw, sh; // rects: 0, 0, then their size
    float x, y, w, h;
    uint32_t flags; // sprites: DrawFlags; rects: color
};
const std::vector<DrawCall>& testDrawCalls();
void testResetDrawCalls();
