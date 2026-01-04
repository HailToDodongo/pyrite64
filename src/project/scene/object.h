/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "simdjson.h"
#include "../../utils/prop.h"
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

      PROP_U64(uuidPrefab);

      PROP_VEC3(pos);
      PROP_QUAT(rot);
      PROP_VEC3(scale);

      bool enabled{true};
      bool selectable{true};
      bool isPrefabEdit{false};

      std::unordered_map<uint64_t, GenericValue> propOverrides{};

      std::vector<std::shared_ptr<Object>> children{};
      std::vector<Component::Entry> components{};

      explicit Object(Object& parent) : parent{&parent} {}
      Object() = default;

      void addComponent(int compID);
      void removeComponent(uint64_t uuid);

      std::string serialize() const;
      void deserialize(Scene *scene, const simdjson::simdjson_result<simdjson::dom::element> &doc);

      bool isPrefabInstance() const {
        return uuidPrefab.value != 0;
      }

      template<typename T>
      void addPropOverride(const Property<T>& prop)
      {
        GenericValue genVal{};
        genVal.set<T>(prop.value);
        propOverrides[prop.id] = genVal;
      }

      template<typename T>
      void removePropOverride(const Property<T>& prop) {
        propOverrides.erase(prop.id);
      }
  };
}
