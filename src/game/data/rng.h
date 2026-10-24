// A random sequence (the port's stand-in for Unity's Random): xorshift32, one per system so each
// stays reproducible on its own (tests replay them).
#pragma once

#include <cstdint>

class Rng {
public:
    explicit constexpr Rng(uint32_t seed = 0x9e3779b9u) : s_(seed ? seed : 1) {} // (0 would stay 0)
    float unit() { // [0, 1)
        s_ ^= s_ << 13;
        s_ ^= s_ >> 17;
        s_ ^= s_ << 5;
        return (float)(s_ >> 8) / (float)(1u << 24);
    }

private:
    uint32_t s_;
};
