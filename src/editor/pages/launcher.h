/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <vector>
#include "SDL3/SDL_gpu.h"
#include "../../renderer/texture.h"

namespace Editor
{
  struct ProjectEntry {
    std::string name;
    std::string path;
    std::string editorVersion;
    std::string lastModified;
  };

  class Launcher
  {
    private:
      Renderer::Texture texTitle;
      Renderer::Texture texBtnAdd;
      Renderer::Texture texBtnOpen;
      Renderer::Texture texBtnTool;
      Renderer::Texture texBG;
      std::vector<ProjectEntry> projectEntries;

    public:
      Launcher(SDL_GPUDevice* device);
      ~Launcher();

      void draw();
      void showContextMenu(const std::string& path);

  };
}
