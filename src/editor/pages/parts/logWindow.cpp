/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "logWindow.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/logger.h"
#include "../../imgui/theme.h"
#include "imgui_internal.h"
#include "../../../utils/fs.h"
#include "../../../utils/time.h"
#include "../../imgui/notification.h"

namespace
{
  constinit uint32_t lastLen{0};
}

void Editor::LogWindow::draw()
{
  auto log = Utils::Logger::getLog();

  //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // Remove space between children

  ImGui::BeginChild("TOP", ImVec2(0, 22),
    ImGuiChildFlags_AlwaysUseWindowPadding,
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
  );

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    constexpr float btnWidth = 64;
    if(ImGui::Button("Clear", {btnWidth, 0})) {
      Utils::Logger::clear();
    }

    ImGui::SameLine();
    if(ImGui::Button("Copy", {btnWidth, 0})) {
      ImGui::SetClipboardText(log.c_str());
    }

    ImGui::SameLine();
    if(ImGui::Button("Save", {btnWidth, 0}))
    {
      std::string dateTimeStr = Utils::Time::getDateTimeStr();
      auto path = fs::path{ctx.project->getPath()} / ("log_" + dateTimeStr + ".txt");
      if(Utils::FS::saveTextFile(path, log)) {
        Editor::Noti::add(Noti::Type::SUCCESS, "Log saved to: " + path.string());
      } else {
        Editor::Noti::add(Noti::Type::ERROR, "Failed to save log to: " + path.string());
      }
    }

    ImGui::PopStyleVar(2);

  ImGui::EndChild();

  ImGui::BeginChild("LOG", ImVec2(0, 0), ImGuiChildFlags_Borders);
  ImGui::PushFont(ImGui::getFontMono());

  ImGui::PushID("LOG");
  ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.05f, 0.05f, 0.06f, 1.0f});

  auto &logStripped = Utils::Logger::getLogStripped();
  ImGui::InputTextMultiline("", (char*)logStripped.data(), logStripped.size()+1, {
    ImGui::GetWindowSize().x-6,
    ImGui::GetWindowSize().y-6
  }, ImGuiInputTextFlags_ReadOnly);


  if (lastLen != log.length()) {
    lastLen = log.length();
    const char* child_window_name = NULL;
    ImFormatStringToTempBuffer(&child_window_name, NULL, "Log/LOG_43838E2B/%08X", ImGui::GetID(""));
    ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);
    if(child_window) {
      ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
    }
  }

  ImGui::PopStyleColor();
  ImGui::PopID();

  ImGui::PopFont();
  ImGui::EndChild();

  ImGui::PopStyleVar(2);
}
