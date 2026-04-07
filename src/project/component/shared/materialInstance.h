/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "json.hpp"
#include "../../../utils/prop.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../assets/material.h"

namespace Project
{
  namespace Assets
  {
    struct Model3D;
  }

  class Object;
}

namespace Project::Component::Shared
{
  struct MaterialInstance
  {
    PROP_BOOL(setDepth);
    PROP_S32(depth);

    PROP_BOOL(setPrim);
    PROP_VEC4(prim);

    PROP_BOOL(setEnv);
    PROP_VEC4(env);

    PROP_BOOL(setLighting);
    PROP_BOOL(lighting);

    PROP_BOOL(setFresnel);
    PROP_S32(fresnel);
    PROP_VEC4(fresnelColor);

    std::array<Assets::MaterialTex, 8> texSlots{};

    [[nodiscard]] nlohmann::json serialize() const;
    void deserialize(const nlohmann::json &doc);

    void build(Utils::BinaryFile &file, Build::SceneCtx &ctx, Object& obj);

    void validateWithModel(const Assets::Model3D &model);
  };
}
