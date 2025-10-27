/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetInspector.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../../context.h"
#include "../../../utils/textureFormats.h"

using FileType = Project::AssetManager::FileType;

int Selecteditem  = 0;

Editor::AssetInspector::AssetInspector() {
}

void Editor::AssetInspector::draw() {
  if (ctx.selAssetUUID == 0) {
    ImGui::Text("No Asset selected");
    return;
  }

  auto asset = ctx.project->getAssets().getEntryByUUID(ctx.selAssetUUID);
  if (!asset) {
    ctx.selAssetUUID = 0;
    return;
  }

  bool hasAssetConf = true;
  if (asset->type == FileType::CODE || asset->type == FileType::PREFAB) {
    hasAssetConf = false;
  }

  ImGui::Text("File: %s", asset->name.c_str());
  if (hasAssetConf && ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InpTable::start("Settings");

    if (asset->type == FileType::IMAGE)
    {
      ImGui::InpTable::addComboBox("Format", asset->conf.format, Utils::TEX_TYPES, Utils::TEX_TYPE_COUNT);
    }
    else if (asset->type == FileType::MODEL_3D)
    {
      if (ImGui::InpTable::addInputInt("Base-Scale", asset->conf.baseScale)) {
        ctx.project->getAssets().reloadAssetByUUID(asset->uuid);
      }
      ImGui::InpTable::addCheckBox("Create BVH", asset->conf.gltfBVH);
    }

    int idxCompr = static_cast<int>(asset->conf.compression);
    const char* const ComprItems[] = {
      "Project Default", "None",
      "Level 1 - Fast",
      "Level 2 - Good",
      "Level 3 - High",
    };
    ImGui::InpTable::addComboBox("Compression", idxCompr, ComprItems, 5);
    asset->conf.compression = static_cast<Project::ComprTypes>(idxCompr);

    ImGui::InpTable::addCheckBox("Exclude", asset->conf.exclude);

    ImGui::InpTable::end();
  }

  if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (asset->type == FileType::IMAGE && asset->texture) {
      ImGui::Image(ImTextureRef(asset->texture->getGPUTex()), asset->texture->getSize(4.0f));
      ImGui::Text("%dx%dpx", asset->texture->getWidth(), asset->texture->getHeight());
    }
    if (asset->type == FileType::MODEL_3D) {
      uint32_t triCount = 0;
      for (auto &model : asset->t3dmData.models) {
        triCount += model.triangles.size();
      }
      ImGui::Text("Meshes: %d", static_cast<int>(asset->t3dmData.models.size()));
      ImGui::Text("Triangles: %d", triCount);
      ImGui::Text("Bones: %d", static_cast<int>(asset->t3dmData.skeletons.size()));
      ImGui::Text("Animations: %d", static_cast<int>(asset->t3dmData.animations.size()));
    }
  }
}
