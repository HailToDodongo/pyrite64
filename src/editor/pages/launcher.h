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
    bool expand;
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
      bool expandAll;

    public:
      Launcher(SDL_GPUDevice* device);
      ~Launcher();

      void draw();
      void updateProjectEntries();
      void showHeaderContextMenu();
      void showProjectContextMenu(const std::string& path);

  };
}
