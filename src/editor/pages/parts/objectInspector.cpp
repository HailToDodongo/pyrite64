/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "objectInspector.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"
#include "../../../context.h"
#include "../../../project/component/components.h"

Editor::ObjectInspector::ObjectInspector() {
}

void Editor::ObjectInspector::draw() {
  if (ctx.selObjectUUID == 0) {
    ImGui::Text(message(MSG_OBJECT_NONE_SELECTED));
    return;
  }

  bool isPrefabInst = false;

  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  auto obj = scene->getObjectByUUID(ctx.selObjectUUID);
  if (!obj) {
    ctx.selObjectUUID = 0;
    return;
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
    if (ImTable::start("General")) {
      ImTable::add(message(MSG_OBJECT_NAME), obj->name);

      int idProxy = obj->id;
      ImTable::add(message(MSG_OBJECT_ID), idProxy);
      obj->id = static_cast<uint16_t>(idProxy);

      //ImTable::add("UUID");
      //ImGui::Text("0x%16lX", obj->uuid);

      if(isPrefabInst) {
        ImTable::add(message(MSG_OBJECT_PREFAB));

        auto name = std::string{ICON_MDI_PENCIL " "};
        name += obj->isPrefabEdit ? (message(MSG_OBJECT_PREFAB_BACK)) : (std::vformat(message(MSG_OBJECT_PREFAB_EDIT), std::make_format_args(srcObj->name)));

        if(ImGui::Button(name.c_str())) {
          obj->isPrefabEdit = !obj->isPrefabEdit;

          if(!obj->isPrefabEdit) {
            prefab->save();
          }
        }
      }

      ImTable::end();
    }
  }

  if (ImGui::CollapsingHeader(message(MSG_OBJECT_TRANSFORM), ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImTable::start("Transform", obj.get())) {
      ImTable::addObjProp(message(MSG_OBJECT_POS), srcObj->pos);
      ImTable::addObjProp(message(MSG_OBJECT_SCALE), srcObj->scale);
      ImTable::addObjProp(message(MSG_OBJECT_ROT), srcObj->rot);
      ImTable::end();
    }
  }

  uint64_t compDelUUID = 0;
  Project::Component::Entry *compCopy = nullptr;

  auto drawComp = [&](Project::Object* obj, Project::Component::Entry &comp, bool isInstance)
  {
    auto oldPrefabUUID = obj->uuidPrefab.value;
    if(isInstance) {
      obj->uuidPrefab.value = 0;
    }
    ImGui::PushID(&comp);

    auto &def = Project::Component::TABLE[comp.id];
    auto name = std::string{def.icon} + "  " + message(comp.name);
    if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
      if(obj->uuidPrefab.value == 0 || obj->isPrefabEdit)
      {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          ImGui::OpenPopup("CompCtx");
        }

        if(ImGui::BeginPopupContextItem("CompCtx"))
        {
          if (ImGui::MenuItem((std::string{ICON_MDI_CONTENT_COPY} + message(MSG_OBJECT_COMPONENT_DUPLICATE)).c_str())) {
            compCopy = &comp;
          }
          if (ImGui::MenuItem((std::string{ICON_MDI_TRASH_CAN_OUTLINE} + message(MSG_OBJECT_COMPONENT_DELETE)).c_str())) {
            compDelUUID = comp.uuid;
          }
          ImGui::EndPopup();
        }
      }

      def.funcDraw(*obj, comp);
    }
    ImGui::PopID();
    if(isInstance) {
      obj->uuidPrefab.value = oldPrefabUUID;
    }
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

  if (compCopy) {
    srcObj->addComponent(compCopy->id);
    srcObj->components.back().name = compCopy->name + message(MSG_OBJECT_COMPONENT_COPY);
  }
  if (compDelUUID) {
    srcObj->removeComponent(compDelUUID);
  }

  auto addLabel = std::string{ICON_MDI_PLUS_BOX_OUTLINE} + message(MSG_OBJECT_COMPONENT_ADD);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(addLabel.c_str()).x) * 0.5f - 4);
  if (ImGui::Button(addLabel.c_str())) {
    ImGui::OpenPopup("CompSelect");
  }

  if (ImGui::BeginPopupContextItem("CompSelect"))
  {
    for (auto &comp : Project::Component::TABLE) {
      auto name = std::string{comp.icon} + " " + message(comp.name);
      if(ImGui::MenuItem(name.c_str())) {
        srcObj->addComponent(comp.id);
      }
    }
    ImGui::EndPopup();
  }
}
