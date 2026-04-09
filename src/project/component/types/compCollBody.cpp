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
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

#include "../../../../n64/engine/include/collision/types.h"

namespace
{
  constexpr int32_t TYPE_BOX      = 0;
  constexpr int32_t TYPE_SPHERE   = 1;
  constexpr int32_t TYPE_CYLINDER = 2;
  constexpr int32_t TYPE_CAPSULE  = 3;
  constexpr int32_t TYPE_CONE     = 4;
  constexpr int32_t TYPE_PYRAMID  = 5;
  
}

namespace Project::Component::CollBody
{
  struct Data
  {
    PROP_VEC3(halfExtend);
    PROP_VEC3(offset);
    PROP_S32(type);
    PROP_BOOL(isTrigger);
    PROP_U32(maskRead);
    PROP_U32(maskWrite);
    PROP_FLOAT(friction);
    PROP_FLOAT(bounce);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    data->halfExtend.value = {10.0f, 10.0f, 10.0f};
    data->friction.value = 0.8f;
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.halfExtend)
      .set(data.offset)
      .set(data.type)
      .set(data.isTrigger)
      .set(data.maskRead)
      .set(data.maskWrite)
      .set(data.friction)
      .set(data.bounce)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->halfExtend, glm::vec3{1.0f, 1.0f, 1.0f});
    Utils::JSON::readProp(doc, data->offset);
    Utils::JSON::readProp(doc, data->type);
    Utils::JSON::readProp(doc, data->isTrigger, false);
    Utils::JSON::readProp(doc, data->maskRead, 0x00u);
    Utils::JSON::readProp(doc, data->maskWrite, 0x00u);
    Utils::JSON::readProp(doc, data->friction, 0.8f);
    Utils::JSON::readProp(doc, data->bounce, 0.0f);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    ctx.fileObj.write(data.halfExtend.resolve(obj.propOverrides));
    ctx.fileObj.write(data.offset.resolve(obj.propOverrides));

    uint8_t type = (uint8_t)P64::Coll::ShapeType::Box;
    switch (data.type.resolve(obj.propOverrides)) {
      case TYPE_BOX: type = (uint8_t)P64::Coll::ShapeType::Box; break;
      case TYPE_SPHERE: type = (uint8_t)P64::Coll::ShapeType::Sphere; break;
      case TYPE_CYLINDER: type = (uint8_t)P64::Coll::ShapeType::Cylinder; break;
      case TYPE_CAPSULE: type = (uint8_t)P64::Coll::ShapeType::Capsule; break;
      case TYPE_CONE: type = (uint8_t)P64::Coll::ShapeType::Cone; break;
      case TYPE_PYRAMID: type = (uint8_t)P64::Coll::ShapeType::Pyramid; break;
    }
    ctx.fileObj.write<uint8_t>(type);
    ctx.fileObj.write<uint8_t>(data.isTrigger.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.maskRead.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.maskWrite.resolve(obj.propOverrides));
    ctx.fileObj.write<float>(data.friction.resolve(obj.propOverrides));
    ctx.fileObj.write<float>(data.bounce.resolve(obj.propOverrides));
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      auto &ext = data.halfExtend.resolve(obj.propOverrides);

      ImTable::addComboBox("Type", data.type.value, {"Box", "Sphere", "Cylinder", "Capsule", "Cone", "Pyramid"});
      if(data.type.resolve(obj.propOverrides) == TYPE_SPHERE) {
        ImTable::add("Radius", ext.y);
        ext.x = ext.y;
        ext.z = ext.y;
      } else if (data.type.resolve(obj.propOverrides) == TYPE_BOX) {
        ImTable::addObjProp("Half Size", data.halfExtend);
      } else if (data.type.resolve(obj.propOverrides) == TYPE_CYLINDER) {
        ImTable::add("Radius", ext.x);
        ImTable::add("Half Height", ext.y);
        ext.z = ext.x;
      } else if (data.type.resolve(obj.propOverrides) == TYPE_CAPSULE) {
        ImTable::add("Radius", ext.x);
        ImTable::add("Inner Half Height", ext.y);
        ext.z = ext.x;
      } else if (data.type.resolve(obj.propOverrides) == TYPE_CONE) {
        ImTable::add("Radius", ext.x);
        ImTable::add("Half Height", ext.y);
        ext.z = ext.x;
      } else if (data.type.resolve(obj.propOverrides) == TYPE_PYRAMID) {
        ImTable::add("Base X Half Size", ext.x);
        ImTable::add("Base Z Half Size", ext.z);
        ImTable::add("Half Height", ext.y);
      }
      ImTable::addObjProp("Offset", data.offset);
      ImTable::addObjProp("Trigger", data.isTrigger);
      ImTable::addBitMask8("Mask Read", data.maskRead.resolve(obj.propOverrides));
      ImTable::addBitMask8("Mask Write", data.maskWrite.resolve(obj.propOverrides));
      auto &friction = data.friction.resolve(obj.propOverrides);
      if(ImTable::add("Friction", friction)) {
        friction = std::clamp(friction, 0.0f, 1.0f);
      }

      auto &bounce = data.bounce.resolve(obj.propOverrides);
      if(ImTable::add("Bounce", bounce)) {
        bounce = std::clamp(bounce, 0.0f, 1.0f);
      }


      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto &objPos = obj.pos.resolve(obj.propOverrides);
    auto &objRot = obj.rot.resolve(obj.propOverrides);
    auto &objScale = obj.scale.resolve(obj.propOverrides);

    glm::vec3 halfExt = data.halfExtend.resolve(obj.propOverrides) * objScale;
    glm::vec3 localOffset = data.offset.resolve(obj.propOverrides);
    glm::vec3 center = objPos + (objRot * (localOffset * objScale));
    auto type = data.type.resolve(obj.propOverrides);

    if(type == TYPE_BOX) // Box
    {
      glm::vec4 aabbCol{0.0f, 1.0f, 1.0f, 1.0f};

      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol, objRot);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol, objRot);
    } else if(type == TYPE_SPHERE) // Sphere
    {
      Utils::Mesh::addLineSphere(*vp.getLines(), center, halfExt, glm::vec4{0.0f, 1.0f, 1.0f, 1.0f}, objRot);
    }
    else if(type == TYPE_CYLINDER) // Cylinder
    {
      Utils::Mesh::addLineCylinder(*vp.getLines(), center, halfExt, glm::vec4{0.0f, 1.0f, 1.0f, 1.0f}, objRot);
    }
    else if(type == TYPE_CAPSULE) // Capsule
    {
      Utils::Mesh::addLineCapsule(*vp.getLines(), center, halfExt, glm::vec4{0.0f, 1.0f, 1.0f, 1.0f}, objRot);
    }
    else if(type == TYPE_CONE) // Cone
    {
      Utils::Mesh::addLineCone(*vp.getLines(), center, halfExt, glm::vec4{0.0f, 1.0f, 1.0f, 1.0f}, objRot);
    }
    else if(type == TYPE_PYRAMID) // Pyramid
    {
      Utils::Mesh::addLinePyramid(*vp.getLines(), center, halfExt, glm::vec4{0.0f, 1.0f, 1.0f, 1.0f}, objRot);
    }
  }
}
