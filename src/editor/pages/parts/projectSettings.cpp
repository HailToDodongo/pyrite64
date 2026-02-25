/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectSettings.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/logger.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"

bool Editor::ProjectSettings::draw()
{
  if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("General");
    ImTable::add("Name", ctx.project->conf.name);
    ImTable::add("ROM-Name", ctx.project->conf.romName);
    ImTable::end();
  }
  if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Environment");
    ImTable::addPath("Emulator", ctx.project->conf.pathEmu);
    ImTable::addPath("N64_INST", ctx.project->conf.pathN64Inst, true, "$N64_INST");
    ImTable::end();
  }

  // close button, positioned to bottom right corner
  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 54);
  ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 36);
  if (ImGui::Button("Save")) {
    ctx.project->save();
    return true;
  }
  return false;
}
