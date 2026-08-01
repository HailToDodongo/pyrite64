/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sceneGraph.h"
#include "assetsBrowser.h"

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

  // Filters the tree by object name; empty means no filtering
  std::string searchFilter{};

  // Set per-frame at the start of draw(). When non-null a prefab is being edited and
  // selection is restricted to its own definition, with everything else dimmed and inert.
  Project::Object* prefabEditObj{nullptr};

  struct DragDropTask {
    uint32_t sourceUUID{0};
    uint32_t targetUUID{0};
    bool isInsert{false};
  };

  DragDropTask dragDropTask{};

  struct AssetDropTask {
    uint64_t assetUUID{0};
    uint32_t targetUUID{0};
    bool asChild{false};
  };

  AssetDropTask assetDropTask{};
  ImVec2 lastInsertLineStart{};
  ImVec2 lastInsertLineEnd{};
  bool hasInsertLine{false};

  /**
   * Accepts a prefab or 3D model asset and records where its scene object should be created.
   * @param targetUUID Destination object UUID, or zero to add at the scene root.
   * @param asChild Whether the new instance should become a child of the target.
   */
  void acceptSceneAssetDrop(uint32_t targetUUID, bool asChild)
  {
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType("ASSET")) return;

    uint64_t assetUUID = *static_cast<const uint64_t*>(payload->Data);
    auto asset = ctx.project->getAssets().getEntryByUUID(assetUUID);
    if (!asset || (asset->type != Project::FileType::PREFAB
        && asset->type != Project::FileType::MODEL_3D)) return;

    if (ImGui::AcceptDragDropPayload("ASSET")) {
      assetDropTask.assetUUID = assetUUID;
      assetDropTask.targetUUID = targetUUID;
      assetDropTask.asChild = asChild;
    }
  }

  /**
   * Builds the icon prefix shown before the node name in the scene tree.
   *
   * The prefix may contain the prefab marker plus either the first component icon or a fallback icon.
   *
   * @param obj Scene object whose tree-row icons will be generated.
   * @return Concatenated icon string displayed before the node name.
   */
  std::string getNodeIcons(const Project::Object &obj)
  {
    std::string prefix{};

    // Is a prefab --> Add prefab icon
    if(obj.uuidPrefab.value)
      prefix += ICON_MDI_PACKAGE_VARIANT_CLOSED " ";

    bool gotComponentIcon = false;
    // The object has components
    if (!obj.components.empty()) {
      // Reuse the first component icon so the node hints at its main role
      const Project::Component::Entry &compEntry = obj.components.front();

      // Is a valid component
      if (compEntry.id >= 0 && (size_t)compEntry.id < Project::Component::TABLE.size()) {
        const Project::Component::CompInfo &def = Project::Component::TABLE[compEntry.id];

        // The component has a custom icon --> Use it
        if (def.icon) {
          prefix += def.icon;
          gotComponentIcon = true;
        }
      }
    }

    // Couldn't get a component icon --> Fall back to a root icon or a generic cube icon
    if (!gotComponentIcon) {
      prefix += (obj.parent == nullptr)
        ? ICON_MDI_MOVIE_OPEN_OUTLINE " "
        : ICON_MDI_CUBE_OUTLINE " ";
    }

    return prefix;
  }

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
    lastInsertLineStart = {overlayStart.x, overlayStart.y};
    lastInsertLineEnd = {overlayEnd.x, overlayStart.y};
    hasInsertLine = true;

    // Push a dummy cursor to draw hit zone *without affecting layout*
    ImGui::SetCursorScreenPos(overlayStart);
    ImGui::PushID(("drop_overlay_" + std::to_string(uuid)).c_str());
    ImGui::InvisibleButton("##dropzone", ImVec2(fullWidth, hitHeight));
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
    bool acceptsPayload = activePayload && activePayload->IsDataType("OBJECT");
    if (activePayload && activePayload->IsDataType("ASSET")) {
      uint64_t assetUUID = *static_cast<const uint64_t*>(activePayload->Data);
      auto asset = ctx.project->getAssets().getEntryByUUID(assetUUID);
      acceptsPayload = asset && (asset->type == Project::FileType::PREFAB
        || asset->type == Project::FileType::MODEL_3D);
    }

    if (hovered && acceptsPayload) {
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
      if (!prefabEditObj)
        acceptSceneAssetDrop(uuid, false);
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
    renamePos.x += ImGui::CalcTextSize(getNodeIcons(obj).c_str()).x;
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

  // Display of a prefab instance's definition tree (nested prefab content). The nodes
  // aren't scene objects, so they're shown dimmed and selecting one targets it as a
  // nested override (rootUuid = instance, path = chain of definition-node uuids).
  void drawPrefabDefNode(Project::Object &node, int depth, uint32_t rootUuid,
                         std::vector<uint32_t> path, bool selectable,
                         bool parentEnabled = true)
  {
    if(depth > 64)return; // guard against self-referencing prefabs
    path.push_back(node.uuid);

    Project::Object* src = Editor::SelectionUtils::prefabDefOf(&node);

    bool isSelected = (ctx.selObjectUUID == rootUuid && ctx.selSubPath == path);
    bool dim = (prefabEditObj && !selectable) || !parentEnabled || !node.enabled;

    ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
      | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_FramePadding
      | ImGuiTreeNodeFlags_SpanAllColumns;
    if(src->children.empty())flag |= ImGuiTreeNodeFlags_Leaf;
    if(isSelected)flag |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 3_px));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
    std::string nameID = getNodeIcons(node) + node.name + "##pf"
      + std::to_string(reinterpret_cast<uintptr_t>(&node));
    if(dim)ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    bool isOpen = ImGui::TreeNodeEx(nameID.c_str(), flag);
    if(dim)ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    if(selectable && ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
      ctx.setNestedSelection(rootUuid, path);
    }

    if(isOpen) {
      // Outside edit mode the whole def tree is selectable.
      // In edit mode we may descend through regular children but stop at nested prefab instances.
      bool childSelectable = prefabEditObj ? (selectable && !node.isPrefabInstance()) : true;
      for(auto &child : src->children) {
        drawPrefabDefNode(*child, depth + 1, rootUuid, path, childSelectable,
                          parentEnabled && node.enabled);
      }
      ImGui::TreePop();
    }
  }

  /**
   * Whether an object or any of its descendants matches the current search filter.
   */
  bool subtreeMatchesFilter(const Project::Object &obj)
  {
    if (ImTable::labelMatchesFilter(obj.name.c_str(), searchFilter))
      return true;

    for (const auto &child : obj.children) {
      if (subtreeMatchesFilter(*child))
        return true;
    }
    return false;
  }

  void drawObjectNode(
    Project::Scene &scene, Project::Object &obj, bool keyDelete,
    bool parentEnabled = true
  )
  {
    bool hasSearchFilter = !searchFilter.empty();
    // Searching and this branch has no match anywhere --> Hide it entirely
    if (hasSearchFilter && !subtreeMatchesFilter(obj))
      return;

    bool selfMatchesFilter = !hasSearchFilter || ImTable::labelMatchesFilter(obj.name.c_str(), searchFilter);

    ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
      | ImGuiTreeNodeFlags_OpenOnDoubleClick
      | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAllColumns;

    // A prefab instance shows its definition tree (read-only) below, so it can expand
    // even though the thin instance itself has no children.
    Project::Object* prefabDef = nullptr;
    if(obj.isPrefabInstance()) {
      auto prefab = ctx.project->getAssets().getPrefabByUUID(obj.uuidPrefab.value);
      if(prefab && !prefab->obj.children.empty())prefabDef = &prefab->obj;
    }

    // While searching, a child only renders if its own branch has a match, so re-check
    // that here to avoid showing an expandable arrow with nothing visible underneath.
    bool anyVisibleChild = !hasSearchFilter
      ? !obj.children.empty()
      : std::any_of(obj.children.begin(), obj.children.end(),
          [](const auto &child) { return subtreeMatchesFilter(*child); });

    if (!anyVisibleChild && !prefabDef) {
      flag |= ImGuiTreeNodeFlags_Leaf;
    }

    // Searching and the match is in a descendant --> Force this node open so it stays visible
    if (hasSearchFilter && !selfMatchesFilter) {
      ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    bool isSelected = ctx.isObjectSelected(obj.uuid);
    if (isSelected) {
      flag |= ImGuiTreeNodeFlags_Selected;
    }

    if (isSelected && obj.parent && keyDelete) {
      deleteSelection = true;
    }

    // While editing a prefab, only that instance may be selected here. Its own definition
    // is handled by drawPrefabDefNode. All other scene objects are dimmed and inert.
    bool canSelect = !prefabEditObj || (&obj == prefabEditObj);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 3_px));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

    std::string nameID = getNodeIcons(obj) + obj.name + "##" + std::to_string(obj.uuid);

    // Set style disabled when editing a prefab or the element or an ancestor is disabled
    const bool dimNode = !canSelect || !parentEnabled || !obj.enabled;
    if(dimNode)ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    bool isOpen = ImGui::TreeNodeEx(nameID.c_str(), flag);
    if(dimNode)ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImVec2 nodeRectMin = ImGui::GetItemRectMin();
    ImVec2 nodeRectMax = ImGui::GetItemRectMax();

    // Mark object being edited in prefab-edit mode
    if(ctx.isPrefabEditing(obj.uuid)) {
      ImVec2 bgMax = ImGui::GetItemRectMax();
      bgMax.x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
      ImU32 editCol = ImGui::Theme::getColorU32("prefabEditBg", IM_COL32(190, 55, 55, 60));
      ImGui::GetWindowDrawList()->AddRectFilled(nodeRectMin, bgMax, editCol);
    }

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
        dragDropTask.sourceUUID = *static_cast<const uint32_t*>(payload->Data);
        dragDropTask.targetUUID = obj.uuid;
        dragDropTask.isInsert = true;
      }
      ImGui::EndDragDropTarget();
    }

    // Keep asset child drops in the centre of the row, away from insertion lines
    if (!prefabEditObj && !obj.isPrefabInstance()) {
      ImRect assetTargetRect{nodeRectMin, nodeRectMax};
      assetTargetRect.Min.y += 4_px;
      assetTargetRect.Max.y -= 4_px;
      ImGui::PushID(obj.uuid);
      if (ImGui::BeginDragDropTargetCustom(assetTargetRect, ImGui::GetID("SceneAssetChildDrop"))) {
        acceptSceneAssetDrop(obj.parent ? obj.uuid : 0, obj.parent != nullptr);
        ImGui::EndDragDropTarget();
      }
      ImGui::PopID();
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

    if (nodeIsClicked && canSelect) {
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
            // Defer: createPrefabFromObject reloads assets (frees GPU textures), which is
            // unsafe mid-frame while ImGui draw data still references them.
            auto *scenePtr = &scene;
            uint32_t uuid = obj.uuid;
            ctx.deferAction([scenePtr, uuid]() {
              uint64_t prefabUUID = scenePtr->createPrefabFromObject(uuid);
              if(prefabUUID)
                Editor::AssetsBrowser::focusPrefab(prefabUUID);
            });
          }

          if (obj.isPrefabInstance() && ImGui::MenuItem(ICON_MDI_PACKAGE_VARIANT " Unpack Prefab")) {
            // Defer: modifies the scene tree (adds objects) - unsafe mid-iteration.
            auto *scenePtr = &scene;
            uint32_t uuid = obj.uuid;
            ctx.deferAction([scenePtr, uuid]() {
              Editor::UndoRedo::getHistory().markChanged("Unpack Prefab");
              scenePtr->unpackPrefabInstance(uuid);
            });
          }

          if (ImGui::MenuItem(ICON_MDI_TRASH_CAN " Delete"))deleteObj = &obj;
        }
        ImGui::EndPopup();
      }

      // The scene root provides the insertion point before its first object
      if (obj.parent == nullptr && !obj.children.empty() && ImGui::IsDragDropActive()) {
        if (DrawDropTarget(dragDropTask.sourceUUID, obj.uuid)) {
          dragDropTask.targetUUID = obj.uuid;
        }
      }

      for(size_t i = 0; i < obj.children.size(); ++i) {
        auto &child = obj.children[i];
        drawObjectNode(scene, *child, keyDelete, parentEnabled && obj.enabled);

        // Nested lists leave their final boundary to the parent's sibling line
        bool needsInsertLine = (i + 1 < obj.children.size()) || obj.parent == nullptr;
        if (needsInsertLine && ImGui::IsDragDropActive()) {
          if (DrawDropTarget(dragDropTask.sourceUUID, child->uuid)) {
            dragDropTask.targetUUID = child->uuid;
          }
        }
      }

      // Prefab definition tree showing nested prefab content under the instance. Nodes are
      // selectable for nested override editing, keyed relative to the prefab root. While
      // editing a prefab, only the edited instance's own definition is selectable.
      if(prefabDef) {
        for(auto &child : prefabDef->children) {
          drawPrefabDefNode(*child, 0, obj.uuid, {}, canSelect, parentEnabled && obj.enabled);
        }
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
  assetDropTask = {};
  hasInsertLine = false;
  deleteObj = nullptr;
  deleteSelection = false;
  prefabEditObj = Editor::SelectionUtils::getPrefabEditObject(*scene);
  bool isFocus = ImGui::IsWindowFocused();
  // While rename is active, shortcuts stay disabled, so the text field can own the keyboard input
  bool isRenaming = renameObjectUUID != 0;

  // Ctrl+F focuses the search box, matching common scene-tree behavior
  if (isFocus && !isRenaming && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) {
    ImGui::SetKeyboardFocusHere();
  }

  // Search box to filter the tree by object name; matching branches force their ancestors open
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##sceneGraphSearch", ICON_MDI_MAGNIFY " Search...", &searchFilter,
    ImGuiInputTextFlags_AutoSelectAll);
  bool isSearchActive = ImGui::IsItemActive();
  if (isSearchActive && !searchFilter.empty() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    searchFilter.clear();
    ImGui::ClearActiveID();
  }

  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0_px);
  bool keyDelete = isFocus && !isRenaming && !isSearchActive && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace));
  // F2 starts renaming the current object, matching common scene-tree/file-explorer behavior
  bool keyRename = isFocus && !isRenaming && !isSearchActive && ImGui::IsKeyPressed(ImGuiKey_F2);

  if (keyRename) {
    const std::vector<uint32_t> &selectedIds = ctx.getSelectedObjectUUIDs();
    // Inline renaming only makes sense for a single target; multi-select keeps its current state
    if (selectedIds.size() == 1) {
      // Rename the selected object
      startRenaming(selectedIds.front());
    }
  }

  auto &root = scene->getRootObject();
  if (!searchFilter.empty() && !subtreeMatchesFilter(root)) {
    ImGui::TextDisabled("No matching objects");
  } else {
    drawObjectNode(*scene, root, keyDelete);
  }

  // Use the remaining tree space as a drop target for root-level prefab or model objects
  if (!prefabEditObj && ImGui::IsDragDropActive()) {
    ImVec2 emptySize = ImGui::GetContentRegionAvail();
    constexpr float INSERT_DROP_HEIGHT = 8.0f;
    if (emptySize.x > 0 && emptySize.y > INSERT_DROP_HEIGHT) {
      ImVec2 emptyStart = ImGui::GetCursorScreenPos();
      ImVec2 lineStart = hasInsertLine ? lastInsertLineStart : emptyStart;
      ImVec2 lineEnd = hasInsertLine
        ? lastInsertLineEnd
        : ImVec2{ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x, emptyStart.y};
      ImGui::SetCursorScreenPos({emptyStart.x, emptyStart.y + INSERT_DROP_HEIGHT});
      emptySize.y -= INSERT_DROP_HEIGHT;
      ImGui::InvisibleButton("##ScenePrefabDropTarget", emptySize);

      const ImGuiPayload* payload = ImGui::GetDragDropPayload();
      auto draggedAsset = payload && payload->IsDataType("ASSET")
        ? ctx.project->getAssets().getEntryByUUID(*static_cast<const uint64_t*>(payload->Data))
        : nullptr;
      bool isSceneAsset = draggedAsset && (draggedAsset->type == Project::FileType::PREFAB
        || draggedAsset->type == Project::FileType::MODEL_3D);
      if (isSceneAsset && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        ImGui::GetWindowDrawList()->AddLine(
          lineStart,
          lineEnd,
          ImGui::GetColorU32(ImGuiCol_DragDropTarget),
          2_px
        );
      }

      // Hide the default full-area frame while preserving the empty-space hit zone
      ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0, 0, 0, 0));
      if (ImGui::BeginDragDropTarget()) {
        acceptSceneAssetDrop(0, false);
        ImGui::EndDragDropTarget();
      }
      ImGui::PopStyleColor();
    }
  }

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

  if (assetDropTask.assetUUID) {
    auto asset = ctx.project->getAssets().getEntryByUUID(assetDropTask.assetUUID);
    auto &root = scene->getRootObject();
    bool targetIsRoot = assetDropTask.targetUUID == root.uuid;
    std::shared_ptr<Project::Object> target{};
    if (assetDropTask.targetUUID && !targetIsRoot) {
      target = scene->getObjectByUUID(assetDropTask.targetUUID);
    }

    bool targetExists = !assetDropTask.targetUUID || targetIsRoot || target;
    bool canAddAsChild = !assetDropTask.asChild || targetIsRoot
      || (target && !target->isPrefabInstance());

    // A stale target or a child drop on a prefab must not create an object elsewhere
    if (asset && targetExists && canAddAsChild) {
      bool isPrefab = asset->type == Project::FileType::PREFAB;
      auto added = isPrefab
        ? scene->addPrefabInstance(assetDropTask.assetUUID)
        : scene->addModelObject(assetDropTask.assetUUID);
      if (added) {
        // Root-level objects start at the scene origin
        glm::vec3 position{0.0f};
        // Dropped over an object --> Set same global position and set as child
        if (assetDropTask.asChild && target) {
          position = target->pos.resolve(target->propOverrides);
          scene->moveObject(added->uuid, target->uuid, true);
        // Dropped beside an object --> Use the shared parent position and set as sibling
        } else if (assetDropTask.targetUUID) {
          // It is being set as a child of another object --> Set same global position
          if (target && target->parent)
            position = target->parent->pos.resolve(target->parent->propOverrides);
          scene->moveObject(added->uuid, assetDropTask.targetUUID, false);
        }
        // Apply the position after moving the object to its final place in the tree
        added->pos.resolve(added->propOverrides) = position;
        // Focus the newly created object in the editor
        ctx.setObjectSelection(added->uuid);
        // Record the completed drop as an undoable action
        UndoRedo::getHistory().markChanged(isPrefab ? "Add Prefab" : "Add Model");
      }
    }
  }

  if (deleteSelection || deleteObj) {
    if (deleteObj && !ctx.isObjectSelected(deleteObj->uuid)) {
      ctx.setObjectSelection(deleteObj->uuid);
    }

    UndoRedo::getHistory().markChanged("Delete Object");
    Editor::SelectionUtils::deleteSelectedObjects(*scene);
  }
}
