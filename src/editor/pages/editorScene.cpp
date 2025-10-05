/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorScene.h"

#include "IconsFontAwesome4.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "../actions.h"
#include "../../context.h"

namespace
{
  constexpr float HEIGHT_TOP_BAR = 26.0f;
  constexpr float HEIGHT_STATUS_BAR = 32.0f;

  constinit bool projectSettingsOpen{false};
}

void Editor::Scene::draw()
{
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

    auto dockLeftID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Left, 0.15f, nullptr, &dockSpaceID);
    auto dockRightID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Right, 0.25f, nullptr, &dockSpaceID);
    auto dockBottomID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Down, 0.25f, nullptr, &dockSpaceID);

    // Center
    ImGui::DockBuilderDockWindow("3D-Viewport", dockSpaceID);

    // Left
    //ImGui::DockBuilderDockWindow("Project", dockLeftID);
    ImGui::DockBuilderDockWindow("Scene", dockLeftID);

    // Right
    ImGui::DockBuilderDockWindow("Asset", dockRightID);
    ImGui::DockBuilderDockWindow("Object", dockRightID);

    // Bottom
    ImGui::DockBuilderDockWindow("Scenes", dockBottomID);
    ImGui::DockBuilderDockWindow("Assets", dockBottomID);
    ImGui::DockBuilderDockWindow("Log", dockBottomID);


    ImGui::DockBuilderFinish(dockSpaceID);
  }

  ImGui::Begin("3D-Viewport");
    viewport3d.draw();
  ImGui::End();

  //if (ctx.selAssetUUID) {
  ImGui::Begin("Asset");
    assetInspector.draw();
  ImGui::End();
  //}

  ImGui::Begin("Object");
    ImGui::Text("Objects");
  ImGui::End();

  ImGui::Begin("Scenes");
    sceneBrowser.draw();
  ImGui::End();

  if (ctx.project->getScenes().getLoadedScene()) {
    ImGui::Begin("Scene");
    sceneInspector.draw();
    ImGui::End();
  }

  ImGui::Begin("Assets");
    assetsBrowser.draw();
  ImGui::End();

  ImGui::Begin("Log");
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
    ImGui::Begin(ICON_FA_COG " Project Settings", &projectSettingsOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    if (projectSettings.draw()) {
      projectSettingsOpen = false;
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);
  }

  // Top bar
  ImGui::SetNextWindowPos({0,0}, ImGuiCond_Appearing, {0.0f, 0.0f});
  ImGui::Begin("TOP_BAR", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking
  );

  if(ImGui::BeginMenuBar())
  {
    if(ImGui::BeginMenu("Project"))
    {
      if(ImGui::MenuItem(ICON_FA_FLOPPY_O " Save"))ctx.project->save();
      if(ImGui::MenuItem(ICON_FA_COG " Settings"))projectSettingsOpen = true;
      if(ImGui::MenuItem(ICON_FA_SIGN_OUT " Close"))Actions::call(Actions::Type::PROJECT_CLOSE);
      ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Build"))
    {
      if(ImGui::MenuItem(ICON_FA_BUILDING " Build"))Actions::call(Actions::Type::PROJECT_BUILD);
      ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("View"))
    {
      if(ImGui::MenuItem("Reset Layout"))dockSpaceInit = false;
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
  ImGui::Text("%.2f FPS", io.Framerate);
  ImGui::End();
}
