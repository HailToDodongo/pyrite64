/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>

#include "../shader/defines.h"
#include "glm/vec4.hpp"

// Based on: https://n64brew.dev/wiki/Reality_Display_Processor/Commands#0x3C_-_Set_Combine_Mode

namespace N64::CC
{
  constexpr uint8_t MAP_COLOR[4][0xFF] {
    {
      CC_C_COMB,
      CC_C_TEX0,
      CC_C_TEX1,
      CC_C_PRIM,
      CC_C_SHADE,
      CC_C_ENV,
      CC_C_1,
      CC_C_NOISE,
      CC_C_0,
    }, {
      CC_C_COMB,
      CC_C_TEX0,
      CC_C_TEX1,
      CC_C_PRIM,
      CC_C_SHADE,
      CC_C_ENV,
      CC_C_CENTER,
      CC_C_K4,
      CC_C_0,
    }, {
      CC_C_COMB,
      CC_C_TEX0,
      CC_C_TEX1,
      CC_C_PRIM,
      CC_C_SHADE,
      CC_C_ENV,
      CC_C_SCALE,
      CC_C_COMB_ALPHA,
      CC_C_TEX0_ALPHA,
      CC_C_TEX1_ALPHA,
      CC_C_PRIM_ALPHA,
      CC_C_SHADE_ALPHA,
      CC_C_ENV_ALPHA,
      CC_C_LOD_FRAC,
      CC_C_PRIM_LOD_FRAC,
      CC_C_K5,
      CC_C_0
    }, {
      CC_C_COMB,
      CC_C_TEX0,
      CC_C_TEX1,
      CC_C_PRIM,
      CC_C_SHADE,
      CC_C_ENV,
      CC_C_1,
      CC_C_0,
    }
  };

  constexpr uint8_t MAP_ALPHA[4][0xFF] {
    {
      CC_A_COMB,
      CC_A_TEX0,
      CC_A_TEX1,
      CC_A_PRIM,
      CC_A_SHADE,
      CC_A_ENV,
      CC_A_1,
      CC_A_0,
    }, {
      CC_A_COMB,
      CC_A_TEX0,
      CC_A_TEX1,
      CC_A_PRIM,
      CC_A_SHADE,
      CC_A_ENV,
      CC_A_1,
      CC_A_0,
    }, {
      CC_A_LOD_FRAC,
      CC_A_TEX0,
      CC_A_TEX1,
      CC_A_PRIM,
      CC_A_SHADE,
      CC_A_ENV,
      CC_A_PRIM_LOD_FRAC,
      CC_A_0,
    }, {
      CC_A_COMB,
      CC_A_TEX0,
      CC_A_TEX1,
      CC_A_PRIM,
      CC_A_SHADE,
      CC_A_ENV,
      CC_A_1,
      CC_A_0,
    }
  };

  constexpr std::array NAMES_COL_A = {
    "Combined", "TEX0", "TEX1",
    "Prim Color", "Shade", "Env Color",
    "1", "Noise", "0"
  };
  constexpr std::array NAMES_COL_B = {
    "Combined", "TEX0", "TEX1",
    "Prim Color", "Shade", "Env Color",
    "Center", "K4", "0"
  };
  constexpr std::array NAMES_COL_C = {
    "Combined", "TEX0", "TEX1",
    "Prim Color", "Shade", "Env Color",
    "Scale", "Combined Alpha",
    "TEX0 Alpha", "TEX1 Alpha", "Prim Alpha",
    "Shade Alpha", "Env Alpha", "LOD",
    "Prim LOD", "K5", "0",
  };
  constexpr std::array NAMES_COL_D = {
    "Combined", "TEX0", "TEX1",
    "Prim Color", "Shade",
    "Env Color", "1", "0",
  };

  constexpr std::array NAMES_ALPHA_A = {
    "Combined", "TEX0", "TEX1", "Prim Alpha", "Shade",
    "Env Alpha", "1", "0"
  };
  constexpr std::array NAMES_ALPHA_B = {
    "Combined", "TEX0", "TEX1", "Prim Alpha", "Shade",
    "Env Alpha", "1", "0"
  };
  constexpr std::array NAMES_ALPHA_C = {
    "LOD", "TEX0", "TEX1", "Prim Alpha",
    "Shade", "Env Alpha", "Prim LOD", "0"
  };
  constexpr std::array NAMES_ALPHA_D = {
    "Combined", "TEX0", "TEX1", "Prim Alpha", "Shade",
    "Env Alpha", "1", "0"
  };

  void unpackCC(uint64_t cc, glm::ivec4 &cc0Color, glm::ivec4 &cc0Alpha, glm::ivec4 &cc1Color, glm::ivec4 &cc1Alpha);
  uint64_t packCC(const glm::ivec4 &cc0Color, const glm::ivec4 &cc0Alpha, const glm::ivec4 &cc1Color, const glm::ivec4 &cc1Alpha);

  void unpackMappedCC(uint64_t cc, glm::ivec4 &cc0Color, glm::ivec4 &cc0Alpha, glm::ivec4 &cc1Color, glm::ivec4 &cc1Alpha);
}