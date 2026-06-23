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
#include "parts/preferenceOverlay.h"
#include "parts/projectSettings.h"
#include "parts/sceneGraph.h"
#include "parts/sceneInspector.h"
#include "parts/viewport3D.h"

namespace Editor
{
  class ModelEditor;

  class Scene
  {
    private:
      std::vector<std::shared_ptr<Viewport3D>> viewports{};
      // Closed viewports are kept alive here until the next frame: their framebuffer texture
      // is still referenced by this frame's ImGui draw list, which renders after draw() returns.
      std::vector<std::shared_ptr<Viewport3D>> viewportsPendingClose{};
      uint32_t nextViewportWinId{0};
      std::shared_ptr<Viewport3D> hoveredViewport{};
      bool wantNewViewport{false};
      bool wantResetLayout{false};

      void addViewport();

      // Editors
      std::vector<std::shared_ptr<NodeEditor>> nodeEditors{};
      std::map<uint64_t, std::shared_ptr<ModelEditor>> modelEditors{};
      PreferenceOverlay prefOverlay{};
      ProjectSettings projectSettings{};
      AssetsBrowser assetsBrowser{};
      AssetInspector assetInspector{};
      SceneInspector sceneInspector{};
      LayerInspector layerInspector{};
      ObjectInspector objectInspector{};
      LogWindow logWindow{};
      MemoryDashboard memoryDashboard{};
      SceneGraph sceneGraph{};

      ImGuiID dockLeftID;
      ImGuiID dockRightID;
      ImGuiID dockBottomID;

      uint64_t pendingNodeEditorCloseUUID{0};
      bool pendingNodeEditorClosePopup{false};

      // Which model/graph windows were open, remembered per project (keyed by project path)
      // so switching projects never restores another project's windows.
      struct WindowSet { std::vector<uint64_t> models{}; std::vector<uint64_t> graphs{}; };
      std::map<std::string, WindowSet> sessionWindows{};
      // Path of the project whose windows are currently restored ("" = none yet).
      std::string restoredForProject{};

      void loadSession();
      void saveSession();
      void restoreWindows();
      void closeAllEditors();
      // Save the currently-open windows for the active project.
      void persistOpenWindows();

    public:
      Scene();
      ~Scene();

      void openModelEditor(uint64_t assetUUID);

      // Call before the active project is torn down (switch/close/exit): captures its open
      // windows while still valid, then closes the editors so they can't leak into the next.
      void onProjectClosing();

      void draw();
      void save();
  };
}
