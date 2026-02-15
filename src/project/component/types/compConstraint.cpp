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
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

namespace
{
  constexpr uint32_t TYPE_COPY_OBJ = 0;
  constexpr uint32_t TYPE_COPY_CAM = 2;
  constexpr uint32_t TYPE_REL_OFFSET = 1;
}

namespace Project::Component::Constraint
{
  struct Data
  {
    PROP_U32(type);
    PROP_U32(objectUUID);
    PROP_BOOL(usePos);
    PROP_BOOL(useScale);
    PROP_BOOL(useRot);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.type)
      .set(data.objectUUID)
      .set(data.usePos)
      .set(data.useScale)
      .set(data.useRot)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->type);
    Utils::JSON::readProp(doc, data->objectUUID);
    Utils::JSON::readProp(doc, data->usePos);
    Utils::JSON::readProp(doc, data->useScale);
    Utils::JSON::readProp(doc, data->useRot);
    return data;
  }

  void build(Object&, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto obj = ctx.scene->getObjectByUUID(data.objectUUID.value);
    uint16_t objId = obj ? obj->id : 0;

    uint8_t flags = 0;
    if (data.usePos.value)   flags |= 1 << 0;
    if (data.useScale.value) flags |= 1 << 1;
    if (data.useRot.value)   flags |= 1 << 2;

    ctx.fileObj.write<uint16_t>(objId);
    ctx.fileObj.write<uint8_t>(data.type.value);
    ctx.fileObj.write<uint8_t>(flags);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      std::vector<ImTable::ComboEntry> typeList{
        {TYPE_COPY_OBJ, "Copy Trans. (Object)"},
        {TYPE_COPY_CAM, "Copy Trans. (Camera)"},
        {TYPE_REL_OFFSET, "Relative Offset"},
      };

      ImTable::addVecComboBox("Type", typeList, data.type.value);

      // @TODO: do this in scene itself
      auto &map = ctx.project->getScenes().getLoadedScene()->objectsMap;
      std::vector<ImTable::ComboEntry> objList;
      objList.push_back({0, "<Parent>"});

      for (auto &[id, object] : map) {
        objList.push_back({
          .value = object->uuid,
          .name = object->name,
        });
      }

      if(data.type.value != TYPE_COPY_CAM)
      {
        ImTable::addObjectVecComboBox("Ref. Object", objList, data.objectUUID.value);
      }

      if(data.type.value == TYPE_COPY_OBJ || data.type.value == TYPE_COPY_CAM)
      {
        ImTable::addProp("Position", data.usePos);
        ImTable::addProp("Scale",    data.useScale);
        ImTable::addProp("Rotation", data.useRot);
      }

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    //Data &data = *static_cast<Data*>(entry.data.get());
    //Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4);
  }
}
