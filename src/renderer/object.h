/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>

#include "mesh.h"
#include "uniforms.h"

namespace Renderer
{
  class Object
  {
    private:
      std::shared_ptr<Mesh> mesh{nullptr};
      glm::vec3 pos{0,0,0};
      bool transformDirty{true};

    public:
      UniformsObject uniform{};

      void setObjectID(uint32_t id) {
        uniform.objectID = id;
      }
      void setMesh(const std::shared_ptr<Mesh>& m) { mesh = m; }

      void setPos(const glm::vec3& p) { pos = p; transformDirty = true; }

      void draw(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* cmdBuff);
  };
}
