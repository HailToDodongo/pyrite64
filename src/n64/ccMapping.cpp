/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "ccMapping.h"
#include "libdragon.h"

namespace
{
  constexpr uint32_t getBits(uint64_t value, uint32_t start, uint32_t end) {
    return (value << (63 - end)) >> (63 - end + start);
  }

  constexpr uint64_t setBits(uint64_t newValue, uint32_t start, uint32_t end) {
    return ((newValue & ((1ULL << (end - start + 1)) - 1)) << start);
  }

  constexpr void switchColTex2Cycle(int32_t &c) {
    if (c == CC_C_TEX0) c = CC_C_TEX1;
    else if (c == CC_C_TEX1) c = CC_C_TEX0;
    else if (c == CC_C_TEX0_ALPHA) c = CC_C_TEX1_ALPHA;
    else if (c == CC_C_TEX1_ALPHA) c = CC_C_TEX0_ALPHA;
  }

  constexpr void switchAlphaTex2Cycle(int32_t &c) {
    if (c == CC_A_TEX0) c = CC_A_TEX1;
    else if (c == CC_A_TEX1) c = CC_A_TEX0;
  }
}

void N64::CC::unpackCC(uint64_t cc, glm::ivec4 &cc0Color, glm::ivec4 &cc0Alpha, glm::ivec4 &cc1Color,
  glm::ivec4 &cc1Alpha)
{
  cc0Color = { getBits(cc, 52, 55), getBits(cc, 28, 31), getBits(cc, 47, 51), getBits(cc, 15, 17) };
  cc0Alpha = { getBits(cc, 44, 46), getBits(cc, 12, 14), getBits(cc, 41, 43), getBits(cc, 9, 11)  };
  cc1Color = { getBits(cc, 37, 40), getBits(cc, 24, 27), getBits(cc, 32, 36), getBits(cc, 6, 8)   };
  cc1Alpha = { getBits(cc, 21, 23), getBits(cc, 3, 5),   getBits(cc, 18, 20), getBits(cc, 0, 2)   };
}

uint64_t N64::CC::packCC(const glm::ivec4 &cc0Color, const glm::ivec4 &cc0Alpha, const glm::ivec4 &cc1Color,
  const glm::ivec4 &cc1Alpha)
{
  uint64_t cc{0};
  cc |= setBits(cc0Color[0], 52, 55) | setBits(cc0Color[1], 28, 31) | setBits(cc0Color[2], 47, 51) | setBits(cc0Color[3], 15, 17);
  cc |= setBits(cc0Alpha[0], 44, 46) | setBits(cc0Alpha[1], 12, 14) | setBits(cc0Alpha[2], 41, 43) | setBits(cc0Alpha[3],  9, 11);
  cc |= setBits(cc1Color[0], 37, 40) | setBits(cc1Color[1], 24, 27) | setBits(cc1Color[2], 32, 36) | setBits(cc1Color[3],  6, 8);
  cc |= setBits(cc1Alpha[0], 21, 23) | setBits(cc1Alpha[1], 3,   5) | setBits(cc1Alpha[2], 18, 20) | setBits(cc1Alpha[3],  0, 2);
  return cc;
}

void N64::CC::unpackMappedCC(uint64_t cc, glm::ivec4 &cc0Color, glm::ivec4 &cc0Alpha, glm::ivec4 &cc1Color,
  glm::ivec4 &cc1Alpha)
{
  unpackCC(cc, cc0Color, cc0Alpha, cc1Color, cc1Alpha);

  for (int i=0; i<4; ++i) {
    cc0Color[i] = MAP_COLOR[i][cc0Color[i]];
    cc1Color[i] = MAP_COLOR[i][cc1Color[i]];

    cc0Alpha[i] = MAP_ALPHA[i][cc0Alpha[i]];
    cc1Alpha[i] = MAP_ALPHA[i][cc1Alpha[i]];
  }

  for (int i=0; i<4; ++i)
  {
    switchColTex2Cycle(cc1Color[i]);
    switchAlphaTex2Cycle(cc1Alpha[i]);
  }
}

N64::CC::Usage N64::CC::getUsage(uint64_t cc)
{
  Usage usage{};

  usage.twoCycle = (cc & RDPQ_COMBINER_2PASS) != 0;
  glm::ivec4 ccColor[2];
  glm::ivec4 ccAlpha[2];
  unpackMappedCC(cc, ccColor[0], ccAlpha[0], ccColor[1], ccAlpha[1]);

  for (int c = 0; c < (usage.twoCycle ? 2 : 1); ++c)
  {
    for (int i=0; i<4; ++i)
    {
      switch(ccColor[c][i]) {
        case CC_C_TEX0: usage.tex0 = true; break;
        case CC_C_TEX1: usage.tex1 = true; break;
        case CC_C_PRIM: usage.prim = true; break;
        case CC_C_SHADE: usage.shade = true; break;
        case CC_C_ENV: usage.env = true; break;
        case CC_C_TEX0_ALPHA: usage.tex0 = true; break;
        case CC_C_TEX1_ALPHA: usage.tex1 = true; break;
        case CC_C_PRIM_ALPHA: usage.prim = true; break;
        case CC_C_SHADE_ALPHA: usage.shade = true; break;
        case CC_C_ENV_ALPHA: usage.env = true; break;
        case CC_C_PRIM_LOD_FRAC: usage.lod = true; break;
        case CC_C_K4: usage.k4k5 = true; break;
        case CC_C_K5: usage.k4k5 = true; break;
        default: break;
      }

      switch(ccAlpha[c][i]) {
        case CC_A_TEX0: usage.tex0 = true; break;
        case CC_A_TEX1: usage.tex1 = true; break;
        case CC_A_PRIM: usage.prim = true; break;
        case CC_A_SHADE: usage.shade = true; break;
        case CC_A_ENV: usage.env = true; break;
        case CC_A_PRIM_LOD_FRAC: usage.lod = true; break;
        default: break;
      }
    }
  }

  return usage;
}

