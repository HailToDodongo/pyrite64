/**
* @copyright 2025 - Max Bebök
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

namespace Project::Component::Audio2D
{
  struct Data
  {
    PROP_U64(audioUUID);
    PROP_FLOAT(volume);
    PROP_BOOL(loop);
    PROP_BOOL(autoPlay);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};
    builder.set(data.audioUUID);
    builder.set(data.volume);
    builder.set(data.loop);
    builder.set(data.autoPlay);
    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->audioUUID);
    Utils::JSON::readProp(doc, data->volume, 1.0f);
    Utils::JSON::readProp(doc, data->loop);
    Utils::JSON::readProp(doc, data->autoPlay);
    return data;
  }

  void build(Object&, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto res = ctx.assetUUIDToIdx.find(data.audioUUID.value);
    uint16_t id = 0xDEAD;
    if (res == ctx.assetUUIDToIdx.end()) {
      Utils::Logger::log("Component Model: Audio UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = res->second;
    }

    uint8_t flags = 0;
    if(data.loop.value)flags |= 1 << 0;
    if(data.autoPlay.value)flags |= 1 << 1;

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint16_t>((uint16_t)(data.volume.value * 0xFFFF));
    ctx.fileObj.write<uint8_t>(flags);
    ctx.fileObj.write<uint8_t>(0); // padding
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      auto &audioList = ctx.project->getAssets().getTypeEntries(FileType::AUDIO);
      ImTable::addVecComboBox("Audio", audioList, data.audioUUID.value);
      ImTable::addProp("Volume", data.volume);
      ImTable::addProp("Loop", data.loop);
      ImTable::addProp("Auto-Play", data.autoPlay);

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    glm::u8vec4 col{0xFF};
    bool isSelected = ctx.isObjectSelected(obj.uuid);
    if (isSelected) {
      col = Utils::Colors::kSelectionTint;
    }
    Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4, col);
  }
}
