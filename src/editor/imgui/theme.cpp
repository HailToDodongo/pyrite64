/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "theme.h"
#include "imgui.h"
#include "IconsMaterialDesignIcons.h"
#include "ImGuizmo.h"

namespace
{
  constinit ImFont* fontMono{nullptr};
  constexpr ImVec4 COLOR_HIGHLIGHT{1.0f, 0.5f, 0.0f, 1.0f};
}

void ImGui::applyTheme()
{
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  // Primary background
  colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);  // #131318
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f); // #131318

  colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.40f, 0.40f, 0.40f, 0.60f);

  // Headers
  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);

  // Buttons
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.38f, 0.50f, 1.00f);

  // Frame BG
  colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.50f, 1.00f);
  colors[ImGuiCol_TabSelected] = ImVec4(0.25f, 0.25f, 0.38f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
  colors[ImGuiCol_TabSelectedOverline] = COLOR_HIGHLIGHT;
  colors[ImGuiCol_DragDropTarget] = COLOR_HIGHLIGHT;

  // Title
  colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

  // Borders
  colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  // Text
  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

  // Highlights
  colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.70f, 1.00f, 0.50f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.80f, 1.00f, 0.75f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.90f, 1.00f, 1.00f);

  // Scrollbar
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);

  // Style tweaks
  constexpr float rounding = 3.0f;
  style.TabBarOverlineSize = 2.0f;
  style.WindowRounding = rounding;
  style.FrameRounding = rounding;
  style.GrabRounding = rounding;
  style.TabRounding = 0;
  style.PopupRounding = rounding;
  style.ScrollbarRounding = rounding;
  style.WindowPadding = ImVec2(10, 10);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 6);
  style.PopupBorderSize = 0.f;

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
}

void ImGui::loadFonts(float contentScale) {
  ImGuiIO& io = ImGui::GetIO();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(contentScale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
  style.FontScaleDpi = contentScale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

  style.FontSizeBase = 15.0f;
  ImFont* font = io.Fonts->AddFontFromFileTTF("./data/Altinn-DINExp.ttf");
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

ImFont* ImGui::getFontMono() {
  return fontMono;
}