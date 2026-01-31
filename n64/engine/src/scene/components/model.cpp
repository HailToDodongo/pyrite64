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
#include "renderer/material.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

namespace
{
  struct InitData
  {
    uint16_t assetIdx;
    uint8_t layer;
    uint8_t flags;
    P64::Renderer::Material material;
    uint8_t meshIdxCount;
    uint8_t meshIndices[];
  };

  void recordWholeModel(T3DModel *model)
  {
    rspq_block_begin();

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
    model->userBlock = rspq_block_end();
  }

  void drawNoCullFilter(P64::Comp::Model* data)
  {
    for(uint8_t i = 0; i < data->meshIdxCount; ++i) {
      auto mesh = t3d_model_get_object_by_index(data->model, data->meshIndices[i]);
      rspq_block_run(mesh->userBlock);
    }
  }

  void drawCullFilter(P64::Comp::Model* data)
  {
    for(uint8_t i = 0; i < data->meshIdxCount; ++i) {
      auto mesh = t3d_model_get_object_by_index(data->model, data->meshIndices[i]);
      if(mesh->isVisible) {
        rspq_block_run(mesh->userBlock);
        mesh->isVisible = false;
      }
    }
  }

  void drawCullNoFilter(P64::Comp::Model* data)
  {
    auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_OBJECT);
    while(t3d_model_iter_next(&it)) {
      if(it.object->isVisible) {
        rspq_block_run(it.object->userBlock);
        it.object->isVisible = false;
      }
    }
  }
}

namespace P64::Comp
{
  uint32_t Model::getAllocSize(uint16_t* initData)
  {
    return sizeof(Model) + (sizeof(uint8_t) * ((InitData*)initData)->meshIdxCount);
  }

  void Model::initDelete([[maybe_unused]] Object& obj, Model* data, void* initData_)
  {
    auto *initData = (InitData*)initData_;
    if (initData == nullptr) {
      data->~Model();
      return;
    }

    new(data) Model();

    data->model = (T3DModel*)AssetManager::getByIndex(initData->assetIdx);
    assert(data->model != nullptr);
    data->layerIdx = initData->layer;
    data->flags = initData->flags;
    data->material = initData->material;

    data->meshIdxCount = initData->meshIdxCount;
    for(uint8_t i = 0; i < initData->meshIdxCount; ++i) {
      data->meshIndices[i] = initData->meshIndices[i];
    }

    bool isBigTex = SceneManager::getCurrent().getConf().pipeline == SceneConf::Pipeline::BIG_TEX_256;
    bool separate = (data->flags & FLAG_CULLING) || (data->meshIdxCount != 0);

    if(isBigTex) {
      Renderer::BigTex::patchT3DM(*data->model);
      return;
    }

    if(separate)
    {
      auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_OBJECT);
      while(t3d_model_iter_next(&it)) {
        if(it.object->userBlock)return; // already recorded the model
        rspq_block_begin();
          t3d_model_draw_material(it.object->material, nullptr);
          t3d_model_draw_object(it.object, nullptr);

          if(it.object->material->vertexFxFunc) { // @TODO: fix this in t3d
            t3d_state_set_vertex_fx(T3D_VERTEX_FX_NONE, 0,0);
          }
        it.object->userBlock = rspq_block_end();
      }
      //t3d_state_set_vertex_fx(T3D_VERTEX_FX_NONE, 0,0);
    } else {
      if(data->model->userBlock)return; // already recorded the model
      recordWholeModel(data->model);
    }
  }

  void Model::draw(Object &obj, Model* data, float deltaTime)
  {
    auto mat = data->matFP.getNext();
    t3d_mat4fp_from_srt(mat, obj.scale, obj.rot, obj.pos);

    if(data->layerIdx)DrawLayer::use3D(data->layerIdx);

    data->material.begin(obj);

    t3d_matrix_set(mat, true);

    //debugf("[%d] data->meshIdxCount: %u separate: %d\n", obj.id, data->meshIdxCount, separate);

    if (data->flags & FLAG_CULLING) {
      auto frustum = t3d_viewport_get()->viewFrustum;
      t3d_frustum_scale(&frustum, obj.scale.x); // @TODO: handle non-uniform scale

      const T3DBvh *bvh = t3d_model_bvh_get(data->model); assert(bvh);
      t3d_model_bvh_query_frustum(bvh, &frustum);

      if(data->meshIdxCount > 0) {
        drawCullFilter(data);
      } else {
        drawCullNoFilter(data);
      }
    } else {
      if(data->meshIdxCount == 0) {
        rspq_block_run(data->model->userBlock);
      } else{
        drawNoCullFilter(data);
      }
    }

    data->material.end();
    if(data->layerIdx)DrawLayer::useDefault();
  }
}
