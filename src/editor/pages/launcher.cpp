/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "launcher.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <filesystem>
#include <algorithm>

#include "imgui.h"
#include "../imgui/theme.h"
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
  bool isHoverAdd = false;
  bool isHoverLast = false;
  bool isHoverTool = false;

  void renderSubText(
    float centerPosX, const ImVec2 &btnSizeLast,
    float midBgPointY, const char* text, float fontSize
  ) {
    ImGui::PushFont(nullptr, fontSize);
    ImGui::SetCursorPos({
      centerPosX - (ImGui::CalcTextSize(text).x / 2) + 6_px,
      midBgPointY + (btnSizeLast.y / 2) + 10_px
    });

    ImGui::Text("%s", text);
    ImGui::PopFont();
  }
}

Editor::Launcher::Launcher(SDL_GPUDevice* device)
  : gpuDevice{device},
  texTitle{device, "data/img/titleLogo.png"},
  texBtnAdd{device, "data/img/cardAdd.svg"},
  texBtnOpen{device, "data/img/cardLast.svg"},
  texBtnTool{device, "data/img/cardTool.svg"},
  texBtnCard{device, "data/img/cardBlank.png"},
  texBG{device, "data/img/splashBG.png"}
{
  ctx.toolchain.scan();
}

Editor::Launcher::~Launcher() {
}

Renderer::Texture* Editor::Launcher::getCardImage(const std::string &path)
{
  if(path.empty() || !std::filesystem::exists(path)) return nullptr;

  auto it = cardImageCache.find(path);
  if(it == cardImageCache.end()) {
    it = cardImageCache.emplace(path, std::make_unique<Renderer::Texture>(gpuDevice, path)).first;
  }
  auto *tex = it->second.get();
  return (tex && tex->getWidth() > 0) ? tex : nullptr;
}

