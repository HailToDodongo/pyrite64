/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <SDL3/SDL.h>

#include "glm/vec4.hpp"

namespace Renderer
{
  SDL_GPUTextureFormat getDepthStencilFormat();

  class Framebuffer
  {
    private:
      SDL_GPUTextureCreateInfo texInfo{};
      SDL_GPUTexture* gpuTex{nullptr};
      SDL_GPUTexture* gpuTexObj{nullptr};
      SDL_GPUTexture* gpuTexDepth{nullptr};
      std::array<SDL_GPUColorTargetInfo, 2> targetInfo{};
      SDL_GPUDepthStencilTargetInfo depthTargetInfo{};

      SDL_GPUTransferBuffer *transBufferRead{nullptr};

      void* startGenericRead(uint32_t x, uint32_t y);
      void endGenericRead();

    public:
      Framebuffer();
      ~Framebuffer();

      void setClearColor(const glm::vec4 &color) {
        targetInfo[0].clear_color.r = color.r;
        targetInfo[0].clear_color.g = color.g;
        targetInfo[0].clear_color.b = color.b;
        targetInfo[0].clear_color.a = color.a;
      }

      void resize(uint32_t width, uint32_t height);

      uint32_t getWidth() const { return texInfo.width; }
      uint32_t getHeight() const { return texInfo.height; }

      [[nodiscard]] const SDL_GPUColorTargetInfo *getTargetInfo() const { return targetInfo.data(); }
      [[nodiscard]] uint32_t getTargetInfoCount() const { return targetInfo.size(); }

      [[nodiscard]] const SDL_GPUDepthStencilTargetInfo& getDepthTargetInfo() const { return depthTargetInfo; }
      [[nodiscard]] SDL_GPUTexture* getTexture() const { return gpuTex; }

      glm::u8vec4 readColor(uint32_t x, uint32_t y);
      uint32_t readObjectID(uint32_t x, uint32_t y);
  };
}
