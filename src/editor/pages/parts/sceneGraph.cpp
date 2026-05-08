/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sceneGraph.h"

#include <algorithm>
#include <string>
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../../context.h"
#include "../../imgui/helper.h"
#include "IconsMaterialDesignIcons.h"
#include "imgui_internal.h"
#include "../../undoRedo.h"
#include "../../selectionUtils.h"

namespace
{
  Project::Object* deleteObj{nullptr};
  bool deleteSelection{false};
  uint32_t renameObjectUUID{0};
  std::string renameBuffer{};
  bool startingRename{false};

  struct DragDropTask {
    uint32_t sourceUUID{0};
    uint32_t targetUUID{0};
    bool isInsert{false};
  };

  DragDropTask dragDropTask{};

  /**
   * Computes the horizontal area reserved for the controls at the right side of a row.
   *
   * @return Width that must remain free at the right side of the row.
   */
  float calcRightControlAreaWidth()
  {
    const int iconAmount = 2;
    const ImGuiStyle& style = ImGui::GetStyle();

    // Sum the width of all the buttons
    return ImGui::CalcTextSize(ICON_MDI_CURSOR_DEFAULT).x * iconAmount
      // Sum the width of margins between buttons
      + style.ItemInnerSpacing.x * (iconAmount - 1)
      // Keep a small buffer against the window edge
      + style.WindowPadding.x
      // Add the width of the scrollbar if not present
      + (ImGui::GetCurrentWindow()->ScrollbarY ? 0 : style.ScrollbarSize);
  }

  /**
   * Clears the current inline renaming state.
   */
  void clearRenaming()
  {
    renameObjectUUID = 0;
    renameBuffer.clear();
    startingRename = false;
  }

  /**
   * Starts inline renaming for an object.
   *
   * @param objectUUID UUID of the object to rename
   * @param objectName Current name
   */
  void startRenaming(uint32_t objectUUID)
  {
    // Get the scene to look for the object
    auto scene = ctx.project->getScenes().getLoadedScene();
    if (!scene) return;

    // Can find object with such UUID --> Start renaming
    if (const std::shared_ptr<Project::Object> theObject = scene->getObjectByUUID(objectUUID)) {
      renameObjectUUID = objectUUID;
      renameBuffer = theObject->name;
      startingRename = true;
    // Cannot find object with such UUID (selection may have gone stale between frames) --> Cancel renaming
    } else {
      clearRenaming();
    }
  }

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
      cursorScreen.x - 4_px,
      cursorScreen.y - (hitHeight / 2) + 3_px
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

  /**
   * Draws an inline rename text field on top of a scene-graph node label.
   *
   * The edit is confirmed on Enter or when the field loses focus, and cancelled with Escape.
   *
   * @param obj The scene object currently being renamed.
  */
  void drawRenameInput(Project::Object &obj, const ImVec2 &nodeRectMin)
  {
    const ImVec2 oldCursorPos = ImGui::GetCursorPos();

    // Anchor input to the tree label position
    ImVec2 renamePos = nodeRectMin;
    const ImGuiStyle& style = ImGui::GetStyle();
    renamePos.x += ImGui::GetTreeNodeToLabelSpacing() / 2 - style.FramePadding.x + 2;
    renamePos.y -= 1;
    ImGui::SetCursorScreenPos(renamePos);

    // Clamp input width to the usable row space so it stays inside the window
    float rightLimit = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - calcRightControlAreaWidth() - style.FramePadding.x;
    float inputWidth = rightLimit - ImGui::GetCursorScreenPos().x;
    if (inputWidth < 1_px)
      inputWidth = 1_px;

    // Is the first frame --> Focus input
    if (startingRename) {
      ImGui::SetKeyboardFocusHere();
      startingRename = false;
    }

    // Place input and read value
    ImGui::SetNextItemWidth(inputWidth);
    bool confirmRename = ImGui::InputText(
      ("##Rename" + std::to_string(obj.uuid)).c_str(),
      &renameBuffer,
      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
    );

    // Escape aborts rename
    bool cancelRename = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
    // Enter or losing focus commits name
    bool finishRename = confirmRename || ImGui::IsItemDeactivated();
    // Canceled --> Clear renaming
    if (cancelRename) {
      clearRenaming();
    // Finished renaming --> Commit name
    } else if (finishRename) {
      // Given new name --> Apply to object
      if (!renameBuffer.empty() && obj.name != renameBuffer) {
        obj.name = renameBuffer;
        Editor::UndoRedo::getHistory().markChanged("Edit object name");
      }
      clearRenaming();
    }

    ImGui::SetCursorPos(oldCursorPos);
  }

  void drawObjectNode(
    Project::Scene &scene, Project::Object &obj, bool keyDelete,
    bool parentEnabled = true
  )
  {
    ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
      | ImGuiTreeNodeFlags_OpenOnDoubleClick
      | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAllColumns;

    if (obj.children.empty()) {
      flag |= ImGuiTreeNodeFlags_Leaf;
    }

    bool isSelected = ctx.isObjectSelected(obj.uuid);
    if (isSelected) {
      flag |= ImGuiTreeNodeFlags_Selected;
    }

    if (isSelected && obj.parent && keyDelete) {
      deleteSelection = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 3_px));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

