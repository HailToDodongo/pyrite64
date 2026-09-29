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
    uint8_t maskRead{};
    uint8_t maskWrite{};
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
      if(data->meshCollider) {
        obj.getScene().getCollision().removeMeshCollider(data->meshCollider);
        data->meshCollider->destroyData();
        delete data->meshCollider;
        data->meshCollider = nullptr;
      }

      data->~CollMesh();
      return;
    }

    new(data) CollMesh();
    data->flags = initData->flags;

    void *rawData = AssetManager::getByIndex(initData->assetIdx);
    data->meshCollider = Coll::MeshCollider::createFromRawData(rawData, &obj);

    assertf(data->meshCollider, "Invalid collision asset; rebuild the ROM with the current editor version to ensure the collision asset is up to date.");
    if(!data->meshCollider) return;
    data->meshCollider->setCollisionMask(initData->maskRead, initData->maskWrite);
    if(obj.isEnabled()) {
      obj.getScene().getCollision().addMeshCollider(data->meshCollider);
    }
  }

  void CollMesh::onEvent(Object &obj, CollMesh* data, const ObjectEvent &event)
  {
    if(event.type == EVENT_TYPE_DISABLE) {
      if(data->meshCollider) {
        obj.getScene().getCollision().removeMeshCollider(data->meshCollider);
      }
      return;
    }
    if(event.type == EVENT_TYPE_ENABLE) {
      if(data->meshCollider) {
        obj.getScene().getCollision().addMeshCollider(data->meshCollider);
      }
    }
  }

  void CollMesh::update(Object &obj, CollMesh* data, float deltaTime)
  {
  }
}
