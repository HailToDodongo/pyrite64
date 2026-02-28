/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "../../../project/project.h"
#include "../../../project/graph/graph.h"

namespace Editor
{
  class NodeEditor
  {
    private:
      Project::AssetManagerEntry *currentAsset{nullptr};
      Project::Graph::Graph graph{};
      std::string name{};
      std::string savedState{};
      std::string trackedDirtyState{};
      bool dirty{false};
      bool isInit{false};

    public:
      NodeEditor(uint64_t assetUUID);
      ~NodeEditor();
      bool draw(ImGuiID defDockId);
      void save();
      void discardUnsavedChanges();

      [[nodiscard]] bool isDirty() const { return dirty; }
      [[nodiscard]] uint64_t getAssetUUID() const { return currentAsset ? currentAsset->getUUID() : 0; }
      [[nodiscard]] const std::string &getName() const { return name; }
  };
}
