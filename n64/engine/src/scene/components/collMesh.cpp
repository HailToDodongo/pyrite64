/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assets/assetManager.h"
#include "scene/object.h"
#include "assets/assetManager.h"
#include "scene/components/collMesh.h"
#include <t3d/t3dmodel.h>

#include "scene/scene.h"
#include "scene/sceneManager.h"

namespace
{
  struct InitData
  {
    uint16_t assetIdx;
    uint8_t flags;
    uint8_t _padding;
  };

  constexpr uint8_t FLAG_EXTERNAL = 1 << 0;
}

namespace P64::Comp
{
  uint32_t CollMesh::getAllocSize(uint16_t* initData)
  {
    return sizeof(CollMesh);
  }

  void CollMesh::initDelete([[maybe_unused]] Object& obj, CollMesh* data, uint16_t* initData_)
  {
    auto *initData = (InitData*)initData_;
    if (initData == nullptr) {
      if(data->meshInstance.mesh) {
        obj.getScene().getCollision().unregisterMesh(&data->meshInstance);
      }

      data->~CollMesh();
      return;
    }

    new(data) CollMesh();
    //debugf("Mesh: %d | id: %d\n", assetIdx, obj.id);
    data->flags = initData->flags;

    void *rawData = AssetManager::getByIndex(initData->assetIdx);
    data->meshInstance.object = &obj;
    data->meshInstance.mesh = Coll::Mesh::load(rawData);
    obj.getScene().getCollision().registerMesh(&data->meshInstance);
  }

  void CollMesh::onEvent(Object &obj, CollMesh* data, const ObjectEvent &event)
  {
    if(event.type == EVENT_TYPE_DISABLE) {
      return obj.getScene().getCollision().unregisterMesh(&data->meshInstance);
    }
    if(event.type == EVENT_TYPE_ENABLE) {
      obj.getScene().getCollision().registerMesh(&data->meshInstance);
    }
  }

  void CollMesh::update(Object &obj, CollMesh* data, float deltaTime)
  {
  }
}
