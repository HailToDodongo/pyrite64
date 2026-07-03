/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assets/assetManager.h"
#include "scene/object.h"
#include "scene/components/animModel.h"
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

    struct AnimEntry {
      uint8_t idxA;  // 0xFF = none
      uint8_t idxB;  // 0xFF = none
      uint8_t flags; // AnimModel::ANIM_FLAG_*
      uint8_t _pad;
      float speed;
      float factor;
    } animEntries[P64::Comp::AnimModel::LAYER_COUNT];

    P64::Renderer::MaterialInstance material;
  };

  constexpr uint8_t ANIM_ENTRY_NONE = 0xFF;
}

namespace P64::Comp
{
  int16_t AnimModel::findAnim(uint32_t nameHash) const
  {
    for (uint16_t i = 0; i < animCount; ++i) {
      if (animHashes[i] == nameHash) return (int16_t)i;
    }
    return ANIM_NONE;
  }

  int16_t AnimModel::findBone(uint32_t nameHash) const
  {
    auto skelRef = skelMain.skeletonRef;
    for (uint16_t i = 0; i < skelRef->boneCount; ++i) {
      const char* name = skelRef->bones[i].name;
      if (P64::crc32Runtime(name, strlen(name)) == nameHash) return (int16_t)i;
    }
    return -1;
  }

  void AnimModel::ensureScratchSkeleton()
  {
    if (!skelScratch.bones) {
      skelScratch = t3d_skeleton_clone(&skelMain, false);
    }
  }

  void AnimModel::playByIdxImpl(int16_t idx, uint32_t layer)
  {
    assert(layer < LAYER_COUNT);
    assert(idx >= 0 && idx < animCount);
    auto &l = layers[layer];

    if (l.animB >= 0 && l.animB != idx) {
      t3d_anim_set_playing(&anims[l.animB], false);
    }
    l.animB = ANIM_NONE;
    l.factor = 0.0f;
    l.fadeDuration = 0.0f;

    if (l.animA == idx) {
      t3d_anim_set_playing(&anims[idx], true);
      return;
    }
    if (l.animA >= 0) {
      t3d_anim_set_playing(&anims[l.animA], false);
    }
    l.animA = idx;
    t3d_anim_attach(&anims[idx], &skelMain);
    t3d_anim_set_time(&anims[idx], 0.0f);
    t3d_anim_set_playing(&anims[idx], true);
  }

  void AnimModel::blendByIdxImpl(int16_t idx, float factor, uint32_t layer)
  {
    assert(layer < LAYER_COUNT);
    assert(idx >= 0 && idx < animCount);
    auto &l = layers[layer];

    ensureScratchSkeleton();
    if (l.animB != idx) {
      if (l.animB >= 0) {
        t3d_anim_set_playing(&anims[l.animB], false);
      }
      t3d_anim_attach(&anims[idx], &skelScratch);
      t3d_anim_set_time(&anims[idx], 0.0f);
    }
    t3d_anim_set_playing(&anims[idx], true);
    l.animB = idx;
    l.factor = factor;
    l.fadeDuration = 0.0f; // manual blend control cancels a crossfade
  }

  void AnimModel::crossfadeImpl(uint32_t nameHash, float duration, uint32_t layer)
  {
    assert(layer < LAYER_COUNT);
    int16_t idx = resolveChecked(nameHash);
    auto &l = layers[layer];

    if (l.animA == idx && l.fadeDuration <= 0.0f) return;
    if (l.animB == idx && l.fadeDuration > 0.0f) return;

    if (duration <= 0.0f) {
      playByIdxImpl(idx, layer);
      return;
    }

    blendByIdxImpl(idx, 0.0f, layer);
    l.fadeTime = 0.0f;
    l.fadeDuration = duration;
  }

  void AnimModel::stopImpl(uint32_t layer)
  {
    assert(layer < LAYER_COUNT);
    auto &l = layers[layer];
    if (l.animA >= 0) t3d_anim_set_playing(&anims[l.animA], false);
    if (l.animB >= 0) t3d_anim_set_playing(&anims[l.animB], false);
    l = Layer{};
  }

  void AnimModel::clearBlendImpl(uint32_t layer)
  {
    assert(layer < LAYER_COUNT);
    auto &l = layers[layer];
    if (l.animB >= 0) {
      t3d_anim_set_playing(&anims[l.animB], false);
    }
    l.animB = ANIM_NONE;
    l.factor = 0.0f;
    l.fadeDuration = 0.0f;
  }

  uint32_t AnimModel::getAllocSize(uint16_t* initData)
  {
    return sizeof(AnimModel) - sizeof(Renderer::MaterialInstance) + ((InitData*)initData)->material.getSize();
  }

