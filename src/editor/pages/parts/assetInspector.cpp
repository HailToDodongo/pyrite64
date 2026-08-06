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
#include "../../audioPreview.h"
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

  void formatTime(char *out, size_t outSize, float seconds) {
    int total = (int)seconds;
    snprintf(out, outSize, "%d:%02d", total / 60, total % 60);
  }

  // Estimated size of the converted audio in ROM, mirroring what the
  // converters produce for each compression type at the current asset settings.
  double estimateAudioRomSize(const Editor::AudioPreview::Info &info, const Project::AssetConf &conf)
  {
    namespace WavCompr = Project::WavCompression;
    // only streamed .wav64 goes through a resampler, .tsw keeps the native rate
    double rate = (WavCompr::isStreamed(conf.wavCompression.value) && conf.wavResampleRate.value != 0)
      ? conf.wavResampleRate.value : info.sampleRate;
    double channels = conf.wavForceMono.value ? 1 : info.channels;

    double bytesPerSec;
    switch (conf.wavCompression.value)
    {
      case WavCompr::VADPCM: // 16 samples -> 9 bytes per channel
        bytesPerSec = rate * channels * (9.0 / 16.0);
        break;
      case WavCompr::VADPCM2: // 16 samples -> 5 bytes per channel
        bytesPerSec = rate * channels * (5.0 / 16.0);
        break;
      case WavCompr::PCM8:
        bytesPerSec = rate * channels;
        break;
      case WavCompr::ULC:
      case WavCompr::OPUS: // audioconv64's automatic "good quality" bitrate
        bytesPerSec = (60.0 * 50.0 + rate * channels) / 8.0;
        break;
      default: // uncompressed 16-bit PCM
        bytesPerSec = rate * channels * 2.0;
        break;
    }
    return bytesPerSec * info.duration;
  }

  void drawAudioPreview(const Project::AssetManagerEntry *asset)
  {
    bool isPlaying = Editor::AudioPreview::isPlaying(asset->path);

    if (ImGui::Button(isPlaying ? ICON_MDI_STOP " Stop" : ICON_MDI_PLAY " Play")) {
      if (isPlaying) {
        Editor::AudioPreview::stop();
      } else {
        Editor::AudioPreview::play(asset->path);
      }
      isPlaying = !isPlaying;
    }

    Editor::AudioPreview::Info info{};
    bool hasInfo = Editor::AudioPreview::getInfo(asset->path, info);

    ImGui::SameLine();
    float progress = isPlaying ? Editor::AudioPreview::getProgress() : 0.0f;

    char timeText[64]{};
    if (hasInfo) {
      char posStr[16], durStr[16];
      formatTime(posStr, sizeof(posStr), progress * info.duration);
      formatTime(durStr, sizeof(durStr), info.duration);
      snprintf(timeText, sizeof(timeText), "%s / %s", posStr, durStr);
    }
    ImGui::ProgressBar(progress, {-FLT_MIN, 0}, timeText);

    // click into the bar to seek
    if (isPlaying && ImGui::IsItemClicked()) {
      ImVec2 rMin = ImGui::GetItemRectMin();
      ImVec2 rMax = ImGui::GetItemRectMax();
      Editor::AudioPreview::seek((ImGui::GetMousePos().x - rMin.x) / (rMax.x - rMin.x));
    }

    if (hasInfo) {
      ImGui::Text("%u Hz, %s", info.sampleRate, info.channels == 1 ? "Mono" : "Stereo");

      double romSize = estimateAudioRomSize(info, asset->conf);
      if (romSize >= 1024.0 * 1024.0) {
        ImGui::Text("Est. size in ROM: %.1f MB", romSize / (1024.0 * 1024.0));
      } else {
        ImGui::Text("Est. size in ROM: %.0f KB", romSize / 1024.0);
      }
    } else {
      ImGui::TextDisabled("Failed to read audio file");
    }
  }
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
      namespace WavCompr = Project::WavCompression;
      bool isMp3 = asset->path.ends_with(".mp3");
      bool isStreamed = WavCompr::isStreamed(asset->conf.wavCompression.value);

      ImTable::addProp("Force-Mono", asset->conf.wavForceMono);

      if (isMp3) {
        // mp3 only converts through audioconv64, so it always streams
        ImTable::addVecComboBox<ImTable::ComboEntry>("Compression", {
            { WavCompr::OPUS, "Opus (streamed)" },
            { WavCompr::ULC, "ULC (streamed)" },
          }, asset->conf.wavCompression.value
        );
      } else {
        ImTable::addVecComboBox<ImTable::ComboEntry>("Compression", {
            { WavCompr::NONE, "None (PCM16)" },
            { WavCompr::PCM8, "PCM8" },
            { WavCompr::VADPCM, "VADPCM" },
            { WavCompr::VADPCM2, "VADPCM2" },
            { WavCompr::OPUS, "Opus (streamed)" },
            { WavCompr::ULC, "ULC (streamed)" },
          }, asset->conf.wavCompression.value
        );
      }

      if (isStreamed) {
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
      } else {
        ImTable::addProp("Loop", asset->conf.wavLoop);
      }
    }
    else if (asset->type == FileType::SOUND_FONT)
    {
      // downsamples any bank sample above this rate
      ImTable::addVecComboBox<ImTable::ComboEntry>("Max Rate", {
          { 0, "Original" },
          { 16000, "16000 Hz" },
          { 22050, "22050 Hz" },
          { 32000, "32000 Hz" },
        }, asset->conf.wavResampleRate.value
      );
    }

    if (asset->type != FileType::AUDIO && asset->type != FileType::SEQUENCE
      && asset->type != FileType::SOUND_FONT)
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
    if (asset->type == FileType::AUDIO) {
      drawAudioPreview(asset);
    }
    if (asset->type == FileType::SEQUENCE || asset->type == FileType::SOUND_FONT) {
      ImGui::TextDisabled("No preview available");
    }
  }
}
