/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "SDL3/SDL_gpu.h"
#include "../../renderer/texture.h"

namespace Editor
{
  class Launcher
  {
    private:
      Renderer::Texture texTitle;
      Renderer::Texture texBtnAdd;
      Renderer::Texture texBtnOpen;
      Renderer::Texture texBtnTool;
      Renderer::Texture texBG;

    public:
      Launcher(SDL_GPUDevice* device);
      ~Launcher();

      void draw();
  };
}
