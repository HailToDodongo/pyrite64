/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "objectInspector.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "imgui_internal.h"
#include "../../imgui/helper.h"
#include "../../../context.h"
#include "../../../project/component/components.h"
#include "../../selectionUtils.h"
#include "../../undoRedo.h"

namespace
{
  constexpr int COMP_ID_CODE = 0;

  /**
   * Returns whether the current drag payload is an object-script asset.
   * @param scriptUUID Optional output for the dragged script UUID.
   * @return true when the active payload is a CODE_OBJ asset.
   */
  bool isDraggedObjectScript(uint64_t *scriptUUID = nullptr)
  {
    // Access ImGui's drag-drop state
    ImGuiContext &g = *GImGui;
    // There is no drag operation or no project assets to inspect --> Do nothing
    if (!g.DragDropActive || !ctx.project) return false;

    // Only asset payloads with UUIDs can create Code components here
    const ImGuiPayload *payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType("ASSET") || payload->DataSize != sizeof(uint64_t))
      return false;

    // Resolve the dragged UUID back to an asset entry and verify that it is an object script
    const uint64_t uuid = *static_cast<const uint64_t*>(payload->Data);
    auto *script = ctx.project->getAssets().getEntryByUUID(uuid);
    if (!script || script->type != Project::FileType::CODE_OBJ)
      return false;

    // Requeted to output the UUID --> Assign it
    if (scriptUUID)
      *scriptUUID = uuid;

    return true;
  }

  /**
   * Creates a Code component using dropped Script.
   * @param targetObj Object receiving the new component.
   * @param scriptUUID UUID of the dragged script.
   * @return true when the component was added successfully.
   */
  bool createCodeComponentFromScript(Project::Object *targetObj, uint64_t scriptUUID)
  {
    // There is no target object --> Do nothing
    if (!targetObj) return false;

    // Track the component count to detect whether addComponent actually appended one
    const auto oldSize = targetObj->components.size();
    // Record undo history before modifying the component list
    Editor::UndoRedo::getHistory().markChanged("Add Code Component");
    // Create a Code component
    targetObj->addComponent(COMP_ID_CODE);
    if (targetObj->components.size() <= oldSize) return false;

    // Set the Script field for the created Code component
    auto &comp = targetObj->components.back();
    Project::Component::Code::setScript(comp, scriptUUID, false);
    return true;
  }

  /**
   * Handles dropping an object script onto the inspector panel.
   * @param targetObj Object that should receive a new Code component.
   * @param dropRect Custom panel-space drop rectangle.
   * @param highlightWindow True to draw the inspector-wide highlight border.
   * @return True when a valid script is hovering or was delivered to this target.
   */
  bool handleScriptComponentDropTarget(Project::Object *targetObj, const ImRect &dropRect, bool highlightWindow)
  {
    // Register the inspector panel as a custom drop target covering the requested rectangle
    if (!ImGui::BeginDragDropTargetCustom(dropRect, ImGui::GetID("##ScriptComponentDropTarget"))) return false;

    // Peek the asset payload while it hovers so the panel can highlight before delivery
    const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
      "ASSET",
      ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
    );
    // There is no compatible payload --> Skip the target
    if (!payload || payload->DataSize != sizeof(uint64_t)) {
      ImGui::EndDragDropTarget();
      return false;
    }

    // Resolve the hovered asset UUID and reject anything that is not an object Script
    const uint64_t scriptUUID = *static_cast<const uint64_t*>(payload->Data);
    auto *script = ctx.project->getAssets().getEntryByUUID(scriptUUID);
    // Is not a Code component --> Skip the target
    if (!script || script->type != Project::FileType::CODE_OBJ) {
      ImGui::EndDragDropTarget();
      return false;
    }

    if (highlightWindow) {
      // Use the window's inner rectangle so the border matches the visible inspector panel
      ImGuiWindow *window = ImGui::GetCurrentWindowRead();
      auto col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
      ImRect borderRect = window->InnerRect;
      // Fix the rect size not to write outside of the panel
      borderRect.Min.y += 1.0f;
      borderRect.Min.x += 2.0f;
      borderRect.Max.x -= 2.0f;
      borderRect.Max.y -= 2.0f;
      // Draw an outline to highlight the object inspector panel
      ImGui::GetWindowDrawList()->AddRect(borderRect.Min, borderRect.Max, col, 0.0f, 0, 2.0f);
    }

    bool accepted = false;
    // Released the mouse --> Commit drop, create Code component
    if (payload->Delivery) {
      accepted = createCodeComponentFromScript(targetObj, scriptUUID);
    // Hovering valid script --> Mark the panel as active drop target
    } else {
      accepted = true;
    }

    // Close the custom target scope opened above
    ImGui::EndDragDropTarget();
    return accepted;
  }
}

