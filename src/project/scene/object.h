/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "json.hpp"
#include "../../utils/aabb.h"
#include "../../utils/prop.h"
#include "../component/components.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

namespace Project
{
  class Scene;

  class Object
  {
    public:
      Object* parent{nullptr};

      std::string name{};
      uint32_t uuid{0};
      // Runtime-only object id. Assigned during the project build (Scene::assignRuntimeIds);
      // it is never set, generated or persisted in the editor. Do not rely on it outside of build.
      uint16_t runtimeId{};

      PROP_U64(uuidPrefab);

      PROP_VEC3(pos);
      PROP_QUAT(rot);
      PROP_VEC3(scale);

      bool proportionalScale{false};
      bool enabled{true};
      bool selectable{true};

      std::unordered_map<uint64_t, GenericValue> propOverrides{};

      std::vector<std::shared_ptr<Object>> children{};
      std::vector<Component::Entry> components{};

      explicit Object(Object& parent) : parent{&parent} {}
      Object() = default;

      void addComponent(int compID);
      void removeComponent(uint64_t uuid);

      nlohmann::json serialize() const;
      void deserialize(Scene *scene, nlohmann::json &doc);

      bool isPrefabInstance() const {
        return uuidPrefab.value != 0;
      }

      // Authoring targets the outermost cascade layer, the prefab instance being edited.
      // An override on a deeply nested object is stored on that instance with a
      // path-relative key the build resolves identically. Falls back to this object's own
      // map when no instance context is active, such as direct transform edits.
      template<typename T>
      void addPropOverride(const Property<T>& prop)
      {
        GenericValue genVal{};
        genVal.set<T>(prop.value);
        if(const auto *layer = PropScope::authorLayer()) {
          // Only the scoped key. The bare key is a different slot, the instance's own
          // placement, and must not be touched.
          auto *map = const_cast<std::unordered_map<uint64_t, GenericValue>*>(layer->overrides);
          (*map)[PropScope::combine(layer->pathHash, prop.id)] = genVal;
        } else {
          propOverrides[prop.id] = genVal;
        }
      }

      template<typename T>
      void removePropOverride(const Property<T>& prop) {
        if(const auto *layer = PropScope::authorLayer()) {
          const_cast<std::unordered_map<uint64_t, GenericValue>*>(layer->overrides)
            ->erase(PropScope::combine(layer->pathHash, prop.id));
        } else {
          propOverrides.erase(prop.id);
        }
      }

      template<typename T>
      bool hasPropOverride(const Property<T>& prop) const {
        if(const auto *layer = PropScope::authorLayer()) {
          // Only the scoped key. The bare key is a different slot, the instance's own
          // transform, so checking it would falsely report nested props as overridden.
          return layer->overrides->contains(PropScope::combine(layer->pathHash, prop.id));
        }
        return propOverrides.contains(prop.id);
      }

      Utils::AABB getLocalAABB() const {
        Utils::AABB aabb{};
        bool hasVolume = false;
        for (const auto &entry : components) {
          const auto &info = Component::TABLE[entry.id];
          if (!info.funcGetAABB) continue;
          PropScope::Dispatch dispatchScope(propOverrides, entry.uuid);
          Utils::AABB compAABB = info.funcGetAABB(const_cast<Object&>(*this), const_cast<Component::Entry&>(entry));
          aabb.addPoint(compAABB.min);
          aabb.addPoint(compAABB.max);
          hasVolume = true;
        }

        if(!hasVolume ||
           std::isinf(aabb.min.x) || std::isinf(aabb.min.y) || std::isinf(aabb.min.z) ||
           std::isinf(aabb.max.x) || std::isinf(aabb.max.y) || std::isinf(aabb.max.z)) {
          aabb.min = {-1,-1,-1};
          aabb.max = {1,1,1};
        }
        return aabb;
      }

      Utils::AABB getWorldAABB() {
        Utils::AABB aabb = getLocalAABB();
        glm::vec3 t = pos.resolve(propOverrides);
        glm::quat r = rot.resolve(propOverrides);
        glm::vec3 s = scale.resolve(propOverrides);
        aabb.transform(t, r, s);
        return aabb;
      }
  };
}
