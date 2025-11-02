/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sceneGraph.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../imgui/helper.h"
#include "IconsMaterialDesignIcons.h"
#include "imgui_internal.h"

namespace
{
  Project::Object* deleteObj{nullptr};

  struct DragDropTask {
    uint32_t sourceUUID{0};
    uint32_t targetUUID{0};
    bool isInsert{false};
  };

  DragDropTask dragDropTask{};

  bool DrawDropTarget(uint32_t& dragDropTarget, uint32_t uuid, float thickness = 2.0f, float hitHeight = 6.0f)
  {
    // Only show when drag-drop is active
    if (!ImGui::IsDragDropActive())
      return false;

    bool res = false;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
    float fullWidth = ImGui::GetContentRegionAvail().x;

    // Compute overlay position
    ImVec2 overlayStart = cursorScreen;
    overlayStart.y -= hitHeight / 2;
    ImVec2 overlayEnd = ImVec2(cursorScreen.x + fullWidth, cursorScreen.y + hitHeight);

    // Push a dummy cursor to draw hit zone *without affecting layout*
    ImGui::SetCursorScreenPos(overlayStart);
    ImGui::PushID(("drop_overlay_" + std::to_string(uuid)).c_str());
    ImGui::InvisibleButton("##dropzone", ImVec2(fullWidth, hitHeight));
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    if (hovered) {
      drawList->AddLine(
          ImVec2(overlayStart.x, overlayStart.y),
          ImVec2(overlayEnd.x, overlayStart.y),
          ImGui::GetColorU32(ImGuiCol_DragDropTarget),
          thickness
      );
    }

    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0,0,0,0));
    // Accept drag payload
    if (ImGui::BeginDragDropTarget())
    {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OBJECT"))
      {
        dragDropTarget = *((uint32_t*)payload->Data);
        res = true;
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::PopStyleColor();

    ImGui::PopID();

    ImGui::SetCursorScreenPos(cursorScreen);
    return res;
  }

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
      //ImGui::SetWindowFocus("Object");
      //ImGui::SetWindowFocus("Graph");
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ctx.selObjectUUID = obj.uuid;
      ImGui::OpenPopup("NodePopup");
    }

    if (obj.parent && ImGui::BeginDragDropSource())
    {
      ImGui::SetDragDropPayload("OBJECT", &obj.uuid, sizeof(obj.uuid));
      ImGui::EndDragDropSource();
    }

    if (obj.parent && obj.isGroup && ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OBJECT")) {
        dragDropTask.sourceUUID = *((uint32_t*)payload->Data);
        dragDropTask.targetUUID = obj.uuid;
        dragDropTask.isInsert = true;
      }
      ImGui::EndDragDropTarget();
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

    if(ImGui::IsDragDropActive()) {
      if(DrawDropTarget(dragDropTask.sourceUUID, obj.uuid)) {
        dragDropTask.targetUUID = obj.uuid;
      }
    }

    if(isOpen)
    {
      if (ImGui::BeginPopupContextItem("NodePopup"))
      {
        if(obj.isGroup)
        {
          if (ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Add Object")) {
            ctx.selObjectUUID = scene.addObject(obj)->uuid;
          }

          if (ImGui::MenuItem(ICON_MDI_VIEW_GRID_PLUS " Add Group")) {
            auto newObj = scene.addObject(obj);
            newObj->isGroup = true;
            ctx.selObjectUUID = newObj->uuid;
          }
        }

        if (obj.parent) {
          if(obj.isGroup)ImGui::Separator();
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

  dragDropTask = {};
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

  if(dragDropTask.sourceUUID && dragDropTask.targetUUID) {
    printf("dragDropTarget %08X -> %08X (%d)\n", dragDropTask.sourceUUID, dragDropTask.targetUUID, dragDropTask.isInsert);
  }

  if (deleteObj) {
    scene->removeObject(*deleteObj);
    deleteObj = nullptr;
  }
}
