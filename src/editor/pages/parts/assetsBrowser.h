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
      static uint64_t pendingPrefabFocusUUID;

    public:
      /**
       * Requests that the Prefabs view displays and selects a prefab.
       * @param prefabUUID UUID of the prefab to focus.
       */
      static void focusPrefab(uint64_t prefabUUID);
      void draw();
      void showContextMenu(const std::string& path, bool showOpenItem = true);
  };
}