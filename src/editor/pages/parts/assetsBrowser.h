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
      int activeTab{0};
      std::array<std::string, 4> tabDirs{};
      std::string searchFilter{};
      std::string renamePath{};
      std::string deletePath{};

    public:
      void draw();
      void showContextMenu(const std::string& path);
      void showInFileBrowser(const std::string& path);
  };
}