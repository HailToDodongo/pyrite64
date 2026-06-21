/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "theme.h"

#include <array>
#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "IconsMaterialDesignIcons.h"
#include "ImGuizmo.h"
#include "notification.h"
#include "json.hpp"
#include "../../utils/prop.h"
#include "../../utils/json.h"

namespace fs = std::filesystem;

constinit float ImGui::Theme::zoomFactor = 1.0f;

namespace
{
  std::string currentThemeId{"dark"};
  nlohmann::json currentThemeJson{};

  // Finds the ImGuiCol_ index for a color name as returned by ImGui::GetStyleColorName.
  int colorIndexByName(const std::string &name)
  {
    for(int i = 0; i < ImGuiCol_COUNT; ++i) {
      if(name == ImGui::GetStyleColorName(i)) return i;
    }
    return -1;
  }

  // Applies the currently loaded theme JSON (colors + a known set of style scalars) onto `style`.
  void applyThemeJson(ImGuiStyle &style)
  {
    if(!currentThemeJson.is_object()) return;

    if(auto it = currentThemeJson.find("colors"); it != currentThemeJson.end()) {
      for(auto &[key, val] : it->items()) {
        int idx = colorIndexByName(key);
        if(idx >= 0 && val.is_array() && val.size() >= 4) {
          style.Colors[idx] = ImVec4(val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>());
        }
      }
    }

    if(auto it = currentThemeJson.find("style"); it != currentThemeJson.end()) {
      const auto &s = *it;
      auto setF = [&](const char* k, float &dst) { if(s.contains(k)) dst = s[k].get<float>(); };
      auto setV = [&](const char* k, ImVec2 &dst) {
        if(s.contains(k) && s[k].is_array() && s[k].size() >= 2) dst = ImVec2(s[k][0].get<float>(), s[k][1].get<float>());
      };
      setF("TabBarOverlineSize", style.TabBarOverlineSize);
      setF("WindowRounding", style.WindowRounding);
      setF("FrameRounding", style.FrameRounding);
      setF("GrabRounding", style.GrabRounding);
      setF("TabRounding", style.TabRounding);
      setF("PopupRounding", style.PopupRounding);
      setF("ScrollbarRounding", style.ScrollbarRounding);
      setF("PopupBorderSize", style.PopupBorderSize);
      setF("WindowBorderSize", style.WindowBorderSize);
      setF("FrameBorderSize", style.FrameBorderSize);
      setV("WindowPadding", style.WindowPadding);
      setV("FramePadding", style.FramePadding);
      setV("ItemSpacing", style.ItemSpacing);
    }
  }

  constexpr std::array<float, 12> ZOOM_VALUES{
    1.0f / 2.0f,
    1.0f / 1.75f,
    1.0f / 1.50f,
    1.0f / 1.25f,
    1.0f,
    1.25f,
    1.50f,
    1.75f,
    2.0f,
    2.5f,
    3.0f,
    4.0f
  };

  constinit ImFont* fontMono{nullptr};
  constinit bool needsUpdate{true};
  constinit int zoomLevel{4};

  void loadFonts(float contentScale = 1.0f)
  {
    ImGuiStyle& style = ImGui::GetStyle();

    if(!fontMono)
    {
      ImGuiIO& io = ImGui::GetIO();
      style.ScaleAllSizes(1.0f);
      style.FontScaleDpi = 1.0f;

      style.FontSizeBase = 15.0f;
      [[maybe_unused]] ImFont* font = io.Fonts->AddFontFromFileTTF("./data/Altinn-DINExp.ttf");
      IM_ASSERT(font != nullptr);

      static const ImWchar icons_ranges[] = { ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };
      ImFontConfig icons_config;
      icons_config.MergeMode = true;
      icons_config.PixelSnapH = true;
      icons_config.GlyphMinAdvanceX = 16.0f;
      font = io.Fonts->AddFontFromFileTTF("./data/materialdesignicons-webfont.ttf", 16, &icons_config, icons_ranges);
      IM_ASSERT(font != nullptr);

      fontMono = io.Fonts->AddFontFromFileTTF("./data/GoogleSansCode.ttf", 16);
      IM_ASSERT(fontMono != nullptr);
    }

    style.FontSizeBase = 15.0f * contentScale;
  }
}

