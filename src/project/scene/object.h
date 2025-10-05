/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "simdjson.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"


namespace Project
{
  class Scene;

  class Object
  {
    public:
      Object* parent{nullptr};

      std::string name{};
      uint64_t uuid{0};
      uint16_t id{};

      glm::vec3 pos{};
      glm::quat rot{};
      glm::vec3 scale{};

      std::vector<std::shared_ptr<Object>> children{};

      explicit Object(Object& parent) : parent{&parent} {}
      Object() = default;

      std::string serialize();
      void deserialize(Scene &scene, const simdjson::simdjson_result<simdjson::dom::element> &doc);
  };
}
