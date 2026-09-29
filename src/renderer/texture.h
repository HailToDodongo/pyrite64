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
      std::string path{};
      bool isMono{false};
      int rasterWidth{0};
      int rasterHeight{0};

      int width{0};
      int height{0};

      // lazily filled by ensureUploaded()
      mutable SDL_GPUTexture* texture{nullptr};
      mutable SDL_GPUTextureSamplerBinding texBinding{};
      mutable bool uploadTried{false};

      // Decodes the source image into a BGRA32 surface, nullptr on failure (caller owns it).
      [[nodiscard]] SDL_Surface* decode() const;
      // Creates + uploads the GPU texture, only ever does work on the first call.
      void ensureUploaded() const;

    public:
      Texture(SDL_GPUDevice* device, const std::string &imgPath, bool isMono = false, int rasterWidth = 0, int rasterHeight = 0);
      ~Texture();

      Texture(const Texture&) = delete;
      Texture& operator=(const Texture&) = delete;

      [[nodiscard]] int getWidth() const { return width; };
      [[nodiscard]] int getHeight() const { return height; };

      // Note: triggers the GPU upload, don't call it for images you are not about to draw.
      [[nodiscard]] SDL_GPUTexture* getGPUTex() const { ensureUploaded(); return texture; };

      ImVec2 getSize(float scale = 1.0f) const {
        return {(float)width * scale, (float)height * scale};
      };

      void bind(SDL_GPURenderPass* pass);
  };
}
