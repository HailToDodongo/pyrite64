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
#include "../../../utils/colors.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"
#include "../../scene/scene.h"

namespace
{
  constexpr uint32_t ROT_OBJECT = 0;
  constexpr uint32_t ROT_CAMERA = 1;

  bool comboProp(std::vector<ImTable::ComboEntry> &list, uint32_t *val)
  {
    uint32_t proxy = *val;
    ImGui::VectorComboBox("##", list, proxy);
    if(proxy == *val)return false;
    *val = proxy;
    return true;
  }
}

namespace Project::Component::AudioListener
{
  struct Data
  {
    PROP_U32(posObjectUUID); // 0 = own object
    PROP_U32(rotSource);
  };

  std::shared_ptr<void> init(Object &obj) {
    return std::make_shared<Data>();
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.posObjectUUID)
      .set(data.rotSource)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->posObjectUUID);
    Utils::JSON::readProp(doc, data->rotSource);
    return data;
  }

  void build(Object &obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto posUUID = data.posObjectUUID.resolve(obj);
    auto posObj = (ctx.scene && posUUID) ? ctx.scene->getObjectByUUID(posUUID) : nullptr;

    uint8_t flags = 0;
    if(data.rotSource.resolve(obj) == ROT_CAMERA)flags |= 1 << 0;

    ctx.fileObj.write<uint16_t>(posObj ? posObj->runtimeId : 0);
    ctx.fileObj.write<uint8_t>(flags);
    ctx.fileObj.write<uint8_t>(0); // padding
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      std::vector<ImTable::ComboEntry> objList{{0, "<Self>"}};
      for (auto &[id, object] : ctx.project->getScenes().getLoadedScene()->objectsMap) {
        objList.push_back({.value = object->uuid, .name = object->name});
      }

      std::vector<ImTable::ComboEntry> rotList{
        {ROT_OBJECT, "<Self>"},
        {ROT_CAMERA, "Active Camera"},
      };

      ImTable::addObjProp<uint32_t>("Position", data.posObjectUUID, [&objList](uint32_t *val) {
        return comboProp(objList, val);
      }, nullptr);
      ImTable::addObjProp<uint32_t>("Rotation", data.rotSource, [&rotList](uint32_t *val) {
        return comboProp(rotList, val);
      }, nullptr);

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    glm::u8vec4 col{0xFF};
    if (ctx.isObjectSelected(obj.uuid)) {
      col = Utils::Colors::kSelectionTint;
    }
    Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4, col);
  }
}
