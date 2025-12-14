/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once

#include "mesh.h"
#include "shapes.h"
#include <set>
#include <vector>

namespace Coll
{
  class Scene {
    private:
      constexpr static uint32_t VOID_SPHERE_COUNT = 2;

      std::set<MeshInstance*> meshes{};
      std::vector<BCS*> collBCS{};

      CollInfo vsBCS(BCS &bcs, const fm_vec3_t &velocity, float deltaTime);

    public:
      uint64_t ticks{0};
      uint64_t ticksBVH{0};
      uint64_t raycastCount{0};

      void registerMesh(MeshInstance *mesh) {
        meshes.insert(mesh);
      }

      void unregisterMesh(MeshInstance *mesh) {
        meshes.erase(mesh);
      }

      void registerBCS(BCS *bcs) {
        collBCS.push_back(bcs);
      }

      void unregisterBCS(BCS *bcs) {
        for(auto it = collBCS.begin(); it != collBCS.end(); ++it) {
          if(*it == bcs)return (void)collBCS.erase(it);
        }
      }

      RaycastRes raycastFloor(const fm_vec3_t &pos);

      [[nodiscard]] const std::vector<BCS*> &getSpheres() const {
        return collBCS;
      }

      void update(float deltaTime);

      void debugDraw(bool showMesh, bool showSpheres);
  };
 }