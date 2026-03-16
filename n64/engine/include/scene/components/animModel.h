/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3dmodel.h>
#include <t3d/t3danim.h>

#include "assets/assetManager.h"
#include "lib/matrixManager.h"
#include "renderer/drawLayer.h"
#include "renderer/material.h"
#include "scene/object.h"
#include "script/scriptTable.h"

namespace P64::Comp
{
  struct AnimModel
  {
    static constexpr uint32_t ID = 10;

    private:
      T3DModel *model{};

      T3DSkeleton skelMain{};
      T3DSkeleton *skelAnim{};
      T3DAnim *anims{};

      int16_t animIdxMain{-1};
      int16_t animIdxBlend{-1};

      RingMat4FP matFP{};
      uint8_t layerIdx{0};
      uint8_t flags{0};

    public:
      Renderer::MaterialInstance material{};
      float blendFactor{0.5f};

      void setMainAnim(int16_t idx);
      void setBlendAnim(int16_t idx);

      T3DAnim* getMainAnim() {
        if (animIdxMain < 0) return nullptr;
        return &anims[animIdxMain];
      }

      T3DAnim* getBlendAnim() {
        if (animIdxBlend < 0) return nullptr;
        return &anims[animIdxBlend];
      }

      T3DAnim* getAnim(int16_t idx) const {
        return &anims[idx];
      }

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData);

    static void initDelete([[maybe_unused]] Object& obj, AnimModel* data, void* initData);

    static void update(Object& obj, AnimModel* data, [[maybe_unused]] float deltaTime);

    static void draw([[maybe_unused]] Object& obj, AnimModel* data, [[maybe_unused]] float deltaTime);
  };
}