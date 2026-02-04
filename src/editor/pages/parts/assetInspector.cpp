/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetInspector.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"
#include "../../../context.h"
#include "../../../utils/textureFormats.h"

using FileType = Project::FileType;

int Selecteditem  = 0;

Editor::AssetInspector::AssetInspector() {
}

void Editor::AssetInspector::draw() {
  if (ctx.selAssetUUID == 0) {
    ImGui::Text(message(Message::NO_ASSET_SELECTED));
    return;
  }

  auto asset = ctx.project->getAssets().getEntryByUUID(ctx.selAssetUUID);
  if (!asset) {
    ctx.selAssetUUID = 0;
    return;
  }

  bool hasAssetConf = true;
  if (asset->type == FileType::CODE_OBJ
    || asset->type == FileType::CODE_GLOBAL
    || asset->type == FileType::PREFAB)
  {
    hasAssetConf = false;
  }

  ImGui::Text(message(Message::ASSET_FILE), asset->name.c_str());
  if (hasAssetConf && ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImTable::start("Settings");

    if (asset->type == FileType::IMAGE)
    {
      ImTable::addComboBox("Format", asset->conf.format, Utils::TEX_TYPES, Utils::TEX_TYPE_COUNT);
    }
    else if (asset->type == FileType::MODEL_3D)
    {
      if (ImTable::add("Base-Scale", asset->conf.baseScale)) {
        ctx.project->getAssets().reloadAssetByUUID(asset->getUUID());
      }
      ImTable::addCheckBox("Create BVH", asset->conf.gltfBVH);
      ImTable::addProp("Collision", asset->conf.gltfCollision);
    } else if (asset->type == FileType::FONT)
    {
      ImTable::add("Size", asset->conf.baseScale);
      ImTable::addProp("ID", asset->conf.fontId);

      ImTable::add("Charset");
      ImGui::InputTextMultiline("##", &asset->conf.fontCharset.value);
    }
    else if (asset->type == FileType::AUDIO)
    {
      ImTable::addProp("Force-Mono", asset->conf.wavForceMono);

      //ImTable::addProp("Sample-Rate", asset->conf.wavResampleRate);
      ImTable::addVecComboBox<ImTable::ComboEntry>("Sample-Rate", {
          { 0, "Original" },
          { 8000, "8000 Hz" },
          { 11025, "11025 Hz" },
          { 16000, "16000 Hz" },
          { 22050, "22050 Hz" },
          { 32000, "32000 Hz" },
          { 44100, "44100 Hz" },
        }, asset->conf.wavResampleRate.value
      );
      ImTable::addComboBox("Compression", asset->conf.wavCompression.value, {
        "None", "VADPCM", "Opus",
      });
    }

    if (asset->type != FileType::AUDIO)
    {
      ImTable::addComboBox("Compression", (int&)asset->conf.compression, {
        "Project Default", "None",
        "Level 1 - Fast",
        "Level 2 - Good",
        "Level 3 - High",
      });
    }

    ImTable::addCheckBox("Exclude", asset->conf.exclude);

    ImTable::end();
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
