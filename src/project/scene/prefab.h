/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "object.h"
#include "../../utils/prop.h"
#include "../component/components.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Project
{
  class Scene;

  class Prefab
  {
    public:
      PROP_U32(uuid);
      Object obj{};

      std::string serialize(const Object &obj) const;
      std::string serialize() const { return serialize(obj); }

      void deserialize(const std::string &str);

      void save(const std::string &path);
  };
}