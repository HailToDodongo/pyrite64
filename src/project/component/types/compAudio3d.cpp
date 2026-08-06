/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../../utils/logger.h"
#include "../../../utils/colors.h"
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

namespace Project::Component::Audio3D
{
  struct Data
  {
    PROP_U64(audioUUID);
    PROP_FLOAT(volume);
    PROP_FLOAT(pitch);
    PROP_BOOL(loop);
    PROP_BOOL(autoPlay);
    PROP_FLOAT(nearDist);
    PROP_FLOAT(maxDist);
    PROP_FLOAT(rolloff);
    PROP_FLOAT(doppler);
    PROP_FLOAT(lowPassStart);
    PROP_FLOAT(reverbNear);
    PROP_FLOAT(reverbFar);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    data->volume.value = 1.0f;
    data->nearDist.value = 1.0f;
    data->maxDist.value = 50.0f;
    data->rolloff.value = 1.0f;
    data->doppler.value = 0.0f;
    data->lowPassStart.value = 1.0f;
    data->reverbNear.value = 0.0f;
    data->reverbFar.value = 0.0f;
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};
    builder.set(data.audioUUID);
    builder.set(data.volume);
    builder.set(data.pitch);
    builder.set(data.loop);
    builder.set(data.autoPlay);
    builder.set(data.nearDist);
    builder.set(data.maxDist);
    builder.set(data.rolloff);
    builder.set(data.doppler);
    builder.set(data.lowPassStart);
    builder.set(data.reverbNear);
    builder.set(data.reverbFar);
    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->audioUUID);
    Utils::JSON::readProp(doc, data->volume, 1.0f);
    Utils::JSON::readProp(doc, data->pitch, 0.0f);
    Utils::JSON::readProp(doc, data->loop);
    Utils::JSON::readProp(doc, data->autoPlay);
    Utils::JSON::readProp(doc, data->nearDist, 1.0f);
    Utils::JSON::readProp(doc, data->maxDist, 50.0f);
    Utils::JSON::readProp(doc, data->rolloff, 1.0f);
    Utils::JSON::readProp(doc, data->doppler, 0.0f);
    Utils::JSON::readProp(doc, data->lowPassStart, 1.0f);
    Utils::JSON::readProp(doc, data->reverbNear, 0.0f);
    Utils::JSON::readProp(doc, data->reverbFar, 0.0f);
    return data;
  }

  void build(Object&, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto res = ctx.assetUUIDToIdx.find(data.audioUUID.value);
    uint16_t id = 0xDEAD;
    if (res == ctx.assetUUIDToIdx.end()) {
      Utils::Logger::log("Component Audio3D: Audio UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = res->second;
    }

    auto asset = ctx.project->getAssets().getEntryByUUID(data.audioUUID.value);
    if (asset && !(asset->type == FileType::AUDIO && asset->outPath.ends_with(".tsw"))) {
      Utils::Logger::log("Component Audio3D: only .tsw samples can be positioned: " + asset->name, Utils::Logger::LEVEL_ERROR);
    }

    uint8_t flags = 0;
    if(data.loop.value)flags |= 1 << 0;
    if(data.autoPlay.value)flags |= 1 << 1;

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint16_t>((uint16_t)(data.volume.value * 0xFFFF));
    ctx.fileObj.write<uint8_t>(flags);
    ctx.fileObj.write<uint8_t>(0); // padding
    ctx.fileObj.write<uint8_t>(0);
    ctx.fileObj.write<uint8_t>(0);
    ctx.fileObj.write<float>(data.nearDist.value);
    ctx.fileObj.write<float>(data.maxDist.value);
    ctx.fileObj.write<float>(data.rolloff.value);
    ctx.fileObj.write<float>(data.doppler.value);
    ctx.fileObj.write<float>(data.lowPassStart.value);
    ctx.fileObj.write<float>(data.reverbNear.value);
    ctx.fileObj.write<float>(data.reverbFar.value);
    ctx.fileObj.write<float>(data.pitch.value);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      // only non-streamed samples can be spatialized
      auto audioList = ctx.project->getAssets().getTypeEntries(FileType::AUDIO);
      std::erase_if(audioList, [](const auto &e) { return !e.outPath.ends_with(".tsw"); });

      ImTable::addAssetVecComboBox("Audio", audioList, data.audioUUID.resolve(obj), [](auto){});
      ImTable::addObjProp("Volume", data.volume);
      ImTable::addObjProp("Pitch", data.pitch);
      ImTable::addObjProp("Loop", data.loop);

      // a looping sample needs its loop point baked into the .tsw
      auto asset = ctx.project->getAssets().getEntryByUUID(data.audioUUID.value);
      if (asset && data.loop.value && !asset->conf.wavLoop.value) {
        ImTable::add("");
        ImGui::TextDisabled("enable \"Loop\" on the asset,\nor this plays one-shot");
      }

      ImTable::addObjProp("Auto-Play", data.autoPlay);
      ImTable::addObjProp("Near Dist.", data.nearDist);
      ImTable::addObjProp("Max Dist.", data.maxDist);
      ImTable::addObjProp("Rolloff", data.rolloff);
      ImTable::addObjProp("Doppler", data.doppler);
      ImTable::addObjProp("Low-Pass Start", data.lowPassStart);
      ImTable::addObjProp("Reverb Near", data.reverbNear);
      ImTable::addObjProp("Reverb Far", data.reverbFar);

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    glm::u8vec4 col{0xFF};
    bool isSelected = ctx.isObjectSelected(obj.uuid);
    if (isSelected) {
      col = Utils::Colors::kSelectionTint;
    }
    Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4, col);
  }
}
