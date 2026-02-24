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
#include "../../inputConfig.h"

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
  if (ImGui::CollapsingHeader("Keymap", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Keymap");
    if (ImTable::addComboBox("Preset", (int&)ctx.project->conf.inputPreset, { "Blender", "Industry Compatible" })) {
      ctx.inputs.applyPreset((Editor::InputPreset)ctx.project->conf.inputPreset);
    }
    ImTable::end();
    if (ImGui::TreeNodeEx("3D View", ImGuiTreeNodeFlags_SpanFullWidth)) {
      ImTable::start("3D View");
      ImTable::addKeybind("Move Forward", ctx.inputs.moveForward);
      ImTable::addKeybind("Move Back", ctx.inputs.moveBack);
      ImTable::addKeybind("Move Left", ctx.inputs.moveLeft);
      ImTable::addKeybind("Move Right", ctx.inputs.moveRight);
      ImTable::addKeybind("Move Up", ctx.inputs.moveUp);
      ImTable::addKeybind("Move Down", ctx.inputs.moveDown);
      ImTable::addKeybind("Toggle Ortho", ctx.inputs.toggleOrtho);
      ImTable::addKeybind("Focus Object", ctx.inputs.focusObject);
      ImTable::addKeybind("Gizmo Translate", ctx.inputs.gizmoTranslate);
      ImTable::addKeybind("Gizmo Rotate", ctx.inputs.gizmoRotate);
      ImTable::addKeybind("Gizmo Scale", ctx.inputs.gizmoScale);
      ImTable::addKeybind("Delete Object", ctx.inputs.deleteObject);
      ImTable::addKeybind("Snap Object", ctx.inputs.snapObject);
      ImTable::end();
      ImGui::TreePop();
    }
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