void ImGui::Theme::setTheme(const std::string &name)
{
  currentThemeId = name;
  try {
    currentThemeJson = Utils::JSON::loadFile("data/themes/" + name + ".json");
  } catch(const std::exception &e) {
    printf("Failed to load theme '%s': %s\n", name.c_str(), e.what());
    currentThemeJson = {};
  }
  needsUpdate = true;
}

const std::string &ImGui::Theme::getCurrentTheme()
{
  return currentThemeId;
}

ImVec4 ImGui::Theme::getColor(const std::string &key, const ImVec4 &fallback)
{
  if(auto it = currentThemeJson.find("custom"); it != currentThemeJson.end()) {
    if(auto c = it->find(key); c != it->end() && c->is_array() && c->size() >= 4) {
      return ImVec4((*c)[0].get<float>(), (*c)[1].get<float>(), (*c)[2].get<float>(), (*c)[3].get<float>());
    }
  }
  return fallback;
}

ImU32 ImGui::Theme::getColorU32(const std::string &key, ImU32 fallback)
{
  ImVec4 fb = ImGui::ColorConvertU32ToFloat4(fallback);
  return ImGui::GetColorU32(getColor(key, fb));
}

std::vector<ImGui::Theme::ThemeInfo> ImGui::Theme::getThemes()
{
  std::vector<ThemeInfo> themes;
  std::error_code ec;
  for(const auto &entry : fs::directory_iterator("data/themes", ec)) {
    if(entry.path().extension() != ".json") continue;
    std::string id = entry.path().stem().string();
    std::string name = id;
    try {
      auto j = Utils::JSON::loadFile(entry.path());
      if(j.is_object()) name = j.value("name", id);
    } catch(...) {}
    themes.push_back({id, name});
  }
  std::sort(themes.begin(), themes.end(), [](const ThemeInfo &a, const ThemeInfo &b) {
    return a.name < b.name;
  });
  return themes;
}

void ImGui::Theme::changeZoom(int levelDirection)
{
  setZoomLevel(zoomLevel + levelDirection);
  Editor::Noti::showAction("Zoom: " + std::to_string((int)(zoomFactor * 100)) + "%");
}

float ImGui::Theme::getZoom()
{
  return zoomFactor;
}

int ImGui::Theme::getZoomLevel()
{
  return zoomLevel;
}

void ImGui::Theme::setZoomLevel(int level)
{
  zoomLevel = std::max(0, std::min((int)ZOOM_VALUES.size() - 1, level));
  zoomFactor = ZOOM_VALUES[zoomLevel];
  needsUpdate = true;
}


void ImGui::Theme::update()
{
  if(!needsUpdate)return;
  needsUpdate = false;

  printf("Updating ImGui theme '%s' with zoom level: %.2f\n", currentThemeId.c_str(), zoomFactor);
  ImGuiStyle &style = ImGui::GetStyle();
  style = ImGuiStyle();

  // Colors and style scalars come from the loaded theme JSON (data/themes/<id>.json).
  applyThemeJson(style);

  // Guizmos
  auto &gStyle = ImGuizmo::GetStyle();
  float col1 = 0.9f;
  float col0 = 0.4f;

  gStyle.Colors[ImGuizmo::COLOR::DIRECTION_X] = {col1,col0,col0,1};
  gStyle.Colors[ImGuizmo::COLOR::DIRECTION_Y] = {col0,col1,col0, 1.0f};
  gStyle.Colors[ImGuizmo::COLOR::DIRECTION_Z] = {col0,col0,col1,1};
  gStyle.Colors[ImGuizmo::COLOR::PLANE_X] = gStyle.Colors[ImGuizmo::COLOR::DIRECTION_X];
  gStyle.Colors[ImGuizmo::COLOR::PLANE_Y] = gStyle.Colors[ImGuizmo::COLOR::DIRECTION_Y];
  gStyle.Colors[ImGuizmo::COLOR::PLANE_Z] = gStyle.Colors[ImGuizmo::COLOR::DIRECTION_Z];

  gStyle.TranslationLineThickness = 4;
  gStyle.TranslationLineArrowSize = 7;

  ImGuizmo::SetGizmoSizeClipSpace(0.14f);

  loadFonts(zoomFactor);
  GetStyle().ScaleAllSizes(zoomFactor);
}

ImFont* ImGui::Theme::getFontMono() {
  return fontMono;
}