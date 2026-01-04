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

namespace P64::Comp
{
  void CollMesh::initDelete([[maybe_unused]] Object& obj, CollMesh* data, uint16_t* initData)
  {
    auto &scene = SceneManager::getCurrent();
    if (initData == nullptr) {
      if(data->meshInstance.mesh) {
        scene.getCollision().unregisterMesh(&data->meshInstance);
      }

      data->~CollMesh();
      return;
    }

    uint16_t assetIdx = initData[0];
    new(data) CollMesh();

    auto asset = (T3DModel*)AssetManager::getByIndex(assetIdx);
    auto it = t3d_model_iter_create(asset, (T3DModelChunkType)'0');
    if(t3d_model_iter_next(&it)) {
      data->meshInstance.pos = obj.pos;
      data->meshInstance.scale = obj.scale;
      data->meshInstance.mesh = Coll::Mesh::load(it.chunk);
      scene.getCollision().registerMesh(&data->meshInstance);
    }
  }
}
