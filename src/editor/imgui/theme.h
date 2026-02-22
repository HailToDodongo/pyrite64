/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

#include "imgui.h"

namespace ImGui::Theme
{
  void setTheme(const std::string &name = "default");
  void setZoom(float zoomLevel = 1.0f);

  void update();
  ImFont *getFontMono();
}
