/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "parts/assetInspector.h"
#include "parts/assetsBrowser.h"
#include "parts/layerInspector.h"
#include "parts/logWindow.h"
#include "parts/memoryDashboard.h"
#include "parts/nodeEditor.h"
#include "parts/objectInspector.h"
#include "parts/projectSettings.h"
#include "parts/sceneGraph.h"
#include "parts/sceneInspector.h"
#include "parts/viewport3D.h"

namespace Editor
{
  class Scene
  {
    private:
      Viewport3D viewport3d{};

      // Editors
      std::vector<std::shared_ptr<NodeEditor>> nodeEditors{};
      ProjectSettings projectSettings{};
      AssetsBrowser assetsBrowser{};
      AssetInspector assetInspector{};
      SceneInspector sceneInspector{};
      LayerInspector layerInspector{};
      ObjectInspector objectInspector{};
      LogWindow logWindow{};
      MemoryDashboard memoryDashboard{};
      SceneGraph sceneGraph{};

      bool dockSpaceInit{false};
      ImGuiID dockLeftID;
      ImGuiID dockRightID;
      ImGuiID dockBottomID;

      uint64_t pendingNodeEditorCloseUUID{0};
      bool pendingNodeEditorClosePopup{false};

    public:
      Scene();
      ~Scene();

      void draw();
      void save();
  };
}
