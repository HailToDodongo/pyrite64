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
#include "../../scene/scene.h"
#include "glm/gtc/constants.hpp"
#include <cmath>

namespace
{
  // objectUUID 0 means "<Parent>", matching the engine's refObjId 0 -> obj.group
  Project::Object* resolveRefObject(Project::Object &obj, uint32_t refUUID, Project::Scene *scene)
  {
    if(refUUID == 0)return obj.parent;
    return scene ? scene->getObjectByUUID(refUUID).get() : nullptr;
  }

  constexpr uint32_t TYPE_COPY_OBJ = 0;
  constexpr uint32_t TYPE_REL_OFFSET = 1;
  constexpr uint32_t TYPE_COPY_CAM = 2;
  constexpr uint32_t TYPE_BILLBOARD_Y = 3;
  constexpr uint32_t TYPE_BILLBOARD_XYZ = 4;
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

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto objRef = ctx.scene ? ctx.scene->getObjectByUUID(data.objectUUID.value) : nullptr;
    uint16_t objId = objRef ? objRef->runtimeId : 0;

    uint8_t flags = 0;
    if (data.usePos.resolve(obj)  )flags |= 1 << 0;
    if (data.useScale.resolve(obj))flags |= 1 << 1;
    if (data.useRot.resolve(obj)  )flags |= 1 << 2;

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
        {TYPE_BILLBOARD_Y, "Billboard Y"},
        {TYPE_BILLBOARD_XYZ, "Billboard Full"},
      };

      ImTable::addObjProp<uint32_t>("Type", data.type, [&typeList](uint32_t *val) -> bool {
        uint32_t proxy = *val;
        ImGui::VectorComboBox("##", typeList, proxy);
        if (proxy == *val) {
          return false;
        }
        *val = proxy;
        return true;
      }, nullptr);

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

      if(data.type.value == TYPE_COPY_OBJ || data.type.value == TYPE_REL_OFFSET)
      {
        ImTable::addObjProp<uint32_t>("Ref. Object", data.objectUUID, [&objList](uint32_t *val) -> bool {
          uint32_t proxy = *val;
          ImGui::VectorComboBox("##", objList, proxy);
          if (proxy == *val) {
            return false;
          }
          *val = proxy;
          return true;
        }, nullptr);
      }

      if(data.type.value == TYPE_COPY_OBJ || data.type.value == TYPE_COPY_CAM)
      {
        ImTable::addObjProp("Position", data.usePos);
        ImTable::addObjProp("Scale",    data.useScale);
        ImTable::addObjProp("Rotation", data.useRot);
      }

      ImTable::end();
    }
  }

  void evalTransform(Object &obj, Entry &entry, const EvalCtx &evalCtx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    switch(data.type.value)
    {
      case TYPE_BILLBOARD_Y:
      {
        // same formula as the engine, so the preview and the ROM agree
        float angle = std::atan2(evalCtx.camViewDir.x, evalCtx.camViewDir.z) + glm::pi<float>();
        obj.display.rot = glm::angleAxis(angle, glm::vec3{0,1,0});
      } break;

      case TYPE_BILLBOARD_XYZ:
        obj.display.rot = evalCtx.camRot;
        break;

      case TYPE_COPY_CAM:
        if(data.usePos.resolve(obj))obj.display.pos = evalCtx.camPos;
        break;

      case TYPE_COPY_OBJ:
      {
        auto *ref = resolveRefObject(obj, data.objectUUID.value, evalCtx.scene);
        if(!ref)break;
        auto refTrans = ref->getDisplayTrans();
        if(data.usePos.resolve(obj))  obj.display.pos   = refTrans.pos;
        if(data.useScale.resolve(obj))obj.display.scale = refTrans.scale;
        if(data.useRot.resolve(obj))  obj.display.rot   = refTrans.rot;
      } break;

      default: break;
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    //Data &data = *static_cast<Data*>(entry.data.get());
    //Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4);
  }
}
