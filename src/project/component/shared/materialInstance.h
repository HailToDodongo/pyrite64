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

    Assets::MaterialTex texSlots[8]{};

    nlohmann::json serialize() const {
      return Utils::JSON::Builder{}
        .set(setDepth).set(depth)
        .set(setPrim).set(prim)
        .set(setEnv).set(env)
        .set(setLighting).set(lighting)
        .set(setFresnel).set(fresnel)
        .set(fresnelColor)
        .set("tex0", texSlots[0].serialize())
        .set("tex1", texSlots[1].serialize())
        .set("tex2", texSlots[2].serialize())
        .set("tex3", texSlots[3].serialize())
        .set("tex4", texSlots[4].serialize())
        .set("tex5", texSlots[5].serialize())
        .set("tex6", texSlots[6].serialize())
        .set("tex7", texSlots[7].serialize())
        .doc;
    }

    void deserialize(const nlohmann::json &doc) {
      Utils::JSON::readProp(doc, setDepth, false);
      Utils::JSON::readProp(doc, depth);
      Utils::JSON::readProp(doc, setPrim, false);
      Utils::JSON::readProp(doc, prim, {1, 1, 1, 1});
      Utils::JSON::readProp(doc, setEnv, false);
      Utils::JSON::readProp(doc, env, {1, 1, 1, 1});
      Utils::JSON::readProp(doc, setLighting, false);
      Utils::JSON::readProp(doc, lighting, true);
      Utils::JSON::readProp(doc, setFresnel, false);
      Utils::JSON::readProp(doc, fresnel, 0);
      Utils::JSON::readProp(doc, fresnelColor, {1,1,1,1});

      for(int i=0; i<8; ++i) {
        if(doc.contains("tex" + std::to_string(i))) {
          texSlots[i].deserialize(doc["tex" + std::to_string(i)]);
        }
      }
    }

    void build(Utils::BinaryFile &file, Build::SceneCtx &ctx, Object& obj);

    void validateWithModel(const Assets::Model3D &model);
  };
}
