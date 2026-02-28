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
#include "../../imgui/helper.h"
#include "../../../context.h"
#include "../../../project/component/components.h"
#include "../../selectionUtils.h"
#include "../../undoRedo.h"

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

      int idProxy = obj->id;
      ImTable::add("ID", idProxy);
      obj->id = static_cast<uint16_t>(idProxy);

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

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImTable::start("Transform", obj.get())) {
      ImTable::addObjProp("Pos", srcObj->pos);
      ImTable::addObjProp("Scale", srcObj->scale);
      ImTable::addObjProp("Rot", srcObj->rot);
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
    if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
      if(!ImTable::isPrefabLocked(obj))
      {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          ImGui::OpenPopup("CompCtx");
        }

        if(ImGui::BeginPopupContextItem("CompCtx"))
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
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(addLabel).x) * 0.5f - 4);
  if (ImGui::Button(addLabel)) {
    ImGui::OpenPopup("CompSelect");
  }

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
