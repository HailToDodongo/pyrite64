/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetsBrowser.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "../../imgui/helper.h"
#include "../../imgui/notification.h"
#include "../../../context.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <SDL3/SDL.h>
#include <string>
#include "../../../utils/logger.h"

using FileType = Project::FileType;
namespace fs = std::filesystem;

namespace
{
  constexpr int TAB_IDX_SCENES = 0;
  constexpr int TAB_IDX_ASSETS = 1;
  constexpr int TAB_IDX_SCRIPTS = 2;
  constexpr int TAB_IDX_PREFABS = 3;

  struct TabDef
  {
    const char* name;
    std::vector<FileType> fileTypes{};
    bool showScenes{false};
  };

  std::string normalizeDir(std::string dir)
  {
    for (auto &c : dir) {
      if (c == '\\') c = '/';
    }
    while (!dir.empty() && dir.front() == '/') dir.erase(dir.begin());
    while (!dir.empty() && dir.back() == '/') dir.pop_back();
    return dir;
  }

  std::string joinDir(const std::string &left, const std::string &right)
  {
    if (left.empty()) return right;
    if (right.empty()) return left;
    return left + "/" + right;
  }

}

void Editor::AssetsBrowser::draw() {
  auto &scenes = ctx.project->getScenes().getEntries();

  const std::array<TabDef, 4> TABS{
    TabDef{
      .name = ICON_MDI_EARTH_BOX "  Scenes",
      .showScenes = true
    },
    TabDef{
      .name = ICON_MDI_FILE "  Assets",
      .fileTypes = {FileType::IMAGE, FileType::AUDIO, FileType::MODEL_3D, FileType::FONT}
    },
    TabDef{
      .name = ICON_MDI_SCRIPT_OUTLINE "  Scripts",
      .fileTypes = {FileType::CODE_OBJ, FileType::CODE_GLOBAL, FileType::NODE_GRAPH}
    },
    TabDef{
      .name = ICON_MDI_PACKAGE_VARIANT_CLOSED "  Prefabs",
      .fileTypes = {FileType::PREFAB}
    },
  };

  ImGui::BeginChild("LEFT", ImVec2(94, 0), ImGuiChildFlags_Borders);
  for (int i=0; i<TABS.size(); ++i) {
    bool isActive = i == activeTab;
    if (ImGui::Selectable(TABS[i].name, isActive))activeTab = i;
  }
  ImGui::EndChild();

  float sceneOptionsWidth = 140;

  if(activeTab == TAB_IDX_SCENES)
  {
    // right align
    ImGui::SameLine();
    ImGui::BeginChild("END", ImVec2(sceneOptionsWidth, 0), ImGuiChildFlags_Borders);

    ImGui::Text("On Boot");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::VectorComboBox("##Boot", scenes, ctx.project->conf.sceneIdOnBoot);

    ImGui::Text("On Reset");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::VectorComboBox("##Reset", scenes, ctx.project->conf.sceneIdOnReset);

    ImGui::EndChild();
  }

  const auto &tab = TABS[activeTab];
  auto &dirState = tabDirs[activeTab];
  dirState = normalizeDir(dirState);

  fs::path basePath{};
  fs::path basePathAbs{};
  const char* baseLabel = nullptr;
  if (activeTab == TAB_IDX_ASSETS || activeTab == TAB_IDX_PREFABS) {
    basePath = fs::path(ctx.project->getPath()) / "assets";
    baseLabel = ICON_MDI_FOLDER " Assets";
  } else if (activeTab == TAB_IDX_SCRIPTS) {
    basePath = fs::path(ctx.project->getPath()) / "src" / "user";
    baseLabel = ICON_MDI_FOLDER " Scripts";
  }
  if (baseLabel) {
    std::error_code absEc;
    basePathAbs = fs::absolute(basePath, absEc);
    if (absEc) {
      basePathAbs = basePath;
    }
  }

  auto availWidth = ImGui::GetContentRegionAvail().x - 24;
  if(activeTab == TAB_IDX_SCENES)availWidth -= sceneOptionsWidth - 4;

  ImGui::SameLine();
  ImGui::BeginChild("RIGHT");

  if (baseLabel)
  {
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_WindowBg));

    ImGui::BeginChild("PATH", ImVec2(0, 21), 0,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove
    );
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));

    std::vector<std::string> crumbParts{};
    if (!dirState.empty()) {
      size_t start = 0;
      while (start < dirState.size()) {
        size_t sep = dirState.find('/', start);
        if (sep == std::string::npos) sep = dirState.size();
        crumbParts.push_back(dirState.substr(start, sep - start));
        start = sep + 1;
      }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    if (ImGui::Button(baseLabel)) {
      dirState.clear();
    }
    std::string accum{};
    for (const auto &part : crumbParts) {
      ImGui::SameLine();
      ImGui::TextUnformatted("/");
      ImGui::SameLine();
      accum = joinDir(accum, part);
      if (ImGui::Button(part.c_str())) {
        dirState = accum;
      }
    }
    ImGui::PopStyleVar(2);

    // search field on the right side
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 160 - 2);
    ImGui::SetNextItemWidth(160);
    ImGui::InputTextWithHint("##search", "Filter...", &searchFilter);

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
  } else {
    ImGui::Dummy({0, 4});
  }

  ImGui::BeginChild("ASSETS");

  float imageSize = 64;
  float itemWidth = imageSize + 18;
  float currentWidth = 0.0f;
  ImVec2 textBtnSize{imageSize+12, imageSize+8};

  float cursorStartX = ImGui::GetCursorPosX();
  float cursorY = ImGui::GetCursorPosY();

  auto checkLineBreak = [&]() {
    if ((currentWidth+itemWidth*2) > availWidth) {
      currentWidth = 0.0f;
      cursorY += imageSize + 28;
      if (activeTab == TAB_IDX_SCENES)cursorY -= 12;

      ImGui::SetCursorPos({cursorStartX, cursorY});
    } else {
      if (currentWidth != 0)ImGui::SameLine();
    }
    currentWidth += itemWidth;
  };

  auto drawLabel = [&](const std::string &label, const ImVec2 &startPos) {
    auto size = ImGui::CalcTextSize(label.c_str());
    ImVec2 rextMin{startPos.x,                startPos.y + imageSize + 8};
    ImVec2 rextMax{startPos.x + imageSize+14, startPos.y + imageSize + 8 + 16};

    if((size.x+3) > (rextMax.x - rextMin.x))
    {
      ImGui::RenderTextEllipsis(
        ImGui::GetWindowDrawList(), rextMin, rextMax, 0,
        label.c_str(), label.c_str() + label.size(),
        nullptr
      );
    } else {
      ImGui::GetWindowDrawList()->AddText(
        {rextMin.x + ((rextMax.x - rextMin.x) - size.x) * 0.5f,
         rextMin.y + ((rextMax.y - rextMin.y) - size.y) * 0.5f},
        ImGui::GetColorU32(ImGuiCol_Text),
        label.c_str()
      );
    }
  };

  auto drawGridButton = [&](const char* id, ImTextureRef icon, const char* iconTxt,
    const std::string &label, bool selected, float alpha) {
    bool clicked = false;
    if(selected) {
      ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
    }

    ImGui::PushID(id);
    auto sPos = ImGui::GetCursorScreenPos();
    drawLabel(label, sPos);

    if(icon._TexID)
    {
      clicked = ImGui::ImageButton("##img", icon,
        {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0},
        {1,1,1, alpha}
      );

    } else {
      ImGui::PushFont(nullptr, 40.0f);
        clicked = ImGui::Button(iconTxt, textBtnSize);
      ImGui::PopFont();
    }

    ImGui::PopID();

    if(selected)ImGui::PopStyleColor(2);
    return clicked;
  };

  std::vector<std::string> folders{};
  std::unordered_set<std::string> folderSet{};
  // Mark folders that contain assets for the active tab
  std::unordered_map<std::string, bool> folderHasAssets{};
  std::vector<const Project::AssetManagerEntry*> assets{};

  if (baseLabel) {
    for(const auto type : tab.fileTypes)
    {
      for (const auto &asset : ctx.project->getAssets().getTypeEntries(type))
      {
        std::error_code ec;
        std::error_code absEc;
        auto assetPathAbs = fs::absolute(fs::path(asset.path), absEc);
        if (absEc) {
          assetPathAbs = fs::path(asset.path);
        }
        auto rel = assetPathAbs.lexically_relative(basePathAbs);
        if (ec) continue;
        auto relStr = rel.generic_string();
        if (relStr == ".") continue;
        if (relStr.starts_with("..")) {
          if (dirState.empty()) {
            assets.push_back(&asset);
          }
          continue;
        }

        if (!dirState.empty()) {
          auto prefix = dirState + "/";
          if (!relStr.starts_with(prefix)) {
            continue;
          }
          relStr = relStr.substr(prefix.size());
        }

        // Split into folders vs files at the current depth
        auto slashPos = relStr.find('/');
        if (slashPos != std::string::npos) {
          auto folder = relStr.substr(0, slashPos);
          if (folderSet.insert(folder).second) {
            folders.push_back(folder);
          }
          folderHasAssets[folder] = true;
        } else {
          assets.push_back(&asset);
        }
      }
    }

    // Also list folders that exist on disk even if empty for this tab
    if (!basePathAbs.empty()) {
      fs::path listRoot = basePathAbs;
      if (!dirState.empty()) {
        listRoot /= dirState;
      }
      std::error_code dirEc;
      for (auto it = fs::directory_iterator(listRoot, dirEc);
           !dirEc && it != fs::directory_iterator();
           it.increment(dirEc)) {
        const auto &dirEntry = *it;
        if (!dirEntry.is_directory()) continue;
        auto name = dirEntry.path().filename().string();
        if (folderSet.insert(name).second) {
          folders.push_back(name);
        }
      }
    }

    std::sort(folders.begin(), folders.end());
    std::sort(assets.begin(), assets.end(), [](const auto *a, const auto *b) {
      return a->name < b->name;
    });

    for (const auto &folder : folders) {
      if(!searchFilter.empty() && !folder.contains(searchFilter)) {
        continue;
      }
      checkLineBreak();

      // Show a filled folder when it contains assets for this tab, outlined (empty) folder otherwise
      const char* folderIcon = folderHasAssets[folder] ? ICON_MDI_FOLDER : ICON_MDI_FOLDER_OUTLINE;
      if (drawGridButton(folder.c_str(), ImTextureRef(nullptr), folderIcon, folder, false, 1.0f)) {
        dirState = joinDir(dirState, folder);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Folder: %s", joinDir(dirState, folder).c_str());
      }
    }
  }

  for (const auto *assetPtr : assets)
  {
    const auto &asset = *assetPtr;
    if(!searchFilter.empty() && !asset.name.contains(searchFilter)) {
      continue;
    }

    checkLineBreak();

    auto icon = ImTextureRef(nullptr);
    const char* iconTxt = ICON_MDI_FILE_OUTLINE;
    if (asset.texture) {
      icon = ImTextureRef(asset.texture->getGPUTex());
    } else {
      if (asset.type == FileType::MODEL_3D) {
        iconTxt = ICON_MDI_CUBE_OUTLINE;
      } else if (asset.type == FileType::AUDIO) {
        iconTxt = ICON_MDI_MUSIC;
      } else if (asset.type == FileType::FONT) {
        iconTxt = ICON_MDI_FORMAT_FONT;
      } else if (asset.type == FileType::PREFAB) {
        iconTxt = ICON_MDI_PACKAGE_VARIANT_CLOSED;
      } else if (asset.type == FileType::CODE_OBJ || asset.type == FileType::CODE_GLOBAL) {
        iconTxt = ICON_MDI_LANGUAGE_CPP;
      } else if (asset.type == FileType::NODE_GRAPH) {
        iconTxt = ICON_MDI_GRAPH_OUTLINE;
      }
    }

    bool isSelected = (ctx.selAssetUUID == asset.getUUID());
    bool clicked = drawGridButton(
      asset.name.c_str(),
      icon,
      iconTxt,
      asset.name,
      isSelected,
      asset.conf.exclude ? 0.25f : 1.0f
    );

    if (clicked) {
      ctx.selAssetUUID = asset.getUUID() == ctx.selAssetUUID ? 0 : asset.getUUID();
    }

    if (ImGui::BeginDragDropSource()) {
      if(icon._TexID) {
        ImGui::ImageButton(asset.name.c_str(), icon, {imageSize*0.75f, imageSize*0.75f});
      } else {
        ImGui::Button(iconTxt, textBtnSize);
      }
      ImGui::SetDragDropPayload("ASSET", &asset.conf.uuid, sizeof(asset.conf.uuid));
      ImGui::EndDragDropSource();
    }

    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
      auto tooltipPath = dirState.empty() ? asset.name : (dirState + "/" + asset.name);
      ImGui::SetTooltip("File: %s", tooltipPath.c_str());
    }

    if(ImGui::BeginPopupContextItem(asset.path.c_str())) {
      if(ImGui::MenuItem(ICON_MDI_FOLDER " Show in File Browser")) {
        showInFileBrowser(asset.path);
      }

      ImGui::EndPopup();
    }
  }

  if(tab.showScenes)
  {
    for (const auto &scene : scenes)
    {
      checkLineBreak();
      auto activeScene = ctx.project->getScenes().getLoadedScene();

      bool isSelected = activeScene && (activeScene->getId() == scene.id);
      if(isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
      }

      if (ImGui::Button(scene.name.c_str(), textBtnSize)) {
        ctx.project->getScenes().loadScene(scene.id);
        ctx.project->conf.sceneIdLastOpened = scene.id;
        ctx.project->saveConfig();
      }

      if(isSelected)ImGui::PopStyleColor(2);

      if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
        ImGui::SetTooltip("Scene: %s\nID: %d", scene.name.c_str(), scene.id);
      }
    }
  }

  static std::string newScriptDir{};

  if (activeTab == TAB_IDX_SCRIPTS || activeTab == TAB_IDX_SCENES)
  {
    checkLineBreak();
    // set textsize to larger

    ImGui::PushFont(nullptr, 32.0f);
    if (ImGui::Button(
      (activeTab == TAB_IDX_SCRIPTS) ? ICON_MDI_FILE_DOCUMENT_PLUS_OUTLINE : ICON_MDI_EARTH_BOX_PLUS,
      textBtnSize
    )) {
      if(activeTab == TAB_IDX_SCRIPTS) {
        newScriptDir = dirState;
        ImGui::OpenPopup("NewScript");
      } else {
        ctx.project->getScenes().add();
      }
    }

    ImGui::PopFont();
  }

  if (activeTab == TAB_IDX_SCRIPTS && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
  {
    ImGui::SetTooltip("Create new Script");
  }
  if (activeTab == TAB_IDX_SCENES && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
  {
    ImGui::SetTooltip("Create new Scene");
  }

  ImGui::Dummy({0, 10});

  if(ImGui::BeginPopup("NewScript"))
  {
    static char scriptName[128] = "New_Script";
    ImGui::Text("Enter script name:");
    ImGui::InputText("##Name", scriptName, sizeof(scriptName));
    if (ImGui::Button("Create")) {
      if (ctx.project->getAssets().createScript(scriptName, newScriptDir)) {
        ImGui::CloseCurrentPopup();
      } else {
        Editor::Noti::add(
          Editor::Noti::Type::ERROR,
          "Failed to create script. File Name may not contain any of [/, \\, :, *, ?, \", <, >, |]."
        );
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::EndChild();
  ImGui::EndChild();
}

void Editor::AssetsBrowser::showInFileBrowser(const std::string& path) {
  Utils::Logger::log(path);//DELETE ME
#if defined(_WIN32)
  const char* args[] = {
    "explorer.exe",
    ("/select," + path).c_str(),
    NULL
  };
#elif defined(__APPLE__)
  const char* args[] = {
    "open",
    "-R",
    path.c_str(),
    NULL
  };
#else
  const char* args[] = {
    "dbus-send",
    "--session",
    "--dest=org.freedesktop.FileManager1",
    "--type=method_call",
    "/org/freedesktop/FileManager1",
    "org.freedesktop.FileManager1.ShowItems",
    ("array:string:\"file://" + path + "\"").c_str(),
    "string:\"\"",
    NULL
  };
#endif

  SDL_CreateProcess(args, false);
}
