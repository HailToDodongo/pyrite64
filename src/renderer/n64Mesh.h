/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "mesh.h"
#include "uniforms.h"
#include "tiny3d/tools/gltf_importer/src/structs.h"

namespace Renderer
{
  class N64Mesh
  {
    private:
      struct MeshPart
      {
        uint32_t indicesOffset{0};
        uint32_t indicesCount{0};
        UniformN64Material material{};
      };

      Mesh mesh{};
      std::vector<MeshPart> parts{};

    public:
      void fromT3DM(const T3DMData &t3dmData);

      void recreate(Renderer::Scene &scene);
      void draw(SDL_GPURenderPass* pass, UniformsObject &uniforms);

      const Utils::AABB& getAABB() const { return mesh.getAABB(); }
  };
}
