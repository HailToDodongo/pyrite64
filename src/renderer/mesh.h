/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <SDL3/SDL.h>

#include "vertBuffer.h"
#include "vertex.h"
#include "../utils/aabb.h"
#include "glm/vec3.hpp"

namespace Renderer
{
  class Scene;

  class Mesh
  {
    private:
      Renderer::VertBuffer *vertBuff{nullptr};
      bool dataReady = false;
      Utils::AABB aabb{};

    public:
      std::vector<Renderer::Vertex> vertices{};
      std::vector<Renderer::LineVertex> vertLines{};
      std::vector<uint16_t> indices{};

      // meters per quantized vertex unit, used to report the AABB in meters
      float vertexScale{1.0f};

      void recreate(Renderer::Scene &scene, bool clearData = true);

      void draw(SDL_GPURenderPass* pass, uint32_t indexOffset = 0, uint32_t indexCount = 0);

      /// Bounds of the mesh in meters.
      const Utils::AABB& getAABB() const { return aabb; }

      Mesh();
      ~Mesh();
  };
}