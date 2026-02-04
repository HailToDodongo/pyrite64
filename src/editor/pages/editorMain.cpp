/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorMain.h"

#include <atomic>
#include <cstdio>
#include <mutex>

#include "imgui.h"
#include "../actions.h"
#include "../../utils/filePicker.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "parts/createProjectOverlay.h"
#include "SDL3/SDL_dialog.h"

void ImDrawCallback_ImplSDLGPU3_SetSamplerRepeat(const ImDrawList* parent_list, const ImDrawCmd* cmd);

namespace
{
  constexpr float BTN_SPACING = 170;

  bool isHoverAdd = false;
  bool isHoverLast = false;

  void renderSubText(
    float centerPosX, const ImVec2 &btnSizeLast,
    float midBgPointY, const char* text
  ) {
    ImGui::PushFont(nullptr, 24);
    ImGui::SetCursorPos({
      centerPosX - (ImGui::CalcTextSize(text).x / 2) + 6,
      midBgPointY + (btnSizeLast.y / 2) + 10
    });

    ImGui::Text("%s", text);
    ImGui::PopFont();
  }
}

Editor::Main::Main(SDL_GPUDevice* device)
  : texTitle{device, "data/img/titleLogo.png"},
  texBtnAdd{device, "data/img/cardAdd.svg"},
  texBtnOpen{device, "data/img/cardLast.svg"},
  texBG{device, "data/img/splashBG.png"}
{
}

Editor::Main::~Main() {
}

void Editor::Main::draw()
{
  auto &io = ImGui::GetIO();

  ImGui::SetNextWindowPos({0,0}, ImGuiCond_Appearing, {0.0f, 0.0f});
  ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y}, ImGuiCond_Always);
  ImGui::Begin("WIN_MAIN", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
    | ImGuiWindowFlags_NoScrollWithMouse
  );

  ImVec2 centerPos = {io.DisplaySize.x / 2, io.DisplaySize.y / 2};

  // BG
  ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ImplSDLGPU3_SetSamplerRepeat, nullptr);

  float topBgHeight = 7;
  float bottomBgHeight = 3;
  float bgRepeatsX = io.DisplaySize.x / texBG.getWidth();
  ImGui::SetCursorPos({0,0});
  ImGui::Image(ImTextureID(texBG.getGPUTex()),
    {io.DisplaySize.x, (float)texBG.getHeight() * topBgHeight},
    {0,topBgHeight}, {bgRepeatsX,0}
  );
  // bottom

  ImGui::SetCursorPos({0, io.DisplaySize.y - ((float)texBG.getHeight() * bottomBgHeight)});
  ImGui::Image(ImTextureID(texBG.getGPUTex()),
    {io.DisplaySize.x, (float)texBG.getHeight() * bottomBgHeight},
    {0,0}, {bgRepeatsX,bottomBgHeight}
  );

  float midBgPointY = (float)texBG.getHeight() * topBgHeight;
  midBgPointY += io.DisplaySize.y - ((float)texBG.getHeight() * bottomBgHeight);
  midBgPointY /= 2.0f;

  ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

  // Title
  if (isHoverAdd || isHoverLast) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  } else {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
  }

  auto logoSize = texTitle.getSize(0.65f);
  ImGui::SetCursorPos({
    centerPos.x - (logoSize.x/2) + 16,
    28
  });
  ImGui::Image(ImTextureID(texTitle.getGPUTex()),logoSize);

  auto getBtnPos = [&](ImVec2 size, bool isLeft) {
    return ImVec2{
      centerPos.x - (size.x/2) + (isLeft ? -BTN_SPACING : BTN_SPACING),
      midBgPointY - (size.y/2)
    };
  };

  auto renderButton = [&](Renderer::Texture &img, const char* text, bool& hover, bool isLeft) -> bool
  {
    auto btnSizeAdd = img.getSize(hover ? 0.85f : 0.8f);
    ImGui::SetCursorPos(getBtnPos(btnSizeAdd, isLeft));
    bool res = ImGui::ImageButton(isLeft ? "L" : "R",
        ImTextureID(img.getGPUTex()),
        btnSizeAdd, {0,0}, {1,1}, {0,0,0,0},
        {1,1,1, hover ? 1 : 0.8f}
    );
    hover = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);

    if(hover)renderSubText(
      centerPos.x + (isLeft ? -BTN_SPACING : BTN_SPACING),
      btnSizeAdd, midBgPointY, text
    );


    return res;
  };

  // Buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));

  if(renderButton(texBtnAdd, "Create Project", isHoverAdd, true))
  {
    CreateProjectOverlay::open();
  }

  if (renderButton(texBtnOpen, "Open Project", isHoverLast, false)) {
    Utils::FilePicker::open([](const std::string &path) {
      if (path.empty()) return;
      Actions::call(Actions::Type::PROJECT_OPEN, path);
    }, true, "Choose Project Folder");
  }

  ImGui::PopStyleColor(3);

  // version
  ImGui::SetCursorPos({14, io.DisplaySize.y - 30});
  ImGui::Text("Pyrite64 [v0.0.0-alpha]");

  constexpr const char* creditsStr = "©2025-2026 ~ Max Bebök (HailToDodongo)";
  ImGui::SetCursorPos({
    io.DisplaySize.x - 14 - ImGui::CalcTextSize(creditsStr).x,
    io.DisplaySize.y - 30
  });
  ImGui::Text(creditsStr);

  CreateProjectOverlay::draw();

  ImGui::End();
}
