/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <SDL3/SDL_gpu.h>

#include "imgui.h"

namespace Renderer
{
  class Texture
  {
    private:
      SDL_GPUDevice *gpuDevice{nullptr};
      SDL_GPUTexture* texture{nullptr};

      SDL_GPUTextureSamplerBinding texBinding{};

      int width{0};
      int height{0};

    public:
      Texture(SDL_GPUDevice* device, const std::string &imgPath, bool isMono = false, int rasterWidth = 0, int rasterHeight = 0);
      ~Texture();

      [[nodiscard]] int getWidth() const { return width; };
      [[nodiscard]] int getHeight() const { return height; };
      [[nodiscard]] SDL_GPUTexture* getGPUTex() const { return texture; };

      ImVec2 getSize(float scale = 1.0f) const {
        return {(float)width * scale, (float)height * scale};
      };

      void bind(SDL_GPURenderPass* pass);
  };
}
