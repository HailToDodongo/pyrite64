/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assets/assetManager.h"
#include "scene/object.h"
#include "scene/components/model.h"
#include "assets/assetManager.h"
#include <t3d/t3dmodel.h>

#include "../../renderer/bigtex/bigtex.h"
#include "scene/sceneManager.h"

namespace P64::Comp
{
  void Model::initDelete([[maybe_unused]] Object& obj, Model* data, uint16_t* initData)
  {
    if (initData == nullptr) {
      data->~Model();
      return;
    }

    uint16_t assetIdx = initData[0];
    new(data) Model();

    data->model = (T3DModel*)AssetManager::getByIndex(assetIdx);
    assert(data->model != nullptr);
    data->layerIdx = initData[1] >> 8;

    if(data->model->userBlock)return;

    if(SceneManager::getCurrent().getConf().pipeline == SceneConf::Pipeline::BIG_TEX_256) {
      Renderer::BigTex::patchT3DM(*data->model);
    } else {
      rspq_block_begin();
        t3d_model_draw(data->model);
      data->model->userBlock = rspq_block_end();
    }

  }
}
