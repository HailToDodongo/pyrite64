/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "json.hpp"
#include "../../utils/prop.h"
#include "tiny3d/tools/gltf_importer/src/structs.h"

namespace Project
{
  class AssetManager;
}

namespace Project::Assets
{
  struct MaterialTex
  {
    PROP_BOOL(set);
    PROP_U64(texUUID);
    PROP_IVEC2(texSize);

    PROP_BOOL(dynTexture);
    PROP_BOOL(dynTileScroll);

    PROP_VEC2(offset);
    PROP_IVEC2(scale);
    PROP_VEC2(repeat);
    PROP_BOOL(mirrorS);  PROP_BOOL(mirrorT);

    [[nodiscard]] nlohmann::json serialize() const;
    void deserialize(const nlohmann::json &doc);
  };

  struct Material
  {
    // Internal settings
    PROP_BOOL(isCustom);

    // Render-Mode settings
    PROP_U64(cc);
    PROP_BOOL(ccSet);

    PROP_U32(blender); PROP_BOOL(blenderSet);
    PROP_S32(aa);      PROP_BOOL(aaSet);
    PROP_U32(fog);     PROP_BOOL(fogSet);
    PROP_S32(dither);  PROP_BOOL(ditherSet);
    PROP_S32(filter);  PROP_BOOL(filterSet);

    PROP_S32(zmode);
    PROP_BOOL(zmodeSet);

    PROP_S32(zprim); PROP_BOOL(zprimSet);
    PROP_S32(zdelta);

    PROP_BOOL(persp);    PROP_BOOL(perspSet);
    PROP_S32(alphaComp); PROP_BOOL(alphaCompSet);

    // Textures
    MaterialTex tex0{};
    MaterialTex tex1{};

    // T3D settings
    PROP_S32(vertexFX);
    PROP_U32(drawFlags);
    PROP_BOOL(fogToAlpha);

    // Values
    PROP_IVEC2(k4k5);
    PROP_BOOL(k4k5Set);

    PROP_U32(primLod); PROP_BOOL(primLodSet);
    PROP_VEC4(primColor); PROP_BOOL(primColorSet);
    PROP_VEC4(envColor); PROP_BOOL(envColorSet);

    [[nodiscard]] nlohmann::json serialize() const;
    void deserialize(const nlohmann::json &doc);

    void fromT3D(::Project::AssetManager &assets, const T3DM::Material &mat);
  };
}
