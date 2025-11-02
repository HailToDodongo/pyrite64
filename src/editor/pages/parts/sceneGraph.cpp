/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sceneGraph.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../imgui/helper.h"
#include "IconsMaterialDesignIcons.h"

namespace
{
  Project::Object* deleteObj{nullptr};

  void drawObjectNode(
    Project::Scene &scene, Project::Object &obj, bool keyDelete,
    bool parentEnabled = true
  )
  {
    ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
      | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesFull;

    if (obj.children.empty()) {
      flag |= ImGuiTreeNodeFlags_Leaf;
    }

    bool isSelected = ctx.selObjectUUID == obj.uuid;
    if (isSelected) {
      flag |= ImGuiTreeNodeFlags_Selected;
    }

    if (isSelected && obj.parent && keyDelete) {
      deleteObj = &obj;
    }

    auto nameID = obj.name + "##" + std::to_string(obj.uuid);
    bool isOpen = ImGui::TreeNodeEx(nameID.c_str(), flag);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      ctx.selObjectUUID = obj.uuid;
      ImGui::SetWindowFocus("Object");
      //ImGui::SetWindowFocus("Graph");
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ctx.selObjectUUID = obj.uuid;
      ImGui::OpenPopup("NodePopup");
    }


    if(obj.parent)
    {
      float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
      constexpr float buttonSize = 12;
      // Align right side
      ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x - buttonSize * 2 - spacing);

      if(!parentEnabled)ImGui::BeginDisabled();

      // Example: visibility checkbox
      ImGui::PushID(("vis_" + std::to_string(obj.uuid)).c_str());

      ImGui::IconToggle(obj.selectable, ICON_MDI_CURSOR_DEFAULT, ICON_MDI_CURSOR_DEFAULT_OUTLINE);
      ImGui::SameLine(0, spacing);
      ImGui::IconToggle(obj.enabled, ICON_MDI_CHECKBOX_MARKED, ICON_MDI_CHECKBOX_BLANK_OUTLINE);

      ImGui::PopID();

      if(!parentEnabled)ImGui::EndDisabled();
    }

    if(isOpen)
    {
      if (ImGui::BeginPopupContextItem("NodePopup"))
      {
        if (ImGui::MenuItem(ICON_MDI_PLUS_BOX_OUTLINE " Add Empty")) {
          ctx.selObjectUUID = scene.addObject(obj)->uuid;
        }

        if (obj.parent) {
          ImGui::Separator();
          if (ImGui::MenuItem(ICON_MDI_TRASH_CAN " Delete"))deleteObj = &obj;
        }
        ImGui::EndPopup();
      }

      for(auto &child : obj.children) {
        drawObjectNode(scene, *child, keyDelete, parentEnabled && obj.enabled);
      }

      ImGui::TreePop();
    }
  }
}

void Editor::SceneGraph::draw()
{
  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  bool isFocus = ImGui::IsWindowFocused();

  // Menu
  if(ImGui::BeginMenuBar())
  {
    if(ImGui::BeginMenu(ICON_MDI_PLUS_BOX_OUTLINE)) {
      if(ImGui::MenuItem("Empty")){}
      if(ImGui::MenuItem("Group")){}
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  // Graph
  //style.IndentSpacing
  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 18.0f);

  bool keyDelete = isFocus && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace));

  auto &root = scene->getRootObject();
  drawObjectNode(*scene, root, keyDelete);

  ImGui::PopStyleVar();

  if (deleteObj) {
    scene->removeObject(*deleteObj);
    deleteObj = nullptr;
  }
}
