/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "material.h"

#include "../../utils/json.h"
#include "../../utils/jsonBuilder.h"
#include "../project.h"
#include "tiny3d/tools/gltf_importer/src/parser/rdp.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace J = Utils::JSON;

nlohmann::json Project::Assets::MaterialTex::serialize() const
{
  auto doc = Utils::JSON::Builder{}
    .set(set)
    .set(texUUID)
    .set(dynTexture)
    .set(dynTileScroll)
    .set(dynTileIdx)
    .set(dynPlaceholder)
    .set(texSize)
    .set(offset)
    .set(scale)
    .set(repeat)
    .set(mirrorS).set(mirrorT)
    .doc;

  return doc;
}

void Project::Assets::MaterialTex::deserialize(const nlohmann::json &doc)
{
  J::readProp(doc, set);
  J::readProp(doc, texUUID);
  J::readProp(doc, dynTexture);
  J::readProp(doc, dynTileScroll);
  J::readProp(doc, dynTileIdx);
  J::readProp(doc, dynPlaceholder);
  J::readProp(doc, texSize);
  J::readProp(doc, offset);
  J::readProp(doc, scale);
  J::readProp(doc, repeat);
  J::readProp(doc, mirrorS);
  J::readProp(doc, mirrorT);
}

void Project::Assets::MaterialTex::build(
  Utils::BinaryFile &file,
  Build::SceneCtx &sceneCtx
) const
{
  if(dynTexture.value) {
    file.write<uint16_t>(0xFFFF);
    file.write<uint16_t>(dynPlaceholder.value);
  } else {
    auto assetIdx = sceneCtx.assetUUIDToIdx.find(texUUID.value);
    if(assetIdx == sceneCtx.assetUUIDToIdx.end()) {
      throw std::runtime_error("Material Texture UUID not found: " + std::to_string(texUUID.value));
    }

    file.write<uint16_t>(assetIdx->second);
    file.write<uint16_t>(dynPlaceholder.value);
  }

  file.write<uint16_t>(offset.value[0] * 64.0f);
  file.write<uint16_t>(repeat.value[0] * 16);
  file.write<int8_t>(scale.value[0]);
  file.write<int8_t>(mirrorS.value ? 1 : 0);

  file.write<uint16_t>(offset.value[1] * 64.0f);
  file.write<uint16_t>(repeat.value[1] * 16);
  file.write<int8_t>(scale.value[1]);
  file.write<int8_t>(mirrorT.value ? 1 : 0);
}

nlohmann::json Project::Assets::Material::serialize() const
{
  auto doc = Utils::JSON::Builder{}
    .set(isCustom)
    .set(cc).set(ccSet)
    .set(blender).set(blenderSet)
    .set(aa).set(aaSet)
    .set(fog).set(fogSet)
    .set(dither).set(ditherSet)
    .set(filter).set(filterSet)
    .set(zmode).set(zmodeSet)

    .set(zprim).set(zprimSet)
    .set(zdelta)

    .set(persp).set(perspSet)
    .set(alphaComp).set(alphaCompSet)
    .set(vertexFX)
    .set(drawFlags)
    .set(fogToAlpha)

    .set(k4k5).set(k4k5Set)

    .set(primLod).set(primLodSet)
    .set(primColor).set(primColorSet)
    .set(envColor).set(envColorSet)
    .doc;

  doc["tex0"] = tex0.serialize();
  doc["tex1"] = tex1.serialize();
  return doc;
}