    std::string nameID{};
    if(obj.uuidPrefab.value) {
      nameID += ICON_MDI_PACKAGE_VARIANT_CLOSED " ";
    }
    bool gotComponentIcon = false;
    if (!obj.components.empty()) {
      const auto &compEntry = obj.components.front();
      if (compEntry.id >= 0 && (size_t)compEntry.id < Project::Component::TABLE.size()) {
        const auto &def = Project::Component::TABLE[compEntry.id];
        if (def.icon) {
          nameID += def.icon;
          gotComponentIcon = true;
        }
      }
    }
    if (!gotComponentIcon) {
      nameID += (obj.parent == nullptr)
        ? ICON_MDI_MOVIE_OPEN_OUTLINE " "
        : ICON_MDI_CUBE_OUTLINE " ";
    }
    nameID += obj.name + "##" + std::to_string(obj.uuid);

    bool isOpen = ImGui::TreeNodeEx(nameID.c_str(), flag);
    ImGui::PopStyleVar(2);
    ImVec2 nodeRectMin = ImGui::GetItemRectMin();

    bool nodeIsClicked = ImGui::IsItemHovered()
      && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
      && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    bool nodeIsDoubleClicked = ImGui::IsItemHovered()
      && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
      && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("NodePopup");
    }

    // Double-clicked a node --> Start renaming
    if (nodeIsDoubleClicked)
      startRenaming(obj.uuid);

    bool isRenaming = renameObjectUUID == obj.uuid;

    if (obj.parent && ImGui::BeginDragDropSource())
    {
      ImGui::SetDragDropPayload("OBJECT", &obj.uuid, sizeof(obj.uuid));
      ImGui::TextUnformatted(obj.name.c_str());
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

    // Is renaming the object node
    if (isRenaming)
      drawRenameInput(obj, nodeRectMin);

    if(obj.parent)
    {
      float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
      ImVec2 iconSize{16_px, 21_px};

      auto oldCursorPos = ImGui::GetCursorPos();

      float offsetRight = calcRightControlAreaWidth();
      ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - offsetRight);

      if(!parentEnabled)ImGui::BeginDisabled();

      ImGui::PushID(("vis_" + std::to_string(obj.uuid)).c_str());

      int clicked = 0;
      clicked |= ImGui::IconToggle(obj.selectable, ICON_MDI_CURSOR_DEFAULT, ICON_MDI_CURSOR_DEFAULT_OUTLINE, iconSize);
      ImGui::SetItemTooltip("%s Object Selection", obj.selectable ? "Disable" : "Enable");
      ImGui::SameLine(0, spacing);
      clicked |= ImGui::IconToggle(obj.enabled, ICON_MDI_CHECKBOX_MARKED, ICON_MDI_CHECKBOX_BLANK_OUTLINE, iconSize);
      ImGui::SetItemTooltip("%s Object", obj.enabled ? "Disable" : "Enable");

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
      bool isCtrlDown = ImGui::GetIO().KeyCtrl;
      if (isCtrlDown) {
        ctx.toggleObjectSelection(obj.uuid);
      } else {
        ctx.setObjectSelection(obj.uuid);
      }
      //ImGui::SetWindowFocus("Object");
      //ImGui::SetWindowFocus("Graph");
    }

    if(isOpen)
    {
      if (ImGui::BeginPopupContextItem("NodePopup"))
      {
        if (ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Add Object")) {
          auto added = scene.addObject(obj);
          if (added) {
            ctx.setObjectSelection(added->uuid);
            startRenaming(added->uuid);
          }
          Editor::UndoRedo::getHistory().markChanged("Add Object");
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
  deleteObj = nullptr;
  deleteSelection = false;
  bool isFocus = ImGui::IsWindowFocused();
  // While rename is active, shortcuts stay disabled, so the text field can own the keyboard input
  bool isRenaming = renameObjectUUID != 0;

  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0_px);
  bool keyDelete = isFocus && !isRenaming && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace));
  // F2 starts renaming the current object, matching common scene-tree/file-explorer behavior
  bool keyRename = isFocus && !isRenaming && ImGui::IsKeyPressed(ImGuiKey_F2);

  if (keyRename) {
    const std::vector<uint32_t> &selectedIds = ctx.getSelectedObjectUUIDs();
    // Inline renaming only makes sense for a single target; multi-select keeps its current state
    if (selectedIds.size() == 1) {
      // Rename the selected object
      startRenaming(selectedIds.front());
    }
  }

  auto &root = scene->getRootObject();
  drawObjectNode(*scene, root, keyDelete);

  ImGui::PopStyleVar(1);

  bool isCtrlDown = ImGui::GetIO().KeyCtrl;
  if (!isCtrlDown
      && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
      && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
      && !ImGui::IsAnyItemHovered()) {
    ctx.clearObjectSelection();
  }

  if(dragDropTask.sourceUUID && dragDropTask.targetUUID) {
    //printf("dragDropTarget %08X -> %08X (%d)\n", dragDropTask.sourceUUID, dragDropTask.targetUUID, dragDropTask.isInsert);
    bool moved = scene->moveObject(
      dragDropTask.sourceUUID,
      dragDropTask.targetUUID,
      dragDropTask.isInsert
    );

    // Could move --> Add to history
    if (moved)
      UndoRedo::getHistory().markChanged("Move Object");
  }

  if (deleteSelection || deleteObj) {
    if (deleteObj && !ctx.isObjectSelected(deleteObj->uuid)) {
      ctx.setObjectSelection(deleteObj->uuid);
    }

    UndoRedo::getHistory().markChanged("Delete Object");
    Editor::SelectionUtils::deleteSelectedObjects(*scene);
  }
}
