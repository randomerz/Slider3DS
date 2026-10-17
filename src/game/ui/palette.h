// Slider's UI colors, shared by the menus (menu_skin) and the top-screen HUD (hud).
#pragma once

#include <cstdint>

#include "platform/platform.h"

namespace palette {

constexpr uint32_t OFF_WHITE = plat::rgba(0xe6, 0xe6, 0xe6); // Slider's white: light text, the white flash panel
constexpr uint32_t DARK = plat::rgba(0x1a, 0x1a, 0x1a);      // dark text and panels, the bottom screen's background

// `color` with alpha `a` (plat::rgba packs alpha in the low byte).
constexpr uint32_t withAlpha(uint32_t color, uint8_t a) { return (color & ~0xffu) | a; }

} // namespace palette
