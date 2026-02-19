/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "toolchainOverlay.h"
#include "../../actions.h"
#include "../../imgui/helper.h"
#include "../../../context.h"
#include <iostream>
#include <cstdlib>

namespace
{
  constexpr ImVec4 BG_COLOR = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

  constexpr ImVec4 STEP_ACTIVE = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  constexpr ImVec4 STEP_INACTIVE = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);


  constexpr ImVec2 BUTTON_SIZE = ImVec2(110, 90);
  constexpr float BUTTON_SPACING = 50.0f;
  constinit int checkTimer = 0;

  std::string projectName{};
  std::string projectSafeName{};
  std::string projectPath{};

  // draws a rounded square with text inside
  void drawStep(ImVec2 &pps, const char* text, bool done, bool nextArrow = true)
  {
    ImGui::PushStyleColor(ImGuiCol_Button, done ? STEP_ACTIVE : STEP_INACTIVE);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, done ? STEP_ACTIVE : STEP_INACTIVE);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, done ? STEP_ACTIVE : STEP_INACTIVE);

    ImGui::SetCursorPos(pps);
    ImGui::Button(text, BUTTON_SIZE);

    const char* icon = done ? ICON_MDI_CHECK_CIRCLE : ICON_MDI_ALERT_CIRCLE;
    ImGui::SetCursorPos({
      pps.x + BUTTON_SIZE.x - 24,
      pps.y,
    });

    ImGui::PushFont(nullptr, 24);
    ImGui::TextColored(
      done ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
      "%s", icon
    );
    ImGui::PopFont();

    ImGui::PopStyleColor(3);

    if (nextArrow) {
      ImGui::SetCursorPos({
        pps.x + BUTTON_SIZE.x + 10,
        pps.y + (BUTTON_SIZE.y / 2) - 10
      });
      ImGui::PushFont(nullptr, 32);
      ImGui::TextColored(
        {1.0f, 1.0f, 1.0f, 0.4f},
        ICON_MDI_ARROW_RIGHT_BOLD
      );
      ImGui::PopFont();
    }

    pps.x += BUTTON_SIZE.x + BUTTON_SPACING;
  }
}

void Editor::ToolchainOverlay::open()
{
  ImGui::OpenPopup("Toolchain");
}

bool Editor::ToolchainOverlay::draw()
{
  #if defined(_WIN32)
    constexpr bool isWindows = true;
  #else
    constexpr bool isWindows = false;
  #endif

  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({800, 400}, ImGuiCond_Always);

  if (ImGui::BeginPopupModal("Toolchain", nullptr,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoTitleBar

  ))
  {
    // set width/height
    if((--checkTimer) <= 0) {
      ctx.toolchain.scan();
      checkTimer = 30;
    }

    auto &toolState = ctx.toolchain.getState();
  
    ImGui::Dummy({0, 2});
    ImGui::PushFont(nullptr, 24);
      const char* title = "Toolchain Manager";
      float titleWidth = ImGui::CalcTextSize(title).x;
      ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleWidth) * 0.5f);
      ImGui::Text("%s", title);
    ImGui::PopFont();

    ImGui::Dummy({0, 10});

    constexpr const char *STEPS[] = {
      isWindows ? "MSYS2" : "N64_INST",
      "Toolchain",
      "Libdragon",
      "Tiny3D"
    };
    bool STEP_DONE[] = {
      !toolState.mingwPath.empty(),
      toolState.hasToolchain,
      toolState.hasLibdragon,
      toolState.hasTiny3d
    };
    constexpr int steps = std::size(STEPS);

    float contentWidth = (BUTTON_SIZE.x * steps) + (BUTTON_SPACING * (steps-1));
    ImVec2 startPos = {
      (ImGui::GetWindowWidth() - contentWidth) * 0.5f,
      ImGui::GetCursorPosY() + 40
    };
    
    bool allDone = true;
    for (int i = 0; i < 4; i++) {
      drawStep(startPos, STEPS[i], STEP_DONE[i], i < 3);
      allDone = allDone && STEP_DONE[i];
    }

    float posX = 106;
    ImGui::SetCursorPos({posX, startPos.y + BUTTON_SIZE.y + 15});

    if(!ctx.toolchain.isInstalling()) 
    {
      if(isWindows)
      {
        if(allDone) {
          ImGui::Text(
            "Toolchain found in: %s\n"
            "The N64 toolchain is correctly installed.\n"
            "If you wish to update it, press the update button below.",
            ctx.toolchain.getState().toolchainPath.string().c_str()
          );
        } else if(STEP_DONE[0]) {
          ImGui::Text(
            "The N64 toolchain is missing or not properly installed.\n"
            "Click the button below to install and update the required components.\n"
            "This process may take a few minutes, and a console popup will appear during installation."
          );
        } else {
          ImGui::Text("MSYS2 is not installed, please download and install it from the link below:");
          ImGui::SetCursorPosX(posX);
          ImGui::TextLinkOpenURL("https://www.msys2.org/", "https://www.msys2.org/");
          ImGui::SetCursorPosX(posX);
          ImGui::Text("During the installation, keep the default path as is at \"C:\\msys64\".");
        }
      } else
      {
        if(allDone) {
          ImGui::Text("The N64 toolchain is correctly installed.");
        } else
        {
          ImGui::Text(
            "The N64 toolchain is missing or not properly installed.\n"
            "Automatic installation is currently only available on Windows.\n"
            "Please follow the guide for libdragon and tiny3d here:\n"
          );

          ImGui::Dummy({0, 4});
          ImGui::SetCursorPosX(posX);

          ImGui::TextLinkOpenURL("Libdragon Wiki", "https://github.com/DragonMinded/libdragon/wiki/Installing-libdragon");
          ImGui::SameLine(); ImGui::Text(" + "); ImGui::SameLine();
          ImGui::TextLinkOpenURL("Tiny3D Docs", "https://github.com/HailToDodongo/tiny3d?tab=readme-ov-file#build");

          ImGui::Dummy({0, 4});
          ImGui::SetCursorPosX(posX);

          ImGui::Text(
            "Make sure to use the 'preview' branch of libdragon,\n"
            "and set the N64_INST environment variable accordingly."
          );
        }
      }
      
      ImGui::SetCursorPos({
        (ImGui::GetWindowWidth() - 150) * 0.5f,
        ImGui::GetCursorPosY() + 20
      });

      if(STEP_DONE[0] && isWindows) {
        if (ImGui::Button(allDone ? "Update" : "Install", {150, 40})) {
          ctx.toolchain.install();
        }
      }

    } else {
      ImGui::Text(
        "Installing and updating the toolchain.\n"
        "This process may take a few minutes, please wait..."
      );
    }
  
    // back button
    if(!ctx.toolchain.isInstalling()) 
    {
      ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110 - 20);
      ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);

      if (ImGui::Button("Back", {100, 0})) {
        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::EndPopup();
  }
  return true;
}