void Editor::Launcher::draw()
{
  float BTN_SPACING = 300_px;
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

  float topBgHeight = 5_px;
  float bottomBgHeight = 2.5_px;
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
  midBgPointY -= 12_px;

  // When recents show, they become the largest row and the action cards shrink to a small row below them.
  // Otherwise the action cards stay full-size and centered, with no recents below
  bool toolchainReady = toolState.hasToolchain && toolState.hasLibdragon && toolState.hasTiny3d && toolState.upToDateLibs;
  bool showRecents = toolchainReady && !ctx.prefs.recentProjects.empty();

  float zoom = ImGui::Theme::zoomFactor;
  float mainScale = showRecents ? 0.52f : 0.80f;
  float subTextSize = showRecents ? 17_px : 24_px;
  BTN_SPACING = showRecents ? 195_px : 300_px;

  constexpr float RECENT_SCALE = 0.75f;
  float recentsRowY = midBgPointY - 180_px;
  float cardsY = midBgPointY;
  if(showRecents) {
    float recentH = texBtnOpen.getSize(RECENT_SCALE).y * zoom;
    float actionH = texBtnOpen.getSize(mainScale).y * zoom;
    cardsY = recentsRowY + recentH + 90_px + actionH * 0.5f;
  }

  ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

  // Title
  if (isHoverAdd || isHoverLast) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  } else {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
  }

  auto logoSize = texTitle.getSize(0.45f * ImGui::Theme::zoomFactor);
  ImGui::SetCursorPos({
    centerPos.x - (logoSize.x/2) + 16_px,
    21_px
  });
  ImGui::Image(ImTextureID(texTitle.getGPUTex()),logoSize);

  auto renderButton = [&](Renderer::Texture &img, const char* text, bool& hover, int &posX) -> bool
  {
    auto btnSizeAdd = img.getSize(hover ? mainScale * 1.0625f : mainScale);
    btnSizeAdd *= ImGui::Theme::zoomFactor;

    ImVec2 btnPos{
      posX  - (btnSizeAdd.x/2),
      cardsY - (btnSizeAdd.y/2),
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
      btnSizeAdd, cardsY, text, subTextSize
    );

    posX += BTN_SPACING;
    return res;
  };

  // Buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));

  bool validToolchain = toolState.hasToolchain && toolState.hasLibdragon && toolState.hasTiny3d;
  int buttonCount = (validToolchain && toolState.upToDateLibs) ? 3 : 1;

  // screen center
  int posX = (int)centerPos.x - 6_px;
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

  const char* toolchainText = validToolchain ?
    (toolState.upToDateLibs ? "Toolchain" : "Update Toolchain")
    : "Install Toolchain";
  if(renderButton(texBtnTool, toolchainText, isHoverTool, posX))
  {
    ToolchainOverlay::open();
  }

  if(!validToolchain || !toolState.upToDateLibs) {
    ImGui::PushFont(nullptr, 32_px);
    const char* warnText = validToolchain
      ? (ICON_MDI_ALERT " Toolchain not up to date")
      : (ICON_MDI_ALERT " Toolchain not found");

    float textWidth = ImGui::CalcTextSize(warnText).x;
    ImGui::SetCursorPos({
      centerPos.x - (textWidth / 2),
      cardsY - (texBtnTool.getHeight() * 0.8f / 2) - 50_px
    });
    
    ImGui::TextColored({1.0f, 0.2f, 0.2f, 1.0f}, "%s", warnText);
    ImGui::PopFont();
    
  }

  ImGui::PopStyleColor(3);

  auto &recents = ctx.prefs.recentProjects;
  if(showRecents)
  {
    constexpr int MAX_SLOTS = 10;
    static bool recentHover[MAX_SLOTS]{};

    // Size off the main-card art so it's independent of the cartridge PNG resolution.
    ImVec2 cardBase = texBtnOpen.getSize(RECENT_SCALE);
    cardBase.x *= zoom; cardBase.y *= zoom;
    float gap = cardBase.x * 0.20f;

    float sideMargin = 80_px;
    float slotW = cardBase.x + gap;
    int fitSlots = (int)((io.DisplaySize.x - 2 * sideMargin + gap) / slotW);
    int total = (int)recents.size();
    int slots = std::clamp(fitSlots, 1, MAX_SLOTS);

    // Match slot-count to the full entry count so entries stay centered with equal placeholder padding on each side
    if((slots % 2) != (total % 2)) slots = std::max(1, slots - 1);
    int cards = std::min(total, slots);
    int pad = (slots - cards) / 2;

    float totalW = slots * cardBase.x + (slots - 1) * gap;
    float startX = centerPos.x - (totalW / 2);

    float rowY = recentsRowY;

    // label cutout in cardBlank.png, as fractions of the card art
    constexpr float LABEL_X = 0.257f;
    constexpr float LABEL_Y = 0.105f;
    constexpr float LABEL_WIDTH = 0.482f;
    constexpr float LABEL_HEIGHT = 0.789f;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    int removeIdx = -1;
    auto* dl = ImGui::GetWindowDrawList();
    for(int i = 0; i < slots; ++i)
    {
      ImGui::PushID(i);
      float slotX = startX + i * (cardBase.x + gap);

      int cardIdx = i - pad;
      if(cardIdx < 0 || cardIdx >= cards) {
        ImGui::SetCursorPos({slotX, rowY});
        ImVec2 s = ImGui::GetCursorScreenPos();
        ImVec2 center{s.x + cardBase.x * 0.5f, s.y + cardBase.y * 0.5f};
        dl->AddCircle(center, 14_px, ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.35f), 0, 2.5f);
        ImGui::PopID();
        continue;
      }

      const auto &r = recents[cardIdx];
      bool exists = std::filesystem::exists(r.path);
      bool &hover = recentHover[i];

      ImVec2 cardSz = texBtnOpen.getSize(hover ? RECENT_SCALE * 1.0625f : RECENT_SCALE);
      cardSz.x *= zoom; cardSz.y *= zoom;
      ImGui::SetCursorPos({slotX + (cardBase.x - cardSz.x) * 0.5f, rowY + (cardBase.y - cardSz.y) * 0.5f});
      ImVec2 cardScreen = ImGui::GetCursorScreenPos();

      float alpha = exists ? (hover ? 1.0f : 0.85f) : 0.5f;
      constexpr ImU32 tint = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);

      // Draw the label first, the cartridge PNG is a transparent cutout
      ImVec2 labelMin{cardScreen.x + LABEL_X * cardSz.x, cardScreen.y + LABEL_Y * cardSz.y};
      ImVec2 labelMax{labelMin.x + LABEL_WIDTH * cardSz.x, labelMin.y + LABEL_HEIGHT * cardSz.y};
      dl->AddRectFilled(labelMin, labelMax, IM_COL32(58, 61, 68, 255));
      if(auto *cardImg = exists ? getCardImage(r.cardImage) : nullptr) {
        // Aspect-preserving: fill height, crop width when too wide, else center with side bars.
        float winWidth = labelMax.x - labelMin.x, winHeight = labelMax.y - labelMin.y;
        float imAspect = (float)cardImg->getWidth() / (float)cardImg->getHeight();
        float winAspect = winWidth / winHeight;
        if(imAspect > winAspect) {
          float u0 = (1.0f - winAspect / imAspect) * 0.5f;
          dl->AddImage(ImTextureID(cardImg->getGPUTex()), labelMin, labelMax, {u0, 0}, {1.0f - u0, 1}, tint);
        } else {
          float dispW = winHeight * imAspect;
          float cx = (labelMin.x + labelMax.x) * 0.5f;
          dl->AddImage(ImTextureID(cardImg->getGPUTex()), {cx - dispW * 0.5f, labelMin.y}, {cx + dispW * 0.5f, labelMax.y}, {0,0}, {1,1}, tint);
        }
      } else if(!exists) {
        ImVec2 posTxt{(labelMin.x + labelMax.x) * 0.5f, (labelMin.y + labelMax.y) * 0.5f};
        float gs = 28_px;
        dl->AddText(ImGui::GetFont(), gs, {posTxt.x - gs * 0.4f, posTxt.y - gs * 0.5f}, IM_COL32(255,120,120,200), ICON_MDI_ALERT);
      }

      bool clicked = ImGui::ImageButton("##recent",
        ImTextureID(texBtnCard.getGPUTex()), cardSz, {0,0}, {1,1}, {0,0,0,0},
        {alpha, alpha, alpha, 1}
      );
      hover = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
      if(hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

      if(ImGui::BeginPopupContextItem("##recentCtx")) {
        if(ImGui::MenuItem(ICON_MDI_DELETE " Remove from list")) removeIdx = cardIdx;
        ImGui::EndPopup();
      }

      std::string name = r.name.empty() ? std::filesystem::path(r.path).stem().string() : r.name;
      ImGui::PushFont(nullptr, 18_px);
      float textW = ImGui::CalcTextSize(name.c_str()).x;
      ImGui::SetCursorPos({slotX + (cardBase.x - textW) / 2, rowY + (cardBase.y + cardSz.y) / 2 + 4_px});
      if(exists) ImGui::Text("%s", name.c_str());
      else       ImGui::TextDisabled("%s", name.c_str());
      ImGui::PopFont();

      if(clicked) {
        if(!exists) {
          removeIdx = cardIdx;
          Editor::Noti::add(Editor::Noti::ERROR, "Project no longer exists, removed from list.");
        } else if(!Actions::call(Actions::Type::PROJECT_OPEN, r.path)) {
          Editor::Noti::add(Editor::Noti::ERROR, "Could not open project!");
        }
      }
      ImGui::PopID();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if(removeIdx >= 0) {
      ctx.prefs.removeRecentProject(recents[removeIdx].path);
      ctx.prefs.save();
    }
  }

  // version + credits
  {
    float PADDING = 24_px;
    float FONT_SIZE = 18_px;

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

  // Debug: hold Space to show guide lines
  /*if(ImGui::IsKeyDown(ImGuiKey_Space)) {
    auto* fg = ImGui::GetForegroundDrawList();
    auto vline = [&](float f, ImU32 col) { fg->AddLine({io.DisplaySize.x * f, 0}, {io.DisplaySize.x * f, io.DisplaySize.y}, col, 1.0f); };
    auto hline = [&](float f, ImU32 col) { fg->AddLine({0, io.DisplaySize.y * f}, {io.DisplaySize.x, io.DisplaySize.y * f}, col, 1.0f); };

    ImU32 c8 = IM_COL32(255, 0, 0, 55);
    ImU32 c4 = IM_COL32(255, 0, 0, 120);
    ImU32 c2 = IM_COL32(255, 0, 0, 255);

    for(float f : {0.125f, 0.375f, 0.625f, 0.875f}) { vline(f, c8); hline(f, c8); }
    for(float f : {0.25f, 0.75f}) { vline(f, c4); hline(f, c4); }
    vline(0.5f, c2); hline(0.5f, c2);
  }*/

  ImGui::End();
}
