/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/collBody.h"

#include "scene/scene.h"
#include "scene/sceneManager.h"
#include <cmath>

namespace
{
  struct InitData
  {
    fm_vec3_t halfExtend{};
    fm_vec3_t offset{};
    uint8_t type{};
    uint8_t isTrigger{};
    uint8_t maskRead{};
    uint8_t maskWrite{};
    float friction{};
    float bounce{};
  };
}

namespace P64::Comp
{
  void CollBody::initDelete([[maybe_unused]] Object& obj, CollBody* data, void* initData_)
  {
    InitData* initData = static_cast<InitData*>(initData_);
    auto &coll = SceneManager::getCurrent().getCollision();

    if (initData == nullptr) {
      coll.removeCollider(&data->collider);
      data->~CollBody();
      return;
    }

    new(data) CollBody();

    data->orgScale = initData->halfExtend;

    data->collider = {};
    data->collider.setShapeType(static_cast<P64::Coll::ShapeType>(initData->type));
    data->collider.setFriction(initData->friction);
    data->collider.setBounce(initData->bounce);
    data->collider.setOwner(&obj);
    data->collider.setParentOffset(initData->offset);

    fm_vec3_t scaledHalfExtend = initData->halfExtend * obj.scale;
    scaledHalfExtend.x = fabsf(scaledHalfExtend.x);
    scaledHalfExtend.y = fabsf(scaledHalfExtend.y);
    scaledHalfExtend.z = fabsf(scaledHalfExtend.z);

    data->collider.setTrigger(initData->isTrigger);
    data->collider.setCollisionMask(initData->maskRead, initData->maskWrite);
    switch(data->collider.shapeType())
    {
      case P64::Coll::ShapeType::Sphere:
        data->collider.sphereShape().radius = fmaxf(scaledHalfExtend.x, fmaxf(scaledHalfExtend.y, scaledHalfExtend.z));
      break;
      case P64::Coll::ShapeType::Box:
        data->collider.boxShape().halfSize = scaledHalfExtend;
      break;
      case P64::Coll::ShapeType::Cylinder:
        data->collider.cylinderShape().radius = fmaxf(scaledHalfExtend.x, scaledHalfExtend.z);
        data->collider.cylinderShape().halfHeight = scaledHalfExtend.y;
      break;
      case P64::Coll::ShapeType::Capsule:
        data->collider.capsuleShape().radius = fmaxf(scaledHalfExtend.x, scaledHalfExtend.z);
        data->collider.capsuleShape().innerHalfHeight = scaledHalfExtend.y;
      break;
      case P64::Coll::ShapeType::Cone:
        data->collider.coneShape().radius = fmaxf(scaledHalfExtend.x, scaledHalfExtend.z);
        data->collider.coneShape().halfHeight = scaledHalfExtend.y;
      break;
      case P64::Coll::ShapeType::Pyramid:
        data->collider.pyramidShape().baseHalfWidthX = scaledHalfExtend.x;
        data->collider.pyramidShape().baseHalfWidthZ = scaledHalfExtend.z;
        data->collider.pyramidShape().halfHeight = scaledHalfExtend.y;
      break;
    }
    if (obj.isEnabled()) {
      coll.addCollider(&data->collider);
    }
  }

  void CollBody::onEvent(Object &obj, CollBody* data, const ObjectEvent &event)
  {
    if(event.type == EVENT_TYPE_DISABLE) {
      return obj.getScene().getCollision().removeCollider(&data->collider);
    }
    if(event.type == EVENT_TYPE_ENABLE) {
      return obj.getScene().getCollision().addCollider(&data->collider);
    }
  }

  void CollBody::update(Object &obj, CollBody* data, float deltaTime)
  {
    fm_vec3_t scaledHalfExtend = data->orgScale * obj.scale;
    scaledHalfExtend.x = fabsf(scaledHalfExtend.x);
    scaledHalfExtend.y = fabsf(scaledHalfExtend.y);
    scaledHalfExtend.z = fabsf(scaledHalfExtend.z);

    switch(data->collider.shapeType())
    {
      case P64::Coll::ShapeType::Sphere:
        data->collider.sphereShape().radius = fmaxf(scaledHalfExtend.x, fmaxf(scaledHalfExtend.y, scaledHalfExtend.z));
      break;
      case P64::Coll::ShapeType::Box:
        data->collider.boxShape().halfSize = scaledHalfExtend;
      break;
      case P64::Coll::ShapeType::Cylinder:
        data->collider.cylinderShape().radius = fmaxf(scaledHalfExtend.x, scaledHalfExtend.z);
        data->collider.cylinderShape().halfHeight = scaledHalfExtend.y;
      break;
      case P64::Coll::ShapeType::Capsule:
        data->collider.capsuleShape().radius = fmaxf(scaledHalfExtend.x, scaledHalfExtend.z);
        data->collider.capsuleShape().innerHalfHeight = scaledHalfExtend.y;
      break;
      case P64::Coll::ShapeType::Cone:
        data->collider.coneShape().radius = fmaxf(scaledHalfExtend.x, scaledHalfExtend.z);
        data->collider.coneShape().halfHeight = scaledHalfExtend.y;
      break;
      case P64::Coll::ShapeType::Pyramid:
        data->collider.pyramidShape().baseHalfWidthX = scaledHalfExtend.x;
        data->collider.pyramidShape().baseHalfWidthZ = scaledHalfExtend.z;
        data->collider.pyramidShape().halfHeight = scaledHalfExtend.y;
      break;
    }
  }
}
