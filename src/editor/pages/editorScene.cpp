/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorScene.h"

#include "IconsMaterialDesignIcons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "../actions.h"
#include "../undoRedo.h"
#include "../../context.h"

#define IMVIEWGUIZMO_IMPLEMENTATION 1
#include "ImGuizmo.h"
#include "ImViewGuizmo.h"
#include "../../utils/ringBuffer.h"
#include "../imgui/theme.h"

namespace
{
  constexpr float HEIGHT_TOP_BAR = 28.0f;
  constexpr float HEIGHT_STATUS_BAR = 24.0f;

  constinit bool preferencesOpen{false};
  constinit bool projectSettingsOpen{false};
  constinit Utils::RingBuffer<double, 16> fpsRingBuffer{};
}

Editor::Scene::Scene()
{
  Editor::Actions::registerAction(Editor::Actions::Type::OPEN_NODE_GRAPH, [this](const std::string& asset)
  {
    printf("OPEN_NODE_GRAPH action called with asset: %s\n", asset.c_str());
    if(!ctx.project)return false;
    auto assetEntry = ctx.project->getAssets().getEntryByUUID(std::stoull(asset));
    if(assetEntry) {
      nodeEditors.push_back(std::make_unique<NodeEditor>(assetEntry->getUUID()));
      return true;
    }
    return false;
  });
}

Editor::Scene::~Scene()
{
  Editor::Actions::registerAction(Editor::Actions::Type::OPEN_NODE_GRAPH, nullptr);
}

