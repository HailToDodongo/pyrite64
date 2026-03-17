/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assets/assetManager.h"
#include "scene/object.h"
#include "scene/components/animModel.h"
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
    P64::Renderer::MaterialInstance material;
  };
}

namespace P64::Comp
{
  void AnimModel::setMainAnim(int16_t idx) {
    animIdxMain = idx;
  }

  void AnimModel::setBlendAnim(int16_t idx) {
    if (animIdxBlend != idx) {
      t3d_anim_attach(&anims[idx], &skelAnim[idx]);
    }
    animIdxBlend = idx;
  }


  uint32_t AnimModel::getAllocSize(uint16_t* initData)
  {
    return sizeof(AnimModel);
  }

  void AnimModel::initDelete([[maybe_unused]] Object& obj, AnimModel* data, void* initData_)
  {
    auto *initData = (InitData*)initData_;
    if (initData == nullptr) {

      auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_ANIM);
      uint32_t i=0;
      while(t3d_model_iter_next(&it)) {
        t3d_anim_destroy(&data->anims[i]);
        t3d_skeleton_destroy(&data->skelAnim[i]);
        ++i;
      }
      t3d_skeleton_destroy(&data->skelMain);
      free(data->skelAnim);
      free(data->anims);

      data->~AnimModel();
      return;
    }

    new(data) AnimModel();

    data->model = (T3DModel*)AssetManager::getByIndex(initData->assetIdx);
    assert(data->model != nullptr);
    data->layerIdx = initData->layer;
    data->flags = initData->flags;
    data->material = initData->material;

    /*bool isBigTex = SceneManager::getCurrent().getConf().pipeline == SceneConf::Pipeline::BIG_TEX_256;

    if(isBigTex) {
      Renderer::BigTex::patchT3DM(*data->model);
      return;
    }*/

    // @TODO: all of this is rather hacky and unoptimized
    // @TODO: refactor and rethink animations here

    auto animCount = t3d_model_get_animation_count(data->model);

    // one main skeleton for drawing, others for potential blending
    data->skelMain = t3d_skeleton_create_buffered(data->model, 3); // @TODO: take from scene settings once added
    data->skelAnim = static_cast<T3DSkeleton*>(malloc(sizeof(T3DSkeleton) * animCount));
    data->anims = static_cast<T3DAnim*>(malloc(sizeof(T3DAnim) * animCount));

    t3d_skeleton_update(&data->skelMain);

    auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_ANIM);
    uint32_t i = 0;

    // @TODO: handles names vs indices in the public API

    //debugf("AnimModel: count=%lu\n", animCount);
    while(t3d_model_iter_next(&it)) {
      data->skelAnim[i] = t3d_skeleton_clone(&data->skelMain, false);
      data->anims[i] = t3d_anim_create(data->model, it.anim->name); // @TOOD: add  create by-index to t3d API
      t3d_anim_attach(&data->anims[i], &data->skelMain); // by default assuming anything is attached to the main skeleton
      //debugf(" - %s: %lu\n", it.anim->name, i);
      ++i;
    }

    Renderer::MaterialState state{};

    if(data->model->userBlock)return; // already recorded the model
    rspq_block_begin();

    auto boneSeg = (const T3DMat4FP*)t3d_segment_placeholder(T3D_SEGMENT_SKELETON);
    it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_OBJECT);
    while(t3d_model_iter_next(&it))
    {
      auto *mat = (P64::Renderer::Material*)it.object->material;
      mat->begin(state);
      t3d_model_draw_object(it.object, boneSeg);
      mat->end(state);
    }

    data->model->userBlock = rspq_block_end();
  }

  void AnimModel::update(Object&obj, AnimModel* data, float deltaTime) {
    if (data->animIdxMain >= 0) {
      t3d_anim_update(&data->anims[data->animIdxMain], deltaTime);
    }
    if (data->animIdxBlend >= 0) {
      t3d_anim_update(&data->anims[data->animIdxBlend], deltaTime);

      t3d_skeleton_blend(
        &data->skelMain,
        &data->skelMain,
        &data->skelAnim[data->animIdxBlend],
        data->blendFactor
      );
    }

    t3d_skeleton_update(&data->skelMain);
  }

  void AnimModel::draw(Object &obj, AnimModel* data, float deltaTime)
  {
    auto mat = data->matFP.getNext();
    t3d_mat4fp_from_srt(mat, obj.scale, obj.rot, obj.pos);

    if(data->layerIdx)DrawLayer::use3D(data->layerIdx);

    data->material.begin(obj);

    t3d_skeleton_use(&data->skelMain);
    t3d_matrix_set(mat, true);
    rspq_block_run(data->model->userBlock);

    data->material.end();
    if(data->layerIdx)DrawLayer::useDefault();
  }
}
