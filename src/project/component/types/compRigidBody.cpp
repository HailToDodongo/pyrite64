/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <limits>
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
#include <algorithm>

namespace Project::Component::RigidBody
{
  struct Data
  {
    PROP_FLOAT(mass);
    PROP_BOOL(isKinematic);
    PROP_BOOL(constrainPosX);
    PROP_BOOL(constrainPosY);
    PROP_BOOL(constrainPosZ);
    PROP_BOOL(constrainRotX);
    PROP_BOOL(constrainRotY);
    PROP_BOOL(constrainRotZ);
    PROP_BOOL(hasGravity);
    PROP_FLOAT(gravityScalar);
    PROP_FLOAT(timeScalar);
    PROP_FLOAT(angularDamping);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    data->mass.value = 1.0f;
    data->hasGravity.value = true;
    data->gravityScalar.value = 1.0f;
    data->timeScalar.value = 1.0f;
    data->angularDamping.value = 0.03f;
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.mass)
      .set(data.isKinematic)
      .set(data.constrainPosX)
      .set(data.constrainPosY)
      .set(data.constrainPosZ)
      .set(data.constrainRotX)
      .set(data.constrainRotY)
      .set(data.constrainRotZ)
      .set(data.hasGravity)
      .set(data.gravityScalar)
      .set(data.timeScalar)
      .set(data.angularDamping)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->mass, 1.0f);
    Utils::JSON::readProp(doc, data->isKinematic, false);
    Utils::JSON::readProp(doc, data->constrainPosX, false);
    Utils::JSON::readProp(doc, data->constrainPosY, false);
    Utils::JSON::readProp(doc, data->constrainPosZ, false);
    Utils::JSON::readProp(doc, data->constrainRotX, false);
    Utils::JSON::readProp(doc, data->constrainRotY, false);
    Utils::JSON::readProp(doc, data->constrainRotZ, false);
    Utils::JSON::readProp(doc, data->hasGravity, true);
    Utils::JSON::readProp(doc, data->gravityScalar, 1.0f);
    Utils::JSON::readProp(doc, data->timeScalar, 1.0f);
    Utils::JSON::readProp(doc, data->angularDamping, 0.03f);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    ctx.fileObj.write(data.mass.resolve(obj.propOverrides));
    ctx.fileObj.write(data.isKinematic.resolve(obj.propOverrides));
    ctx.fileObj.write(data.constrainPosX.resolve(obj.propOverrides));
    ctx.fileObj.write(data.constrainPosY.resolve(obj.propOverrides));
    ctx.fileObj.write(data.constrainPosZ.resolve(obj.propOverrides));
    ctx.fileObj.write(data.constrainRotX.resolve(obj.propOverrides));
    ctx.fileObj.write(data.constrainRotY.resolve(obj.propOverrides));
    ctx.fileObj.write(data.constrainRotZ.resolve(obj.propOverrides));
    ctx.fileObj.write(data.hasGravity.resolve(obj.propOverrides));
    ctx.fileObj.write(data.gravityScalar.resolve(obj.propOverrides));
    ctx.fileObj.write(data.timeScalar.resolve(obj.propOverrides));
    ctx.fileObj.write(data.angularDamping.resolve(obj.propOverrides));
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      auto &mass = data.mass.resolve(obj.propOverrides);
      if (ImTable::add("Mass", mass))
      {
        mass = std::clamp(mass, 0.01f, std::numeric_limits<float>::max());
      }
      ImTable::addObjProp("Is Kinematic", data.isKinematic);
      ImTable::addObjProp("Constrain Pos X", data.constrainPosX);
      ImTable::addObjProp("Constrain Pos Y", data.constrainPosY);
      ImTable::addObjProp("Constrain Pos Z", data.constrainPosZ);
      ImTable::addObjProp("Constrain Rot X", data.constrainRotX);
      ImTable::addObjProp("Constrain Rot Y", data.constrainRotY);
      ImTable::addObjProp("Constrain Rot Z", data.constrainRotZ);
      ImTable::addObjProp("Has Gravity", data.hasGravity);
      ImTable::addObjProp("Gravity Scalar", data.gravityScalar);
      ImTable::addObjProp("Time Scalar", data.timeScalar);
      ImTable::addObjProp("Angular Damping", data.angularDamping);


      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
  }
}
