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

    data->offset = initData->offset;
    data->bcs = {
      .center = obj.pos + data->offset,
      .halfExtend = initData->halfExtend,
      .objectId = obj.id,
      .maskRead = initData->maskRead,
      .maskWrite = initData->maskWrite,
      .flags = initData->flags,
    };
    coll.registerBCS(&data->bcs);
  }
}
