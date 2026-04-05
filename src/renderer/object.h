/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>

#include "mesh.h"
#include "n64Mesh.h"
#include "uniforms.h"
#include "../project/component/shared/materialInstance.h"

namespace Renderer
{
  class Object
  {
    private:
      std::shared_ptr<Mesh> mesh{nullptr};
      std::shared_ptr<N64Mesh> n64Mesh{nullptr};

      glm::vec3 pos{0,0,0};
      float scale{1.0f};
      bool transformDirty{true};

    public:
      UniformsObject uniform{};

      void setObjectID(uint32_t id) {
        uniform.objectID = id;
      }

      void setMesh(const std::shared_ptr<Mesh>& m) { mesh = m; n64Mesh = nullptr; }
      void setMesh(const std::shared_ptr<N64Mesh>& m) { n64Mesh = m; mesh = nullptr; }
      void removeMesh() { mesh = nullptr; n64Mesh = nullptr; }

      bool isMeshLoaded() const {
        if (mesh)return true;
        if (n64Mesh)return n64Mesh->isLoaded();
        return false;
      }

      void setPos(const glm::vec3& p) { pos = p; transformDirty = true; }
      void setScale(float s) { scale = s; transformDirty = true; }

      void draw(
        SDL_GPURenderPass* pass,
        SDL_GPUCommandBuffer* cmdBuff,
        const N64Mesh::ObjectRef *ref = nullptr
      );

      void draw(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* cmdBuff, const N64Mesh::ObjectRef &ref) {
        draw(pass, cmdBuff, &ref);
      }
  };
}
