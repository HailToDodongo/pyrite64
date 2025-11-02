/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "helper.h"

namespace
{
  constexpr ImVec4 COLOR_NONE{0,0,0,0};
}

bool ImGui::IconButton(const char* label, const ImVec4 &color)
{
  const ImVec2 labelSize{16,16};
  ImVec2 min = GetCursorScreenPos();
  ImVec2 max = ImVec2(min.x + labelSize.x, min.y + labelSize.y);
  bool hovered = IsMouseHoveringRect(min, max);
  bool clicked = IsMouseClicked(ImGuiMouseButton_Left) && hovered;

  PushStyleColor(ImGuiCol_Text,
    hovered ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : color
  );

  PushStyleColor(ImGuiCol_Button, COLOR_NONE);
  PushStyleColor(ImGuiCol_ButtonActive, COLOR_NONE);
  PushStyleColor(ImGuiCol_ButtonHovered, COLOR_NONE);

  //SmallButton(label);
  Text("%s", label);

  PopStyleColor(4);
  return clicked;
}
