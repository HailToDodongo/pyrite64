/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "launcher.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include <filesystem>
#include <format>

#include "imgui.h"
#include "../imgui/theme.h"
#include "../actions.h"
#include "../../utils/filePicker.h"
#include "../recentProjects.h"
#include "../../context.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "parts/createProjectOverlay.h"
#include "parts/toolchainOverlay.h"
#include "SDL3/SDL_dialog.h"
#include "../imgui/notification.h"

namespace fs = std::filesystem;

void ImDrawCallback_ImplSDLGPU3_SetSamplerRepeat(const ImDrawList* parent_list, const ImDrawCmd* cmd);

namespace
{
  bool isHoverAdd = false;
  bool isHoverLast = false;
  bool isHoverTool = false;

  void renderSubText(
    float centerPosX, const ImVec2 &btnSizeLast,
    float midBgPointY, const char* text
  ) {
    ImGui::PushFont(nullptr, 24_px);
    ImGui::SetCursorPos({
      centerPosX - (ImGui::CalcTextSize(text).x / 2) + 6_px,
      midBgPointY + (btnSizeLast.y / 2) + 10_px
    });

    ImGui::Text("%s", text);
    ImGui::PopFont();
  }
}

Editor::Launcher::Launcher(SDL_GPUDevice* device)
  : texTitle{device, "data/img/titleLogo.png"},
  texBtnAdd{device, "data/img/cardAdd.svg"},
  texBtnOpen{device, "data/img/cardLast.svg"},
  texBtnTool{device, "data/img/cardTool.svg"},
  texBG{device, "data/img/splashBG.png"}
{
  ctx.toolchain.scan();
  Editor::RecentProjects::getMostRecentPath();
  projectEntries = {};
  for(auto path : Editor::RecentProjects::recentPaths) {
    auto json = Utils::JSON::loadFile(path);
    if (json.empty()) continue;
    Editor::ProjectEntry entry;
    entry.name = json.value("name", "");
    entry.path = path;
    entry.editorVersion = json.value("editorVersion", PYRITE_VERSION);
    fs::path projPath{path};
    auto writeTime = fs::last_write_time(projPath);
    entry.lastModified = std::format("{:%Y-%m-%d}", writeTime);
    projectEntries.push_back(entry);
  }
}

Editor::Launcher::~Launcher() {
}

