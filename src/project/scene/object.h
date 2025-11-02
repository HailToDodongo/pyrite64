/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "simdjson.h"
#include "../component/components.h"
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
      uint32_t uuid{0};
      uint16_t id{};

      glm::vec3 pos{};
      glm::quat rot{};
      glm::vec3 scale{};

      bool enabled{true};
      bool selectable{true};

      std::vector<std::shared_ptr<Object>> children{};
      std::vector<Component::Entry> components{};

      explicit Object(Object& parent) : parent{&parent} {}
      Object() = default;

      void addComponent(int compID);
      void removeComponent(uint64_t uuid);

      std::string serialize();
      void deserialize(Scene &scene, const simdjson::simdjson_result<simdjson::dom::element> &doc);
  };
}
