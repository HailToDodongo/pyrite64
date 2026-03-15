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

  int integerToPow2(int x){
    int res = 0;
    while(1<<res < x) res++;
    return res;
  }
}

void Renderer::N64Material::convert(N64Mesh::MeshPart &part, const Project::Assets::Material &t3dMat)
{
  auto &texA = t3dMat.tex0;
  auto &texB = t3dMat.tex1;

  uint64_t cc = t3dMat.cc.value;

  part.material.vertexFX = t3dMat.vertexFX.value;

  // @TODO
  //part.material.otherModeH = t3dMat.otherModeValue >> 32;
  // @TODO
  //part.material.otherModeL = t3dMat.otherModeValue & 0xFFFFFFFF;

  part.material.flags = 0;//t3dMat.drawFlags; // @TODO

  // @TODO
  //part.material.flags |= t3dMat.setBlendColor ? UniformN64Material::FLAG_SET_BLEND_COL : 0;
  // @TODO
  //part.material.flags |= t3dMat.setEnvColor ? UniformN64Material::FLAG_SET_ENV_COL : 0;
  // @TODO
  //part.material.flags |= t3dMat.setPrimColor ? UniformN64Material::FLAG_SET_PRIM_COL : 0;

  if (cc & RDPQ_COMBINER_2PASS) {
    part.material.otherModeH |= G_CYC_2CYCLE;
  }

  part.material.lightDir[0].w = 0.0f; // no alpha clip
  if (t3dMat.alphaComp.value  != 0) {
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

  part.material.colPrim = t3dMat.primColor.value;
  part.material.colEnv = t3dMat.envColor.value;

  part.material.mask = {
    std::pow(2, integerToPow2(texA.width.value)),
    std::pow(2, integerToPow2(texA.height.value)),
    std::pow(2, integerToPow2(texB.width.value)),
    std::pow(2, integerToPow2(texB.height.value)),
  };

  part.material.low = {
    texA.offsetS.value, texA.offsetT.value,
    texB.offsetS.value, texB.offsetT.value,
  };
  part.material.high = part.material.low + glm::vec4{
    texA.width.value - 1, texA.height.value - 1,
    texB.width.value - 1, texB.height.value - 1,
  };

  part.material.shift = {
    1.0f / std::pow(2, texA.scaleS.value),
    1.0f / std::pow(2, texA.scaleT.value),
    1.0f / std::pow(2, texB.scaleS.value),
    1.0f / std::pow(2, texB.scaleT.value),
  };

  /*
 # quantize the low/high values into 0.25 pixel increments
 conf[8:] = np.round(conf[8:] * 4) / 4
  */

  if (texA.repeatS.value <= 1.0f) part.material.mask[0] = -part.material.mask[0];
  if (texA.repeatT.value <= 1.0f) part.material.mask[1] = -part.material.mask[1];
  if (texB.repeatS.value <= 1.0f) part.material.mask[2] = -part.material.mask[2];
  if (texB.repeatT.value <= 1.0f) part.material.mask[3] = -part.material.mask[3];

  if (texA.mirrorS.value) part.material.high[0] = -part.material.high[0];
  if (texA.mirrorT.value) part.material.high[1] = -part.material.high[1];
  if (texB.mirrorS.value) part.material.high[2] = -part.material.high[2];
  if (texB.mirrorT.value) part.material.high[3] = -part.material.high[3];
}
