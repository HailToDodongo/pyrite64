/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <SDL3/SDL_gpu.h>

namespace Renderer
{
  class Texture
  {
    private:
      SDL_GPUDevice *gpuDevice{nullptr};
      SDL_GPUTexture* texture{nullptr};

      int width{0};
      int height{0};

    public:
      Texture(SDL_GPUDevice* device, const std::string &imgPath);
      ~Texture();

      [[nodiscard]] int getWidth() const { return width; };
      [[nodiscard]] int getHeight() const { return height; };
      [[nodiscard]] SDL_GPUTexture* getGPUTex() const { return texture; };
  };
}
