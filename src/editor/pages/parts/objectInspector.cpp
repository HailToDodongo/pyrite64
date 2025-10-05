/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "objectInspector.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../../context.h"

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
    ImGui::InpTable::start("General");

    ImGui::InpTable::addString("Name", obj->name);

    int idProxy = obj->id;
    ImGui::InpTable::addInputInt("ID", idProxy);
    obj->id = static_cast<uint16_t>(idProxy);

    //ImGui::InpTable::add("UUID");
    //ImGui::Text("0x%16lX", obj->uuid);

    ImGui::InpTable::end();
  }

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InpTable::start("Transform");

    ImGui::InpTable::addInputVec3("Pos", obj->pos);
    ImGui::InpTable::addInputVec3("Scale", obj->scale);
    ImGui::InpTable::addInputQuat("Rot", obj->rot);

    ImGui::InpTable::end();
  }

  if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
    // center button
    const char* addLabel = ICON_FA_PLUS_SQUARE " Add Component";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(addLabel).x) * 0.5f - 4);
    ImGui::Button(addLabel);
  }
}
