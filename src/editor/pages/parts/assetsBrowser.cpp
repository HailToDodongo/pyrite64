/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetsBrowser.h"

#include "imgui.h"
#include "../../imgui/theme.h"
#include "../../../context.h"

using FileType = Project::AssetManager::FileType;

namespace
{
  constexpr int ICON_MAX_SIZE = 64;

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
      .fileTypes = {FileType::IMAGE, FileType::AUDIO, FileType::MODEL_3D}
    },
    TabDef{
      .name = ICON_MDI_SCRIPT_OUTLINE "  Scripts",
      .fileTypes = {FileType::CODE_OBJ, FileType::CODE_GLOBAL}
    },
    TabDef{
      .name = ICON_MDI_CUBE_OUTLINE "  Prefabs",
      .fileTypes = {FileType::PREFAB}
    },
  };

  ImGui::BeginChild("LEFT", ImVec2(94, 0), ImGuiChildFlags_Border);
  for (int i=0; i<TABS.size(); ++i) {
    bool isActive = i == activeTab;
    if (ImGui::Selectable(TABS[i].name, isActive))activeTab = i;
  }
  ImGui::EndChild();

  const auto &tab = TABS[activeTab];

  ImGui::SameLine();
  ImGui::BeginChild("RIGHT");

  auto availWidth = ImGui::GetContentRegionAvail().x - 4;
  float imageSize = 48;
  float itemWidth = imageSize + 18;
  float currentWidth = 0.0f;
  ImVec2 textBtnSize{imageSize+12, imageSize+8};

  auto checkLineBreak = [&]() {
    if ((currentWidth+itemWidth) > availWidth) {
      currentWidth = 0.0f;
    } else {
      if (currentWidth != 0)ImGui::SameLine();
    }
    currentWidth += itemWidth;
  };

  for(const auto type : tab.fileTypes)
  {
    for (const auto &asset : ctx.project->getAssets().getTypeEntries(type))
    {
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
        } else if (asset.type == FileType::CODE_OBJ || asset.type == FileType::CODE_GLOBAL) {
          iconTxt = ICON_MDI_LANGUAGE_CPP;
        }
      }

      bool isSelected = (ctx.selAssetUUID == asset.uuid);
      if(isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
      }

      bool clicked{false};
      if(icon._TexID)
      {
        clicked = ImGui::ImageButton(asset.name.c_str(), icon,
          {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0},
          {1,1,1, asset.conf.exclude ? 0.25f : 1.0f}
        );
      } else {
        ImGui::PushFont(nullptr, 32.0f);
        ImGui::PushID((int)asset.uuid);
        clicked = ImGui::Button(iconTxt, textBtnSize);
        ImGui::PopID();
        ImGui::PopFont();
      }

      if (clicked) {
        ctx.selAssetUUID = asset.uuid == ctx.selAssetUUID ? 0 : asset.uuid;
      }

      if (ImGui::BeginDragDropSource()) {
        ImGui::ImageButton(asset.name.c_str(), icon, {imageSize*0.75f, imageSize*0.75f});
        ImGui::SetDragDropPayload("ASSET", &asset.uuid, sizeof(asset.uuid));
        ImGui::EndDragDropSource();
      }

      if(isSelected)ImGui::PopStyleColor(2);

      if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
        ImGui::SetTooltip("File: %s\nUUID: %08lX-%08lX", asset.name.c_str(),
          asset.uuid >> 32, asset.uuid & 0xFFFFFFFF
        );
      }
    }
  }

  if(tab.showScenes)
  {
    for (const auto &scene : scenes)
    {
      checkLineBreak();
      int selId = ctx.project->getScenes().getLoadedScene()->getId();

      bool isSelected = (selId == scene.id);
      if(isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
      }

      if (ImGui::Button(scene.name.c_str(), textBtnSize)) {
        ctx.project->getScenes().loadScene(scene.id);
      }

      if(isSelected)ImGui::PopStyleColor(2);
    }
  }

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
        ImGui::OpenPopup("NewScript");
      } else {
        ctx.project->getScenes().add();
      }
    }
    ImGui::PopFont();
  }

  if(ImGui::BeginPopup("NewScript"))
  {
    static char scriptName[128] = "New_Script";
    ImGui::Text("Enter script name:");
    ImGui::InputText("##Name", scriptName, sizeof(scriptName));
    if (ImGui::Button("Create")) {
      ctx.project->getAssets().createScript(scriptName);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }


  ImGui::EndChild();
}
