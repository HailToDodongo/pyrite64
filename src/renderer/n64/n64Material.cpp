/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "n64Material.h"
#include "../../n64/ccMapping.h"
#include "tiny3d/tools/gltf_importer/src/parser/rdp.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace
{
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

  uint64_t otherModes = 0;
  if (cc & RDPQ_COMBINER_2PASS) {
    otherModes |= SOM_CYCLE_2;
  }
  if (t3dMat.filterSet.value) {
    otherModes |= ((uint64_t)t3dMat.filter.value << SOM_SAMPLE_SHIFT) & SOM_SAMPLE_MASK;
  }
  part.material.otherModeH = otherModes >> 32;
  part.material.otherModeL = otherModes & 0xFFFFFFFF;

  part.material.primLodDepth[1] = t3dMat.primLod.value / 256.0f; // intentional 0x100 instead 0xFF

  // @TODO
  //part.material.flags |= t3dMat.setBlendColor ? UniformN64Material::FLAG_SET_BLEND_COL : 0;

  part.material.flags = t3dMat.drawFlags.value;

  part.material.flags |= t3dMat.envColorSet.value ? UniformN64Material::FLAG_SET_ENV_COL : 0;
  part.material.flags |= t3dMat.primColorSet.value ? UniformN64Material::FLAG_SET_PRIM_COL : 0;

  part.material.lightDir[0].w = t3dMat.alphaComp.value / 255.0f;

  N64::CC::unpackMappedCC(cc, part.material.cc0Color, part.material.cc0Alpha, part.material.cc1Color, part.material.cc1Alpha);

  part.material.colPrim = t3dMat.primColor.value * 255.0f / 256.0f;
  part.material.colEnv = t3dMat.envColor.value * 255.0f / 256.0f;

  part.material.mask = {
    std::pow(2, integerToPow2(texA.texSize.value[0])),
    std::pow(2, integerToPow2(texA.texSize.value[1])),
    std::pow(2, integerToPow2(texB.texSize.value[0])),
    std::pow(2, integerToPow2(texB.texSize.value[1])),
  };

  part.material.low = {
    texA.offset.value,
    texB.offset.value
  };
  part.material.high = part.material.low + glm::vec4{
    texA.texSize.value[0] - 1, texA.texSize.value[0] - 1,
    texB.texSize.value[0] - 1, texB.texSize.value[0] - 1,
  };

  part.material.shift = {
    1.0f / std::pow(2, texA.scale.value[0]),
    1.0f / std::pow(2, texA.scale.value[1]),
    1.0f / std::pow(2, texB.scale.value[0]),
    1.0f / std::pow(2, texB.scale.value[1]),
  };

  /*
 # quantize the low/high values into 0.25 pixel increments
 conf[8:] = np.round(conf[8:] * 4) / 4
  */

  if (texA.repeat.value[0] <= 1.0f) part.material.mask[0] = -part.material.mask[0];
  if (texA.repeat.value[1] <= 1.0f) part.material.mask[1] = -part.material.mask[1];
  if (texB.repeat.value[0] <= 1.0f) part.material.mask[2] = -part.material.mask[2];
  if (texB.repeat.value[1] <= 1.0f) part.material.mask[3] = -part.material.mask[3];

  if (texA.mirrorS.value) part.material.high[0] = -part.material.high[0];
  if (texA.mirrorT.value) part.material.high[1] = -part.material.high[1];
  if (texB.mirrorS.value) part.material.high[2] = -part.material.high[2];
  if (texB.mirrorT.value) part.material.high[3] = -part.material.high[3];
}
