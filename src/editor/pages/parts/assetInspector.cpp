/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetInspector.h"
#include "imgui.h"
#include "../editorScene.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../../context.h"
#include "../../../utils/textureFormats.h"
#include "../../thumbnailCache.h"

using FileType = Project::FileType;

int Selecteditem  = 0;

namespace
{
  uint32_t countChildBones(const T3DM::Bone &node) {
    uint32_t count = node.children.size();
    for (const auto &child : node.children) {
      count += countChildBones(*child);
    }
    return count;
  };
}

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
  if (asset->type == FileType::CODE_OBJ
    || asset->type == FileType::CODE_GLOBAL
    || asset->type == FileType::PREFAB)
  {
    hasAssetConf = false;
  }

  ImGui::Text("File: %s", asset->name.c_str());
  if (hasAssetConf && ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
  {
    auto confBefore = asset->conf.serialize();

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

      ImTable::addVecComboBox<ImTable::ComboEntry>("Compression", {
          { 0, "None" },
          { 1, "VADPCM" },
          { 3, "Opus" },
        }, asset->conf.wavCompression.value
      );
    }

    if (asset->type != FileType::AUDIO && asset->type != FileType::MUSIC_XM)
    {
      int compression = (int)asset->conf.compression;
      if(ImTable::addComboBox("Compression", compression, {
        "Project Default", "None",
        "Level 1 - Fast",
        "Level 2 - Good",
        "Level 3 - High",
      })) {
        asset->conf.compression = (Project::ComprTypes)compression;
      }
    }

    ImTable::addCheckBox("Exclude", asset->conf.exclude);

    ImTable::end();

    if (confBefore != asset->conf.serialize()) {
      ctx.project->getAssets().markAssetMetaDirty(asset->getUUID());
    }
  }

  if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (asset->type == FileType::IMAGE && asset->texture) {
      auto imgSize = asset->texture->getSize();

      float maxWidth = ImGui::GetContentRegionAvail().x - 8_px;
      if(maxWidth > 256_px)maxWidth = 256_px;
      float imgRatio = imgSize.x / imgSize.y;
      imgSize.x = maxWidth;
      imgSize.y = maxWidth / imgRatio;

      ImGui::Image(ImTextureRef(asset->texture->getGPUTex()), imgSize);
      ImGui::Text("%dx%dpx", asset->texture->getWidth(), asset->texture->getHeight());
    }
    if (asset->type == FileType::MODEL_3D) {
      if (ctx.thumbnails) {
        if (auto thumb = ctx.thumbnails->getModelTexture(asset->getUUID())) {
          float w = ImGui::GetContentRegionAvail().x - 8_px;
          if (w > 256_px)w = 256_px;
          ImVec2 size{w, w};
          ImVec2 rmin = ImGui::GetCursorScreenPos();
          ImVec2 uv0, uv1;
          ctx.thumbnails->getScrubUV(rmin, {rmin.x + size.x, rmin.y + size.y}, uv0, uv1);
          ImGui::Image(ImTextureRef(thumb), size, uv0, uv1);
        }
      }

      uint32_t triCount = 0;
      for (auto &model : asset->model.t3dm.models) {
        triCount += model.triangles.size();
      }

      uint32_t boneCount = 0;
      for(auto &skel : asset->model.t3dm.skeletons) {
        boneCount += countChildBones(skel);
      }

      ImGui::BeginTable("ModelInfo", 2);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
        ImGui::Text("Meshes");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", static_cast<int>(asset->model.t3dm.models.size()));

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
        ImGui::Text("Triangles");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", triCount);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
        ImGui::Text("Bones");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", boneCount);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
        ImGui::Text("Animations");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", static_cast<int>(asset->model.t3dm.animations.size()));

      ImGui::EndTable();

      if(ImGui::Button(ICON_MDI_PENCIL " Open Model Editor")) {
        ctx.editorScene->openModelEditor(asset->getUUID());
      }
    }
  }
}
