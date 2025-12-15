/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assets/assetManager.h"
#include "scene/object.h"
#include "assets/assetManager.h"
#include "scene/components/collMesh.h"
#include <t3d/t3dmodel.h>

#include "scene/sceneManager.h"

namespace P64::Comp
{
  void CollMesh::initDelete([[maybe_unused]] Object& obj, CollMesh* data, uint16_t* initData)
  {
    if (initData == nullptr) {
      data->~CollMesh();
      return;
    }

    uint16_t assetIdx = initData[0];
    new(data) CollMesh();

    auto &scene = SceneManager::getCurrent();

    auto asset = (T3DModel*)AssetManager::getByIndex(assetIdx);
    auto it = t3d_model_iter_create(asset, (T3DModelChunkType)'0');
    if(t3d_model_iter_next(&it)) {
      data->meshInstance.pos = {};
      data->meshInstance.rot = {};
      data->meshInstance.scale = {1.0f, 1.0f, 1.0f};
      data->meshInstance.mesh = Coll::Mesh::load(it.chunk);
      scene.getCollision().registerMesh(&data->meshInstance);
    }
  }
}
