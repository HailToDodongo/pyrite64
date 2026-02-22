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
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

#include "../../../../n64/engine/include/collision/flags.h"

namespace
{
  constexpr int32_t TYPE_BOX      = 0;
  constexpr int32_t TYPE_SPHERE   = 1;
  constexpr int32_t TYPE_CYLINDER = 2;
}

namespace Project::Component::CollBody
{
  struct Data
  {
    PROP_VEC3(halfExtend);
    PROP_VEC3(offset);
    PROP_S32(type);
    PROP_BOOL(isTrigger);
    PROP_BOOL(isFixed);
    PROP_U32(maskRead);
    PROP_U32(maskWrite);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.halfExtend)
      .set(data.offset)
      .set(data.type)
      .set(data.isTrigger)
      .set(data.isFixed)
      .set(data.maskRead)
      .set(data.maskWrite)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->halfExtend, glm::vec3{1.0f, 1.0f, 1.0f});
    Utils::JSON::readProp(doc, data->offset);
    Utils::JSON::readProp(doc, data->type);
    Utils::JSON::readProp(doc, data->isTrigger, false);
    Utils::JSON::readProp(doc, data->isFixed, false);
    Utils::JSON::readProp(doc, data->maskRead, 0xFFu);
    Utils::JSON::readProp(doc, data->maskWrite, 0xFFu);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    ctx.fileObj.write(data.halfExtend.resolve(obj.propOverrides));
    ctx.fileObj.write(data.offset.resolve(obj.propOverrides));

    uint8_t flags = data.type.resolve(obj.propOverrides) == TYPE_BOX ? P64::Coll::BCSFlags::SHAPE_BOX : 0;
    if(data.isTrigger.resolve(obj.propOverrides)) {
      flags |= P64::Coll::BCSFlags::TRIGGER;
    }
    if(data.isFixed.resolve(obj.propOverrides)) {
      flags |= P64::Coll::BCSFlags::FIXED_XYZ;
    }

    ctx.fileObj.write<uint8_t>(flags);
    ctx.fileObj.write<uint8_t>(data.maskRead.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.maskWrite.resolve(obj.propOverrides));
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      auto &ext = data.halfExtend.resolve(obj.propOverrides);

      ImTable::addComboBox("Type", data.type.value, {"Box", "Sphere", "Cylinder"});
      if(data.type.resolve(obj.propOverrides) == TYPE_SPHERE) {
        ImTable::add("Size", ext.y);
        ext.x = ext.y;
        ext.z = ext.y;
      } else {
        ImTable::addObjProp("Size", data.halfExtend);
      }
      ImTable::addObjProp("Offset", data.offset);
      ImTable::addObjProp("Trigger", data.isTrigger);
      ImTable::addObjProp("Fixed-Pos", data.isFixed);
      ImTable::addBitMask8("Mask Read", data.maskRead.resolve(obj.propOverrides));
      ImTable::addBitMask8("Mask Write", data.maskWrite.resolve(obj.propOverrides));

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto &objPos = obj.pos.resolve(obj.propOverrides);
    auto &objScale = obj.scale.resolve(obj.propOverrides);

    glm::vec3 halfExt = data.halfExtend.resolve(obj.propOverrides) * objScale;
    glm::vec3 center = objPos + data.offset.resolve(obj.propOverrides);
    auto type = data.type.resolve(obj.propOverrides);

    if(type == TYPE_BOX) // Box
    {
      glm::vec4 aabbCol{0.0f, 1.0f, 1.0f, 1.0f};

      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol);
    } else if(type == TYPE_SPHERE) // Sphere
    {
      Utils::Mesh::addLineSphere(*vp.getLines(), center, halfExt, glm::vec4{0.0f, 1.0f, 1.0f, 1.0f});
    }
  }

  Utils::AABB getAABB(Object &obj, Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::AABB aabb;
    glm::vec3 halfExt = data.halfExtend.resolve(obj.propOverrides) * (float)0xFFFF;
    glm::vec3 offset = data.offset.resolve(obj.propOverrides) * (float)0xFFFF;
    aabb.addPoint(offset - halfExt);
    aabb.addPoint(offset + halfExt);
    return aabb;
  }
}
