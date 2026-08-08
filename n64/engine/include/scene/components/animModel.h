/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3dmodel.h>
#include <t3d/t3danim.h>

#include "assets/assetManager.h"
#include "lib/matrixManager.h"
#include "lib/types.h"
#include "renderer/drawLayer.h"
#include "renderer/material.h"
#include "scene/object.h"
#include "script/scriptTable.h"

namespace P64::Comp
{
  /**
   * Skinned model with animation playback.
   *
   * Animations are referenced by name-hash ("Run"_hash), resolved per-instance
   * against this model's animation list. Hashes are model-independent, so shared
   * scripts work across different models. For per-frame hot paths, resolve once
   * via findAnim() and use the index-based calls.
   *
   * Playback is organized in layers, addressed by index via getLayer(). 
   * All layers write to the same skeleton and are meant for anims targeting disjoint bones 
   * (on overlap, higher layers win). 
   * Each layer holds up to two animations blended by a factor (0 = A, 1 = B), 
   * used for manual blending and crossfades.
   */
  struct AnimModel
  {
    static constexpr uint32_t ID = 10;

    static constexpr uint32_t LAYER_COUNT = 4;
    static constexpr int16_t ANIM_NONE = -1;

    // flags for baked anim entries (see editor component)
    static constexpr uint8_t ANIM_FLAG_LOOP     = 1 << 0;
    static constexpr uint8_t ANIM_FLAG_AUTOPLAY = 1 << 1;

    // per-layer data (POD); accessed through LayerRef
    struct Layer
    {
      int16_t animA{ANIM_NONE};
      int16_t animB{ANIM_NONE};
      float factor{0.0f};       // blend between A (0) and B (1)
      float fadeTime{0.0f};
      float fadeDuration{0.0f}; // > 0 while a crossfade is running
    };

    /**
     * Handle to one layer, returned by getLayer().
     */
    struct LayerRef
    {
      AnimModel* owner;
      uint32_t index;

      void play(uint32_t nameHash);                  
      void blend(uint32_t nameHash, float factor);
      void crossfade(uint32_t nameHash, float duration);
      void stop();                                   
      void clearBlend();                             

      void setFactor(float factor);
      float getFactor() const;
      bool isFinished() const;                       

      void playByIdx(int16_t animIdx);
      void blendByIdx(int16_t animIdx, float factor);

      T3DAnim* getAnim() const;
    };

    private:
      T3DModel *model{};

      T3DSkeleton skelMain{};
      T3DSkeleton skelScratch{}; // shared blend scratch, cloned on first use
      T3DAnim *anims{};
      uint32_t *animHashes{};

      Layer layers[LAYER_COUNT]{};
      uint16_t animCount{};

      RingMat4FP matFP{};
      uint8_t layerIdx{0};
      uint8_t flags{0};

      void ensureScratchSkeleton();

      int16_t resolveChecked(uint32_t nameHash) const {
        int16_t idx = findAnim(nameHash);
        assertf(idx >= 0, "AnimModel: animation hash %08lx not found", (unsigned long)nameHash);
        return idx;
      }

      // per-layer implementation, invoked through LayerRef
      void playByIdxImpl(int16_t idx, uint32_t layer);
      void blendByIdxImpl(int16_t idx, float factor, uint32_t layer);
      void crossfadeImpl(uint32_t nameHash, float duration, uint32_t layer);
      void stopImpl(uint32_t layer);
      void clearBlendImpl(uint32_t layer);

    public:
      Renderer::MaterialInstance material{}; // keep last, variable size

      Renderer::MaterialInstance& getMatInstance() {
        return material;
      }

      /**
       * @brief Returns animation layer at given index (0-3)
       * @return animation layer
       */
      LayerRef getLayer(uint32_t idx = 0) {
        assert(idx < LAYER_COUNT);
        return LayerRef{this, idx};
      }

      /**
       * Resolves an animation name-hash to this model's animation index.
       * @return index, or ANIM_NONE if this model has no such animation
       */
      int16_t findAnim(uint32_t nameHash) const;

      /**
       * Resolves a bone name-hash to a bone index by walking the skeleton.
       * One-time lookup, cache the result (if bones are accessed per-frame).
       * @return index, or -1 if not found
       */
      int16_t findBone(uint32_t nameHash) const;

      // per-animation control (by hash, independent of layer)
      void setSpeed(uint32_t nameHash, float speed)  { t3d_anim_set_speed(&anims[resolveChecked(nameHash)], speed); }
      void setLooping(uint32_t nameHash, bool loop)  { t3d_anim_set_looping(&anims[resolveChecked(nameHash)], loop); }
      float getLength(uint32_t nameHash) const       { return t3d_anim_get_length(&anims[resolveChecked(nameHash)]); }

      T3DAnim* getAnim(int16_t idx) const {
        assert(idx >= 0 && idx < animCount);
        return &anims[idx];
      }

      T3DSkeleton& getSkeleton() { return skelMain; }

      static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData);

      static void initDelete([[maybe_unused]] Object& obj, AnimModel* data, void* initData);

      static void update(Object& obj, AnimModel* data, [[maybe_unused]] float deltaTime);

      static void draw([[maybe_unused]] Object& obj, AnimModel* data, [[maybe_unused]] float deltaTime);
  };

  // LayerRef forwards to AnimModel, defined out-of-line so AnimModel is complete.
  // All inline -> the handle compiles away entirely under optimization.
  inline void AnimModel::LayerRef::play(uint32_t nameHash) {
    owner->playByIdxImpl(owner->resolveChecked(nameHash), index);
  }
  inline void AnimModel::LayerRef::blend(uint32_t nameHash, float factor) {
    owner->blendByIdxImpl(owner->resolveChecked(nameHash), factor, index);
  }
  inline void AnimModel::LayerRef::crossfade(uint32_t nameHash, float duration) {
    owner->crossfadeImpl(nameHash, duration, index);
  }
  inline void AnimModel::LayerRef::stop()        { owner->stopImpl(index); }
  inline void AnimModel::LayerRef::clearBlend()  { owner->clearBlendImpl(index); }
  inline void AnimModel::LayerRef::playByIdx(int16_t animIdx)                { owner->playByIdxImpl(animIdx, index); }
  inline void AnimModel::LayerRef::blendByIdx(int16_t animIdx, float factor) { owner->blendByIdxImpl(animIdx, factor, index); }

  inline void AnimModel::LayerRef::setFactor(float factor) {
    owner->layers[index].factor = factor;
    owner->layers[index].fadeDuration = 0.0f; // manual control cancels a crossfade
  }
  inline float AnimModel::LayerRef::getFactor() const {
    return owner->layers[index].factor;
  }
  inline bool AnimModel::LayerRef::isFinished() const {
    const auto &l = owner->layers[index];
    if (l.animA < 0) return false;
    const auto &a = owner->anims[l.animA];
    return !a.isLooping && !a.isPlaying;
  }
  inline T3DAnim* AnimModel::LayerRef::getAnim() const {
    const auto &l = owner->layers[index];
    if (l.animA < 0) return nullptr;
    return &owner->anims[l.animA];
  }
}
