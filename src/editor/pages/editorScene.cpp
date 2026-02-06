/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorScene.h"

#include "IconsMaterialDesignIcons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "../actions.h"
#include "../../context.h"
#include "../imgui/lang.h"

#define IMVIEWGUIZMO_IMPLEMENTATION 1
#include "ImGuizmo.h"
#include "ImViewGuizmo.h"

namespace
{
  constexpr float HEIGHT_TOP_BAR = 26.0f;
  constexpr float HEIGHT_STATUS_BAR = 32.0f;

  constinit bool projectSettingsOpen{false};
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
    ImGui::DockBuilderDockWindow(message(MSG_TAB_3D_VIEWPORT), dockSpaceID);
    // ImGui::DockBuilderDockWindow("Node-Editor", dockSpaceID);

    // Left
    //ImGui::DockBuilderDockWindow("Project", dockLeftID);
    ImGui::DockBuilderDockWindow(message(MSG_TAB_SCENE), dockLeftID);
    ImGui::DockBuilderDockWindow(message(MSG_TAB_GRAPH), dockLeftID);
    ImGui::DockBuilderDockWindow(message(MSG_TAB_LAYERS), dockLeftID);

    // Right
    ImGui::DockBuilderDockWindow(message(MSG_TAB_ASSET), dockRightID);
    ImGui::DockBuilderDockWindow(message(MSG_TAB_OBJECT), dockRightID);

    // Bottom
    ImGui::DockBuilderDockWindow(message(MSG_TAB_FILES), dockBottomID);
    ImGui::DockBuilderDockWindow(message(MSG_TAB_LOG), dockBottomID);

    ImGui::DockBuilderFinish(dockSpaceID);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
  ImGui::Begin(message(MSG_TAB_3D_VIEWPORT));
    viewport3d.draw();
  ImGui::End();
  ImGui::PopStyleVar(1);

  std::vector<uint32_t> delIndices{};
  for(uint32_t i = 0; i < nodeEditors.size(); ++i) {
    auto &nodeEditor = nodeEditors[i];
    if (!nodeEditor->draw(dockSpaceID)) {
      delIndices.push_back(i);
    }
  }
  // Remove closed editors
  for(int32_t i = (int32_t)delIndices.size() - 1; i >= 0; --i) {
    nodeEditors.erase(nodeEditors.begin() + delIndices[i]);
  }

  ImGui::Begin(message(MSG_TAB_OBJECT));
    objectInspector.draw();
  ImGui::End();

  ImGui::Begin(message(MSG_TAB_ASSET));
    assetInspector.draw();
  ImGui::End();

  ImGui::Begin(message(MSG_TAB_FILES));
    assetsBrowser.draw();
  ImGui::End();

  if (ctx.project->getScenes().getLoadedScene()) {

    ImGui::Begin(message(MSG_TAB_GRAPH));
      sceneGraph.draw();
    ImGui::End();

    ImGui::Begin(message(MSG_TAB_SCENE));
      sceneInspector.draw();
    ImGui::End();

    ImGui::Begin(message(MSG_TAB_LAYERS));
      layerInspector.draw();
    ImGui::End();

  }

  ImGui::Begin(message(MSG_TAB_LOG));
    logWindow.draw();
  ImGui::End();


  if (projectSettingsOpen) {
    constexpr ImVec2 windowSize{500,300};
    auto screenSize = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowPos({(screenSize.x - windowSize.x) / 2, (screenSize.y - windowSize.y) / 2}, ImGuiCond_Appearing, {0.0f, 0.0f});
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

    // Thick borders
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    ImGui::Begin((std::string{ICON_MDI_COG} + message(MSG_PROJECT_SETTINGS_TITLE)).c_str(), &projectSettingsOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    if (projectSettings.draw()) {
      projectSettingsOpen = false;
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);
  }

  // Top bar
  ImGui::SetNextWindowSize({io.DisplaySize.x, HEIGHT_TOP_BAR}, ImGuiCond_Always);
  ImGui::SetNextWindowPos({0,0}, ImGuiCond_Appearing, {0.0f, 0.0f});
  ImGui::Begin("TOP_BAR", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking
  );

  if(ImGui::BeginMenuBar())
  {
    if(ImGui::BeginMenu(message(MSG_MENUBAR_PROJECT)))
    {
      if(ImGui::MenuItem((std::string{ICON_MDI_CONTENT_SAVE_OUTLINE} + message(MSG_MENUBAR_PROJECT_SAVE)).c_str()))ctx.project->save();
      if(ImGui::MenuItem((std::string{ICON_MDI_COG} + message(MSG_MENUBAR_PROJECT_SETTINGS)).c_str()))projectSettingsOpen = true;
      if(ImGui::MenuItem((std::string{ICON_MDI_CLOSE} + message(MSG_MENUBAR_PROJECT_CLOSE)).c_str()))Actions::call(Actions::Type::PROJECT_CLOSE);
      ImGui::EndMenu();
    }

    if(ImGui::BeginMenu(message(MSG_MENUBAR_BUILD)))
    {
      if(ImGui::MenuItem((std::string{ICON_MDI_PLAY} + message(MSG_MENUBAR_BUILD_BUILD)).c_str()))Actions::call(Actions::Type::PROJECT_BUILD);
      if(ImGui::MenuItem(message(MSG_MENUBAR_BUILD_CLEAN)))Actions::call(Actions::Type::PROJECT_CLEAN);
      ImGui::EndMenu();
    }

    if(ImGui::BeginMenu(message(MSG_MENUBAR_VIEW)))
    {
      if(ImGui::MenuItem(message(MSG_MENUBAR_VIEW_RESET)))dockSpaceInit = false;
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }
  ImGui::End();

  // Bottom Status bar
  ImGui::SetNextWindowPos({0, io.DisplaySize.y - 32}, ImGuiCond_Always, {0.0f, 0.0f});
  ImGui::SetNextWindowSize({io.DisplaySize.x, 32}, ImGuiCond_Always);
  ImGui::Begin("STATUS_BAR", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking
  );

  int fps = (int)roundf(io.Framerate);
  ImGui::Text("%d FPS", (int)roundf(io.Framerate));
  ImGui::End();
}

void Editor::Scene::save()
{
  for(auto &nodeEditor : nodeEditors) {
    nodeEditor->save();
  }
}
