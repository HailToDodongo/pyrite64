/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <array>
#include <string>

#include "../../../renderer/texture.h"

namespace Editor
{
  class AssetsBrowser
  {
    private:
      int activeTab{1};
      std::array<std::string, 4> tabDirs{};
      std::string searchFilter{};
      std::string selectedFolderPath{};
      int selectedSceneId{-1};
      std::string renamePath{};
      std::string deletePath{};
      char renameBuffer[256];

    public:
      void draw();
      void showContextMenu(const std::string& path, bool showOpenItem = true);
  };
}