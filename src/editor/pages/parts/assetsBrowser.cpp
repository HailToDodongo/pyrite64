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
#include "../../../utils/proc.h"

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

  std::string scriptName{};
  int scriptType{0};
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

  ImGui::BeginChild("LEFT", ImVec2(94_px, 0), ImGuiChildFlags_Borders);
  for (int i=0; i<TABS.size(); ++i) {
    bool isActive = i == activeTab;
    if (ImGui::Selectable(TABS[i].name, isActive))activeTab = i;
  }
  ImGui::EndChild();

  float sceneOptionsWidth = 140_px;

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

  auto availWidth = ImGui::GetContentRegionAvail().x - 24_px;
  if(activeTab == TAB_IDX_SCENES)availWidth -= sceneOptionsWidth - 4_px;

  ImGui::SameLine();
  ImGui::BeginChild("RIGHT");

  if (baseLabel)
  {
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_WindowBg));

    ImGui::BeginChild("PATH", ImVec2(0, 21_px), 0,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove
    );
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4_px, 3_px));

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

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4_px, 4_px));
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
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 160_px - 2_px);
    ImGui::SetNextItemWidth(160_px);
    ImGui::InputTextWithHint("##search", "Filter...", &searchFilter);

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
  } else {
    ImGui::Dummy({0, 4_px});
  }

  ImGui::BeginChild("ASSETS");

  float imageSize = 64_px;
  float itemWidth = imageSize + 18_px;
  float currentWidth = 0.0f;
  ImVec2 textBtnSize{imageSize+12_px, imageSize+8_px};

  float cursorStartX = ImGui::GetCursorPosX();
  float cursorY = ImGui::GetCursorPosY();

  auto checkLineBreak = [&]() {
    if ((currentWidth+itemWidth*2) > availWidth) {
      currentWidth = 0.0f;
      cursorY += imageSize + 28_px;
      if (activeTab == TAB_IDX_SCENES)cursorY -= 12_px;

      ImGui::SetCursorPos({cursorStartX, cursorY});
    } else {
      if (currentWidth != 0)ImGui::SameLine();
    }
    currentWidth += itemWidth;
  };

  auto drawRename = [&](const std::string &label, const ImVec2 &startPos) {
    ImVec2 rectMin{startPos.x,                startPos.y + imageSize + 8};
    ImVec2 rectMax{startPos.x + imageSize + 14_px, startPos.y + imageSize + 8_px + 16_px};

    ImVec2 originalCursor = ImGui::GetCursorPos();
    ImGui::SetCursorScreenPos(rectMin);
    ImGui::SetNextItemWidth(rectMax.x - rectMin.x);
    if (ImGui::IsWindowAppearing() || !ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2_px, 0));
    if (ImGui::InputText("##renameInput", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      fs::path oldPath = renamePath;
      std::string newFileName = std::string(renameBuffer) + oldPath.extension().string();
      fs::path newPath = oldPath.parent_path() / newFileName;

      std::error_code ec;
      if (oldPath != newPath) {
        if (fs::exists(newPath)) Utils::Logger::log("A file with that name already exists.", Utils::Logger::LEVEL_ERROR);
        else {
          fs::rename(oldPath, newPath, ec);
          if (ec) Utils::Logger::log("Rename failed: " + ec.message(), Utils::Logger::LEVEL_ERROR);
          else {
            fs::path oldConf = oldPath.string() + ".conf";
            fs::path newConf = newPath.string() + ".conf";
            fs::rename(oldConf, newConf, ec);
            if (ec) Utils::Logger::log("Failed to move .conf: " + ec.message(), Utils::Logger::LEVEL_ERROR);
          }
        }
      }
      renamePath.clear();
    }
    ImGui::PopStyleVar();

    if ((!ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      renamePath.clear();
    }

    ImGui::SetCursorPos(originalCursor);
  };

  auto drawLabel = [&](const std::string &label, const ImVec2 &startPos) {
    auto size = ImGui::CalcTextSize(label.c_str());
    ImVec2 rextMin{startPos.x,                   startPos.y + imageSize + 8_px};
    ImVec2 rextMax{startPos.x + imageSize+14_px, startPos.y + imageSize + 8_px + 16_px};

    if((size.x+3_px) > (rextMax.x - rextMin.x))
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

  auto drawGridButton = [&](const std::string &path, ImTextureRef icon, const char* iconTxt,
    const std::string &label, bool selected, float alpha) {
    bool clicked = false;
    if(selected) {
      ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
    }

    ImGui::PushID(path.c_str());
    auto sPos = ImGui::GetCursorScreenPos();
    bool isRenaming = path == renamePath;
    if (isRenaming) drawRename(label, sPos);
    else drawLabel(label, sPos);

    if(icon._TexID)
    {
      clicked = ImGui::ImageButton("##img", icon,
        {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0},
        {1,1,1, alpha}
      );

    } else {
      ImGui::PushFont(nullptr, 40_px);
      clicked = ImGui::Button(iconTxt, textBtnSize);
      ImGui::PopFont();
    }

    ImGui::PopID();

    if(selected)ImGui::PopStyleColor(2);
    return clicked && !isRenaming;
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
      std::string folderPath = (basePathAbs / dirState / folder).string();

      // Show a filled folder when it contains assets for this tab, outlined (empty) folder otherwise
      const char* folderIcon = folderHasAssets[folder] ? ICON_MDI_FOLDER : ICON_MDI_FOLDER_OUTLINE;
      if (drawGridButton(folderPath, ImTextureRef(nullptr), folderIcon, folder, false, 1.0f)) {
        dirState = joinDir(dirState, folder);
      }

      if(ImGui::BeginPopupContextItem(folder.c_str())) {
        showContextMenu(folderPath);
        ImGui::EndPopup();
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
      asset.path,
      icon,
      iconTxt,
      asset.name,
      isSelected,
      asset.conf.exclude ? 0.25f : 1.0f
    );
    bool isDblClick = ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered();

    if (clicked) {
      ctx.selAssetUUID = asset.getUUID();
      ImGui::makeTabVisible("Asset");
    }
    if (isDblClick) {
      if (!Utils::Proc::openFile(asset.path))
      {
        Editor::Noti::add(Editor::Noti::Type::ERROR, "Failed to open File. This may be due to WSL path conversion failure.");
      }
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

    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      auto tooltipPath = dirState.empty() ? asset.name : (dirState + "/" + asset.name);
      ImGui::SetTooltip("File: %s", tooltipPath.c_str());
    }

    if(ImGui::BeginPopupContextItem(asset.path.c_str())) {
      showContextMenu(asset.path);
      ImGui::EndPopup();
    } 
  }

  if (!deletePath.empty()) {
    ImGui::OpenPopup("Confirm Delete");
  }
  
  if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("This action cannot be undone!\nAre you sure you want to delete this asset?");
    ImGui::Separator();
    
    if (ImGui::Button("OK", ImVec2(120_px, 0))) {
        fs::remove(deletePath);
        deletePath.clear();
        ImGui::CloseCurrentPopup(); 
    }

    ImGui::SameLine();
    ImGui::SetItemDefaultFocus();
    if (ImGui::Button("Cancel", ImVec2(120_px, 0))) {
        deletePath.clear();
        ImGui::CloseCurrentPopup(); 
    }
    ImGui::EndPopup();
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

    ImGui::PushFont(nullptr, 32_px);
    if (ImGui::Button(
      (activeTab == TAB_IDX_SCRIPTS) ? ICON_MDI_FILE_DOCUMENT_PLUS_OUTLINE : ICON_MDI_EARTH_BOX_PLUS,
      textBtnSize
    )) {
      if(activeTab == TAB_IDX_SCRIPTS) {
        newScriptDir = dirState;
        scriptName = "New_Script";
        scriptType = 0;
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

  ImGui::Dummy({0, 10_px});

  ImGui::SetNextWindowSize(ImVec2(250_px, 0));
  if(ImGui::BeginPopup("NewScript"))
  {
    ImTable::start("SCRIPT");

    ImTable::add("Type");
    const char* scriptTypes[] = {"Object Script", "Global Script", "Node Graph"};
    ImGui::Combo("##ScriptType", &scriptType, scriptTypes, IM_ARRAYSIZE(scriptTypes));

    ImTable::add("Name");
    ImGui::InputText("##ScriptName", &scriptName);

    ImTable::end();
    // right align buttons
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 112_px);
    if (ImGui::Button("Create"))
    {
      bool res{false};
      if(scriptType == 0)res = ctx.project->getAssets().createScript(scriptName, false, newScriptDir);
      if(scriptType == 1)res = ctx.project->getAssets().createScript(scriptName, true, newScriptDir);
      if(scriptType == 2)res = ctx.project->getAssets().createNodeGraph(scriptName) != 0;

      if (res) {
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

void Editor::AssetsBrowser::showContextMenu(const std::string& path) {
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

  if(ImGui::MenuItem(ICON_MDI_OPEN_IN_NEW " Open")) {
    if (!Utils::Proc::openFile(path)) {
      Editor::Noti::add(Editor::Noti::Type::ERROR, "Failed to open File. This may be due to WSL path conversion failure.");
    }
  }
  
  if(ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Copy Path")) {
    SDL_SetClipboardText(path.c_str());
  }
  
  if(ImGui::MenuItem(ICON_MDI_RENAME " Rename")) {
    renamePath = path;
    std::string stem = fs::path(path).stem().string(); 
    strncpy(renameBuffer, stem.c_str(), sizeof(renameBuffer) - 1);
    renameBuffer[sizeof(renameBuffer) - 1] = '\0';
  }
  
  if(ImGui::MenuItem(ICON_MDI_DELETE " Delete")) {
    deletePath = path;
  }
}
