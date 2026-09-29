/**
* @copyright 2025 - Max Bebök
* @license MIT
*/

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

    data->collider.setOwner(&obj);
    data->collider.setFriction(initData->friction);
    data->collider.setBounce(initData->bounce);
    data->collider.setParentOffset(initData->offset);
    data->collider.setTrigger(initData->isTrigger);
    data->collider.setCollisionMask(initData->maskRead, initData->maskWrite);
    data->collider.setShapeType(static_cast<P64::Coll::ShapeType>(initData->type));
    data->collider.setHalfExtend(initData->halfExtend);

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

}
