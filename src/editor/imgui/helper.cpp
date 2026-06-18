/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "helper.h"

#include "imgui_internal.h"
#include "../../context.h"
#include "../../utils/proc.h"

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

bool ImGui::HelpIcon(const char* docPath, const char* tooltip, float glyphSize)
{
  const float btnWidth = glyphSize + 4_px;
  const float rowHeight = GetFrameHeight();
  const ImVec2 pos = GetCursorScreenPos();

  PushID(docPath);
  const bool clicked = InvisibleButton("##helpIcon", {btnWidth, rowHeight});
  const bool hovered = IsItemHovered();
  const bool held = IsItemActive();
  PopID();

  if (hovered) SetMouseCursor(ImGuiMouseCursor_Hand);

  ImU32 col;
  if (held)         col = GetColorU32(GetStyleColorVec4(ImGuiCol_DragDropTarget)); // pressed accent
  else if (hovered) col = GetColorU32(ImGuiCol_Text, 1.0f);
  else              col = GetColorU32(ImGuiCol_TextDisabled, 0.7f);

  // vertically center the glyph within the row
  const ImVec2 glyphPos{pos.x, pos.y + (rowHeight - glyphSize + 1.5_px) * 0.5f};
  GetWindowDrawList()->AddText(GetFont(), glyphSize, glyphPos, col, ICON_MDI_HELP_CIRCLE_OUTLINE);

  if (hovered && tooltip && tooltip[0]) SetTooltip("%s", tooltip);
  if (clicked && docPath && docPath[0]) {
    Utils::Proc::openURL(std::string{PYRITE_DOCS_URL} + docPath + ".html");
  }
  return clicked;
}

glm::vec3 tmpEuler;
bool ImGui::rotationInput(glm::quat &quat) {
  if(!ctx.prefs.showRotAsEuler) return InputFloat4("##", glm::value_ptr(quat));
  
  glm::quat calcRot = glm::normalize(glm::quat(glm::radians(tmpEuler)));
  glm::quat orgRot = glm::normalize(quat);
  if (glm::dot(calcRot, orgRot) < 1) tmpEuler = glm::degrees(glm::eulerAngles(orgRot));

  if (!InputFloat3("##RotEuler", glm::value_ptr(tmpEuler))) return false;
  quat = glm::normalize(glm::quat(glm::radians(tmpEuler)));
  return true;
}

bool ImTable::bitMaskComboImpl(
  const char *label,
  uint32_t &valueMask,
  const std::vector<std::pair<int, std::string>> &bits,
  const std::string &valueEmpty
) {
  std::string preview;
  for (const auto &[bit, name] : bits) {
    if (valueMask & (1u << bit)) {
      if (!preview.empty()) preview += "  |  ";
      preview += name;
    }
  }
  bool isEmpty = preview.empty();
  if (isEmpty) {
    preview = valueEmpty;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  }

  bool changed = false;
  bool open = ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_None);
  if (isEmpty) ImGui::PopStyleColor();

  if (open) {
    for (const auto &[bit, name] : bits) {
      bool selected = (valueMask & (1u << bit)) != 0;
      auto valText = std::string{selected
        ? ICON_MDI_CHECKBOX_MARKED " "
        : ICON_MDI_CHECKBOX_BLANK_OUTLINE " "
      } + name + "##" + std::to_string(bit);

      if (ImGui::Selectable(valText.c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
        if (selected) valueMask &= ~(1u << bit);
        else          valueMask |=  (1u << bit);
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

void ImTable::addMultiSelectMask8(
  const std::string &name,
  uint32_t &valueMask,
  const std::array<std::string, 8> &values,
  const std::string &valueEmpty
) {
    add(name);
    bool disabled = isPrefabLocked();
    if (disabled) ImGui::BeginDisabled();

    // map the fixed 8-slot array to (bit, name) pairs, skipping unnamed slots
    std::vector<std::pair<int, std::string>> bits;
    for (int i = 0; i < 8; ++i) {
        if (!values[i].empty()) bits.push_back({i, values[i]});
    }

    std::string labelHidden = "##" + name;
    if (bitMaskCombo(labelHidden.c_str(), valueMask, bits, valueEmpty)) {
        Editor::UndoRedo::getHistory().markChanged("Edit " + name);
    }

    if (disabled) ImGui::EndDisabled();
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

void ImGui::makeTabVisible(const char* tabName)
{
  ImGuiWindow* window = ImGui::FindWindowByName(tabName);
  if (window == NULL || window->DockNode == NULL || window->DockNode->TabBar == NULL) {
    return;
  }
  window->DockNode->TabBar->NextSelectedTabId = window->TabId;
}
