/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "n64Material.h"
#include "../../shader/defines.h"
#include "ccMapping.h"
#include "tiny3d/tools/gltf_importer/src/parser/rdp.h"

namespace
{
  constexpr uint64_t RDPQ_COMBINER_2PASS = (uint64_t)(1) << 63;

  constexpr uint32_t getBits(uint64_t value, uint32_t start, uint32_t end) {
    return (value << (63 - end)) >> (63 - end + start);
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

void Renderer::N64Material::convert(N64Mesh::MeshPart &part, const T3DM::Material &t3dMat)
{
  auto &texA = t3dMat.texA;
  auto &texB = t3dMat.texB;

  uint64_t cc = t3dMat.colorCombiner;

  part.material.vertexFX = t3dMat.vertexFxFunc;
  part.material.otherModeH = t3dMat.otherModeValue >> 32;
  part.material.otherModeL = t3dMat.otherModeValue & 0xFFFFFFFF;
  part.material.flags = t3dMat.drawFlags;

  if (cc & RDPQ_COMBINER_2PASS) {
    part.material.otherModeH |= G_CYC_2CYCLE;
  }

  part.material.lightDir[0].w = 0.0f; // no alpha clip
  if (t3dMat.otherModeValue & RDP::SOM::ALPHA_COMPARE) {
    part.material.lightDir[0].w = 0.5f;
  }

  part.material.cc0Color = { getBits(cc, 52, 55), getBits(cc, 28, 31), getBits(cc, 47, 51), getBits(cc, 15, 17) };
  part.material.cc0Alpha = { getBits(cc, 44, 46), getBits(cc, 12, 14), getBits(cc, 41, 43), getBits(cc, 9, 11)  };
  part.material.cc1Color = { getBits(cc, 37, 40), getBits(cc, 24, 27), getBits(cc, 32, 36), getBits(cc, 6, 8)   };
  part.material.cc1Alpha = { getBits(cc, 21, 23), getBits(cc, 3, 5),   getBits(cc, 18, 20), getBits(cc, 0, 2)   };

  for (int i=0; i<4; ++i) {
    part.material.cc0Color[i] = CC_MAP_COLOR[i][part.material.cc0Color[i]];
    part.material.cc1Color[i] = CC_MAP_COLOR[i][part.material.cc1Color[i]];

    part.material.cc0Alpha[i] = CC_MAP_ALPHA[i][part.material.cc0Alpha[i]];
    part.material.cc1Alpha[i] = CC_MAP_ALPHA[i][part.material.cc1Alpha[i]];

    switchColTex2Cycle(part.material.cc1Color[i]);
    switchAlphaTex2Cycle(part.material.cc1Alpha[i]);
  }

  part.material.colPrim = {
    t3dMat.primColor[0] / 255.0f,
    t3dMat.primColor[1] / 255.0f,
    t3dMat.primColor[2] / 255.0f,
    t3dMat.primColor[3] / 255.0f
  };
  part.material.colEnv = {
    t3dMat.envColor[0] / 255.0f,
    t3dMat.envColor[1] / 255.0f,
    t3dMat.envColor[2] / 255.0f,
    t3dMat.envColor[3] / 255.0f,
  };

  part.material.mask = {
    texA.s.mask, texA.t.mask,
    texB.s.mask, texB.t.mask,
  };
  part.material.low = {
    texA.s.low, texA.t.low,
    texB.s.low, texB.t.low,
  };
  part.material.high = {
    texA.s.high, texA.t.high,
    texB.s.high, texB.t.high,
  };

  part.material.mask = {
    std::pow(2, texA.s.mask),
    std::pow(2, texA.t.mask),
    std::pow(2, texB.s.mask),
    std::pow(2, texB.t.mask),
  };

  part.material.shift = {
    1.0f / std::pow(2, texA.s.shift),
    1.0f / std::pow(2, texA.t.shift),
    1.0f / std::pow(2, texB.s.shift),
    1.0f / std::pow(2, texB.t.shift),
  };


  /*
 # quantize the low/high values into 0.25 pixel increments
 conf[8:] = np.round(conf[8:] * 4) / 4

 # if clamp is on, negate the mask value
 if t0.S.clamp: conf[0] = -conf[0]
 if t0.T.clamp: conf[1] = -conf[1]
 if t1.S.clamp: conf[2] = -conf[2]
 if t1.T.clamp: conf[3] = -conf[3]

 # if mirror is on, negate the high value
 if t0.S.mirror: conf[12] = -conf[12]
 if t0.T.mirror: conf[13] = -conf[13]
 if t1.S.mirror: conf[14] = -conf[14]
 if t1.T.mirror: conf[15] = -conf[15]
  */
  if (texA.s.clamp) part.material.mask[0] = -part.material.mask[0];
  if (texA.t.clamp) part.material.mask[1] = -part.material.mask[1];
  if (texB.s.clamp) part.material.mask[2] = -part.material.mask[2];
  if (texB.t.clamp) part.material.mask[3] = -part.material.mask[3];

  if (texA.s.mirror) part.material.high[0] = -part.material.high[0];
  if (texA.t.mirror) part.material.high[1] = -part.material.high[1];
  if (texB.s.mirror) part.material.high[2] = -part.material.high[2];
  if (texB.t.mirror) part.material.high[3] = -part.material.high[3];
}
