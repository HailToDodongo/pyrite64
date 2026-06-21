/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>

#include "imgui.h"

namespace ImGui::Theme
{
  extern float zoomFactor;

  struct ThemeInfo
  {
    std::string id;   // file stem, used to load/remember the theme
    std::string name; // display name from the JSON
  };

  // Loads the named theme (data/themes/<name>.json) and applies it on the next frame.
  void setTheme(const std::string &name = "dark");
  // Id of the currently active theme.
  const std::string &getCurrentTheme();
  // Available themes found in data/themes/, sorted by display name.
  std::vector<ThemeInfo> getThemes();

  ImVec4 getColor(const std::string &key, const ImVec4 &fallback);
  ImU32 getColorU32(const std::string &key, ImU32 fallback);

  void changeZoom(int levelDirection);
  float getZoom();

  int getZoomLevel();
  void setZoomLevel(int level);

  void update();
  ImFont *getFontMono();
}

inline float operator""_px(long double value) {
  return static_cast<float>(value) * ImGui::Theme::zoomFactor;
}
inline float operator""_px(unsigned long long value) {
  return static_cast<float>(value) * ImGui::Theme::zoomFactor;
}