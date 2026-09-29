/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "migrationOverlay.h"

#include "../../imgui/helper.h"
#include "../../imgui/notification.h"
#include "../../../context.h"
#include "../../../utils/logger.h"

namespace
{
  using Project::Migration::DocType;

  constinit bool requestOpen = false;
  constinit bool isOpen = false;

  Project::Migration::ScanResult pendingScan{};
  std::string pendingTitle{};
  std::function<void()> pendingConfirm{};
  std::function<void()> pendingCancel{};

  void reset() {
    pendingScan = {};
    pendingConfirm = {};
    pendingCancel = {};
  }

  /// A new document kind renders with the generic icon until it is given one here.
  const char* docIcon(DocType type)
  {
    switch(type) {
      case DocType::SCENE:  return ICON_MDI_MOVIE_OPEN_OUTLINE;
      case DocType::PREFAB: return ICON_MDI_CUBE_OUTLINE;
      default:              return ICON_MDI_FILE_OUTLINE;
    }
  }

  void open(const Project::Migration::ScanResult &scan, const char *title,
            std::function<void()> onConfirm, std::function<void()> onCancel)
  {
    pendingScan = scan;
    pendingTitle = title;
    pendingConfirm = std::move(onConfirm);
    pendingCancel = std::move(onCancel);
    requestOpen = true;
  }
}

void Editor::MigrationOverlay::guard(const Project::Migration::ScanResult &scan, const char *title,
                                     std::function<void()> onConfirm, std::function<void()> onCancel)
{
  if(scan.empty()) {
    if(onConfirm)onConfirm();
    return;
  }
  open(scan, title, std::move(onConfirm), std::move(onCancel));
}

void Editor::MigrationOverlay::draw()
{
  if(requestOpen) {
    requestOpen = false;
    isOpen = true;
    ImGui::OpenPopup("Project Update");
  }
  if(!isOpen)return;

  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({620_px, 0}, ImGuiCond_Always);

  if(!ImGui::BeginPopupModal("Project Update", nullptr,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize))
  {
    return;
  }

  ImGui::PushFont(nullptr, 22_px);
    ImGui::Text(ICON_MDI_UPDATE " %s", pendingTitle.c_str());
  ImGui::PopFont();
  ImGui::Dummy({0, 6_px});

  ImGui::TextWrapped(
    "This project was made with an older version of Pyrite64. "
    "%s have to be updated before they can be used.",
    Project::Migration::describe(pendingScan).c_str()
  );

  if(!pendingScan.summaries.empty())
  {
    ImGui::Dummy({0, 8_px});
    ImGui::SeparatorText("What changes");
    for(const char *summary : pendingScan.summaries) {
      ImGui::Bullet();
      ImGui::TextWrapped("%s", summary);
    }
  }

  ImGui::Dummy({0, 8_px});
  ImGui::SeparatorText("Files that will be rewritten");
  if(ImGui::BeginChild("##migFiles", {0, 120_px}, ImGuiChildFlags_Borders))
  {
    for(const auto &doc : pendingScan.docs) {
      ImGui::Text("%s %s", docIcon(doc.type), doc.name.c_str());
    }
  }
  ImGui::EndChild();

  ImGui::Dummy({0, 8_px});
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    ImGui::TextWrapped(
      ICON_MDI_ALERT " These files are changed in place and the old format cannot be restored. "
      "Commit your project to source control or make a backup before continuing."
    );
  ImGui::PopStyleColor();

  ImGui::Dummy({0, 10_px});

  bool confirmed = false;
  bool cancelled = false;

  if(ImGui::Button(ICON_MDI_CHECK " Update Project", {200_px, 30_px}))confirmed = true;
  ImGui::SameLine();
  if(ImGui::Button(ICON_MDI_CLOSE " Cancel", {150_px, 30_px}))cancelled = true;
  ImGui::SameLine();
  ImGui::TextDisabled("Cancelling closes the project unchanged.");

  if(confirmed || cancelled)
  {
    // copy first, the callback may open this dialog again
    auto scan = pendingScan;
    auto onConfirm = std::move(pendingConfirm);
    auto onCancel = std::move(pendingCancel);
    reset();

    ImGui::CloseCurrentPopup();
    isOpen = false;
    ImGui::EndPopup();

    if(confirmed) {
      Project::Migration::apply(*ctx.project, scan);
      Editor::Noti::add(Editor::Noti::Type::SUCCESS, "Project updated to the current format");
      if(onConfirm)onConfirm();
    } else {
      Utils::Logger::log("Project update declined, nothing was changed");
      if(onCancel)onCancel();
    }
    return;
  }

  ImGui::EndPopup();
}
