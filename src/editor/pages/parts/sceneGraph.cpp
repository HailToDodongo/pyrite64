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
#include "../../undoRedo.h"

namespace
{
  Project::Object* deleteObj{nullptr};

  struct DragDropTask {
    uint32_t sourceUUID{0};
    uint32_t targetUUID{0};
    bool isInsert{false};
  };

  DragDropTask dragDropTask{};

  bool DrawDropTarget(uint32_t& dragDropTarget, uint32_t uuid, float thickness = 2.0f, float hitHeight = 8.0f)
  {
    // Only show when drag-drop is active
    if (!ImGui::IsDragDropActive())
      return false;

    bool res = false;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
    float fullWidth = ImGui::GetContentRegionAvail().x;

    // Compute overlay position
    ImVec2 overlayStart{
      cursorScreen.x - 4,
      cursorScreen.y - (hitHeight / 2) + 3
    };
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
      | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesFull
      | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAllColumns;

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

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 3.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

    std::string nameID{};
    if(obj.uuidPrefab.value) {
      nameID += ICON_MDI_PACKAGE_VARIANT_CLOSED " ";
    }
    nameID += obj.name + "##" + std::to_string(obj.uuid);

    bool isOpen = ImGui::TreeNodeEx(nameID.c_str(), flag);
    ImGui::PopStyleVar(2);

    bool nodeIsClicked = ImGui::IsItemHovered()
      && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
      && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("NodePopup");
    }

    if (obj.parent && ImGui::BeginDragDropSource())
    {
      ImGui::SetDragDropPayload("OBJECT", &obj.uuid, sizeof(obj.uuid));
      ImGui::EndDragDropSource();
    }

    if (obj.parent && ImGui::BeginDragDropTarget()) {
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
      ImVec2 iconSize{16, 21};

      auto oldCursorPos = ImGui::GetCursorPos();
      ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x - buttonSize * 2 - spacing);

      if(!parentEnabled)ImGui::BeginDisabled();

      ImGui::PushID(("vis_" + std::to_string(obj.uuid)).c_str());

      int clicked = 0;
      clicked |= ImGui::IconToggle(obj.selectable, ICON_MDI_CURSOR_DEFAULT, ICON_MDI_CURSOR_DEFAULT_OUTLINE, iconSize);
      ImGui::SameLine(0, spacing);
      clicked |= ImGui::IconToggle(obj.enabled, ICON_MDI_CHECKBOX_MARKED, ICON_MDI_CHECKBOX_BLANK_OUTLINE, iconSize);

      if(clicked)nodeIsClicked = false;

      ImGui::PopID();

      if(!parentEnabled)ImGui::EndDisabled();
      ImGui::SetCursorPosY(oldCursorPos.y);
    }

    if(ImGui::IsDragDropActive()) {
      if(DrawDropTarget(dragDropTask.sourceUUID, obj.uuid)) {
        dragDropTask.targetUUID = obj.uuid;
      }
    }

    if (nodeIsClicked) {
      ctx.selObjectUUID = obj.uuid;
      //ImGui::SetWindowFocus("Object");
      //ImGui::SetWindowFocus("Graph");
    }

    if(isOpen)
    {
      if (ImGui::BeginPopupContextItem("NodePopup"))
      {
        if (ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Add Object")) {
          Editor::UndoRedo::SnapshotScope snapshot(Editor::UndoRedo::getHistory(), "Add Object");
          auto added = scene.addObject(obj);
          if (added) {
            ctx.selObjectUUID = added->uuid;
          }
        }

        if (obj.parent) {
          if (!obj.isPrefabInstance() && ImGui::MenuItem(ICON_MDI_PACKAGE_VARIANT_CLOSED_PLUS " To Prefab")) {
            scene.createPrefabFromObject(obj.uuid);
          }

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

  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);
  bool keyDelete = isFocus && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace));

  auto &root = scene->getRootObject();
  drawObjectNode(*scene, root, keyDelete);

  ImGui::PopStyleVar(1);

  if(dragDropTask.sourceUUID && dragDropTask.targetUUID) {
    //printf("dragDropTarget %08X -> %08X (%d)\n", dragDropTask.sourceUUID, dragDropTask.targetUUID, dragDropTask.isInsert);
    Editor::UndoRedo::SnapshotScope snapshot(Editor::UndoRedo::getHistory(), "Move Object");
    scene->moveObject(
      dragDropTask.sourceUUID,
      dragDropTask.targetUUID,
      dragDropTask.isInsert
    );
  }

  if (deleteObj) {
    Editor::UndoRedo::SnapshotScope snapshot(Editor::UndoRedo::getHistory(), "Delete Object");
    scene->removeObject(*deleteObj);
    deleteObj = nullptr;
  }
}