void Project::Assets::Material::deserialize(const nlohmann::json &doc)
{
  J::readProp(doc, isCustom);
  J::readProp(doc, cc);      J::readProp(doc, ccSet);
  J::readProp(doc, blender); J::readProp(doc, blenderSet);
  J::readProp(doc, aa);      J::readProp(doc, aaSet);
  J::readProp(doc, fog);     J::readProp(doc, fogSet);
  J::readProp(doc, dither);  J::readProp(doc, ditherSet);
  J::readProp(doc, filter);  J::readProp(doc, filterSet);
  J::readProp(doc, zmode);   J::readProp(doc, zmodeSet);

  J::readProp(doc, zprim);   J::readProp(doc, zprimSet);
  J::readProp(doc, zdelta);

  J::readProp(doc, persp);
  J::readProp(doc, perspSet);
  J::readProp(doc, alphaComp);
  J::readProp(doc, alphaCompSet);

  tex0.deserialize(doc["tex0"]);
  tex1.deserialize(doc["tex1"]);

  J::readProp(doc, vertexFX);
  J::readProp(doc, drawFlags);
  J::readProp(doc, fogToAlpha);

  J::readProp(doc, k4k5Set);
  J::readProp(doc, k4k5);

  J::readProp(doc, primLod);   J::readProp(doc, primLodSet);
  J::readProp(doc, primColor); J::readProp(doc, primColorSet);
  J::readProp(doc, envColor);  J::readProp(doc, envColorSet);
}

void Project::Assets::Material::fromT3D(::Project::AssetManager &assets, const T3DM::Material &matT3D)
{
  #define REPEAT_INFINITE 2048
  #define T3D_FOG_MODE_DEFAULT  0
  #define T3D_FOG_MODE_DISABLED 1
  #define T3D_FOG_MODE_ACTIVE   2

  *this = {};

  // implicit defaults, set values but don't actually set
  persp.value = true;
  zmode.value = 0b11;
  dither.value = 15;
  primColor.value = {0.0f, 0.0f, 0.0f, 1.0f};
  envColor.value = {0.5f, 0.5f, 0.5f, 1.0f};

  //isCustom.value = false;
  auto convertTex = [&assets](const T3DM::MaterialTexture &mat, MaterialTex &tex)
  {
    tex.set.value = false;
    tex.texSize.value[0] = mat.texWidth;
    tex.texSize.value[1] = mat.texHeight;

    if(!mat.texPathRom.empty()) {
      auto asset = assets.getByPath(mat.texPath);
      if(asset) {
        tex.set.value = true;
        tex.texUUID.value = asset->getUUID();
        // tex.width.value = asset->texture->getWidth();
        // tex.height.value = asset->texture->getHeight();
      }
    }

    tex.offset.value = {mat.s.low, mat.t.low};
    tex.scale.value = {mat.s.shift, mat.t.shift};
    tex.repeat.value = {
      mat.s.clamp ? 1.0f : REPEAT_INFINITE,
      mat.t.clamp ? 1.0f : REPEAT_INFINITE
    };
    tex.mirrorS.value = mat.s.mirror;
    tex.mirrorT.value = mat.t.mirror;
  };

  convertTex(matT3D.texA, tex0);
  convertTex(matT3D.texB, tex1);

  ccSet.value = true;
  cc.value = matT3D.colorCombiner;
  drawFlags.value = matT3D.drawFlags;

  vertexFX.value = matT3D.vertexFxFunc;

  if(matT3D.fogMode == T3D_FOG_MODE_ACTIVE) {
    //fogSet.value = true;
    fogToAlpha.value = 1;
  } else if(matT3D.fogMode == T3D_FOG_MODE_DISABLED) {
    //fogSet.value = true;
    fogToAlpha.value = 0;
  }

  if(matT3D.otherModeMask & SOM_SAMPLE_MASK)
  {
    filterSet.value = true;
    filter.value = (matT3D.otherModeValue & SOM_SAMPLE_MASK) >> SOM_SAMPLE_SHIFT;
  }

  if(matT3D.setPrimColor)
  {
    primColorSet.value = true;
    primColor.value = glm::vec4{
      matT3D.primColor[0] / 255.0f,
      matT3D.primColor[1] / 255.0f,
      matT3D.primColor[2] / 255.0f,
      matT3D.primColor[3] / 255.0f,
    };
  }

  if(matT3D.setEnvColor)
  {
    envColorSet.value = true;
    envColor.value = glm::vec4{
      matT3D.envColor[0] / 255.0f,
      matT3D.envColor[1] / 255.0f,
      matT3D.envColor[2] / 255.0f,
      matT3D.envColor[3] / 255.0f,
    };
  }
}
