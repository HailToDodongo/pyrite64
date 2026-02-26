/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "createProjectOverlay.h"
#include "../../actions.h"
#include "../../imgui/helper.h"
#include "../../imgui/notification.h"
#include <iostream>
#include <cstdlib>

namespace
{
  constexpr ImU32 BG_COLOR = IM_COL32(0, 0, 0, 190);

  std::string projectName{};
  std::string projectSafeName{};
  std::string projectPath{};

  std::string makeNameSafe(const std::string &name)
  {
    std::string safeName;
    for (char c : name) {
      if ((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_' || c == '-') {
        safeName += c;
      } else if (c == ' ') {
        safeName += '_';
      }
    }
    return safeName;
  }

  std::string expandHomePath(const std::string &path)
  {
    const char* home = std::getenv("HOME");
    if (!home || !*home) return path;

    if (path.rfind("$HOME", 0) == 0) {
      return std::string(home) + path.substr(5);
    }

    if (!path.empty() && path[0] == '~') {
      return std::string(home) + path.substr(1);
    }

    return path;
  }
}

void Editor::CreateProjectOverlay::open()
{
  ImGui::OpenPopup("Create Project");
  projectName = "New Project";
  projectSafeName = makeNameSafe(projectName);
  projectPath = Utils::Proc::getProjectsPath().string();
}

bool Editor::CreateProjectOverlay::draw()
{
  // set width/height
  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({400, 300}, ImGuiCond_Always);

  if (ImGui::BeginPopupModal("Create Project", nullptr,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoTitleBar

  ))
  {
    ImGui::Dummy({0, 2});
    ImGui::PushFont(nullptr, 24);
      const char* title = "Create New Project";
      float titleWidth = ImGui::CalcTextSize(title).x;
      ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleWidth) * 0.5f);
      ImGui::Text("Create New Project");
    ImGui::PopFont();

    ImGui::Dummy({0, 10});

    ImGui::Text("Project Name:");
    if(ImGui::InputText("##name", &projectName)) {
      projectSafeName = makeNameSafe(projectName);
    }
    ImGui::Dummy({0, 4});

    ImGui::Text("Project Path:");
    ImGui::InputText("##path", &projectPath);
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE, {30, 0}))
    {
      Utils::FilePicker::open([](const std::string &path) {
        if (path.empty()) return;
        projectPath = path;
      }, {.title="Choose Folder to create new Project in", .isDirectory = true});
    }

    ImGui::Dummy({0, 4});
    // text in gray
    ImGui::Text("Project will be created in:");
    fs::path fullPath = projectPath;
    fullPath = fullPath / projectSafeName;

    bool isValid = !fullPath.string().contains(' ');
    if(isValid) {
      ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, "%s", fullPath.string().c_str());  
    } else {
      ImGui::TextColored({1.0f, 0.5f, 0.5f, 1.0f}, "The project path must not contain spaces!");
    }

    ImGui::Dummy({0, 10});
    ImGui::Separator();
    ImGui::Dummy({0, 6});
    // right aligned
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);

    if (ImGui::Button("Cancel", {100, 0})) {
      projectName.clear();
      projectSafeName.clear();
      projectPath.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.91f, 0.57f, 0.15f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.65f, 0.20f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.50f, 0.10f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    bool canCreate = isValid && !projectName.empty() && !projectPath.empty();
    if(!canCreate)ImGui::BeginDisabled();
    if (ImGui::Button("Create", {100, 0})) {
      nlohmann::json args{};
      args["path"] = fullPath;
      args["name"] = projectName;
      args["rom"] = projectSafeName;

      if(Editor::Actions::call(Actions::Type::PROJECT_CREATE, args.dump())) {
        projectName.clear();
        projectSafeName.clear();
        projectPath.clear();

        auto configPath = (fullPath / "project.p64proj").string();
        if(Editor::Actions::call(Actions::Type::PROJECT_OPEN, configPath)) {
          Editor::Noti::add(Editor::Noti::SUCCESS, "Project successfully created!");
          ImGui::CloseCurrentPopup();
        }
      }
    }
    if(!canCreate)ImGui::EndDisabled();

    ImGui::PopStyleColor(4);

    ImGui::EndPopup();
  }
  return true;
}
