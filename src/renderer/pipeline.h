/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <vector>
#include <SDL3/SDL.h>

#include "shader.h"

namespace Renderer
{
  class Pipeline
  {
    private:
      SDL_GPUGraphicsPipeline* pipeline{nullptr};

    public:
      struct InfoVertDef
      {
        SDL_GPUVertexElementFormat format{};
        uint32_t offset{};
      };

      struct Info
      {
        const Renderer::Shader &shader;
        SDL_GPUPrimitiveType prim{};
        bool depthTest{true};
        bool depthWrite{true};
        SDL_GPUCompareOp depthCompare{SDL_GPU_COMPAREOP_LESS};
        bool drawsObjID{};
        bool translucent{};
        uint32_t vertPitch{};
        std::vector<InfoVertDef> vertLayout{};
      };

      explicit Pipeline(const Info &info);
      ~Pipeline();

      // SDL_GPUGraphicsPipeline* getPipeline() const { return pipeline; }

      void bind(SDL_GPURenderPass* pass) const {
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
      }
  };
}
