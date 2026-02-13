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
#include "../../context.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "parts/createProjectOverlay.h"
#include "parts/toolchainOverlay.h"
#include "SDL3/SDL_dialog.h"
#include "../imgui/notification.h"

void ImDrawCallback_ImplSDLGPU3_SetSamplerRepeat(const ImDrawList* parent_list, const ImDrawCmd* cmd);

namespace
{
  constexpr float BTN_SPACING = 300;

  bool isHoverAdd = false;
  bool isHoverLast = false;
  bool isHoverTool = false;

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
  texBtnTool{device, "data/img/cardTool.svg"},
  texBG{device, "data/img/splashBG.png"}
{
  ctx.toolchain.scan();
}

Editor::Main::~Main() {
}

void Editor::Main::draw()
{
  const auto &toolState = ctx.toolchain.getState();
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

  auto renderButton = [&](Renderer::Texture &img, const char* text, bool& hover, int &posX) -> bool
  {
    auto btnSizeAdd = img.getSize(hover ? 0.85f : 0.8f);
    ImVec2 btnPos{
      posX  - (btnSizeAdd.x/2),
      midBgPointY - (btnSizeAdd.y/2),
    };

    ImGui::SetCursorPos(btnPos);
    bool res = ImGui::ImageButton(std::to_string(posX).c_str(),
        ImTextureID(img.getGPUTex()),
        btnSizeAdd, {0,0}, {1,1}, {0,0,0,0},
        {1,1,1, hover ? 1 : 0.8f}
    );
    hover = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);

    renderSubText(
      btnPos.x + (btnSizeAdd.x / 2),
      btnSizeAdd, midBgPointY, text
    );

    posX += BTN_SPACING;
    return res;
  };

  // Buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));

  bool validToolchain = toolState.hasToolchain && toolState.hasLibdragon && toolState.hasTiny3d;
  int buttonCount = validToolchain ? 3 : 1;

  // screen center
  int posX = (int)centerPos.x - 6;
  if(buttonCount == 3) {
    posX -= (BTN_SPACING);
  }
  
  if(buttonCount == 3) 
  {
    if(renderButton(texBtnAdd, "Create Project", isHoverAdd, posX))
    {
      CreateProjectOverlay::open();
    }

    if (renderButton(texBtnOpen, "Open Project", isHoverLast, posX)) {
      Utils::FilePicker::open([](const std::string &path) {
        if (path.empty()) return;

        // check if path has spaces
        if(path.contains(' ')) {
          Editor::Noti::add(Editor::Noti::ERROR, "Project-Path contains spaces!\nPlease move the directory to avoid build problems.");
          return;
        }

        if(!Actions::call(Actions::Type::PROJECT_OPEN, path)) {
          Editor::Noti::add(Editor::Noti::ERROR, "Could not open project!");
        }
      }, {
        .title="Choose Project File (.p64proj)",
        .isDirectory = false,
        .customFilters = {{"Pyrite64 Project", "p64proj"}}
      });
    }
  }

  const char* toolchainText = validToolchain ? "Toolchain" : "Install Toolchain";
  if(renderButton(texBtnTool, toolchainText, isHoverTool, posX))
  {
    ToolchainOverlay::open();
  }

  if(!validToolchain) {
    ImGui::PushFont(nullptr, 32);
    const char* warnText = ICON_MDI_ALERT " Toolchain not found";
    float textWidth = ImGui::CalcTextSize(warnText).x;
    ImGui::SetCursorPos({
      centerPos.x - (textWidth / 2),
      midBgPointY - (texBtnTool.getHeight() * 0.8f / 2) - 50
    });
    
    ImGui::TextColored({1.0f, 0.2f, 0.2f, 1.0f}, "%s", warnText);
    ImGui::PopFont();
    
  }

  ImGui::PopStyleColor(3);

  // version + credits
  {
    constexpr float PADDING = 24;
    constexpr float FONT_SIZE = 18;

    ImGui::PushFont(nullptr, FONT_SIZE);
    ImGui::SetCursorPos({PADDING, io.DisplaySize.y - FONT_SIZE - PADDING});
    ImGui::Text("v" PYRITE_VERSION);

    constexpr const char* creditsStr = "©2025-2026 - Max Bebök (HailToDodongo)";
    ImGui::SetCursorPos({
      io.DisplaySize.x - PADDING - ImGui::CalcTextSize(creditsStr).x,
      io.DisplaySize.y - FONT_SIZE - PADDING
    });
    ImGui::Text(creditsStr);
    ImGui::PopFont();
  }

  CreateProjectOverlay::draw();
  ToolchainOverlay::draw();

  ImGui::End();
}
