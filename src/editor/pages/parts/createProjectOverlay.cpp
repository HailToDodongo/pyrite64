/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "createProjectOverlay.h"
#include "../../../utils/proc.h"
#include "json.hpp"
#include "../../../utils/fs.h"
#include "../../actions.h"
#include "../../imgui/helper.h"
#include "../../imgui/notification.h"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace
{
  constexpr ImU32 BG_COLOR = IM_COL32(0, 0, 0, 190);

  std::string projectName{};
  std::string projectSafeName{};
  std::string projectPath{};

  struct Template
  {
    std::string dir{};
    std::string name{};
  };
  std::vector<Template> templates{};
  int templateIdx{0};

  void scanTemplates()
  {
    templates.clear();
    templateIdx = 0;
    std::error_code ec{};
    for(auto &entry : fs::directory_iterator{"n64/examples", ec}) {
      auto confPath = entry.path() / "project.p64proj";
      if(!fs::exists(confPath))continue;

      Template t{.dir = entry.path().filename().string()};
      try {
        t.name = nlohmann::json::parse(Utils::FS::loadTextFile(confPath)).value("name", "");
      } catch(...) {}
      templates.push_back(t);
    }

    std::sort(templates.begin(), templates.end(), [](const Template &a, const Template &b) {
      if((a.dir == "empty") != (b.dir == "empty"))return a.dir == "empty";
      return a.name < b.name;
    });
  }

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

  [[maybe_unused]] std::string expandHomePath(const std::string &path)
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
  scanTemplates();
}

bool Editor::CreateProjectOverlay::draw()
{
  // set width/height
  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({420_px, 360_px}, ImGuiCond_Always);

  if (ImGui::BeginPopupModal("Create Project", nullptr,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoTitleBar

  ))
  {
    ImGui::Dummy({0, 2_px});
    ImGui::PushFont(nullptr, 24_px);
      const char* title = "Create New Project";
      float titleWidth = ImGui::CalcTextSize(title).x;
      ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleWidth) * 0.5f);
      ImGui::Text("Create New Project");
    ImGui::PopFont();

    ImGui::Dummy({0, 10_px});

    ImGui::Text("Project Name:");
    if(ImGui::InputText("##name", &projectName)) {
      projectSafeName = makeNameSafe(projectName);
    }
    ImGui::Dummy({0, 4_px});

    ImGui::Text("Template:");
    const char* templatePreview = templates.empty() ? "" : templates[templateIdx].name.c_str();
    if(ImGui::BeginCombo("##template", templatePreview)) {
      for(int i=0; i<(int)templates.size(); ++i) {
        if(ImGui::Selectable(templates[i].name.c_str(), i == templateIdx))templateIdx = i;
      }
      ImGui::EndCombo();
    }
    ImGui::Dummy({0, 4_px});

    ImGui::Text("Project Path:");
    ImGui::InputText("##path", &projectPath);
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE, {30_px, 0}))
    {
      Utils::FilePicker::open([](const std::string &path) {
        if (path.empty()) return;
        projectPath = path;
      }, {.title="Choose Folder to create new Project in", .isDirectory = true});
    }

    ImGui::Dummy({0, 4_px});
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

    ImGui::Dummy({0, 10_px});
    ImGui::Separator();
    ImGui::Dummy({0, 6_px});
    // right aligned
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220_px);

    if (ImGui::Button("Cancel", {100_px, 0})) {
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

    bool canCreate = isValid && !projectName.empty() && !projectPath.empty() && !templates.empty();
    if(!canCreate)ImGui::BeginDisabled();
    if (ImGui::Button("Create", {100_px, 0})) {
      nlohmann::json args{};
      args["path"] = fullPath;
      args["name"] = projectName;
      args["rom"] = projectSafeName;
      args["template"] = templates[templateIdx].dir;

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
