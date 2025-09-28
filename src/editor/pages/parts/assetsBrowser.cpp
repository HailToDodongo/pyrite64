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
}

Editor::AssetsBrowser::AssetsBrowser()
  : iconFile{ctx.gpu, "data/img/icons/file.svg", ICON_MAX_SIZE, ICON_MAX_SIZE},
   iconMesh{ctx.gpu, "data/img/icons/mesh.svg", ICON_MAX_SIZE, ICON_MAX_SIZE},
   iconMusic{ctx.gpu, "data/img/icons/music.svg", ICON_MAX_SIZE, ICON_MAX_SIZE}
{
}

Editor::AssetsBrowser::~AssetsBrowser() {
}

void Editor::AssetsBrowser::draw()
{
  auto &assets = ctx.project->getAssets().getEntries();

  auto availWidth = ImGui::GetContentRegionAvail().x - 4;
  float imageSize = 64;
  float itemWidth = imageSize + 18;
  float currentWidth = 0.0f;

  for (const auto &asset : assets)
  {
    if (asset.type == FileType::UNKNOWN)continue;
    if ((currentWidth+itemWidth) > availWidth) {
      currentWidth = 0.0f;
    } else {
      if (currentWidth != 0)ImGui::SameLine();
    }

    auto icon = ImTextureRef(iconFile.getGPUTex());
    if (asset.texture) {
        icon = ImTextureRef(asset.texture->getGPUTex());
    } else {
      if (asset.type == FileType::MODEL_3D) {
        icon = ImTextureRef(iconMesh.getGPUTex());
      } else if (asset.type == FileType::AUDIO) {
        icon = ImTextureRef(iconMusic.getGPUTex());
      }
    }

    ImGui::ImageButton(asset.name.c_str(), icon,
      {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0}, {1,1,1, 0.75f}
    );
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
      ImGui::SetTooltip("File: %s\nUUID: %08X-%08X", asset.name.c_str(),
        asset.uuid >> 32, asset.uuid & 0xFFFFFFFF
      );
    }

    currentWidth += itemWidth;

  }
}
