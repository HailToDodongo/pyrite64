/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

#include "SDL3/SDL_gpu.h"

namespace Renderer
{
  class Shader
  {
    private:
      SDL_GPUDevice* gpuDevice{nullptr};
      SDL_GPUShader* shaderVert{nullptr};
      SDL_GPUShader* shaderFrag{nullptr};

    public:
      struct Config
      {
        const std::string& name{};
        uint32_t vertUboCount{0};
        uint32_t fragUboCount{0};
        uint32_t vertTexCount{0};
        uint32_t fragTexCount{0};
        uint32_t vertSboCount{0};
      };

      Shader(SDL_GPUDevice* device, const Config &conf);
      ~Shader();

      void setToPipeline(SDL_GPUGraphicsPipelineCreateInfo &pipelineInfo) const {
        pipelineInfo.vertex_shader = shaderVert;
        pipelineInfo.fragment_shader = shaderFrag;
      }
  };
}
