/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "storageBuffer.h"
#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

#include <string>
#include <tiny3d/tools/gltf_importer/src/math/quat.h>

namespace T3DM
{
  struct Bone;
}

namespace Project::Assets
{
  struct Model3D;
}

namespace Renderer
{
  class Skeleton
  {
    public:
      struct Bone
      {
        glm::vec3 pos{};
        glm::quat rot{};
        glm::vec3 scale{};
        uint32_t parentIdx{};

        void setRot(const Quat &r) {
          this->rot = {r.x(), r.y(), r.z(), r.w()};
        }
      };

    private:
      StorageBuffer storageBuff;
      std::vector<Bone> bones{};
      std::vector<glm::mat4> boneMats{};
      float importScale{1.0f};

      void readBone(const T3DM::Bone &bone, uint32_t parentIdx);

    public:
      explicit Skeleton(
        SDL_GPUDevice* device,
        const Project::Assets::Model3D &model,
        float importScale_
      );

      explicit Skeleton(SDL_GPUDevice* device);

      void update(SDL_GPUCopyPass& pass);
      void use(SDL_GPURenderPass *pass);

      Bone* getBone(uint32_t idx) {
        if (idx >= bones.size())return nullptr;
        return &bones[idx];
      }

      float getImportScale() const { return importScale; }
  };
}