void Editor::Launcher::draw()
{
  float BTN_SPACING = 160_px;
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
  float bottomBgHeight = 3_px;
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

  auto logoSize = texTitle.getSize(0.4 * ImGui::Theme::zoomFactor);
  ImGui::SetCursorPos({32_px, 24_px});
  ImGui::Image(ImTextureID(texTitle.getGPUTex()),logoSize);

  auto renderButton = [&](Renderer::Texture &img, const char* text, bool& hover, int &posX) -> bool
  {
    auto btnSizeAdd = img.getSize(hover ? 0.45f : 0.4f);
    btnSizeAdd *= ImGui::Theme::zoomFactor;

    ImVec2 btnPos{
      posX  - (btnSizeAdd.x/2),
      72_px - (btnSizeAdd.y/2),
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
      btnSizeAdd, 72_px, text
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

  int posX = (int)io.DisplaySize.x - BTN_SPACING + 48_px;
  if(buttonCount == 3) {
    posX -= (BTN_SPACING * 2);
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
        .customFilters = {{"Pyrite64 Project", "p64proj"}},
#ifndef _WIN32 //this bit is already handled on Windows
        .defaultPath = Editor::RecentProjects::getMostRecentPath()
#endif
      });
    }
  }

  const char* toolchainText = validToolchain ? "Toolchain" : "Install Toolchain";
  if(renderButton(texBtnTool, toolchainText, isHoverTool, posX))
  {
    ToolchainOverlay::open();
  }

  if(!validToolchain) {
    ImGui::PushFont(nullptr, 32_px);
    const char* warnText = ICON_MDI_ALERT " Toolchain not found";
    float textWidth = ImGui::CalcTextSize(warnText).x;
    ImGui::SetCursorPos({
      centerPos.x - (textWidth / 2),
      midBgPointY - (texBtnTool.getHeight() * 0.8f / 2) - 50_px
    });
    
    ImGui::TextColored({1.0f, 0.2f, 0.2f, 1.0f}, "%s", warnText);
    ImGui::PopFont();
    
  }

  ImGui::PopStyleColor(3);

  // recent files
  ImGui::SetCursorPos({8_px, (float)texBG.getHeight() * topBgHeight - 40_px});
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0,0,0,0));
  if (ImGui::BeginTable("RecentProjects", 5, ImGuiTableFlags_NoBordersInBody)) {
    ImGui::PushFont(nullptr, 20_px);
    ImGui::TableSetupColumn(ICON_MDI_CHEVRON_RIGHT, ImGuiTableColumnFlags_WidthFixed, 32_px);
    ImGui::TableSetupColumn("Project\nName");
    ImGui::TableSetupColumn("Last\nEdit", ImGuiTableColumnFlags_WidthFixed, 120_px);
    ImGui::TableSetupColumn("Editor\nVersion", ImGuiTableColumnFlags_WidthFixed, 80_px);
    ImGui::TableSetupColumn(ICON_MDI_COG, ImGuiTableColumnFlags_WidthFixed, 32_px);
    ImGui::PopFont();
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 50_px); 
    for (int column = 0; column < 5; column++) {
      ImGui::TableSetColumnIndex(column);
      const char* column_name = ImGui::TableGetColumnName(column);
      if (column == 0 || column == 4) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8_px);
      ImGui::TextUnformatted(column_name);
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    auto paths = Editor::RecentProjects::recentPaths;
    int index = 0;
    for(auto entry : projectEntries) {
      //expand arrow
      ImGui::PushID(index);
      ImGui::TableNextRow(ImGuiTableRowFlags_None, 48_px);//TODO make this number bigger when expanded
      ImGui::TableSetColumnIndex(0);
      float y = ImGui::GetCursorPosY();
      ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
      if (ImGui::Selectable("##rowSelectable", false, selectableFlags, ImVec2(0, 48_px))) {
        Editor::Actions::call(Editor::Actions::Type::PROJECT_OPEN, entry.path);
      }
      ImGui::SetCursorPosY(y + 8_px);
      ImGui::TextUnformatted(ICON_MDI_CHEVRON_RIGHT);

      //project name
      ImGui::TableSetColumnIndex(1);
      ImGui::AlignTextToFramePadding();
      ImGui::BeginGroup();
      ImGui::PushFont(nullptr, 16_px);
      ImGui::TextUnformatted(entry.name.c_str());
      ImGui::PopFont();
      ImGui::PushFont(ImGui::Theme::getFontMono(), 16_px);
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextUnformatted(entry.path.c_str());
      ImGui::PopStyleColor();
      ImGui::PopFont();
      ImGui::EndGroup();

      //last modified
      ImGui::TableSetColumnIndex(2);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(entry.lastModified.c_str());

      //editor ersion
      ImGui::TableSetColumnIndex(3);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(entry.editorVersion.c_str());

      //open context menu
      ImGui::TableSetColumnIndex(4);
      ImGui::AlignTextToFramePadding();
      if (ImGui::Button(ICON_MDI_DOTS_HORIZONTAL)) {
        ImGui::OpenPopup("ProjectContextMenu");
      }

      if (ImGui::BeginPopup("ProjectContextMenu")) {
        Editor::Launcher::showContextMenu(entry.path);
        ImGui::EndPopup();
      }
      ImGui::PopID();
      index++;
    }
    ImGui::EndTable();
    ImGui::PopStyleColor();
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

  ImGui::End();
}


void Editor::Launcher::showContextMenu(const std::string& path) {
#if defined(_WIN32)
  std::string showPrompt = ICON_MDI_FOLDER_OPEN " Show in Explorer";
#elif defined(__APPLE__)
  std::string showPrompt = ICON_MDI_FOLDER_OPEN " Show in Finder";
#else
  std::string showPrompt = ICON_MDI_FOLDER_OPEN " Show in File Manager";
#endif
  if(ImGui::MenuItem(showPrompt.c_str())) {
    if (!Utils::Proc::openInFileBrowser(path)) {
      Editor::Noti::add(Editor::Noti::Type::ERROR, "Failed to open File Explorer. This may be due to WSL path conversion failure.");
    }
  }
  
  if(ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Copy Path")) {
    SDL_SetClipboardText(path.c_str());
  }
  
  if(ImGui::MenuItem(ICON_MDI_RENAME " Rename Project")) {
    //TODO
  }
  
  if(ImGui::MenuItem(ICON_MDI_DELETE " Remove from List")) {
    //TODO
  }
}