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
#include "../../imgui/lang.h"

bool Editor::ProjectSettings::draw()
{
  if (ImGui::CollapsingHeader(message(MSG_PROJECT_SETTINGS_GENERAL), ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("General");
    ImTable::add(message(MSG_PROJECT_SETTINGS_NAME), ctx.project->conf.name);
    ImTable::add(message(MSG_PROJECT_SETTINGS_ROM_NAME), ctx.project->conf.romName);
    ImTable::end();
  }
  if (ImGui::CollapsingHeader(message(MSG_PROJECT_SETTINGS_ENV), ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Environment");
    ImTable::addPath(message(MSG_PROJECT_SETTINGS_EMULATOR), ctx.project->conf.pathEmu);
    ImTable::addPath("N64_INST", ctx.project->conf.pathN64Inst, true, "$N64_INST");
    ImTable::end();
  }

  // close button, positioned to bottom right corner
  const char* saveLabel = message(MSG_PROJECT_SETTINGS_SAVE);
  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(saveLabel).x - 23);
  ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 36);
  if (ImGui::Button(saveLabel)) {
    ctx.project->save();
    return true;
  }
  return false;
}
