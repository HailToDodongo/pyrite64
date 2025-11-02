/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "objectInspector.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../../context.h"
#include "../../../project/component/components.h"

Editor::ObjectInspector::ObjectInspector() {
}

void Editor::ObjectInspector::draw() {
  if (ctx.selObjectUUID == 0) {
    ImGui::Text("No Object selected");
    return;
  }

  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  auto obj = scene->getObjectByUUID(ctx.selObjectUUID);
  if (!obj) {
    ctx.selObjectUUID = 0;
    return;
  }

  //if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::InpTable::start("General")) {
      ImGui::InpTable::addString("Name", obj->name);

      int idProxy = obj->id;
      ImGui::InpTable::addInputInt("ID", idProxy);
      obj->id = static_cast<uint16_t>(idProxy);

      //ImGui::InpTable::add("UUID");
      //ImGui::Text("0x%16lX", obj->uuid);

      ImGui::InpTable::end();
    }
  }

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::InpTable::start("Transform")) {
      ImGui::InpTable::addInputVec3("Pos", obj->pos);
      ImGui::InpTable::addInputVec3("Scale", obj->scale);
      ImGui::InpTable::addInputQuat("Rot", obj->rot);

      ImGui::InpTable::end();
    }
  }

  uint64_t compDelUUID = 0;
  Project::Component::Entry *compCopy = nullptr;
  for (auto &comp : obj->components)
  {
    ImGui::PushID(&comp);

    auto &def = Project::Component::TABLE[comp.id];
    auto name = std::string{def.icon} + "  " + comp.name;
    if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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

      def.funcDraw(*obj, comp);
    }
    ImGui::PopID();
  }

  if (compCopy) {
    obj->addComponent(compCopy->id);
    obj->components.back().name = compCopy->name + " Copy";
  }
  if (compDelUUID) {
    obj->removeComponent(compDelUUID);
  }

  const char* addLabel = ICON_MDI_PLUS_BOX_OUTLINE " Add Component";
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(addLabel).x) * 0.5f - 4);
  if (ImGui::Button(addLabel)) {
    ImGui::OpenPopup("CompSelect");
  }

  if (ImGui::BeginPopupContextItem("CompSelect"))
  {
    for (auto &comp : Project::Component::TABLE) {
      auto name = std::string{comp.icon} + " " + comp.name;
      if(ImGui::MenuItem(name.c_str())) {
        obj->addComponent(comp.id);
      }
    }
    ImGui::EndPopup();
  }
}
