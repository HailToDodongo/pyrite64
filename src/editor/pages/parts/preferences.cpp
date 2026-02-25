/**
* @copyright 2026 - Nolan Baker
* @license MIT
*/

#include "preferences.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/logger.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../keymap.h"


bool Editor::Preferences::draw()
{
  if (ImGui::CollapsingHeader("Keymap", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Keymap");
    if (ImTable::addComboBox("Preset", (int&)ctx.keymapPreset, { "Blender", "Industry Compatible" })) {
      ctx.applyKeymapPreset();
    }
    ImTable::end();
    if (ImGui::TreeNodeEx("3D View", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Selected)) {
      ImTable::start("3D View");
      Editor::Input::Keymap defaults = ctx.getCurrentKeymapPreset();
      ImTable::addKeybind("Move Forward",    ctx.keymap.moveForward,    defaults.moveForward);
      ImTable::addKeybind("Move Back",       ctx.keymap.moveBack,       defaults.moveBack);
      ImTable::addKeybind("Move Left",       ctx.keymap.moveLeft,       defaults.moveLeft);
      ImTable::addKeybind("Move Right",      ctx.keymap.moveRight,      defaults.moveRight);
      ImTable::addKeybind("Move Up",         ctx.keymap.moveUp,         defaults.moveUp);
      ImTable::addKeybind("Move Down",       ctx.keymap.moveDown,       defaults.moveDown);
      ImTable::addKeybind("Toggle Ortho",    ctx.keymap.toggleOrtho,    defaults.toggleOrtho);
      ImTable::addKeybind("Focus Object",    ctx.keymap.focusObject,    defaults.focusObject);
      ImTable::addKeybind("Gizmo Translate", ctx.keymap.gizmoTranslate, defaults.gizmoTranslate);
      ImTable::addKeybind("Gizmo Rotate",    ctx.keymap.gizmoRotate,    defaults.gizmoRotate);
      ImTable::addKeybind("Gizmo Scale",     ctx.keymap.gizmoScale,     defaults.gizmoScale);
      ImTable::addKeybind("Delete Object",   ctx.keymap.deleteObject,   defaults.deleteObject);
      ImTable::addKeybind("Snap Object",     ctx.keymap.snapObject,     defaults.snapObject);
      ImTable::end();
      ImGui::TreePop();
    }
  }

  if (ImGui::Button("Save")) {
    ctx.project->save();
    return true;
  }
  return false;
}