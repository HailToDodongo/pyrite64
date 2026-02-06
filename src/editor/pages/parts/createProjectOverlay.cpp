/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "createProjectOverlay.h"
#include "../../actions.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"

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
}

void Editor::CreateProjectOverlay::open()
{
  ImGui::OpenPopup("Create Project");
  projectName = message(MSG_NEW_PROJECT_DEFAULT_NAME);
  projectSafeName = makeNameSafe(projectName);
  projectPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
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
      const char* title = message(MSG_NEW_PROJECT_TITLE);
      float titleWidth = ImGui::CalcTextSize(title).x;
      ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleWidth) * 0.5f);
      ImGui::Text(title);
    ImGui::PopFont();

    ImGui::Dummy({0, 10});

    ImGui::Text(message(MSG_NEW_PROJECT_NAME));
    if(ImGui::InputText("##name", &projectName)) {
      projectSafeName = makeNameSafe(projectName);
    }
    ImGui::Dummy({0, 4});

    ImGui::Text(message(MSG_NEW_PROJECT_PATH));
    ImGui::InputText("##path", &projectPath);
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE, {30, 0}))
    {
      Utils::FilePicker::open([](const std::string &path) {
        if (path.empty()) return;
        projectPath = path;
      }, true, message(MSG_NEW_PROJECT_CHOOSE_FOLDER));
    }

    ImGui::Dummy({0, 4});
    // text in gray
    ImGui::Text(message(MSG_NEW_PROJECT_CREATE_FOLDER));
    std::filesystem::path fullPath = projectPath;
    fullPath = fullPath / projectSafeName;
    ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, "%s", fullPath.c_str());

    ImGui::Dummy({0, 10});
    ImGui::Separator();
    ImGui::Dummy({0, 6});
    // right aligned
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);

    if (ImGui::Button(message(MSG_NEW_PROJECT_CANCEL), {100, 0})) {
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

    bool canCreate = !projectName.empty() && !projectPath.empty();
    if(!canCreate)ImGui::BeginDisabled();
    if (ImGui::Button(message(MSG_NEW_PROJECT_CREATE), {100, 0})) {
      nlohmann::json args{};
      args["path"] = fullPath;
      args["name"] = projectName;
      args["rom"] = projectSafeName;

      Editor::Actions::call(Actions::Type::PROJECT_CREATE, args.dump());

      projectName.clear();
      projectSafeName.clear();
      projectPath.clear();

      Editor::Actions::call(Actions::Type::PROJECT_OPEN, fullPath.string());

      ImGui::CloseCurrentPopup();
    }
    if(!canCreate)ImGui::EndDisabled();

    ImGui::PopStyleColor(4);

    ImGui::EndPopup();
  }
  return true;
}
