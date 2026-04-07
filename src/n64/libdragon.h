/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)

extern "C" {
}

#include "../vendored/libdragon/include/graphics.h"
#include "../vendored/libdragon/include/rdpq_mode.h"
#include "../vendored/libdragon/include/rdpq_macros.h"

namespace T3D {
  constexpr uint32_t FLAG_DEPTH      = 1 << 0;
  constexpr uint32_t FLAG_TEXTURED   = 1 << 1;
  constexpr uint32_t FLAG_SHADED     = 1 << 2;
  constexpr uint32_t FLAG_CULL_FRONT = 1 << 3;
  constexpr uint32_t FLAG_CULL_BACK  = 1 << 4;
  constexpr uint32_t FLAG_NO_LIGHT   = 1 << 16;
};
