/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "SDL3/SDL_gpu.h"
#include "../renderer/texture.h"

namespace Editor
{
  class Main
  {
    private:
      Renderer::Texture texTest;

    public:
      Main(SDL_GPUDevice* device);
      ~Main();

      void draw();
  };
}
