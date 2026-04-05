/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "mesh.h"
#include "texture.h"
#include "uniforms.h"
#include "../project/assets/model3d.h"
#include "tiny3d/tools/gltf_importer/src/structs.h"

namespace Project {
  class Object;

  namespace Component::Shared
  {
    struct MaterialInstance;
  }

  class AssetManager;
}

namespace Renderer
{
  class N64Mesh
  {
    public:
      struct MeshPart
      {
        uint32_t indicesOffset{0};
        uint32_t indicesCount{0};
        std::string materialName{};
        UniformN64Material material{};
        SDL_GPUTextureSamplerBinding texBindings[2]{};
      };
    private:

      Mesh mesh{};
      std::vector<MeshPart> parts{};
      bool loaded{false};
      Scene *scene{};

    public:
      struct ObjectRef
      {
        const std::vector<uint32_t> &partsIndices;
        const Project::Assets::Model3D *model;
        const Project::Component::Shared::MaterialInstance *matInstance;
        const Project::Object &obj;
        bool isCollision{false};
      };

      void fromT3DM(
        const Project::Assets::Model3D &model3d,
        Project::AssetManager &assetManager
      );

      void recreate(Scene &sc);
      void draw(
        SDL_GPURenderPass* pass, SDL_GPUCommandBuffer *cmdBuff, UniformsObject &uniforms,
        const ObjectRef &ref
      );

      const Utils::AABB& getAABB() const { return mesh.getAABB(); }
      bool isLoaded() const { return loaded; }
  };
}
