/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/collBody.h"

#include "scene/scene.h"
#include "scene/sceneManager.h"

namespace
{
  struct InitData
  {
    fm_vec3_t halfExtend{};
    fm_vec3_t offset{};
    uint8_t flags{};
    uint8_t maskRead{};
    uint8_t maskWrite{};
  };
}

namespace P64::Comp
{
  void CollBody::initDelete([[maybe_unused]] Object& obj, CollBody* data, void* initData_)
  {
    InitData* initData = static_cast<InitData*>(initData_);
    auto &coll = SceneManager::getCurrent().getCollision();

    if (initData == nullptr) {
      coll.unregisterBCS(&data->bcs);
      data->~CollBody();
      return;
    }

    new(data) CollBody();

    data->orgScale = initData->halfExtend;

    data->bcs = {
      .center = obj.pos + initData->offset,
      .halfExtend = data->orgScale * obj.scale,
      .parentOffset = initData->offset,
      .obj = &obj,
      .maskRead = initData->maskRead,
      .maskWrite = initData->maskWrite,
      .flags = initData->flags,
    };
    coll.registerBCS(&data->bcs);
  }

  void CollBody::onEvent(Object &obj, CollBody* data, const ObjectEvent &event)
  {
    if(event.type == EVENT_TYPE_DISABLE) {
      return obj.getScene().getCollision().unregisterBCS(&data->bcs);
    }
    if(event.type == EVENT_TYPE_ENABLE) {
      return obj.getScene().getCollision().registerBCS(&data->bcs);
    }
  }

  void CollBody::update(Object &obj, CollBody* data, float deltaTime)
  {
    data->bcs.halfExtend = data->orgScale * obj.scale;
    if(data->bcs.isTrigger() || data->bcs.isFixed()) {
      data->bcs.center = obj.pos + data->bcs.parentOffset;
    }
  }
}
