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

namespace Project
{
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

    nlohmann::json serialize() const {
      return Utils::JSON::Builder{}
        .set(setDepth).set(depth)
        .set(setPrim).set(prim)
        .set(setEnv).set(env)
        .set(setLighting).set(lighting)
        .set(setFresnel).set(fresnel)
        .set(fresnelColor)
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
    }

    void build(Utils::BinaryFile &file, Object& obj);
  };
}
