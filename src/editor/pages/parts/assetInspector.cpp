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
    ImGui::Text(message(MSG_ASSET_NONE_SELECTED));
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

  ImGui::Text(message(MSG_ASSET_FILE), asset->name.c_str());
  if (hasAssetConf && ImGui::CollapsingHeader(message(MSG_ASSET_SETTINGS), ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImTable::start("Settings");

    if (asset->type == FileType::IMAGE)
    {
      ImTable::addComboBox(message(MSG_IMAGE_FORMAT), asset->conf.format, Utils::TEX_TYPES, Utils::TEX_TYPE_COUNT);
    }
    else if (asset->type == FileType::MODEL_3D)
    {
      if (ImTable::add(message(MSG_MODEL_BASE_SCALE), asset->conf.baseScale)) {
        ctx.project->getAssets().reloadAssetByUUID(asset->getUUID());
      }
      ImTable::addCheckBox(message(MSG_MODEL_CREATE_BVH), asset->conf.gltfBVH);
      ImTable::addProp(message(MSG_MODEL_COLLISION), asset->conf.gltfCollision);
    } else if (asset->type == FileType::FONT)
    {
      ImTable::add(message(MSG_FONT_SIZE), asset->conf.baseScale);
      ImTable::addProp(message(MSG_FONT_ID), asset->conf.fontId);

      ImTable::add(message(MSG_FONT_CHARSET));
      ImGui::InputTextMultiline("##", &asset->conf.fontCharset.value);
    }
    else if (asset->type == FileType::AUDIO)
    {
      ImTable::addProp(message(MSG_AUDIO_FORCE_MONO), asset->conf.wavForceMono);

      //ImTable::addProp("Sample-Rate", asset->conf.wavResampleRate);
      ImTable::addVecComboBox<ImTable::ComboEntry>(message(MSG_AUDIO_SAMPLE_RATE), {
          { 0, message(MSG_AUDIO_SAMPLE_RATE_ORIGINAL) },
          { 8000, "8000 Hz" },
          { 11025, "11025 Hz" },
          { 16000, "16000 Hz" },
          { 22050, "22050 Hz" },
          { 32000, "32000 Hz" },
          { 44100, "44100 Hz" },
        }, asset->conf.wavResampleRate.value
      );
      ImTable::addComboBox(message(MSG_ASSET_COMPRESSION), asset->conf.wavCompression.value, {
        message(MSG_ASSET_COMPRESSION_NONE), "VADPCM", "Opus",
      });
    }

    if (asset->type != FileType::AUDIO)
    {
      ImTable::addComboBox(message(MSG_ASSET_COMPRESSION), (int&)asset->conf.compression, {
        message(MSG_ASSET_COMPRESSION_DEFAULT),
        message(MSG_ASSET_COMPRESSION_NONE),
        message(MSG_ASSET_COMPRESSION_LEVEL_1),
        message(MSG_ASSET_COMPRESSION_LEVEL_2),
        message(MSG_ASSET_COMPRESSION_LEVEL_3),
      });
    }

    ImTable::addCheckBox(message(MSG_ASSET_EXCLUDE), asset->conf.exclude);

    ImTable::end();
  }

  if (ImGui::CollapsingHeader(message(MSG_ASSET_PREVIEW), ImGuiTreeNodeFlags_DefaultOpen)) {
    if (asset->type == FileType::IMAGE && asset->texture) {
      ImGui::Image(ImTextureRef(asset->texture->getGPUTex()), asset->texture->getSize(4.0f));
      ImGui::Text("%dx%dpx", asset->texture->getWidth(), asset->texture->getHeight());
    }
    if (asset->type == FileType::MODEL_3D) {
      uint32_t triCount = 0;
      for (auto &model : asset->t3dmData.models) {
        triCount += model.triangles.size();
      }
      ImGui::Text(message(MSG_ASSET_PREVIEW_COUNT_MESHES), static_cast<int>(asset->t3dmData.models.size()));
      ImGui::Text(message(MSG_ASSET_PREVIEW_COUNT_TRIANGLES), triCount);
      ImGui::Text(message(MSG_ASSET_PREVIEW_COUNT_BONES), static_cast<int>(asset->t3dmData.skeletons.size()));
      ImGui::Text(message(MSG_ASSET_PREVIEW_COUNT_ANIMATIONS), static_cast<int>(asset->t3dmData.animations.size()));
    }
  }
}
