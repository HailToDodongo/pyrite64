/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <SDL3/SDL.h>

#include "vertBuffer.h"
#include "vertex.h"
#include "glm/vec3.hpp"

namespace Renderer
{
  class Scene;

  class Mesh
  {
    private:
      Renderer::VertBuffer *vertBuff{nullptr};
      bool dataReady = false;

    public:
      std::vector<Renderer::Vertex> vertices{};

      void recreate(Renderer::Scene &scene);

      void addBinding(SDL_GPUBufferBinding &binding) const {
        if (vertBuff)vertBuff->addBinding(binding);
      }

      void draw(SDL_GPURenderPass* pass);

      Mesh();
      ~Mesh();
  };
}