void Editor::Scene::draw()
{
  ImViewGuizmo::BeginFrame();
  ImGuizmo::BeginFrame();

  auto &io = ImGui::GetIO();
  auto viewport = ImGui::GetMainViewport();

  bool isRunning = ctx.isBuildOrRunning();

  ImGui::SetNextWindowPos({0, HEIGHT_TOP_BAR});
  ImGui::SetNextWindowSize({
    viewport->WorkSize.x,
    viewport->WorkSize.y - HEIGHT_TOP_BAR - HEIGHT_STATUS_BAR,
  });
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags host_window_flags = 0;
  host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
  host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
  ImGui::Begin("MAIN_DOCK", NULL, host_window_flags);
  ImGui::PopStyleVar(3);

  auto dockSpaceID = ImGui::GetID("DockSpace");
  dockSpaceID = ImGui::DockSpace(dockSpaceID, ImVec2(0.0f, 0.0f), 0, 0);
  ImGui::End();

  if(!dockSpaceInit)
  {
    dockSpaceInit = true;

    ImGui::DockBuilderRemoveNode(dockSpaceID); // Clear out existing layout
    ImGui::DockBuilderAddNode(dockSpaceID); // Add empty node
    ImGui::DockBuilderSetNodeSize(dockSpaceID, ImGui::GetMainViewport()->Size);

    dockLeftID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Left, 0.15f, nullptr, &dockSpaceID);
    dockRightID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Right, 0.25f, nullptr, &dockSpaceID);
    dockBottomID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Down, 0.25f, nullptr, &dockSpaceID);

    // Center
    ImGui::DockBuilderDockWindow("3D-Viewport", dockSpaceID);
    // ImGui::DockBuilderDockWindow("Node-Editor", dockSpaceID);

    // Left
    //ImGui::DockBuilderDockWindow("Project", dockLeftID);
    ImGui::DockBuilderDockWindow("Scene", dockLeftID);
    ImGui::DockBuilderDockWindow("Graph", dockLeftID);
    ImGui::DockBuilderDockWindow("Layers", dockLeftID);

    // Right
    ImGui::DockBuilderDockWindow("Asset", dockRightID);
    ImGui::DockBuilderDockWindow("Object", dockRightID);

    // Bottom
    ImGui::DockBuilderDockWindow("Files", dockBottomID);
    ImGui::DockBuilderDockWindow("Log", dockBottomID);
    ImGui::DockBuilderDockWindow("ROM", dockBottomID);

    ImGui::DockBuilderFinish(dockSpaceID);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
  ImGui::Begin("3D-Viewport");
    viewport3d.draw();
  ImGui::End();
  ImGui::PopStyleVar(1);

  std::vector<uint32_t> delIndices{};
  for(uint32_t i = 0; i < nodeEditors.size(); ++i) {
    auto &nodeEditor = nodeEditors[i];
    if (!nodeEditor->draw(dockSpaceID)) {
      if (nodeEditor->isDirty()) {
        pendingNodeEditorCloseUUID = nodeEditor->getAssetUUID();
        pendingNodeEditorClosePopup = true;
      } else {
        delIndices.push_back(i);
      }
    }
  }
  // Remove closed editors
  for(int32_t i = (int32_t)delIndices.size() - 1; i >= 0; --i) {
    nodeEditors.erase(nodeEditors.begin() + delIndices[i]);
  }

  if (pendingNodeEditorClosePopup) {
    ImGui::OpenPopup("Unsaved Node Graph");
    pendingNodeEditorClosePopup = false;
  }

  if (ImGui::BeginPopupModal("Unsaved Node Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    auto itEditor = std::find_if(nodeEditors.begin(), nodeEditors.end(), [&](const std::shared_ptr<NodeEditor> &editor) {
      return editor && editor->getAssetUUID() == pendingNodeEditorCloseUUID;
    });

    if (itEditor == nodeEditors.end()) {
      pendingNodeEditorCloseUUID = 0;
      ImGui::CloseCurrentPopup();
    } else {
      auto &editor = *itEditor;
      ImGui::Text("The node graph '%s' has unsaved changes.", editor->getName().c_str());
      ImGui::Spacing();
      if (ImGui::Button("Save", {100, 0})) {
        editor->save();
        nodeEditors.erase(itEditor);
        pendingNodeEditorCloseUUID = 0;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Discard", {100, 0})) {
        editor->discardUnsavedChanges();
        nodeEditors.erase(itEditor);
        pendingNodeEditorCloseUUID = 0;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", {100, 0})) {
        pendingNodeEditorCloseUUID = 0;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

  ImGui::Begin("Object");
    objectInspector.draw();
  ImGui::End();

  ImGui::Begin("Asset");
    assetInspector.draw();
  ImGui::End();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
  ImGui::Begin("Files");
  ImGui::PopStyleVar();
    assetsBrowser.draw();
  ImGui::End();

  if (ctx.project->getScenes().getLoadedScene()) {

    ImGui::Begin("Graph");
      sceneGraph.draw();
    ImGui::End();

    ImGui::Begin("Scene");
      sceneInspector.draw();
    ImGui::End();

    ImGui::Begin("Layers");
      layerInspector.draw();
    ImGui::End();

  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
  ImGui::Begin("Log");
  ImGui::PopStyleVar();;
    logWindow.draw();
  ImGui::End();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
  ImGui::Begin("ROM");
  ImGui::PopStyleVar();
    memoryDashboard.draw();
  ImGui::End();

  if (preferencesOpen) {
    constexpr ImVec2 windowSize{500,300};
    auto screenSize = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowPos({(screenSize.x - windowSize.x) / 2, (screenSize.y - windowSize.y) / 2}, ImGuiCond_Appearing, {0.0f, 0.0f});
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

    // Thick borders
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    ImGui::Begin(ICON_MDI_COG " Preferences", &preferencesOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    if (preferences.draw()) {
      preferencesOpen = false;
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);
  }

  if (projectSettingsOpen) {
    constexpr ImVec2 windowSize{500,300};
    auto screenSize = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowPos({(screenSize.x - windowSize.x) / 2, (screenSize.y - windowSize.y) / 2}, ImGuiCond_Appearing, {0.0f, 0.0f});
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

    // Thick borders
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    ImGui::Begin(ICON_MDI_FILE_COG_OUTLINE " Project Settings", &projectSettingsOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    if (projectSettings.draw()) {
      projectSettingsOpen = false;
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);
  }

  // Top bar
  ImGui::SetNextWindowPos({0,0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({io.DisplaySize.x, 4}, ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{8,6});
  if(ImGui::Begin("TOP_BAR", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking
  )) {
    if(ImGui::BeginMenuBar())
    {
      if(ImGui::BeginMenu("Project"))
      {
        if(ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_OUTLINE " Save")) {
          ctx.project->save();
          save();
        }
        if(ImGui::MenuItem(ICON_MDI_FILE_COG_OUTLINE " Settings"))projectSettingsOpen = true;
        if(ImGui::MenuItem(ICON_MDI_CLOSE " Close"))Actions::call(Actions::Type::PROJECT_CLOSE);
        ImGui::EndMenu();
      }

      // Edit Menu with undo/redo functionality including description
      if(ImGui::BeginMenu("Edit"))
      {
        auto& history = UndoRedo::getHistory();

        std::string undoText = ICON_MDI_UNDO " Undo";
        if (history.canUndo()) {
          undoText += " (" + history.getUndoDescription() + ")";
        }
        if(ImGui::MenuItem(undoText.c_str(), "Ctrl+Z", false, history.canUndo())) {
          history.undo();
        }

        std::string redoText = ICON_MDI_REDO " Redo";
        if (history.canRedo()) {
          redoText += " (" + history.getRedoDescription() + ")";
        }
        if(ImGui::MenuItem(redoText.c_str(), "Ctrl+Y", false, history.canRedo())) {
          history.redo();
        }

        if(ImGui::MenuItem(ICON_MDI_COG " Preferences", "Ctrl+."))preferencesOpen = true;

        ImGui::EndMenu();
      }

      if(ImGui::BeginMenu("Build"))
      {
        if(ImGui::MenuItem(ICON_MDI_HAMMER " Build"))Actions::call(Actions::Type::PROJECT_BUILD);
        if(ImGui::MenuItem(ICON_MDI_PLAY " Build & Run"))Actions::call(Actions::Type::PROJECT_BUILD, "run");
        if(ImGui::MenuItem("Clean"))Actions::call(Actions::Type::PROJECT_CLEAN);
        ImGui::EndMenu();
      }

      if(ImGui::BeginMenu("View"))
      {
        if(ImGui::MenuItem("Reset Layout"))dockSpaceInit = false;
        ImGui::EndMenu();
      }

      ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 40);

      const char* tooltip{};
      ImGui::PushFont(nullptr, 20.0f);
      if(isRunning){
        ImGui::BeginDisabled();
        ImGui::MenuItem(ICON_MDI_STOP);
        ImGui::EndDisabled();
      } else {
        ImGui::PushStyleColor(ImGuiCol_Text, {0.6f, 0.85f, 0.6f, 1.0f});
        if(ImGui::MenuItem(ICON_MDI_PLAY))Actions::call(Actions::Type::PROJECT_BUILD, "run");
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))tooltip = "Run (F12)";
        ImGui::PopStyleColor();
      }

      ImGui::PopFont();

      if(tooltip)ImGui::SetTooltip("%s", tooltip);

      ImGui::EndMenuBar();
    }
    ImGui::End();
  }
  ImGui::PopStyleVar();

  // Bottom Status bar
  ImGui::SetNextWindowPos({0, io.DisplaySize.y - HEIGHT_STATUS_BAR}, ImGuiCond_Always, {0.0f, 0.0f});
  ImGui::SetNextWindowSize({io.DisplaySize.x, HEIGHT_STATUS_BAR}, ImGuiCond_Always);
  ImGui::Begin("STATUS_BAR", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking
  );

  fpsRingBuffer.push((double)ctx.timeCpuSelf / 1000.0 / 1000.0);

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
  ImGui::PushFont(ImGui::Theme::getFontMono());
  ImVec4 perfColor{1.0f,1.0f,1.0f,0.4f};
  if (io.Framerate < 45) perfColor = {1.0f, 0.5f, 0.5f, 1.0f};
  ImGui::TextColored(perfColor, "%d FPS | History: %d/%d %s | CPU: %.2fms",
    (int)roundf(io.Framerate),
    UndoRedo::getHistory().getUndoCount(),
    UndoRedo::getHistory().getRedoCount(),
    Utils::byteSize(UndoRedo::getHistory().getMemoryUsage()).c_str(),
    fpsRingBuffer.average()
  );

  perfColor = {1.0f,1.0f,1.0f,0.4f};
  std::string txtInfo = "v" PYRITE_VERSION;
  #ifndef NDEBUG
    perfColor = {1.0f,1.0f,1.0f,0.6f};
    txtInfo += " [DEBUG]";
  #endif

  ImGui::SameLine();
  ImGui::SetCursorPosX(io.DisplaySize.x - 12 - ImGui::CalcTextSize(txtInfo.c_str()).x);
  ImGui::TextColored(perfColor, "%s", txtInfo.c_str());

  ImGui::PopFont();
  ImGui::End();

  // Global keyboard shortcuts
  if (!ImGui::GetIO().WantTextInput) {
    bool isCtrl = ImGui::GetIO().KeyCtrl;
    
    // Undo: Ctrl+Z
    if (isCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      UndoRedo::getHistory().undo();
    }
    
    // Redo: Ctrl+Y
    if (isCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
      UndoRedo::getHistory().redo();
    }

    // Preferences
    if (isCtrl && ImGui::IsKeyPressed(ImGuiKey_Period))preferencesOpen = true;
  }
}

void Editor::Scene::save()
{
  for(auto &nodeEditor : nodeEditors) {
    nodeEditor->save();
  }
  UndoRedo::getHistory().markSaved();
}
