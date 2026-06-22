/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "SDL3/SDL_gpu.h"
#include "../../renderer/texture.h"

namespace Editor
{
  class Launcher
  {
    private:
      SDL_GPUDevice* gpuDevice{nullptr};

      Renderer::Texture texTitle;
      Renderer::Texture texBtnAdd;
      Renderer::Texture texBtnOpen;
      Renderer::Texture texBtnTool;
      Renderer::Texture texBtnCard;
      Renderer::Texture texBG;

      std::unordered_map<std::string, std::unique_ptr<Renderer::Texture>> cardImageCache;
      Renderer::Texture* getCardImage(const std::string &path);

    public:
      Launcher(SDL_GPUDevice* device);
      ~Launcher();

      void draw();
  };
}
