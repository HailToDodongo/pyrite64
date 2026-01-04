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
#include "scene/scene.h"
#include "scene/sceneManager.h"

namespace
{
  void drawModel(T3DModel *model)
  {
    T3DModelState state = t3d_model_state_create();
    state.drawConf = nullptr;
    state.lastBlendMode = 0;

    T3DModelIter it = t3d_model_iter_create(model, T3D_CHUNK_TYPE_OBJECT);
    while(t3d_model_iter_next(&it))
    {
      it.object->material->blendMode = 0;
      t3d_model_draw_material(it.object->material, &state);
      t3d_model_draw_object(it.object, nullptr);
    }

    if(state.lastVertFXFunc != T3D_VERTEX_FX_NONE)t3d_state_set_vertex_fx(T3D_VERTEX_FX_NONE, 0, 0);
  }
}

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
        drawModel(data->model);
      data->model->userBlock = rspq_block_end();
    }

  }
}