Editor::ObjectInspector::ObjectInspector() {
}

void Editor::ObjectInspector::draw() {
  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  ctx.sanitizeObjectSelection(scene);
  const auto &selectedIds = ctx.getSelectedObjectUUIDs();
  if (selectedIds.empty()) {
    ImGui::Text("No Object selected");
    return;
  }

  if (selectedIds.size() > 1) {
    auto selectedObjects = Editor::SelectionUtils::collectSelectedObjects(*scene);

    if (selectedObjects.empty()) {
      ctx.clearObjectSelection();
      ImGui::Text("No Object selected");
      return;
    }

    ImGui::Text("%zu Objects selected", selectedObjects.size());

    auto handleHistory = [&](const std::string &desc) {
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        Editor::UndoRedo::getHistory().markChanged(desc);
      }
    };

    auto floatEqual = [](float a, float b) {
      return std::abs(a - b) <= 0.0001f;
    };

    static std::unordered_map<std::string, std::string> mixedValueCache{};

    auto parseFloatList = [](const std::string &text, float *out, int count) {
      std::string cleaned = text;
      for (auto &ch : cleaned) {
        if (ch == ',' || ch == ';' || ch == '(' || ch == ')' || ch == '[' || ch == ']') {
          ch = ' ';
        }
      }

      std::stringstream stream(cleaned);
      for (int i = 0; i < count; ++i) {
        if (!(stream >> out[i])) {
          return false;
        }
      }
      return true;
    };

    auto parseFloat = [&](const std::string &text, float &out) {
      return parseFloatList(text, &out, 1);
    };

    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImTable::start("General", nullptr)) {
        bool mixedName = false;
        std::string nameValue = selectedObjects.front()->name;
        for (size_t i = 1; i < selectedObjects.size(); ++i) {
          if (selectedObjects[i]->name != nameValue) {
            mixedName = true;
            break;
          }
        }
        if (mixedName) {
          nameValue.clear();
        }

        ImTable::add("Name");
        ImGui::PushID("Name");
        bool edited = ImGui::InputTextWithHint("##Name", mixedName ? "-" : "", &nameValue);
        handleHistory("Edit Name");
        ImGui::PopID();
        if (edited) {
          for (auto *selObj : selectedObjects) {
            selObj->name = nameValue;
          }
        }
        ImTable::end();
      }
    }

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImTable::start("Transform", nullptr)) {
        glm::vec3 posValue = selectedObjects.front()->pos.resolve(selectedObjects.front()->propOverrides);
        glm::vec3 scaleValue = selectedObjects.front()->scale.resolve(selectedObjects.front()->propOverrides);
        glm::quat rotValue = selectedObjects.front()->rot.resolve(selectedObjects.front()->propOverrides);

        bool mixedPos[3] = {false, false, false};
        bool mixedScale[3] = {false, false, false};
        bool mixedRot[4] = {false, false, false, false};
        for (size_t i = 1; i < selectedObjects.size(); ++i) {
          auto *selObj = selectedObjects[i];
          auto pos = selObj->pos.resolve(selObj->propOverrides);
          auto scale = selObj->scale.resolve(selObj->propOverrides);
          auto rot = selObj->rot.resolve(selObj->propOverrides);

          if (!floatEqual(pos.x, posValue.x)) mixedPos[0] = true;
          if (!floatEqual(pos.y, posValue.y)) mixedPos[1] = true;
          if (!floatEqual(pos.z, posValue.z)) mixedPos[2] = true;

          if (!floatEqual(scale.x, scaleValue.x)) mixedScale[0] = true;
          if (!floatEqual(scale.y, scaleValue.y)) mixedScale[1] = true;
          if (!floatEqual(scale.z, scaleValue.z)) mixedScale[2] = true;

          if (!floatEqual(rot.x, rotValue.x)) mixedRot[0] = true;
          if (!floatEqual(rot.y, rotValue.y)) mixedRot[1] = true;
          if (!floatEqual(rot.z, rotValue.z)) mixedRot[2] = true;
          if (!floatEqual(rot.w, rotValue.w)) mixedRot[3] = true;
        }

        auto applyVec3Component = [&](Property<glm::vec3> Project::Object::*prop, int index, float value) {
          for (auto *selObj : selectedObjects) {
            bool createdOverride = false;
            glm::vec3 resolvedBefore = (selObj->*prop).resolve(selObj->propOverrides);
            if (selObj->isPrefabInstance()
                && !selObj->isPrefabEdit
                && selObj->propOverrides.find((selObj->*prop).id) == selObj->propOverrides.end()) {
              selObj->addPropOverride(selObj->*prop);
              createdOverride = true;
            }

            auto &vec = (selObj->*prop).resolve(selObj->propOverrides);
            if (createdOverride) {
              vec = resolvedBefore;
            }
            if (index == 0) vec.x = value;
            if (index == 1) vec.y = value;
            if (index == 2) vec.z = value;
          }
        };

        auto applyQuatComponent = [&](Property<glm::quat> Project::Object::*prop, int index, float value) {
          for (auto *selObj : selectedObjects) {
            bool createdOverride = false;
            glm::quat resolvedBefore = (selObj->*prop).resolve(selObj->propOverrides);
            if (selObj->isPrefabInstance()
                && !selObj->isPrefabEdit
                && selObj->propOverrides.find((selObj->*prop).id) == selObj->propOverrides.end()) {
              selObj->addPropOverride(selObj->*prop);
              createdOverride = true;
            }

            auto &quat = (selObj->*prop).resolve(selObj->propOverrides);
            if (createdOverride) {
              quat = resolvedBefore;
            }
            if (index == 0) quat.x = value;
            if (index == 1) quat.y = value;
            if (index == 2) quat.z = value;
            if (index == 3) quat.w = value;
          }
        };

        auto drawFloatField = [&](\
          const char *widgetKey,
          bool mixed,
          float &value,
          float width,
          const std::string &snapshotLabel,
          const std::function<void(float)> &applyValue
        ) {
          std::string inputId = std::string{"##Value_"} + widgetKey;
          ImGui::SetNextItemWidth(width);
          if (mixed) {
            auto &text = mixedValueCache[inputId];
            ImGui::InputTextWithHint(inputId.c_str(), "-", &text);
            handleHistory(snapshotLabel);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
              float parsed = value;
              if (parseFloat(text, parsed)) {
                value = parsed;
                applyValue(parsed);
              }
              text.clear();
            }
          } else {
            if (ImGui::InputFloat(inputId.c_str(), &value)) {
              applyValue(value);
            }
            handleHistory(snapshotLabel);
          }
        };

        ImTable::add("Pos");
        ImGui::PushID("Pos");
        float posWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
        drawFloatField("PosX", mixedPos[0], posValue.x, posWidth, "Edit Pos", [&](float val) {
          applyVec3Component(&Project::Object::pos, 0, val);
        });
        ImGui::SameLine();
        drawFloatField("PosY", mixedPos[1], posValue.y, posWidth, "Edit Pos", [&](float val) {
          applyVec3Component(&Project::Object::pos, 1, val);
        });
        ImGui::SameLine();
        drawFloatField("PosZ", mixedPos[2], posValue.z, posWidth, "Edit Pos", [&](float val) {
          applyVec3Component(&Project::Object::pos, 2, val);
        });
        ImGui::PopID();

        ImTable::add("Scale");
        ImGui::PushID("Scale");
        float scaleWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
        drawFloatField("ScaleX", mixedScale[0], scaleValue.x, scaleWidth, "Edit Scale", [&](float val) {
          applyVec3Component(&Project::Object::scale, 0, val);
        });
        ImGui::SameLine();
        drawFloatField("ScaleY", mixedScale[1], scaleValue.y, scaleWidth, "Edit Scale", [&](float val) {
          applyVec3Component(&Project::Object::scale, 1, val);
        });
        ImGui::SameLine();
        drawFloatField("ScaleZ", mixedScale[2], scaleValue.z, scaleWidth, "Edit Scale", [&](float val) {
          applyVec3Component(&Project::Object::scale, 2, val);
        });
        ImGui::PopID();

        ImTable::add("Rot");
        ImGui::PushID("Rot");
        float rotWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3.0f) / 4.0f;
        drawFloatField("RotX", mixedRot[0], rotValue.x, rotWidth, "Edit Rot", [&](float val) {
          applyQuatComponent(&Project::Object::rot, 0, val);
        });
        ImGui::SameLine();
        drawFloatField("RotY", mixedRot[1], rotValue.y, rotWidth, "Edit Rot", [&](float val) {
          applyQuatComponent(&Project::Object::rot, 1, val);
        });
        ImGui::SameLine();
        drawFloatField("RotZ", mixedRot[2], rotValue.z, rotWidth, "Edit Rot", [&](float val) {
          applyQuatComponent(&Project::Object::rot, 2, val);
        });
        ImGui::SameLine();
        drawFloatField("RotW", mixedRot[3], rotValue.w, rotWidth, "Edit Rot", [&](float val) {
          applyQuatComponent(&Project::Object::rot, 3, val);
        });
        ImGui::PopID();

        ImTable::end();
      }
    }

    return;
  }

  bool isPrefabInst = false;

  auto obj = scene->getObjectByUUID(selectedIds.front());
  if (!obj) {
    ctx.clearObjectSelection();
    return;
  }
  if (ctx.selObjectUUID != obj->uuid) {
    ctx.setObjectSelection(obj->uuid);
  }

  Project::Object* srcObj = obj.get();
  std::shared_ptr<Project::Prefab> prefab{};
  if(obj->uuidPrefab.value)
  {
    prefab = ctx.project->getAssets().getPrefabByUUID(obj->uuidPrefab.value);
    if(prefab)srcObj = &prefab->obj;
    isPrefabInst = true;
  }


  //if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImTable::start("General", obj.get())) {
      ImTable::add("Name", obj->name);

      //ImTable::add("UUID");
      //ImGui::Text("0x%16lX", obj->uuid);

      if(isPrefabInst) {
        ImTable::add("Prefab");

        auto name = std::string{ICON_MDI_PENCIL " "};
        name += obj->isPrefabEdit ? ("Back to Instance") : ("Edit '" + srcObj->name + "'");

        if(ImGui::Button(name.c_str())) {
          if (obj->isPrefabEdit) {
            ctx.project->getAssets().markPrefabDirty(prefab->uuid.value);
          }
          obj->isPrefabEdit = !obj->isPrefabEdit;
        }
      }

      ImTable::end();
    }
  }

  if(ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if(ImTable::start("Transform", obj.get()))
    {
      ImTable::addObjProp("Pos", srcObj->pos);

      if(srcObj->proportionalScale)
      {
        std::function<bool(glm::vec3*)> cb = [](glm::vec3 *val) -> bool {
          glm::vec3 scale = *val;
          if (scale == glm::vec3(0,0,0)) {
            if (!ImGui::InputFloat3("##", glm::value_ptr(*val))) return false;
            *val = glm::vec3(val->x + val->y + val->z);
            return true;
          }
          ImGuiContext& g = *GImGui;
          ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
          float ratio = 1.0f;
          for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            if (i > 0) ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
            bool isZero = glm::abs(scale[i]) < 0.0001f;
            if (isZero) ImGui::BeginDisabled();
            if (ImGui::InputFloat("", &(*val)[i])) ratio = (*val)[i] / scale[i];
            if (isZero) ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::PopItemWidth();
          }
          *val = scale * ratio;
          return ratio != 1.0f;
        };
        ImTable::addObjProp("Scale", srcObj->scale, cb, nullptr);
      } else {
        ImTable::addObjProp("Scale", srcObj->scale);
      }

      // icon to toggle between proportional and independent scale
      ImGui::SameLine();
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 32_px);
      if(ImGui::IconButton(srcObj->proportionalScale ? ICON_MDI_LINK_VARIANT : ICON_MDI_LINK_VARIANT_OFF, {24_px, 24_px})) {
        ImGui::ClearActiveID();
        srcObj->proportionalScale = !srcObj->proportionalScale;
      }
      ImGui::SetItemTooltip(srcObj->proportionalScale
        ? "Change to Independent Scale"
        : "Change to Proportional Scale"
      );

      ImTable::addObjProp("Rot", srcObj->rot);

      // icon to toggle between quaternion and euler
      ImGui::SameLine();
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 32_px);
      if(ImGui::IconButton(ctx.prefs.showRotAsEuler ? ICON_MDI_AXIS_Z_ROTATE_CLOCKWISE : ICON_MDI_SPHERE, {24_px, 24_px})) {
        ImGui::ClearActiveID();
        ctx.prefs.showRotAsEuler = !ctx.prefs.showRotAsEuler;
        ctx.prefs.save();
      }
      ImGui::SetItemTooltip(ctx.prefs.showRotAsEuler
        ? "Change to Quaternion"
        : "Change to Euler (degrees)"
      );

      ImTable::end();
    }
  }

  uint64_t compDelUUID = 0;
  Project::Component::Entry *compCopy = nullptr;

  auto drawComp = [&](Project::Object* obj, Project::Component::Entry &comp, bool isInstance)
  {
    ImTable::PrefabEditScope prefabScope(isInstance);
    ImGui::PushID(&comp);

    auto &def = Project::Component::TABLE[comp.id];
    auto name = std::string{def.icon} + "  " + comp.name;

    ImGui::SetNextItemAllowOverlap();
    bool headerOpen = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    const bool locked = ImTable::isPrefabLocked(obj);
    const bool headerRightClicked = !locked && ImGui::IsItemClicked(ImGuiMouseButton_Right);

    // Faint help icon near the right edge of the header
    if (def.docSlug && def.docSlug[0]) {
      const float helpSize = 19_px;
      ImGui::SameLine(ImGui::GetContentRegionMax().x - helpSize - 4_px);
      ImGui::HelpIcon(def.docSlug, "Open Docs", helpSize);
    }

    if (headerOpen)
    {
      if(!locked)
      {
        if (headerRightClicked) {
          ImGui::OpenPopup("CompCtx");
        }

        if(ImGui::BeginPopup("CompCtx"))
        {
          if (ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Duplicate")) {
            compCopy = &comp;
          }
          if (ImGui::MenuItem(ICON_MDI_TRASH_CAN_OUTLINE " Delete")) {
            compDelUUID = comp.uuid;
          }
          ImGui::EndPopup();
        }
      }

      def.funcDraw(*obj, comp);
    }
    ImGui::PopID();
  };

  for (auto &comp : srcObj->components) {
    drawComp(obj.get(), comp, false);
  }

  if(isPrefabInst && !obj->isPrefabEdit) {
    for (auto &comp : obj->components) {
      drawComp(obj.get(), comp, true);
    }
    srcObj = obj.get();
  }

  if (isPrefabInst && obj->isPrefabEdit && prefab) {
    ctx.project->getAssets().markPrefabDirty(prefab->uuid.value);
  }

  if (compCopy) {
    const int compCopyId = compCopy->id;
    const std::string compCopyName = compCopy->name;
    UndoRedo::getHistory().markChanged("Duplicate Component");
    srcObj->addComponent(compCopyId);
    srcObj->components.back().name = compCopyName + " Copy";
  }
  if (compDelUUID) {
    UndoRedo::getHistory().markChanged("Delete Component");
    srcObj->removeComponent(compDelUUID);
  }

  const char* addLabel = ICON_MDI_PLUS_BOX_OUTLINE " Add Component";
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4_px);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(addLabel).x) * 0.5f - 4_px);
  if (ImGui::Button(addLabel)) {
    ImGui::OpenPopup("CompSelect");
  }

  const ImVec2 contentMin = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();
  const ImVec2 contentMax = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMax();

  ImRect panelDropRect{contentMin, contentMax};
  handleScriptComponentDropTarget(srcObj, panelDropRect, true);

  if (ImGui::BeginPopupContextItem("CompSelect"))
  {
    for (auto &comp : Project::Component::TABLE_SORTED_BY_NAME) {
      auto name = std::string{comp.icon} + " " + comp.name;
      if(ImGui::MenuItem(name.c_str())) {
        UndoRedo::getHistory().markChanged("Add Component");
        srcObj->addComponent(comp.id);
      }
    }
    ImGui::EndPopup();
  }
}
