/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <SDL3/SDL.h>

namespace Renderer
{
  class Framebuffer
  {
    private:
      SDL_GPUTextureCreateInfo texInfo{};
      SDL_GPUTexture* gpuTex{nullptr};
      SDL_GPUTexture* gpuTexDepth{nullptr};
      SDL_GPUColorTargetInfo targetInfo{};
      SDL_GPUDepthStencilTargetInfo depthTargetInfo{};

    public:
      Framebuffer();
      ~Framebuffer();

      void resize(uint32_t width, uint32_t height);

      [[nodiscard]] const SDL_GPUColorTargetInfo& getTargetInfo() const { return targetInfo; }
      [[nodiscard]] const SDL_GPUDepthStencilTargetInfo& getDepthTargetInfo() const { return depthTargetInfo; }
      [[nodiscard]] SDL_GPUTexture* getTexture() const { return gpuTex; }
  };
}
