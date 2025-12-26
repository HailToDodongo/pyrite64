/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3dmodel.h>

#include "assets/assetManager.h"
#include "lib/matrixManager.h"
#include "renderer/drawLayer.h"
#include "scene/object.h"
#include "script/scriptTable.h"

namespace P64::Comp
{
  struct Model
  {
    static constexpr uint32_t ID = 1;

    T3DModel *model{};
    RingMat4FP matFP{};
    uint8_t layerIdx{0};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(Model);
    }

    static void initDelete([[maybe_unused]] Object& obj, Model* data, uint16_t* initData);

    static void update(Object& obj, Model* data, [[maybe_unused]] float deltaTime) {
      auto mat = data->matFP.getNext();
      t3d_mat4fp_from_srt(mat,
        obj.scale,
        obj.rot,
        obj.pos
      );
    }

    static void draw([[maybe_unused]] Object& obj, Model* data, [[maybe_unused]] float deltaTime)
    {
      if(data->layerIdx)DrawLayer::use3D(data->layerIdx);

      t3d_matrix_set(data->matFP.get(), true);
      rspq_block_run(data->model->userBlock);

      if(data->layerIdx)DrawLayer::useDefault();
    }
  };
}