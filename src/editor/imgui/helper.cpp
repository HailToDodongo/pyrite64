/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "helper.h"

#include "../../context.h"

namespace
{
  constexpr ImVec4 COLOR_NONE{0,0,0,0};
  constinit ImGuiKeyChord* rebindingChord{nullptr};
}

namespace ImTable {
  constinit Project::Object* obj{nullptr};
}

bool ImGui::IconButton(const char* label, const ImVec2 &labelSize, const ImVec4 &color)
{
  ImVec2 min = GetCursorScreenPos();
  ImVec2 max = ImVec2(min.x + labelSize.x, min.y + labelSize.y);
  bool hovered = IsMouseHoveringRect(min, max);
  bool clicked = IsMouseClicked(ImGuiMouseButton_Left) && hovered;

  if(hovered)SetMouseCursor(ImGuiMouseCursor_Hand);
  PushStyleColor(ImGuiCol_Text,
    hovered ? GetStyleColorVec4(ImGuiCol_DragDropTarget) : color
  );

  PushStyleColor(ImGuiCol_Button, COLOR_NONE);
  PushStyleColor(ImGuiCol_ButtonActive, COLOR_NONE);
  PushStyleColor(ImGuiCol_ButtonHovered, COLOR_NONE);

  //SmallButton(label);
  Text("%s", label);

  PopStyleColor(4);
  return clicked;
}

glm::vec3 tmpEuler;
bool ImGui::rotationInput(glm::quat &quat) {
  if(!ctx.prefs.showRotAsEuler) return InputFloat4("##", glm::value_ptr(quat));
  
  glm::quat calcRot = glm::normalize(glm::quat(glm::radians(tmpEuler)));
  glm::quat orgRot = glm::normalize(quat);
  if (glm::dot(calcRot, orgRot) < 1) tmpEuler = glm::degrees(glm::eulerAngles(orgRot));

  InputFloat3("##RotEuler", glm::value_ptr(tmpEuler));
  quat = glm::normalize(glm::quat(glm::radians(tmpEuler)));
  return true;
}

bool ImTable::addKeybind(const std::string &name, ImGuiKeyChord &chord, ImGuiKeyChord defaultValue, bool isChord) {
  add(name);
  ImGui::PushID(name.c_str());

  bool isOverridden = chord != defaultValue;
  float w = isOverridden ? (ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeightWithSpacing()) : -FLT_MIN;

  bool isRebinding = rebindingChord == &chord;

  std::string keyName{};
  const char* label = "Press any key...";
  if(!isRebinding) {
    keyName = Editor::Input::GetKeyChordName(chord);
    label = keyName.empty() ? "<?>" : keyName.c_str();
  }

  if (ImGui::Button(label, ImVec2(w, 0))) {
    rebindingChord = &chord;
  }

  if (isOverridden) {
    ImGui::SameLine(0, 2);
    if (ImGui::Button(ICON_MDI_CLOSE, ImVec2(-FLT_MIN, 0))) {
      chord = defaultValue;
      Editor::UndoRedo::getHistory().markChanged("Reset " + name);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset to default key.");
  }

  if (!isRebinding) {
    ImGui::PopID();
    return false;
  }

  ImGuiIO &io = ImGui::GetIO();
  for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_ReservedForModCtrl; k++) {
    if (!ImGui::IsKeyPressed((ImGuiKey)k)) continue;
    if (isChord && k >= (int)ImGuiKey_LeftCtrl && k <= (int)ImGuiKey_RightSuper) continue;

    ImGuiKeyChord mods = ImGuiKey_None;
    if (isChord) {
      if (io.KeyCtrl)  mods |= ImGuiMod_Ctrl;
      if (io.KeyShift) mods |= ImGuiMod_Shift;
      if (io.KeyAlt)   mods |= ImGuiMod_Alt;
      if (io.KeySuper) mods |= ImGuiMod_Super;
    }

    rebindingChord = nullptr;
    if (k == ImGuiKey_Escape) {
      break;
    } else {
      chord = (ImGuiKey)k | mods;
      Editor::UndoRedo::getHistory().markChanged("Rebind " + name);
      ImGui::PopID();
      return true;
    }
  }

  ImGui::PopID();
  return false;
}
