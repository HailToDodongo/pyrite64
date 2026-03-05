/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

#include "imgui.h"

namespace ImGui::Theme
{
  extern float zoomFactor;

  void setTheme(const std::string &name = "default");
  void setZoom(float zoomLevel = 1.0f);
  void changeZoom(int levelDirection);
  float getZoom();

  void update();
  ImFont *getFontMono();
}

inline float operator""_px(long double value) {
  return static_cast<float>(value) * ImGui::Theme::zoomFactor;
}
inline float operator""_px(unsigned long long value) {
  return static_cast<float>(value) * ImGui::Theme::zoomFactor;
}