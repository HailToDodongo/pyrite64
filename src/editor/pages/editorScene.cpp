/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorScene.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "../actions.h"
#include "../../context.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../imgui/helper.h"

namespace
{
  constexpr float HEIGHT_TOP_BAR = 26.0f;
  constexpr float HEIGHT_STATUS_BAR = 32.0f;
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

    auto dockLeftID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Left, 0.25f, nullptr, &dockSpaceID);
    auto dockRightID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Right, 0.25f, nullptr, &dockSpaceID);
    auto dockBottomID = ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Down, 0.25f, nullptr, &dockSpaceID);

    ImGui::DockBuilderDockWindow("Project", dockLeftID);
    ImGui::DockBuilderDockWindow("Viewport", dockSpaceID);
    ImGui::DockBuilderDockWindow("Inspector", dockRightID);
    ImGui::DockBuilderDockWindow("Assets", dockBottomID);
    ImGui::DockBuilderFinish(dockSpaceID);
  }

  ImGui::Begin("Viewport");
    ImGui::Text("Viewport");
  ImGui::End();

  ImGui::Begin("Inspector");
    ImGui::Text("Inspector");
  ImGui::End();

  ImGui::Begin("Assets");
    ImGui::Text("Assets");
  ImGui::End();

  ImGui::Begin("Project");

  if (ImGui::CollapsingHeader("Project settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InpTable::start("Project settings");
    ImGui::InpTable::addString("Name", ctx.project->conf.name);
    ImGui::InpTable::addString("ROM-Name", ctx.project->conf.romName);
    ImGui::InpTable::end();
  }
  if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InpTable::start("Environment");
    ImGui::InpTable::addPath("Emulator", ctx.project->conf.pathEmu);
    ImGui::InpTable::addPath("Libdragon", ctx.project->conf.pathLibdragon, true);
    ImGui::InpTable::addPath("N64_INST", ctx.project->conf.pathN64Inst, true);
    ImGui::InpTable::end();
  }

  ImGui::End();

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
      if(ImGui::MenuItem("Save"))ctx.project->save();
      if(ImGui::MenuItem("Close"))Actions::call(Actions::Type::PROJECT_CLOSE);
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