  void AnimModel::initDelete([[maybe_unused]] Object& obj, AnimModel* data, void* initData_)
  {
    auto *initData = (InitData*)initData_;
    if (initData == nullptr) {
      for (uint16_t i = 0; i < data->animCount; ++i) {
        t3d_anim_destroy(&data->anims[i]);
      }
      if (data->skelScratch.bones) {
        t3d_skeleton_destroy(&data->skelScratch);
      }
      t3d_skeleton_destroy(&data->skelMain);
      free(data->anims);
      free(data->animHashes);

      data->~AnimModel();
      return;
    }

    new(data) AnimModel();

    data->model = (T3DModel*)AssetManager::getByIndex(initData->assetIdx);
    assert(data->model != nullptr);
    data->layerIdx = initData->layer;
    data->flags = initData->flags;

    // struct has move/copy removed for safety and to avoid accidental copies.
    // but we still need to memcpy here, the warning is wrong anyways as it's still a trivial type
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
      memcpy(&data->material, &initData->material, initData->material.getSize());
    #pragma GCC diagnostic pop

    data->material.init();

    auto animCount = t3d_model_get_animation_count(data->model);

    // one skeleton for drawing; a second one is cloned lazily as blend scratch
    data->skelMain = t3d_skeleton_create_buffered(data->model, 3); // @TODO: take from scene settings once added
    data->anims = static_cast<T3DAnim*>(malloc(sizeof(T3DAnim) * animCount));
    data->animHashes = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * animCount));
    data->animCount = animCount;

    t3d_skeleton_update(&data->skelMain);

    auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_ANIM);
    uint32_t i = 0;
    while(t3d_model_iter_next(&it)) {
      data->anims[i] = t3d_anim_create(data->model, it.anim->name);
      data->animHashes[i] = P64::crc32Runtime(it.anim->name, strlen(it.anim->name));
      t3d_anim_attach(&data->anims[i], &data->skelMain);
      t3d_anim_set_playing(&data->anims[i], false); // only assigned anims play
      ++i;
    }

    // apply baked defaults, entry index == layer index (0 = base)
    for (uint32_t l = 0; l < LAYER_COUNT; ++l) {
      const auto &entry = initData->animEntries[l];
      auto &layer = data->layers[l];

      if (entry.idxA != ANIM_ENTRY_NONE && entry.idxA < data->animCount) {
        auto *anim = &data->anims[entry.idxA];
        t3d_anim_set_looping(anim, entry.flags & ANIM_FLAG_LOOP);
        t3d_anim_set_speed(anim, entry.speed);
        layer.animA = entry.idxA;
        if (entry.flags & ANIM_FLAG_AUTOPLAY) {
          t3d_anim_set_playing(anim, true);
        }
      }

      if (entry.idxB != ANIM_ENTRY_NONE && entry.idxB < data->animCount) {
        auto *anim = &data->anims[entry.idxB];
        data->ensureScratchSkeleton();
        t3d_anim_attach(anim, &data->skelScratch);
        t3d_anim_set_looping(anim, entry.flags & ANIM_FLAG_LOOP);
        t3d_anim_set_speed(anim, entry.speed);
        layer.animB = entry.idxB;
        layer.factor = entry.factor;
        if (entry.flags & ANIM_FLAG_AUTOPLAY) {
          t3d_anim_set_playing(anim, true);
        }
      }
    }

    Renderer::MaterialState state{};

    if(data->model->userBlock)return; // already recorded the model
    rspq_block_begin();

    auto boneSeg = (const T3DMat4FP*)t3d_segment_placeholder(T3D_SEGMENT_SKELETON);
    it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_OBJECT);
    while(t3d_model_iter_next(&it))
    {
      auto *mat = (P64::Renderer::Material*)it.object->material;
      assert(mat);
      mat->begin(state);
      t3d_model_draw_object(it.object, boneSeg);
      mat->end(state);
    }

    data->model->userBlock = rspq_block_end();
  }

  void AnimModel::update([[maybe_unused]] Object& obj, AnimModel* data, float deltaTime) {
    for (uint32_t i = 0; i < LAYER_COUNT; ++i)
    {
      auto &layer = data->layers[i];

      // a running crossfade drives the blend factor
      if (layer.fadeDuration > 0.0f) {
        layer.fadeTime += deltaTime;
        layer.factor = layer.fadeTime >= layer.fadeDuration
          ? 1.0f : (layer.fadeTime / layer.fadeDuration);
      }

      if (layer.animA >= 0) {
        t3d_anim_update(&data->anims[layer.animA], deltaTime);
      }

      if (layer.animB >= 0)
      {
        // scratch = current composite pose, then B writes its channels over it.
        // bones this layer doesn't animate blend between identical values (no-op),
        // which masks the whole-skeleton blend to this layer's channels.
        auto boneCount = data->skelMain.skeletonRef->boneCount;
        memcpy(data->skelScratch.bones, data->skelMain.bones, sizeof(T3DBone) * boneCount);

        t3d_anim_update(&data->anims[layer.animB], deltaTime);
        t3d_skeleton_blend(&data->skelMain, &data->skelMain, &data->skelScratch, layer.factor);

        // finished crossfade: B becomes the layer's animation
        if (layer.fadeDuration > 0.0f && layer.fadeTime >= layer.fadeDuration) {
          if (layer.animA >= 0) {
            t3d_anim_set_playing(&data->anims[layer.animA], false);
          }
          layer.animA = layer.animB;
          layer.animB = ANIM_NONE;
          layer.factor = 0.0f;
          layer.fadeDuration = 0.0f;
          t3d_anim_attach(&data->anims[layer.animA], &data->skelMain);
        }
      }
    }

    t3d_skeleton_update(&data->skelMain);
  }

  void AnimModel::draw(Object &obj, AnimModel* data, [[maybe_unused]] float deltaTime)
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
