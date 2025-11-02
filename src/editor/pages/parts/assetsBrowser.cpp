/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetsBrowser.h"

#include "imgui.h"
#include "../../../context.h"

using FileType = Project::AssetManager::FileType;

namespace
{
  constexpr int ICON_MAX_SIZE = 64;

  constexpr int TAB_IDX_ASSETS = 0;
  constexpr int TAB_IDX_SCRIPTS = 1;
  constexpr int TAB_IDX_PREFABS = 2;
}

Editor::AssetsBrowser::AssetsBrowser()
  : iconFile{ctx.gpu, "data/img/icons/file.svg", ICON_MAX_SIZE, ICON_MAX_SIZE},
   iconMesh{ctx.gpu, "data/img/icons/mesh.svg", ICON_MAX_SIZE, ICON_MAX_SIZE},
   iconMusic{ctx.gpu, "data/img/icons/music.svg", ICON_MAX_SIZE, ICON_MAX_SIZE},
   iconCodeAdd{ctx.gpu, "data/img/icons/scriptAdd.svg", ICON_MAX_SIZE, ICON_MAX_SIZE},
   iconCodeCpp{ctx.gpu, "data/img/icons/scriptCpp.svg", ICON_MAX_SIZE, ICON_MAX_SIZE}
{
  activeTab = 0;
}

Editor::AssetsBrowser::~AssetsBrowser() {
}

void Editor::AssetsBrowser::draw() {
  auto &assets = ctx.project->getAssets().getEntries();

  const char* TABS[] = {
    ICON_MDI_FILE "  Assets",
    ICON_MDI_SCRIPT_OUTLINE "  Scripts",
    ICON_MDI_CUBE_OUTLINE "  Prefabs"
  };

  ImGui::BeginChild("LEFT", ImVec2(94, 0), ImGuiChildFlags_Border);
  for (int i=0; i<3; ++i) {
    bool isActive = i == activeTab;
    if (ImGui::Selectable(TABS[i], isActive))activeTab = i;
  }
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("RIGHT");

  auto availWidth = ImGui::GetContentRegionAvail().x - 4;
  float imageSize = 64;
  float itemWidth = imageSize + 18;
  float currentWidth = 0.0f;

  auto checkLineBreak = [&]() {
    if ((currentWidth+itemWidth) > availWidth) {
      currentWidth = 0.0f;
    } else {
      if (currentWidth != 0)ImGui::SameLine();
    }
    currentWidth += itemWidth;
  };

  for (const auto &typed : assets) {
    for (const auto &asset : typed)
    {
      if (asset.type == FileType::CODE) {
        if (activeTab != TAB_IDX_SCRIPTS)continue;
      } else {
        if (activeTab == TAB_IDX_SCRIPTS)continue;
      }

      if (asset.type == FileType::UNKNOWN)continue;
      checkLineBreak();

      auto icon = ImTextureRef(iconFile.getGPUTex());
      if (asset.texture) {
        icon = ImTextureRef(asset.texture->getGPUTex());
      } else {
        if (asset.type == FileType::MODEL_3D) {
          icon = ImTextureRef(iconMesh.getGPUTex());
        } else if (asset.type == FileType::AUDIO) {
          icon = ImTextureRef(iconMusic.getGPUTex());
        } else if (asset.type == FileType::CODE) {
          icon = ImTextureRef(iconCodeCpp.getGPUTex());
        }
      }

      bool isSelected = (ctx.selAssetUUID == asset.uuid);
      if(isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
      }

      if (ImGui::ImageButton(asset.name.c_str(), icon,
        {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0}, {1,1,1,
          asset.conf.exclude ? 0.25f : 1.0f
        }
      )) {
        ctx.selAssetUUID = asset.uuid == ctx.selAssetUUID ? 0 : asset.uuid;
      }

      if (ImGui::BeginDragDropSource()) {
        ImGui::ImageButton(asset.name.c_str(), icon, {imageSize*0.75f, imageSize*0.75f});
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

  if (activeTab == TAB_IDX_SCRIPTS)
  {
    checkLineBreak();
    if (ImGui::ImageButton("Add", ImTextureRef(iconCodeAdd.getGPUTex()),
     {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0}, {1,1,1,1.0f}
    )) {
      // ask name in popup
      ImGui::OpenPopup("NewScript");
    }
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
